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

#pragma once

#include <base/arch/host/HWInterrupts.h>
#include <base/col/SList.h>
#include <base/util/Util.h>
#include <base/Log.h>

#include <pthread.h>
#include <csignal>

extern int int_target;

namespace kernel {

class Device : public m3::SListItem {
public:
    explicit Device() : m3::SListItem(), _tid(), _run(true) {
    }
    virtual ~Device() {
    }

    void trigger_irq(m3::HWInterrupts::IRQ irq) {
        if(int_target != -1)
            m3::HWInterrupts::trigger(int_target, irq);
    }
    void stop() {
        _run = false;
        pthread_kill(_tid, SIGUSR1);
        pthread_join(_tid, nullptr);
    }

    virtual void run() = 0;

protected:
    void start() {
        signal(SIGUSR1,sighandler);
        int res = pthread_create(&_tid, nullptr, thread_entry, this);
        if(res != 0)
            PANIC("pthread_create");
    }
    bool should_run() const {
        return _run;
    }

private:
    static void sighandler(int) {
    }
    static void *thread_entry(void *arg) {
        Device *d = static_cast<Device*>(arg);
        d->run();
        return nullptr;
    }

    pthread_t _tid;
    volatile bool _run;
};

}
