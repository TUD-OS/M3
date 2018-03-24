/**
 * $Id$
 * Copyright (C) 2008 - 2014 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * Modifications in 2017 by Lukas Landgraf, llandgraf317@gmail.com
 * This file is copied from Escape OS and modified for M3.
 */

#include <base/DTU.h>
#include <base/Panic.h>
#include <m3/VPE.h>
#include <m3/com/GateStream.h>
#include <m3/com/SendGate.h>
#include <m3/com/RecvGate.h>
#include <m3/Syscalls.h>

#include "ata.h"
#include "controller.h"
#include "pcireg.h"

using namespace m3;

/* Endpoints, corresponding to EP_* in base_proxy.cc */
const uint IdeController::RECV_EP = 10;
const uint IdeController::REPLY_EP = 9;
const uint IdeController::SEND_EP = 8;

/* port-bases */
static const int PORTBASE_PRIMARY			= 0x1F0;
static const int PORTBASE_SECONDARY			= 0x170;

static const int IDE_CTRL_CLASS				= 0x01;
static const int IDE_CTRL_SUBCLASS			= 0x01;
static const int IDE_CTRL_BAR				= 4;

static const uint16_t VENDOR_INVALID	= 0xFFFF;

static const size_t BAR_OFFSET			= 0x10;

static const size_t BAR_ADDR_IO_MASK	= 0xFFFFFFFC;
static const size_t BAR_ADDR_MEM_MASK	= 0xFFFFFFF0;

static const uintptr_t IO_ADDRESS_SPACE_BASE = ((uintptr_t) 0x2 << 28);
static const uintptr_t PCI_CONFIG_SPACE_BASE = ((uintptr_t) 0x3 << 28);

static const int MSG_PCI_GET_BY_CLASS		= 900;	/* searches for a PCI device with given class */

static const size_t BMR_SEC_OFFSET			= 0x8;

static const size_t DMA_BUF_SIZE			= 64 * 1024;
static const size_t PRDT_PAGE_COUNT         = 8;

static bool ctrl_isBusResponding(sATAController* ctrl);

/* Struct containing information about the device */
static Device ideCtrl;
/* Class to model the behaviour of the IDE controller */
static IdeController * ideController;
/* Struct containing information on if dma or irqs are used, address of buffers etc. */
static sATAController ctrls[2];

static bool isMemBar(uint32_t size)
{
	return (size & 0x1) == 0;
}

static int getBarType(uint32_t bar)
{
	return (bar >> 1) & 0x3;
}

static bool isBarPrefetchable(uint32_t bar)
{
	return (bar >> 3) & 0x1;
}

IdeController::IdeController()
	: vpe(new VPE("ide_controller",
		PEDesc(PEType::COMP_IMEM, PEISA::IDE_DEV))),
	  srGate(RecvGate::create(
		8,  // order size of buffer
		8)),// order size of messages in buffer
	  sendGate(SendGate::create(&srGate,
		0, // label
		256, // credits, should be accoring to buffer size
		&recvGate)),// gate to which replies should be sent) {
	  recvGate(RecvGate::create_for(*vpe,8,8))
{
	recvGate.activate(REPLY_EP, 0x100000FF);
	srGate.activate(RECV_EP);

	// activate send gate
	sendGate.activate_for(*vpe, SEND_EP);
}

IdeController::~IdeController() {
	delete vpe;
}

IdeController*
IdeController::create() {
	IdeController * controller = new IdeController();
	controller->getVPE()->start();
	return controller;
}

VPE *
IdeController::getVPE() {
	return vpe;
}

Device
IdeController::getConfig() {
	Device dev;

    // TODO: let device be detected and hand via params
	dev.bus  = 0;
	dev.func = 0;
	dev.dev  = 0;

	dev.vendorId  = readRegs<uint16_t>(PCI_VENDOR_ID);
	dev.deviceId  = readRegs<uint16_t>(PCI_DEVICE_ID);
	dev.type      = readRegs<uint8_t>(PCI_HEADER_TYPE);
	dev.revId     = readRegs<uint8_t>(PCI_REVISION_ID);
	dev.progInterface = readRegs<uint8_t>(PCI_CLASS_CODE);
	dev.baseClass = readRegs<uint8_t>(PCI_BASE_CLASS_CODE);
	dev.subClass  = readRegs<uint8_t>(PCI_SUB_CLASS_CODE);

	if (dev.type == 0) { //Type is PCI Generic
		readRegs<uint8_t>(PCI0_INTERRUPT_LINE);
	}

	/* Fill bars with information */
	for(uint i = 0; i<6; i++) {
		fillBar(&(dev.bars[i]), i);
	}

    return dev;
}

