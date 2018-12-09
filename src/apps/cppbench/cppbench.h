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

#define RUN_SUITE(name)                                             \
    m3::cout << "Running benchmark suite " << #name << " ...\n";    \
    name();                                                         \
    m3::cout << "Done\n\n";

#define RUN_BENCH(name)                                             \
    m3::cout << "-- Running benchmark " << #name << " ...\n";          \
    name();                                                         \
    m3::cout << "-- Done\n";

void bslist();
void bdlist();
void btreap();
void bfsmeta();
void bregfile();
void bmemgate();
void bsyscall();
void bpipe();
