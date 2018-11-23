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
 * This file is copied from Escape OS and modified for M3.
 */

#pragma once

#include <base/Common.h>

#include "device.h"

void atapi_softReset(sATADevice *device);

/**
 * Reads from an ATAPI-device
 *
 * @param device the device
 * @param op the operation: just OP_READ here ;)
 * @param buffer the buffer to write to
 * @param lba the block-address to start at
 * @param secSize the size of a sector
 * @param secCount number of sectors
 * @return true on success
 */
bool atapi_read(sATADevice *device, uint op, void *buffer, uint64_t lba, size_t secSize, size_t secCount);

/**
 * Determines the capacity for the given device
 *
 * @param device the device
 * @return the capacity or 0 if failed
 */
size_t atapi_getCapacity(sATADevice *device);
