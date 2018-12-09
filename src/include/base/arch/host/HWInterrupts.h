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

#pragma once

#include <csignal>

namespace m3 {

class HWInterrupts {
public:
    class Guard {
    public:
        explicit Guard() {
            HWInterrupts::disable();
        }
        ~Guard() {
            HWInterrupts::enable();
        }
    };

    enum IRQ {
        TIMER,
        KEYB,
        COUNT
    };

    using handler_func = void (*)(IRQ irq);

    static void set_handler(IRQ irq, handler_func func);

    static void trigger(pid_t pid, IRQ irq) {
        sigval_t val;
        val.sival_int = irq;
        sigqueue(pid, _irq_to_signal[irq], val);
    }

    static void enable() {
        set_enabled(true);
    }
    static void disable() {
        set_enabled(false);
    }
private:
    static void set_enabled(bool enabled) {
        pthread_sigmask(SIG_SETMASK, enabled ? &_emptyset : &_sigset,nullptr);
    }

    static void sig_handler(int, siginfo_t *info, void *);

    static sigset_t _sigset;
    static sigset_t _emptyset;
    static handler_func _handler[IRQ::COUNT];
    static int _irq_to_signal[IRQ::COUNT];
};

}
