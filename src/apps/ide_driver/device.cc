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

static bool device_identify(sATADevice *device,uint cmd);

void device_init(sATADevice *device)
{
	uint16_t buffer[256];

	SLOG(IDE_ALL, "Sending 'IDENTIFY DEVICE' to device " << device->id);
	/* first, identify the device */
	if(!device_identify(device,COMMAND_IDENTIFY)) {
		SLOG(IDE_ALL, "Sending 'IDENTIFY PACKET DEVICE' to device " << device->id);
		/* if that failed, try IDENTIFY PACKET DEVICE. Perhaps its an ATAPI-device */
		if(!device_identify(device,COMMAND_IDENTIFY_PACKET)) {
			SLOG(IDE, "Device " << device->id << " not present");
			return;
		}
	}

	/* TODO for now we simply disable DMA for the ATAPI-drive in my notebook, since
	 * it doesn't work there. no idea why yet :/ */
	/* note that in each word the bytes are in little endian order */
	if(strstr(device->info.modelNo,"STTSocpr")) {
		SLOG(IDE, "Device " << device->id << ": Detected TSSTcorp-device. Disabling DMA");
		device->info.caps.flags.DMA = 0;
	}

	device->present = 1;
	if(!device->info.general.isATAPI) {
		device->secSize = ATA_SEC_SIZE;
		device->rwHandler = ata_readWrite;
		SLOG(IDE, "Device " << device->id <<" is an ATA-device" );
		/* read the partition-table */
		if(!ata_readWrite(device,OP_READ,buffer,0,device->secSize,1)) {
			if(device->ctrl->useDma && device->info.caps.flags.DMA) {
				SLOG(IDE, "Device " << device->id
					<< ": Reading the partition table with DMA failed. Disabling DMA." );
				device->info.caps.flags.DMA = 0;
			}
			else {
				SLOG(IDE, "Device " << device->id
					<< ": Reading the partition table with PIO failed. Retrying.");
			}
			if(!ata_readWrite(device,OP_READ,buffer,0,device->secSize,1)) {
				device->present = 0;
				SLOG(IDE, "Device " << device->id
					<< ": Unable to read partition-table! Disabling device");
				return;
			}
		}

		/* copy partitions to mem */
		SLOG(IDE_ALL, "Parsing partition-table");
		part_fillPartitions(device->partTable,buffer);
	}
	else {
		size_t cap;
		/* disable DMA for reading the capacity; this seems to be necessary for vbox and some
		 * of my real machines */
		bool dma = device->info.caps.flags.DMA;
		device->info.caps.flags.DMA = 0;
		device->secSize = ATAPI_SEC_SIZE;
		device->rwHandler = atapi_read;
		/* pretend that the cd has 1 partition */
		device->partTable[0].present = 1;
		device->partTable[0].start = 0;
		cap = atapi_getCapacity(device);
		device->info.caps.flags.DMA = dma;
		if(cap == 0) {
			cap = atapi_getCapacity(device);
			if(cap == 0) {
				SLOG(IDE, "Device " << device->id
					<< ": Reading the capacity failed again. Disabling device.");
				device->present = 0;
				return;
			}
		}
		device->partTable[0].size = cap;
		SLOG(IDE, "Device "  << device->id << " is an ATAPI-device with "
			<< device->partTable[0].size << " sectors");
	}

	if(device->ctrl->useDma && device->info.caps.flags.DMA) {
		SLOG(IDE, "Device " << device->id << " uses DMA");
	}
	else {
		SLOG(IDE, "Device "  << device->id << " uses PIO");
	}

	SLOG(IDE_ALL, "Finished device init");
}