void IdeController::waitForInterrupt()
{
	uint32_t irq = 0; // Variable for catching the interrupt "message"
	uint32_t content = 0xFFFF; // Just send some recognizable message

	SLOG(IDE_ALL, "Trying to receive msg");
	GateIStream is = receive_msg(srGate);

	is >> irq;
	SLOG(IDE_ALL, "Content irq: 0x" << fmt(irq, "x"));

	//use reply here, credits sent back
	reply_msg(is, &content, sizeof(content));
}

void IdeController::fillBar(Bar * bar, uint i)
{
	uint32_t val = readRegs<uint32_t>((uintptr_t) PCI0_BASE_ADDR0+i*4);

	SLOG(IDE_ALL, "Value of barValue is " << val);
	bar->type = val & 0x1;
	bar->addr = val & (uintptr_t) ~0xF;
	bar->flags = 0;

	writeRegs<uint32_t>((uintptr_t) PCI0_BASE_ADDR0 + i*4, 0xFFFFFFF0 | bar->type);

	bar->size = readRegs<uint32_t>(PCI0_BASE_ADDR0 + i * 4);
	if(bar->size == 0 || bar->size == 0xFFFFFFFF) {
		bar->size = 0;
	}
	else {
		/* memory bar? */
		if(isMemBar(bar->size)) {
			switch(getBarType(val)) {
				case 0x00:
					bar->flags |= Bar::BAR_MEM_32;
					break;
				case 0x02:
					bar->flags |= Bar::BAR_MEM_64;
					break;
			}
			if(isBarPrefetchable(val))
				bar->flags |= Bar::BAR_MEM_PREFETCH;
			bar->size &= BAR_ADDR_MEM_MASK;
		}
		else {
			bar->size &= BAR_ADDR_IO_MASK;
		}
		bar->size &= ~(bar->size - 1);
	}
	writeRegs<uint32_t>(BAR_OFFSET + i * 4, val);
}

template<class T> T IdeController::readPIO(uintptr_t addr)
{
	T read;
	(addr >= 0xCF0 && addr <= 0xCFF) ?
		vpe->mem().read(&read, sizeof(read), addr)
		: vpe->mem().read(&read, sizeof(read), IO_ADDRESS_SPACE_BASE + addr);
	return read;
}

template<class T> void IdeController::writePIO(uintptr_t addr, T content)
{
	(addr >= 0xCF0 && addr <= 0xCFF) ?
		vpe->mem().write(&content, sizeof(content), addr)
		: vpe->mem().write(&content, sizeof(content), IO_ADDRESS_SPACE_BASE + addr);
}

template<class T> T IdeController::readRegs(uintptr_t regAddr)
{
	T read;
	vpe->mem().read(&read, sizeof(read), PCI_CONFIG_SPACE_BASE + regAddr);
	return read;
}

template<class T> void IdeController::writeRegs(uintptr_t regAddr, T content)
{
	vpe->mem().write(&content, sizeof(content),
		PCI_CONFIG_SPACE_BASE + regAddr);
}

uint32_t
IdeController::readStatus()
{
	return readRegs<uint32_t>(PCI_COMMAND);
}

