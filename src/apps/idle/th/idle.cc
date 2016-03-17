/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <m3/Common.h>
#include <m3/Env.h>

#include "print.h"

typedef void (*start_func)();

extern void *_start;
EXTERN_C int run_task();

EXTERN_C void loop() {
    volatile m3::Env *senv = m3::env();
    while(1) {
        // wait for an interrupt
        // TODO is broken on the FFT core
        // TODO is broken on CM core
#if 0
        asm volatile ("waiti   0");
#endif

        // is there something to run?
        start_func ptr = reinterpret_cast<start_func>(senv->entry);
        if(ptr) {
            // remember exit location
            senv->exitaddr = reinterpret_cast<uintptr_t>(&_start);

            // tell crt0 to set this stackpointer
            reinterpret_cast<word_t*>(STACK_TOP)[-1] = 0xDEADBEEF;
            reinterpret_cast<word_t*>(STACK_TOP)[-2] = senv->sp;
            register start_func a2 __asm__ ("a2") = ptr;
            asm volatile (
                "jx    %0;" : : "a"(a2)
            );
        }

#if defined(__t2__)
        run_task();
#endif
    }
}
