/*
 * Copyright (C) 2017, Lukas Landgraf <llandgraf317@gmail.com>
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

/**
 * Modifications in 2017 by Lukas Landgraf, llandgraf317@gmail.com
 * This file is copied and modified from Escape OS.
 */

#pragma once

#include <base/Common.h>
#include <base/log/Log.h>
#include <base/log/Services.h>

#include <m3/com/MemGate.h>

#include "device.h"

/**
 * Reads or writes from/to an ATA-device
 *
 * @param device the device
 * @param op the operation: OP_READ, OP_WRITE or OP_PACKET
 * @param mem the buffer in memory
 * @param offset the offset within the buffer
 * @param lba the block-address to start at
 * @param secSize the size of a sector
 * @param secCount number of sectors
 * @return true on success
 */
bool ata_readWrite(sATADevice *device, uint op, m3::MemGate &mem, size_t offset, uint64_t lba,
                   size_t secSize, size_t secCount);

/**
 * Performs a PIO-transfer
 *
 * @param device the device
 * @param op the operation: OP_READ, OP_WRITE or OP_PACKET
 * @param mem the buffer in memory
 * @param offset the offset within the buffer
 * @param secSize the size of a sector
 * @param secCount number of sectors
 * @param waitFirst whether you want to wait until the device is ready BEFORE the first read
 * @return true if successfull
 */
bool ata_transferPIO(sATADevice *device, uint op, m3::MemGate &mem, size_t offset, size_t secSize,
                     size_t secCount, bool waitFirst);

/**
 * Performs a DMA-transfer
 *
 * @param device the device
 * @param op the operation: OP_READ, OP_WRITE or OP_PACKET
 * @param mem the buffer in memory
 * @param offset the offset within the buffer
 * @param secSize the size of a sector
 * @param secCount number of sectors
 * @return true if successfull
 */
bool ata_transferDMA(sATADevice *device, uint op, m3::MemGate &mem, size_t offset,
                     size_t secSize, size_t secCount);
