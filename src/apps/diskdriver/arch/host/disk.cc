/*
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

#include <base/log/Services.h>

#include <m3/stream/Standard.h>

#include "../../disk.h"
#include "../../partition.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace m3;

static int disk_fd      = -1;
static off_t disk_size  = 0;
static sPartition parts[PARTITION_COUNT];

void disk_init(bool, bool, const char *disk) {
    // open image
    if((disk_fd = open(disk, O_RDWR)) == -1)
        exitmsg("Unable to open disk image '" << disk << "': " << strerror(errno));

    // determine image size
    struct stat info;
    if(fstat(disk_fd, &info) == -1)
        exitmsg("Unable to stat disk image '" << disk << "': " << strerror(errno));
    disk_size = info.st_size;

    SLOG(IDE, "Found disk device (" << (disk_size / (1024 * 1024)) << " MiB)");

    // read partition table
    char partbuf[512];
    pread(disk_fd, partbuf, sizeof(partbuf), 0);
    part_fillPartitions(parts, partbuf);

    for(size_t p = 0; p < PARTITION_COUNT; p++) {
        if(parts[p].present)
            SLOG(IDE, "Registered partition " << p << ": "
                                              << parts[p].start * 512 << ", "
                                              << parts[p].size * 512);
    }
}

void disk_deinit() {
}

bool disk_exists(size_t dev) {
    return dev < PARTITION_COUNT && parts[dev].present == 1;
}

void disk_read(size_t dev, MemGate &mem, size_t memoff, size_t offset, size_t count) {
    sPartition *part = parts + dev;

    if(offset + count > part->size * 512 || offset + count <= offset) {
        SLOG(IDE, "Invalid read-request: offset=" << offset << ", count=" << count
                                                  << ", partSize=" << part->size * 512);
        return;
    }

    offset += part->start * 512;

    char buffer[4096];
    SLOG(IDE_ALL, "Reading " << count << " bytes @ " << offset << " from device " << dev);
    while(count > 0) {
        size_t amount = Math::min(static_cast<size_t>(count), sizeof(buffer));
        pread(disk_fd, buffer, amount, static_cast<off_t>(offset));
        mem.write(buffer, amount, memoff);

        offset += amount;
        memoff += amount;
        count -= amount;
    }
}

void disk_write(size_t dev, MemGate &mem, size_t memoff, size_t offset, size_t count) {
    sPartition *part = parts + dev;

    if(offset + count > part->size * 512 || offset + count <= offset) {
        SLOG(IDE, "Invalid write-request: offset=" << offset << ", count=" << count
                                                   << ", partSize=" << part->size * 512);
        return;
    }

    offset += part->start * 512;

    char buffer[4096];
    SLOG(IDE_ALL, "Writing " << count << " bytes @ " << offset << " to device " << dev);
    while(count > 0) {
        size_t amount = Math::min(static_cast<size_t>(count), sizeof(buffer));
        mem.read(buffer, amount, memoff);
        pwrite(disk_fd, buffer, amount, static_cast<off_t>(offset));

        offset += amount;
        memoff += amount;
        count -= amount;
    }
}
