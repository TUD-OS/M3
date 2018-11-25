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

#pragma once

#include <m3/com/MemGate.h>

/**
 * Initializes all disk devices
 */
void disk_init(bool useDma, bool useIRQ, const char *disk);

/**
 * Deinitializes all disk devices
 */
void disk_deinit();

/**
 * @return true if the given disk device exists
 */
bool disk_exists(size_t dev);

/**
 * Reads <count> bytes from disk at <offset> to <mem>+<memoff>.
 */
void disk_read(size_t dev, m3::MemGate &mem, size_t memoff, size_t offset, size_t count);

/**
 * Writes <count> bytes from given device from <mem>+<memoff> to <offset> on disk.
 */
void disk_write(size_t dev, m3::MemGate &mem, size_t memoff, size_t offset, size_t count);