void ctrl_init(bool useDma,bool useIRQ)
{
	ssize_t i,j;

	/* Getting a new VPE */
	if (ideController == NULL)
		ideController = IdeController::create();
    ideCtrl = ideController->getConfig();

	SLOG(IDE,"Found IDE-controller ("
		<< ideCtrl.bus << "." << ideCtrl.dev << "." << ideCtrl.func << "): vendorId 0x"
		<< fmt(ideCtrl.vendorId,"x") << ", deviceId 0x" << fmt(ideCtrl.deviceId,"x") <<
		", rev " << ideCtrl.revId );

	/* ensure that the I/O space is enabled */
	uint32_t statusCmd = ideController->readStatus();
	ideController->writeRegs(PCI_COMMAND,
		(statusCmd & ((uint32_t)~0x400)) | 0x01);

	ctrls[0].id = DEVICE_PRIMARY;
	ctrls[0].irq = CTRL_IRQ_BASE;
	ctrls[0].portBase = PORTBASE_PRIMARY;

	ctrls[1].id = DEVICE_SECONDARY;
	ctrls[1].irq = CTRL_IRQ_BASE + 1;
	ctrls[1].portBase = PORTBASE_SECONDARY;

	/* request io-ports for bus-mastering */
	if(useDma && ideCtrl.bars[IDE_CTRL_BAR].addr) {
		SLOG(IDE_ALL, "DMA is active! BAR4 address: 0x"
			<< m3::fmt(ideCtrl.bars[IDE_CTRL_BAR].addr, "x"));
	}

	for(i = 0; i < 2; i++) {
		SLOG(IDE_ALL, "Initializing controller " << ctrls[i].id);
		ctrls[i].useIrq = useIRQ;
		ctrls[i].useDma = false;

		SLOG(IDE_ALL, "Portbase for controller " << i << "is 0x"
			<< m3::fmt(ctrls[i].portBase, "x"));
		SLOG(IDE_ALL, "ATA_REG_CONTROL: " << ATA_REG_CONTROL);

		/* check if the bus is empty */
		SLOG(IDE_ALL, "Checking if bus "
			<< ctrls[i].id << " is floating");
		if(!ctrl_isBusResponding(ctrls + i)) {
			SLOG(IDE, "Bus "<< ctrls[i].id << " is floating" );
			continue;
		}
		SLOG(IDE_ALL, "Bus not floating");

		if(useIRQ) {
			// set interrupt-handler
			SLOG(IDE_ALL, "Interrupts active!");
			ctrls[i].irqsem = 1; //TODO: set IRQ semaphore here, safely
			if(ctrls[i].irqsem < 0) {
				SLOG(IDE, "Unable to create irq-semaphore for IRQ " << ctrls[i].irq);
				PANIC("Unable to create irq-semaphore for IRQ " << ctrls[i].irq);
			}
		}

		/* init DMA */
		ctrls[i].bmrBase = ideCtrl.bars[IDE_CTRL_BAR].addr;
		SLOG(IDE_ALL, "ctrls[" << i << "].bmrBase is 0x"
			<< fmt(ctrls[i].bmrBase, "x"));

		if(useDma && ctrls[i].bmrBase) {
			ctrls[i].bmrBase += i * (uint) BMR_SEC_OFFSET;

			// allocate memory for PRDT and buffer
			ctrls[i].dma_prdt_virt =
			    (sPRD *) malloc(sizeof(4096*PRDT_PAGE_COUNT));
			/*original statement in Escape OS:
			    dma_prdt_virt = static_cast<sPRD*>(
				    mmapphys((uintptr_t*)&ctrls[i].dma_prdt_phys,8,
				        4096,MAP_PHYS_ALLOC));*/

			if(!ctrls[i].dma_prdt_virt)
				PANIC("Unable to allocate PRDT for controller " << ctrls[i].id);

			ctrls[i].dma_buf_virt =
			    (void *) malloc(sizeof(DMA_BUF_SIZE));
			/*original statement in Escape OS:
			    dma_buf_virt = mmapphys((uintptr_t*)&ctrls[i].dma_buf_phys,
					DMA_BUF_SIZE,DMA_BUF_SIZE,MAP_PHYS_ALLOC);*/

			if(!ctrls[i].dma_buf_virt)
				PANIC("Unable to allocate dma-buffer for controller " << ctrls[i].id);
			ctrls[i].useDma = true;
			SLOG(IDE_ALL, "useDma is true for device " << i);
		}
		else {
			SLOG(IDE_ALL, "useDma is false for device " << i);
		}

		/* init attached devices; begin with slave */
		for(j = 1; j >= 0; j--) {
			ctrls[i].devices[j].present = false;
			ctrls[i].devices[j].id = i * 2 + j;
			ctrls[i].devices[j].ctrl = ctrls + i;
			device_init(ctrls[i].devices + j);
		}
	}
	SLOG(IDE_ALL, "All controllers initialized");
}

void ctrl_deinit() {
	delete ideController;
}

sATADevice *ctrl_getDevice(uchar id) {
	return ctrls[id / 2].devices + id % 2;
}

sATAController *ctrl_getCtrl(uchar id)
{
	return ctrls + id;
}

void ctrl_outbmrb(sATAController *ctrl,uint16_t reg,uint8_t value)
{
	SLOG(IDE_ALL, "Address is 0x" << m3::fmt(ctrl->bmrBase + reg, "x")
		<< " with value 0x" << m3::fmt(value,"x"));
	ideController->writePIO<uint8_t>((uintptr_t) ctrl->bmrBase + reg,value);
}

void ctrl_outbmrl(sATAController *ctrl,uint16_t reg,uint32_t value)
{
	ideController->writePIO<uint32_t>((uintptr_t) ctrl->bmrBase + reg,value);
}

