/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/Env.h>

#include "print.h"

typedef void (*start_func)();

extern void *_start;
EXTERN_C int run_task();

EXTERN_C void loop() {
    volatile m3::Env *senv = m3::env();
    while(1) {
        // wait for an interrupt
        // TODO this is broken on the CM core on T2, on the FFT core on T3
        // TODO and as it seems, suddenly it doesn't work at all on T2
        // asm volatile ("waiti   0");

        // is there something to run?
        start_func ptr = reinterpret_cast<start_func>(senv->entry);
        if(ptr) {
#if defined(__t2__)
            // TODO why do we need that now????
            for(volatile int i = 0; i < 100000; ++i)
                ;
#endif

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
