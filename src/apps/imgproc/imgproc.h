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

#include <base/Common.h>

#include <m3/vfs/File.h>

enum Mode {
    INDIR       = 0,
    DIR         = 1,
    DIR_MULTI   = 2,
    DIR_SIMPLE  = 3,
};

extern const cycles_t ACCEL_TIMES[];

void chain_direct(const char *in, size_t num, Mode mode);
void chain_indirect(const char *in, size_t num);
