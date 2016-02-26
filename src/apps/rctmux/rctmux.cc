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
#include <m3/util/Math.h>
#include <m3/util/Sync.h>
#include <m3/Syscalls.h>

#include "rctmux.h"
#include "debug.h"

using namespace m3;

#define FLAGS() (*((volatile unsigned *)RCTMUX_FLAGS_LOCAL))
#define SET_FLAG(x) (FLAGS() |= x)
#define UNSET_FLAG(x) (FLAGS() ^= x)
#define RESET_FLAGS() (FLAGS() = 0)
#define IS_SET(x) (FLAGS() & x)

static struct alignas(DTU_PKG_SIZE) syscall_noop {
    Syscalls::Operation syscall_op;
} _sc_tmuxresume = {Syscalls::TMUXRESUME };

EXTERN_C void _start();

inline void init() {
    if (!arch_init()) {
        SET_FLAG(ERROR);
    }

    SET_FLAG(INITIALIZED);
}

inline void activate_idle_mode() {
    arch_idle_mode();
}

inline void save_state() {

    while (!IS_SET(STORAGE_ATTACHED) && !IS_SET(ERROR))
        ;

    if (IS_SET(ERROR))
        return;

    if (!arch_save_state()) {
        SET_FLAG(ERROR);
    }

    UNSET_FLAG(SAVE);
}

inline void restore_state() {

    while (!IS_SET(STORAGE_ATTACHED) && !IS_SET(ERROR))
        ;

    if (IS_SET(ERROR)) {
        activate_idle_mode();
        return;
    }

    if (!arch_restore_state()) {
        SET_FLAG(ERROR);
    }

    UNSET_FLAG(RESTORE);
}

inline void wipe_mem() {
    arch_wipe_mem();
}

EXTERN_C void _loop() {
    RESET_FLAGS();
    arch_setup();

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

inline void notify_kernel() {
    DTU::get().wait_until_ready(DTU::SYSC_EP);
    DTU::get().send(DTU::SYSC_EP, &_sc_tmuxresume, sizeof(_sc_tmuxresume),
        label_t(), 0);
}

EXTERN_C void _interrupt_handler(int) {

    init();

    // check if this interrupt is related to rctmux

    if (IS_SET(SWITCHREQ)) {

        // the kernel has requested a context switch

        if (IS_SET(SAVE)) {
            save_state();
            wipe_mem();
        }

        if (IS_SET(RESTORE)) {
            restore_state();
        } else {
            activate_idle_mode();
        }

        notify_kernel();

        // wait for kernel to complete
        while (IS_SET(SWITCHREQ))
            ;
        if (IS_SET(ERROR))
            activate_idle_mode();

        arch_finalize();
    }

    RESET_FLAGS();
}
