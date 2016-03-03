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

EXTERN_C void _start();

namespace RCTMux {

static struct alignas(DTU_PKG_SIZE) syscall_tmuxctl {
    Syscalls::Operation syscall_op;
} _sc_tmuxctl = { Syscalls::TMUXRESUME };

static void notify_kernel() {
    flag_set(SIGNAL);

    DTU::get().wait_until_ready(DTU::SYSC_EP);
    DTU::get().send(DTU::SYSC_EP, &_sc_tmuxctl, sizeof(_sc_tmuxctl),
        label_t(), 0);

    while (flag_is_set(SIGNAL))
        ;
}

EXTERN_C void _interrupt_handler(int) {

    init();

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

        if (flag_is_set(ERROR)) {
            set_idle_mode();
        }
    }

    finish();
}

} /* namespace RCTMux */
