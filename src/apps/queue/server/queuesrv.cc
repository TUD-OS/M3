/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#include <m3/server/Server.h>
#include <m3/server/EventHandler.h>
#include <m3/service/arch/host/Keyboard.h>
#include <m3/service/arch/host/Interrupts.h>
#include <m3/cap/Session.h>
#include <m3/Syscalls.h>
#include <m3/GateStream.h>
#include <m3/WorkLoop.h>
#include <m3/SendQueue.h>
#include <m3/Log.h>

#include <unistd.h>
#include <fcntl.h>

using namespace m3;

static Server<EventHandler> *server;

static constexpr size_t DATA_SIZE   = 256;

static char *gendata() {
    char *data = new char[DATA_SIZE];
    for(size_t i = 0; i < DATA_SIZE; ++i)
        data[i] = rand() % 256;
    return data;
}

static void timer_irq(RecvGate &, Subscriber<RecvGate&>*) {
    for(auto &h : server->handler()) {
        // skip clients that have a session but no gate yet
        if(h.gate()) {
            SendQueue::get().send(*static_cast<SendGate*>(h.gate()), gendata(),
                DATA_SIZE, SendQueue::array_deleter<char>);
        }
    }
}

int main() {
    Interrupts timerirqs("interrupts", HWInterrupts::TIMER);
    timerirqs.gate().subscribe(timer_irq);

    // now, register service
    server = new Server<EventHandler>("queuetest", new EventHandler(), nextlog2<4096>::val);

    WorkLoop::get().add(&SendQueue::get(), true);
    WorkLoop::get().run();
    return 0;
}
