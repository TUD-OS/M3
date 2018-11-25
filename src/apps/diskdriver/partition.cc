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

#include "partition.h"

#include <base/Compiler.h>
#include <base/stream/IStringStream.h>

#include <m3/stream/Standard.h>

#include <stdio.h>

using namespace m3;

/* offset of partition-table in MBR */
static const size_t PART_TABLE_OFFSET = 0x1BE;

/* a partition on the disk */
typedef struct {
    /* Boot indicator bit flag: 0 = no, 0x80 = bootable (or "active") */
    uint8_t bootable;
    /* start: Cylinder, Head, Sector */
    uint8_t startHead;
    uint16_t startSector : 6, startCylinder : 10;
    uint8_t systemId;
    /* end: Cylinder, Head, Sector */
    uint8_t endHead;
    uint16_t endSector : 6, endCylinder : 10;
    /* Relative Sector (to start of partition -- also equals the partition's starting LBA value) */
    uint32_t start;
    /* Total Sectors in partition */
    uint32_t size;
} PACKED sDiskPart;

void part_fillPartitions(sPartition *table, void *mbr) {
    size_t i;
    sDiskPart *src = (sDiskPart *)((uintptr_t)mbr + PART_TABLE_OFFSET);
    for(i = 0; i < PARTITION_COUNT; i++) {
        table->present = src->systemId != 0;
        table->start   = src->start;
        table->size    = src->size;
        table++;
        src++;
    }
}

void part_print(sPartition *table) {
    size_t i;
    for(i = 0; i < PARTITION_COUNT; i++) {
        cout << m3::fmt(i, "zu")
             << ": present=" << table->present
             << " start=" << table->start
             << " size=" << table->size << "\n";
        table++;
    }
}
