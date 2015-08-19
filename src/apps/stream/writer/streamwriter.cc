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

class Sender : public WorkItem {
public:
    virtual void work() override {
        if(SendQueue::get().length() < 10) {
            int value = rand() % 1000;
            LOG(DEF, "Generated val=" << value);
            for(auto &h : server->handler()) {
                if(h.gate()) {
                    static_assert((sizeof(uint64_t) % DTU_PKG_SIZE) == 0, "Wrong alignment");
                    SendQueue::get().send(*static_cast<SendGate*>(h.gate()),
                        new uint64_t(value), sizeof(uint64_t));
                }
            }
        }
    }
};

int main() {
    // now, register service
    server = new Server<EventHandler>("streamer", new EventHandler());

    WorkLoop::get().add(new Sender(), true);
    WorkLoop::get().add(&SendQueue::get(), true);
    WorkLoop::get().run();
    return 0;
}
