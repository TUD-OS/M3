/**
 * Copyright (C) 2015, René Küttner <rene.kuettner@.tu-dresden.de>
 * Economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/arch/t3/RCTMux.h>
#include <m3/DTU.h>
#include <m3/Syscalls.h>

#include "RCTMux.h"
#include "Debug.h"

using namespace m3;

namespace RCTMux {

static struct alignas(DTU_PKG_SIZE) syscall_noop {
    Syscalls::Operation syscall_op;
} _sc_tmuxresume = { Syscalls::TMUXRESUME };

void notify_kernel() {
    DTU::get().wait_until_ready(DTU::SYSC_EP);
    DTU::get().send(DTU::SYSC_EP, &_sc_tmuxresume, sizeof(_sc_tmuxresume),
        label_t(), 0);
}

EXTERN_C void _loop() {
    setup();

    volatile m3::Env *senv = m3::env();
    while(1) {
        asm volatile ("waiti   0");

        // is there something to run?
        uintptr_t ptr = senv->entry;
        if(ptr) {
            // remember exit location
            senv->exit = reinterpret_cast<uintptr_t>(&_start);

            // tell crt0 to set this stackpointer
            reinterpret_cast<word_t*>(STACK_TOP)[-1] = 0xDEADBEEF;
            reinterpret_cast<word_t*>(STACK_TOP)[-2] = senv->sp;
            register word_t a2 __asm__ ("a2") = ptr;
            asm volatile (
                "jx    %0;" : : "a"(a2)
            );
        }
    }
}

EXTERN_C void _interrupt_handler(int) {

    init_switch();

    // check if a switch has been requested
    if (flag_is_set(SWITCHREQ)) {

        // the kernel has requested a context switch

        if (flag_is_set(STORE)) {
            store();
        }

        if (flag_is_set(RESTORE)) {
            restore();
        } else {
            set_idle_mode();
        }

        notify_kernel();

        while (flag_is_set(SWITCHREQ))
            ;

        if (flag_is_set(ERROR)) {
            set_idle_mode();
        }
    }

    finish_switch();
}

} /* namespace RCTMux */
