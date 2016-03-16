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

#include <m3/Log.h>
#include <m3/Syscalls.h>
#include <m3/GateStream.h>
#include <m3/WorkLoop.h>
#include <m3/server/Server.h>
#include <m3/server/EventHandler.h>
#include <m3/col/SList.h>
#include <m3/arch/host/HWInterrupts.h>
#include <sys/time.h>
#include <cstdlib>

using namespace m3;

class IntSessionData : public EventSessionData {
public:
    explicit IntSessionData(HWInterrupts::IRQ irq) : EventSessionData(), irq(irq) {
    }

    HWInterrupts::IRQ irq;
};

class IntEventHandler : public EventHandler {
public:
    virtual void handle_open(GateIStream &args) override {
        int irq;
        args >> irq;
        EventSessionData *sess = add_session(new IntSessionData(static_cast<HWInterrupts::IRQ>(irq)));
        reply_vmsg_on(args, Errors::NO_ERROR, sess);
    }
};

static IntEventHandler *evhandler;

class HWIrqs : public WorkItem {
public:
    explicit HWIrqs() : _total_pending(), _pending() {
    }

    void add_irq(HWInterrupts::IRQ irq) {
        _pending[irq]++;
        _total_pending++;
    }

    virtual void work() override {
        if(_total_pending > 0) {
            HWInterrupts::Guard noints;
            for(size_t i = 0; i < sizeof(_pending) / sizeof(_pending[0]); ++i) {
                for(; _pending[i] > 0; _pending[i]--) {
                    for(auto &s : *evhandler) {
                        IntSessionData *sess = static_cast<IntSessionData*>(&s);
                        if(sess->gate() && sess->irq == i)
                            send_vmsg(*static_cast<SendGate*>(sess->gate()), sess->irq);
                    }
                }
            }
            _total_pending = 0;
        }
    }

private:
    int _total_pending;
    int _pending[HWInterrupts::IRQ::COUNT];
};

static HWIrqs hwirqs;

static void irq_handler(HWInterrupts::IRQ irq) {
    hwirqs.add_irq(irq);
}

int main() {
    HWInterrupts::set_handler(HWInterrupts::TIMER, irq_handler);
    HWInterrupts::set_handler(HWInterrupts::KEYB, irq_handler);

    evhandler = new IntEventHandler();
    Server<IntEventHandler> srv("interrupts", evhandler);

    env()->backend->workloop->add(&hwirqs, true);
    env()->backend->workloop->run();
    return 0;
}
