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
 * This file is copied and modified from Escape OS. I fitted the logging to M3 style output and
 * inserted DTU sleep commands, therefore with limited modifications.
 */

#include <base/DTU.h>

#include "ata.h"
#include "atapi.h"
#include "controller.h"
#include "device.h"

#define DEBUGGING 0

#if DEBUGGING
void device_dbg_printInfo(sATADevice * device);
#endif

static bool device_identify(sATADevice *device,uint cmd);

void device_init(sATADevice *device)
{
	uint16_t buffer[256];

	ATA_PR1(INFO, "Sending 'IDENTIFY DEVICE' to device " << device->id);
	/* first, identify the device */
	if(!device_identify(device,COMMAND_IDENTIFY)) {
		ATA_PR1(INFO, "Sending 'IDENTIFY PACKET DEVICE' to device " << device->id);
		/* if that failed, try IDENTIFY PACKET DEVICE. Perhaps its an ATAPI-device */
		if(!device_identify(device,COMMAND_IDENTIFY_PACKET)) {
			ATA_LOG(ERR, "Device " << device->id << " not present");
			return;
		}
	}

	/* TODO for now we simply disable DMA for the ATAPI-drive in my notebook, since
	 * it doesn't work there. no idea why yet :/ */
	/* note that in each word the bytes are in little endian order */
	if(strstr(device->info.modelNo,"STTSocpr")) {
		ATA_LOG(INFO, "Device " << device->id << ": Detected TSSTcorp-device. Disabling DMA");
		device->info.capabilities.DMA = 0;
	}

	device->present = 1;
	if(!device->info.general.isATAPI) {
		device->secSize = ATA_SEC_SIZE;
		device->rwHandler = ata_readWrite;
		ATA_LOG(INFO, "Device " << device->id <<" is an ATA-device" );
		/* read the partition-table */
		if(!ata_readWrite(device,OP_READ,buffer,0,device->secSize,1)) {
			if(device->ctrl->useDma && device->info.capabilities.DMA) {
				ATA_LOG(INFO, "Device " << device->id 
					<< ": Reading the partition table with DMA failed. Disabling DMA." );
				device->info.capabilities.DMA = 0;
			}
			else {
				ATA_LOG(ERR, "Device " << device->id 
					<< ": Reading the partition table with PIO failed. Retrying.");
			}
			if(!ata_readWrite(device,OP_READ,buffer,0,device->secSize,1)) {
				device->present = 0;
				ATA_LOG(INFO, "Device " << device->id 
					<< ": Unable to read partition-table! Disabling device");
				return;
			}
		}

		/* copy partitions to mem */
		ATA_PR2(INFO, "Parsing partition-table");
		part_fillPartitions(device->partTable,buffer);
	}
	else {
		size_t cap;
		/* disable DMA for reading the capacity; this seems to be necessary for vbox and some
		 * of my real machines */
		bool dma = device->info.capabilities.DMA;
		device->info.capabilities.DMA = 0;
		device->secSize = ATAPI_SEC_SIZE;
		device->rwHandler = atapi_read;
		/* pretend that the cd has 1 partition */
		device->partTable[0].present = 1;
		device->partTable[0].start = 0;
		cap = atapi_getCapacity(device);
		device->info.capabilities.DMA = dma;
		if(cap == 0) {
			cap = atapi_getCapacity(device);
			if(cap == 0) {
				ATA_LOG(INFO, "Device " << device->id 
					<< ": Reading the capacity failed again. Disabling device.");
				device->present = 0;
				return;
			}
		}
		device->partTable[0].size = cap;
		ATA_LOG(INFO, "Device "  << device->id << " is an ATAPI-device with " 
			<< device->partTable[0].size << " sectors");
	}

	if(device->ctrl->useDma && device->info.capabilities.DMA) {
		ATA_LOG(INFO, "Device " << device->id << " uses DMA");
	}
	else {
		ATA_LOG(INFO, "Device "  << device->id << " uses PIO");
	}

	ATA_PR2(INFO, "Finished device init");
}

