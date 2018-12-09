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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile int wait_for_debugger = 1;

extern "C" void rust_init(int argc, char **argv);
extern "C" void rust_deinit(int status, void *arg);
extern "C" void dummy_func() {
}

extern "C" __attribute__((constructor)) void host_init(int argc, char **argv) {
    char *wait;
    if((wait = getenv("M3_WAIT")) != 0 && strstr(argv[0], wait)) {
        while(wait_for_debugger != 0) {
        }
    }

    rust_init(argc, argv);
    on_exit(rust_deinit, nullptr);
}
