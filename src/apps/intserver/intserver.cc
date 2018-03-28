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

#include <base/arch/host/HWInterrupts.h>
#include <base/col/SList.h>

#include <m3/com/GateStream.h>
#include <m3/server/Server.h>
#include <m3/server/EventHandler.h>

#include <sys/time.h>
#include <cstdlib>

using namespace m3;

class IntSessionData : public EventSessionData {
public:
    explicit IntSessionData(capsel_t srv_sel, HWInterrupts::IRQ irq = HWInterrupts::IRQ::KEYB)
        : EventSessionData(srv_sel),
          irq(irq) {
    }

    HWInterrupts::IRQ irq;
};

class IntEventHandler : public EventHandler<IntSessionData> {
public:
    virtual Errors::Code open(IntSessionData **sess, capsel_t srv_sel, word_t arg) override {
        *sess = new IntSessionData(srv_sel, static_cast<HWInterrupts::IRQ>(arg));
        sessions().append(*sess);
        return Errors::NONE;
    }
};

static IntEventHandler *evhandler;

class HWIrqs : public WorkItem {
public:
    explicit HWIrqs()
        : _total_pending(),
          _pending() {
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
                    for(auto &s : evhandler->sessions()) {
                        IntSessionData *sess = static_cast<IntSessionData*>(&s);
                        if(sess->gate() && sess->irq == static_cast<HWInterrupts::IRQ>(i))
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

    env()->workloop()->add(&hwirqs, true);
    env()->workloop()->run();
    return 0;
}