static bool device_identify(sATADevice *device, uint cmd)
{
	uint8_t status;
	uint16_t *data;
	time_t timeout;
	sATAController *ctrl = device->ctrl;

	SLOG(IDE_ALL, "Selecting device " << device->id);
	ctrl_outb(ctrl,ATA_REG_DRIVE_SELECT,(device->id & SLAVE_BIT) << 4);
	ctrl_wait(ctrl);

	/* disable interrupts */
	ctrl_outb(ctrl,ATA_REG_CONTROL,CTRL_NIEN);

	/* check whether the device exists */
	ctrl_outb(ctrl,ATA_REG_COMMAND,cmd);
	status = ctrl_inb(ctrl,ATA_REG_STATUS);
	SLOG(IDE_ALL, "Got 0x" << m3::fmt(status, "X") << " from status-port");
	if(status == 0) {
		SLOG(IDE, "Device "  << device->id
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
			SLOG(IDE, "Device " << device->id
				<< ": Timeout reached while waiting for device getting ready");
			return false;
		}
		if(res != 0) {
			SLOG(IDE, "Device " << device->id << ": Error " << res << ". Assuming its not present");
			return false;
		}

		/* device is ready, read data */
		SLOG(IDE_ALL, "Reading information about device");
		data = (uint16_t*)&device->info;
		ctrl_inwords(ctrl,ATA_REG_DATA,data,256);

		/* wait until DRQ and BUSY bits are unset */
		res = ctrl_waitUntil(ctrl,ATA_WAIT_TIMEOUT,ATA_WAIT_SLEEPTIME,0,CMD_ST_DRQ | CMD_ST_BUSY);
		if(res == -1) {
			SLOG(IDE, "Device " << device->id
				<< ": Timeout reached while waiting for DRQ bit to clear");
			return false;
		}
		if(res != 0) {
			SLOG(IDE, "Device " << device->id << ": Error " << res
				<< ". Assuming its not present");
			return false;
		}

		/* we don't support CHS atm */
		if(device->info.caps.flags.LBA == 0) {
			SLOG(IDE, "Device doesn't support LBA");
			return false;
		}

		return true;
	}
}

void device_print(sATADevice *device, m3::OStream &os)
{
	size_t i;
	os << "device[" << device->id << "]:\n";
	os << "  cylinderCount = " << device->info.oldCylinderCount << "\n";
	os << "  headCount = " << device->info.oldHeadCount << "\n";
	os << "  secsPerTrack = " << device->info.oldSecsPerTrack << "\n";
	os << "  maxSecsPerIntrpt = " << device->info.maxSecsPerIntrpt << "\n";
	os << "  firmwareRev = '";
	for(i = 0; i < 8; i += 2)
		os << device->info.firmwareRev[i + 1] << device->info.firmwareRev[i];
	os << "'\n";
	os << "  modelNo = '";
	for(i = 0; i < 40; i += 2)
		os << device->info.modelNo[i + 1] << device->info.modelNo[i];
	os << "'\n";
	os << "  serialNumber = '";
	for(i = 0; i < 20; i += 2)
		os << device->info.serialNumber[i + 1] << device->info.serialNumber[i];
	os << "'\n";
	os << "  majorVer = 0x" << m3::fmt(device->info.majorVersion.raw, "x") << "\n";
	os << "  minorVer = 0x" << m3::fmt(device->info.minorVersion, "x") << "\n";
	os << "  ATAPI = " << device->info.general.isATAPI << "\n";
	os << "  remMediaDevice = " << device->info.general.remMediaDevice << "\n";
	os << "  userSectorCount = " << device->info.userSectorCount << "\n";

	const char *caps[] = {
		"DMA", "LBA", "IORDYDis", "IORDYSup",
	};
	os << "  Capabilities = ";
	for(size_t i = 0; i < ARRAY_SIZE(caps); ++i) {
		if(device->info.caps.bits & (1UL << (i + 8)))
			os << caps[i] << " ";
	}
	os << "\n";

	const char *feat[] = {
		"SMART", "securityMode", "removableMedia", "powerManagement", "packet", "writeCache",
		"lookAhead", "releaseInt", "serviceInt", "deviceReset", "hostProtArea", "?",
		"writeBuffer", "readBuffer", "nop", "?",
		"downloadMicrocode", "rwDMAQueued", "CFA", "APM", "removableMedia", "powerupStandby",
		"setFeaturesSpinup", "?", "setMaxSecurity", "autoAcousticMngmnt", "LBA48",
		"devConfigOverlay", "flushCache", "flushCacheExt",
	};
	os << "  Features = ";
	for(size_t i = 0; i < ARRAY_SIZE(feat); ++i) {
		if(device->info.feats.bits & (1UL << i))
			os << feat[i] << " ";
	}
	os << "\n";
}
