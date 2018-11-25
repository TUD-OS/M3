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

/* the number of partitions per disk */
static const size_t PARTITION_COUNT = 4;

/* represents a partition (in memory) */
typedef struct {
    uchar present;
    /* start sector */
    size_t start;
    /* sector count */
    size_t size;
} sPartition;

/**
 * Fills the partition-table with the given MBR
 *
 * @param table the table to fill
 * @param mbr the content of the first sector
 */
void part_fillPartitions(sPartition *table, void *mbr);

/**
 * Prints the given partition table
 *
 * @param table the tables to print
 */
void part_print(sPartition *table);
