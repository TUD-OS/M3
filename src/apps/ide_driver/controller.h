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

#pragma once

#include <base/Common.h>
#include <m3/VPE.h>
#include <m3/com/GateStream.h>
#include <m3/com/SendGate.h>
#include <m3/com/RecvGate.h>

#include <pci/Device.h>

#include "device.h"
/* Defines time_t */
#include "custom_types.h"

enum {
	DEVICE_PRIMARY					= 0,
	DEVICE_SECONDARY				= 1,
};

/* device-identifier */
enum {
	DEVICE_PRIM_MASTER				= 0,
	DEVICE_PRIM_SLAVE				= 1,
	DEVICE_SEC_MASTER				= 2,
	DEVICE_SEC_SLAVE				= 3,
};

enum {
	BMR_REG_COMMAND					= 0x0,
	BMR_REG_STATUS					= 0x2,
	BMR_REG_PRDT					= 0x4,
};

enum {
	BMR_STATUS_IRQ					= 0x4,
	BMR_STATUS_ERROR				= 0x2,
	BMR_STATUS_DMA					= 0x1,
};

enum {
	BMR_CMD_START					= 0x1,
	BMR_CMD_READ					= 0x8,
};

struct sATAController;

struct Bar {
		enum {
			BAR_MEM				= 0,
			BAR_IO				= 1
		};
		enum {
			BAR_MEM_32			= 0x1,
			BAR_MEM_64			= 0x2,
			BAR_MEM_PREFETCH	= 0x4
		};

		uint type;
		uintptr_t addr;
		size_t size;
		uint flags;
};

struct Device {
		uchar bus;
		uchar dev;
		uchar func;
		uchar type;
		ushort deviceId;
		ushort vendorId;
		uchar baseClass;
		uchar subClass;
		uchar progInterface;
		uchar revId;
		uchar irq;
		Bar bars[6];
};

static const size_t DEVICE_COUNT	= 4;

static const int CTRL_IRQ_BASE		= 14;

/**
 * Class to model the behaviour of the IDE controller.
 */
class IdeController {

  public:
	/**
	 * Creates a new controller object.
	 *
	 * @return new controller object
	 */
	static IdeController * create();

	/**
	 * Read the configuration parameters from the controller and return a struct containing the
	 * information.
	 *
	 * @return the struct containing the information
	 */
	Device getConfig();

	/**
	 * Read the status register from the controller.
	 *
	 * @return the content of the status register.
	 */
	uint32_t readStatus();

	/**
	 * Template function to read from a PCI device register with the specified register address.
	 *
	 * @param the register address
	 * @return the content of the register
	 */
	template<class T> T readRegs(uintptr_t offset) {
		return device.readConfig<T>(offset);
	}

	/**
	 * Template function to write to the PCI device register with the specified content.
	 *
	 * @param the register to be written to
	 * @param the value to be written
	 */
	template<class T> void writeRegs(uintptr_t offset, T content) {
		device.writeConfig(offset, content);
	}

	/**
	 * Template function to read from the IO-registers of the device.
	 *
	 * @param the register to be read from
	 * @return the value of the register
	 */
	template<class T> T readPIO(uintptr_t regAddr) {
		return device.readReg<T>(regAddr);
	}

	/**
	 * Template function to write to some IO-register of the device.
	 *
	 * @param the address of the register
	 * @param the value to be written
	 */
	template<class T> void writePIO(uintptr_t regAddr, T content) {
		device.writeReg(regAddr, content);
	}

	/**
	 * Function to wait for an interrupt by the physical controller.
	 */
	void waitForInterrupt();

  private:
  	/**
  	 * Constructor for the class.
  	 */
  	IdeController();

  	/**
  	 * Function to fill the specified Base Address Register with information.
  	 *
  	 * @param the pointer to the BAR to be filled
  	 * @param label number of the current BAR
  	 */
  	void fillBar(Bar * bar, uint i);

  	/* The PCI device. */
	pci::ProxiedPciDevice device;
};

/**
 * Inits the controllers
 *
 * @param useDma whether to use DMA, if possible
 * @param useIRQ whether to use IRQs
 */
void ctrl_init(bool useDma,bool useIRQ);

/**
 * Deinits the controllers
 */
void ctrl_deinit();

/**
 * @param id the id
 * @return the ATA-device with given id
 */
sATADevice *ctrl_getDevice(uchar id);

/**
 * @param id the id
 * @return the ATA-Controller with given id
 */
sATAController *ctrl_getCtrl(uchar id);

/**
 * Writes <value> to the bus-master-register <reg> of the given controller
 *
 * @param ctrl the controller
 * @param reg the register
 * @param value the value
 */
void ctrl_outbmrb(sATAController *ctrl,uint16_t reg,uint8_t value);
void ctrl_outbmrl(sATAController *ctrl,uint16_t reg,uint32_t value);

/**
 * Reads a byte from the bus-master-register <reg> of the given controller
 *
 * @param ctrl the controller
 * @param reg the register
 * @return the value
 */
uint8_t ctrl_inbmrb(sATAController *ctrl,uint16_t reg);

/**
 * Writes <value> to the controller-register <reg>
 *
 * @param ctrl the controller
 * @param reg the register
 * @param value the value
 */
void ctrl_outb(sATAController *ctrl,uint16_t reg,uint8_t value);
/**
 * Writes <count> words from <buf> to the controller-register <reg>
 *
 * @param ctrl the controller
 * @param reg the register
 * @param buf the word-buffer
 * @param count the number of words
 */
void ctrl_outwords(sATAController *ctrl,uint16_t reg,const uint16_t *buf,size_t count);

/**
 * Reads a byte from the controller-register <reg>
 *
 * @param ctrl the controller
 * @param reg the register
 * @return the value
 */
uint8_t ctrl_inb(sATAController *ctrl,uint16_t reg);

/**
 * Reads <count> words from the controller-register <reg> into <buf>
 *
 * @param ctrl the controller
 * @param reg the register
 * @param buf the buffer to write the words to
 * @param count the number of words
 */
void ctrl_inwords(sATAController *ctrl,uint16_t reg,uint16_t *buf,size_t count);

/**
 * Performs a software-reset for the given controller
 *
 * @param ctrl the controller
 */
void ctrl_softReset(sATAController *ctrl);

/**
 * Waits for an interrupt with given controller
 *
 * @param ctrl the controller
 */
void ctrl_waitIntrpt(sATAController *ctrl);

/**
 * Waits for <set> to set and <unset> to unset in the status-register. If <sleepTime> is not zero,
 * it sleeps that number of milliseconds between the checks. Otherwise it checks it actively.
 * It gives up as soon as <timeout> is reached. That are milliseconds if sleepTime is not zero or
 * retries.
 *
 * @param ctrl the controller
 * @param timeout the timeout (milliseconds or retries)
 * @param sleepTime the number of milliseconds to sleep (0 = check actively)
 * @param set the bits to wait until they're set
 * @param unset the bits to wait until they're unset
 * @return 0 on success, -1 if timeout has been reached, other: value of the error-register
 */
int ctrl_waitUntil(sATAController *ctrl,time_t timeout,time_t sleepTime,uint8_t set,uint8_t unset);

/**
 * Performs a few io-port-reads (just to waste a bit of time ;))
 *
 * @param ctrl the controller
 */
void ctrl_wait(sATAController *ctrl);
