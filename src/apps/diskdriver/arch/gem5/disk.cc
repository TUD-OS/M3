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

#include "ata.h"
#include "controller.h"
#include "device.h"

#include "../../custom_types.h"
#include "../../disk.h"
#include "../../partition.h"

using namespace m3;

struct ATAPartitionDevice;

static size_t drvCount = 0;
static ATAPartitionDevice *devs[PARTITION_COUNT * DEVICE_COUNT];

struct ATAPartitionDevice {
    uint32_t id;
    uint32_t partition;

    explicit ATAPartitionDevice(uint32_t id, uint32_t partition)
        : id(id), partition(partition) {
    }
};

static const int RETRY_COUNT    = 3;

void disk_init(bool useDma, bool useIRQ, const char *) {
    ctrl_init(useDma, useIRQ);

    uint deviceIds[] = {DEVICE_PRIM_MASTER, DEVICE_PRIM_SLAVE, DEVICE_SEC_MASTER, DEVICE_SEC_SLAVE};
    for(size_t i = 0; i < DEVICE_COUNT; i++) {
        sATADevice *ataDev = ctrl_getDevice(deviceIds[i]);
        if(ataDev->present == 0)
            continue;

        size_t size = (ataDev->info.userSectorCount * 512) / (1024 * 1024);
        SLOG(IDE, "Found disk device '" << device_model_name(ataDev) << "' (" << size << " MiB)");

        /* register device for every partition */
        for(size_t p = 0; p < PARTITION_COUNT; p++) {
            if(ataDev->partTable[p].present) {
                devs[drvCount] = new ATAPartitionDevice(ataDev->id, p);
                SLOG(IDE, "Registered partition " << drvCount
                                                  << " (device " << ataDev->id
                                                  << ", partition " << p + 1 << ")");

                drvCount++;
            }
        }
    }
}

void disk_deinit() {
    ctrl_deinit();
}

bool disk_exists(size_t dev) {
    return dev < ARRAY_SIZE(devs) && devs[dev] != nullptr;
}

void disk_read(size_t dev, MemGate &mem, size_t memoff, size_t offset, size_t count) {
    ATAPartitionDevice *pdev = devs[dev];
    sATADevice *ataDev = ctrl_getDevice(pdev->id);
    sPartition *part = ataDev->partTable + pdev->partition;

    /* we have to check whether it is at least one sector. otherwise ATA can't
     * handle the request */
    if(offset + count > part->size * ataDev->secSize || offset + count <= offset) {
        SLOG(IDE, "Invalid read-request: offset=" << offset << ", count=" << count
                                                  << ", partSize=" << part->size * ataDev->secSize
                                                  << " (device " << ataDev->id << ")");
        return;
    }

    // TODO cache this
    ctrl_setupDMA(mem);

    size_t rcount = m3::Math::round_up(count, ataDev->secSize);
    SLOG(IDE_ALL, "Reading " << rcount << " bytes @ " << offset
                             << " from device " << ataDev->id);

    int i;
    for(i = 0; i < RETRY_COUNT; i++) {
        if(i > 0)
            SLOG(IDE, "Read failed; retry " << i);
        if(ataDev->rwHandler(ataDev, OP_READ, mem, memoff,
                             offset / ataDev->secSize + part->start,
                             ataDev->secSize, rcount / ataDev->secSize)) {
            return;
        }
    }

    SLOG(IDE, "Giving up after " << i << " retries");
}

void disk_write(size_t dev, MemGate &mem, size_t memoff, size_t offset, size_t count) {
    ATAPartitionDevice *pdev = devs[dev];
    sATADevice *ataDev = ctrl_getDevice(pdev->id);
    sPartition *part = ataDev->partTable + pdev->partition;

    if(offset + count > part->size * ataDev->secSize || offset + count <= offset) {
        SLOG(IDE, "Invalid write-request: offset=0x" << m3::fmt(offset, "x") << ", count=" << count
                                                     << ", partSize=" << part->size * ataDev->secSize
                                                     << " (device " << ataDev->id << ")");
        return;
    }

    // TODO cache this
    ctrl_setupDMA(mem);

    SLOG(IDE_ALL, "Writing " << count << " bytes @ 0x" << m3::fmt(offset, "x")
                             << " to device " << ataDev->id);

    int i;
    for(i = 0; i < RETRY_COUNT; i++) {
        if(i > 0)
            SLOG(IDE, "Write failed; retry " << i);
        if(ataDev->rwHandler(ataDev, OP_WRITE, mem, memoff,
                             offset / ataDev->secSize + part->start,
                             ataDev->secSize, count / ataDev->secSize)) {
            return;
        }
    }

    SLOG(IDE, "Giving up after " << i << " retries");
}
