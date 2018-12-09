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

#include <base/arch/host/HWInterrupts.h>
#include <base/log/Lib.h>

#include <csignal>

namespace m3 {

sigset_t HWInterrupts::_sigset;
sigset_t HWInterrupts::_emptyset;
HWInterrupts::handler_func HWInterrupts::_handler[IRQ::COUNT];
// using one signal per interrupt isn't really scalable because we're going to run out of signals
// that we can use, but it seems to be the only way to get it working. Because, unfortunatly, Linux
// discards additional signals when one is already pending. Thus, by using the same signal for all
// interrupts, we might miss interrupts.
int HWInterrupts::_irq_to_signal[IRQ::COUNT] = {
    /* TIMER */ SIGUSR1,
    /* KEYB */  SIGUSR2,
};

void HWInterrupts::set_handler(IRQ irq, handler_func func) {
    if(!sigismember(&_sigset, _irq_to_signal[0])) {
        sigemptyset(&_sigset);
        for(int i = 0; i < IRQ::COUNT; ++i)
            sigaddset(&_sigset, _irq_to_signal[i]);
    }

    _handler[irq] = func;

    sigset_t maskset;
    sigemptyset(&maskset);

    struct sigaction act;
    act.sa_sigaction = sig_handler;
    act.sa_flags = SA_SIGINFO;
    act.sa_mask = maskset;
    sigaction(_irq_to_signal[irq], &act, nullptr);
}

void HWInterrupts::sig_handler(int, siginfo_t *info, void *) {
    IRQ irq = static_cast<IRQ>(info->si_value.sival_int);
    LLOG(IRQS, "Got signal with " << irq);
    if(_handler[irq])
        _handler[irq](irq);
}

}