uint8_t ctrl_inbmrb(sATAController *ctrl,uint16_t reg)
{
	return ideController->readPIO<uint8_t>((uintptr_t) ctrl->bmrBase + reg);
}

void ctrl_outb(sATAController *ctrl,uint16_t reg,uint8_t value)
{
	ideController->writePIO<uint8_t>(
		(uintptr_t) ctrl->portBase + reg,value);
}

void ctrl_outwords(sATAController *ctrl,uint16_t reg, const uint16_t *buf,size_t count)
{
	size_t i;
	for(i = 0; i < count; i++) {
		ideController->writePIO<uint16_t>((uintptr_t) ctrl->portBase + reg,buf[i]);
	}
}

uint8_t ctrl_inb(sATAController *ctrl,uint16_t reg)
{
	return ideController->readPIO<uint8_t>((uintptr_t) ctrl->portBase + reg);
}

void ctrl_inwords(sATAController *ctrl,
	uint16_t reg,uint16_t *buf,size_t count)
{
	size_t i;
	for(i = 0; i < count; i++) {
		buf[i] = ideController->readPIO<uint16_t>((uintptr_t) ctrl->portBase + reg);
	}
}

void ctrl_softReset(sATAController *ctrl)
{
	uint8_t status;
	ctrl_outb(ctrl,ATA_REG_CONTROL,(uint8_t) 4);
	ctrl_outb(ctrl,ATA_REG_CONTROL,(uint8_t) 0);
	ctrl_wait(ctrl);
	do {
		status = ctrl_inb(ctrl,ATA_REG_STATUS);
	}
	while((status & (CMD_ST_BUSY | CMD_ST_READY)) != CMD_ST_READY);
}

void ctrl_waitIntrpt(sATAController *ctrl)
{
	if(!ctrl->useIrq)
		return;
	ideController->waitForInterrupt();
}

int ctrl_waitUntil(sATAController *ctrl,time_t timeout,time_t sleepTime, uint8_t set,uint8_t unset)
{
	time_t elapsed = 0;
	while(elapsed < timeout) {
		uint8_t status = ctrl_inb(ctrl,ATA_REG_STATUS);
		if(status & CMD_ST_ERROR)
			return ctrl_inb(ctrl,ATA_REG_ERROR);
		if((status & set) == set && !(status & unset))
			return 0;
		SLOG(IDE_ALL, "Status %#x" << status);
		if(sleepTime) {
			m3::DTU::get().sleep(1000 * sleepTime);
			elapsed += sleepTime;
		}
		else
			elapsed++;
	}
	return -1;
}

void ctrl_wait(sATAController *ctrl)
{
	ideController->readRegs<uint8_t>((uintptr_t) ctrl->portBase + ATA_REG_STATUS);
	ideController->readRegs<uint8_t>((uintptr_t) ctrl->portBase + ATA_REG_STATUS);
	ideController->readRegs<uint8_t>((uintptr_t) ctrl->portBase + ATA_REG_STATUS);
	ideController->readRegs<uint8_t>((uintptr_t) ctrl->portBase + ATA_REG_STATUS);
}

static bool ctrl_isBusResponding(sATAController* ctrl)
{
	ssize_t i;

	for(i = 1; i >= 0; i--) {

		// begin with slave. master should respond if there is no slave
		ideController->writePIO<uint8_t>(
			(uintptr_t) ctrl->portBase + ATA_REG_DRIVE_SELECT,i << 4);
		ctrl_wait(ctrl);

		// write some arbitrary values to some registers
		ideController->writePIO<uint8_t>((uintptr_t) ctrl->portBase + ATA_REG_ADDRESS1,0xF1);
		ideController->writePIO<uint8_t>((uintptr_t) ctrl->portBase + ATA_REG_ADDRESS2,0xF2);
		ideController->writePIO<uint8_t>((uintptr_t) ctrl->portBase + ATA_REG_ADDRESS3,0xF3);

		// if we can read them back, the bus is present
		// check for value, one must not be floating

		if(ideController->readPIO<uint8_t>(
			    (uintptr_t) ctrl->portBase + ATA_REG_ADDRESS1) == 0xF1 &&
			ideController->readPIO<uint8_t>(
				(uintptr_t) ctrl->portBase + ATA_REG_ADDRESS2) == 0xF2 &&
			ideController->readPIO<uint8_t>(
				(uintptr_t) ctrl->portBase + ATA_REG_ADDRESS3) == 0xF3)
			return true;

	}
	return false;
}