static bool device_identify(sATADevice *device, uint cmd)
{
	uint8_t status;
	uint16_t *data;
	time_t timeout;
	sATAController *ctrl = device->ctrl;

	ATA_PR2(INFO, "Selecting device " << device->id);
	ctrl_outb(ctrl,ATA_REG_DRIVE_SELECT,(device->id & SLAVE_BIT) << 4);
	ctrl_wait(ctrl);

	/* disable interrupts */
	ctrl_outb(ctrl,ATA_REG_CONTROL,CTRL_NIEN);

	/* check whether the device exists */
	ctrl_outb(ctrl,ATA_REG_COMMAND,cmd);
	status = ctrl_inb(ctrl,ATA_REG_STATUS);
	ATA_PR1(INFO, "Got 0x" << m3::fmt(status, "X") << " from status-port");
	if(status == 0) {
		ATA_LOG(ERR, "Device "  << device->id 
			<< ": Got 0x00 from status-port, device seems not to be present");
		return false;
	}
	else {
		int res;
		/* TODO from the wiki: Because of some ATAPI drives that do not follow spec, at this point
		 * you need to check the LBAmid and LBAhi ports (0x1F4 and 0x1F5) to see if they are
		 * non-zero. If so, the drive is not ATA, and you should stop polling. */

		/* wait while busy; the other bits aren't valid while busy is set */
		time_t elapsed = 0;
		while((ctrl_inb(ctrl,ATA_REG_STATUS) & CMD_ST_BUSY) && elapsed < ATA_WAIT_TIMEOUT) {
			elapsed += 20;
			m3::DTU::get().try_sleep(true, (uint64_t) 1000 * 20);
		}
		/* wait a bit */
		ctrl_wait(ctrl);

		/* wait until ready (or error) */
		timeout = (time_t) (cmd == COMMAND_IDENTIFY_PACKET ? ATAPI_WAIT_TIMEOUT : ATA_WAIT_TIMEOUT);
		res = ctrl_waitUntil(ctrl,timeout,ATA_WAIT_SLEEPTIME,CMD_ST_DRQ,CMD_ST_BUSY);
		if(res == -1) {
			ATA_LOG(ERR, "Device " << device->id
				<< ": Timeout reached while waiting for device getting ready");
			return false;
		}
		if(res != 0) {
			ATA_LOG(ERR, "Device " << device->id << ": Error " << res << ". Assuming its not present");
			return false;
		}

		/* device is ready, read data */
		ATA_PR2(INFO, "Reading information about device");
		data = (uint16_t*)&device->info;
		ctrl_inwords(ctrl,ATA_REG_DATA,data,256);

		/* wait until DRQ and BUSY bits are unset */
		res = ctrl_waitUntil(ctrl,ATA_WAIT_TIMEOUT,ATA_WAIT_SLEEPTIME,0,CMD_ST_DRQ | CMD_ST_BUSY);
		if(res == -1) {
			ATA_LOG(ERR, "Device " << device->id
				<< ": Timeout reached while waiting for DRQ bit to clear");
			return false;
		}
		if(res != 0) {
			ATA_LOG(ERR, "Device " << device->id << ": Error " << res 
				<< ". Assuming its not present");
			return false;
		}

		/* we don't support CHS atm */
		if(device->info.capabilities.LBA == 0) {
			ATA_PR1(ERR, "Device doesn't support LBA");
			return false;
		}

		return true;
	}
}

#if DEBUGGING

void device_dbg_printInfo(sATADevice *device)
{
	size_t i;
	ATA_PR2(INFO, "oldCurCylinderCount = " <<device->info.oldCurCylinderCount);
	ATA_PR2(INFO, "oldCurHeadCount = " <<device->info.oldCurHeadCount);
	ATA_PR2(INFO, "oldCurSecsPerTrack = " <<device->info.oldCurSecsPerTrack);
	ATA_PR2(INFO, "oldCylinderCount = " <<device->info.oldCylinderCount);
	ATA_PR2(INFO, "oldHeadCount = " <<device->info.oldHeadCount);
	ATA_PR2(INFO, "oldSecsPerTrack = " <<device->info.oldSecsPerTrack);
	ATA_PR2(INFO, "oldswDMAActive = " <<device->info.oldswDMAActive);
	ATA_PR2(INFO, "oldswDMASupported = " <<device->info.oldswDMASupported);
	ATA_PR2(INFO, "oldUnformBytesPerSec = " <<device->info.oldUnformBytesPerSec);
	ATA_PR2(INFO, "oldUnformBytesPerTrack = " <<device->info.oldUnformBytesPerTrack);
	ATA_PR2(INFO, "curmaxSecsPerIntrpt = " <<device->info.curmaxSecsPerIntrpt);
	ATA_PR2(INFO, "maxSecsPerIntrpt = " <<device->info.maxSecsPerIntrpt);
	ATA_PR2(INFO, "firmwareRev = '");
	for(i = 0; i < 8; i += 2)
		ATA_PR2(INFO, "%c%c" << device->info.firmwareRev[i + 1] << device->info.firmwareRev[i]);
	ATA_PR2(INFO, "'\n");
	ATA_PR2(INFO, "modelNo = '");
	for(i = 0; i < 40; i += 2)
		ATA_PR2(INFO, "%c%c" << device->info.modelNo[i + 1] << device->info.modelNo[i]);
	ATA_PR2(INFO, "'\n");
	ATA_PR2(INFO, "serialNumber = '");
	for(i = 0; i < 20; i += 2)
		ATA_PR2(INFO, "%c%c" << device->info.serialNumber[i + 1] << device->info.serialNumber[i]);
	ATA_PR2(INFO, "'\n");
	ATA_PR2(INFO, "majorVer = 0x" << m3::fmt(device->info.majorVersion.raw, "x"));
	ATA_PR2(INFO, "minorVer = 0x" << m3::fmt(device->info.minorVersion, "x"));
	ATA_PR2(INFO, "general.isATAPI = " <<device->info.general.isATAPI);
	ATA_PR2(INFO, "general.remMediaDevice = " <<device->info.general.remMediaDevice);
	ATA_PR2(INFO, "mwDMAMode0Supp = " <<device->info.mwDMAMode0Supp);
	ATA_PR2(INFO, "mwDMAMode0Sel = " <<device->info.mwDMAMode0Sel);
	ATA_PR2(INFO, "mwDMAMode1Supp = " <<device->info.mwDMAMode1Supp);
	ATA_PR2(INFO, "mwDMAMode1Sel = " <<device->info.mwDMAMode1Sel);
	ATA_PR2(INFO, "mwDMAMode2Supp = " <<device->info.mwDMAMode2Supp);
	ATA_PR2(INFO, "mwDMAMode2Sel = " <<device->info.mwDMAMode2Sel);
	ATA_PR2(INFO, "minMwDMATransTimePerWord = " <<device->info.minMwDMATransTimePerWord);
	ATA_PR2(INFO, "recMwDMATransTime = " <<device->info.recMwDMATransTime);
	ATA_PR2(INFO, "minPIOTransTime = " <<device->info.minPIOTransTime);
	ATA_PR2(INFO, "minPIOTransTimeIncCtrlFlow = " <<device->info.minPIOTransTimeIncCtrlFlow);
	ATA_PR2(INFO, "multipleSecsValid = " <<device->info.multipleSecsValid);
	ATA_PR2(INFO, "word88Valid = " <<device->info.word88Valid);
	ATA_PR2(INFO, "words5458Valid = " <<device->info.words5458Valid);
	ATA_PR2(INFO, "words6470Valid = " <<device->info.words6470Valid);
	ATA_PR2(INFO, "userSectorCount = " <<device->info.userSectorCount);
	ATA_PR2(INFO, "Capabilities:\n");
	ATA_PR2(INFO, "	DMA = " <<device->info.capabilities.DMA);
	ATA_PR2(INFO, "	LBA = " <<device->info.capabilities.LBA);
	ATA_PR2(INFO, "	IORDYDis = " <<device->info.capabilities.IORDYDisabled);
	ATA_PR2(INFO, "	IORDYSup = " <<device->info.capabilities.IORDYSupported);
	ATA_PR2(INFO, "Features:\n");
	ATA_PR2(INFO, "	APM = " <<device->info.features.apm);
	ATA_PR2(INFO, "	autoAcousticMngmnt = " <<device->info.features.autoAcousticMngmnt);
	ATA_PR2(INFO, "	CFA = " <<device->info.features.cfa);
	ATA_PR2(INFO, "	devConfigOverlay = " <<device->info.features.devConfigOverlay);
	ATA_PR2(INFO, "	deviceReset = " <<device->info.features.deviceReset);
	ATA_PR2(INFO, "	downloadMicrocode = " <<device->info.features.downloadMicrocode);
	ATA_PR2(INFO, "	flushCache = " <<device->info.features.flushCache);
	ATA_PR2(INFO, "	flushCacheExt = " <<device->info.features.flushCacheExt);
	ATA_PR2(INFO, "	hostProtArea = " <<device->info.features.hostProtArea);
	ATA_PR2(INFO, "	lba48 = " <<device->info.features.lba48);
	ATA_PR2(INFO, "	lookAhead = " <<device->info.features.lookAhead);
	ATA_PR2(INFO, "	nop = " <<device->info.features.nop);
	ATA_PR2(INFO, "	packet = " <<device->info.features.packet);
	ATA_PR2(INFO, "	powerManagement = " <<device->info.features.powerManagement);
	ATA_PR2(INFO, "	powerupStandby = " <<device->info.features.powerupStandby);
	ATA_PR2(INFO, "	readBuffer = " <<device->info.features.readBuffer);
	ATA_PR2(INFO, "	releaseInt = " <<device->info.features.releaseInt);
	ATA_PR2(INFO, "	removableMedia = " <<device->info.features.removableMedia);
	ATA_PR2(INFO, "	removableMediaSN = " <<device->info.features.removableMediaSN);
	ATA_PR2(INFO, "	rwDMAQueued = " <<device->info.features.rwDMAQueued);
	ATA_PR2(INFO, "	securityMode = " <<device->info.features.securityMode);
	ATA_PR2(INFO, "	serviceInt = " <<device->info.features.serviceInt);
	ATA_PR2(INFO, "	setFeaturesSpinup = " <<device->info.features.setFeaturesSpinup);
	ATA_PR2(INFO, "	setMaxSecurity = " <<device->info.features.setMaxSecurity);
	ATA_PR2(INFO, "	smart = " <<device->info.features.smart);
	ATA_PR2(INFO, "	writeBuffer = " <<device->info.features.writeBuffer);
	ATA_PR2(INFO, "	writeCache = " <<device->info.features.writeCache);
	ATA_PR2(INFO, "\n");
}

#endif
