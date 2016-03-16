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

#include <m3/Common.h>
#include <m3/server/Server.h>
#include <m3/stream/Serial.h>

using namespace m3;

class MyHandler;

static Server<MyHandler> *srv;
static bool run = true;

class MyHandler : public Handler<> {
public:
    MyHandler() : Handler<>(), _count() {
    }

    virtual void handle_obtain(SessionData *, RecvBuf *, GateIStream &args, uint) override {
        reply_vmsg_on(args, Errors::NOT_SUP);
        if(++_count == 5)
            srv->shutdown();
    }
    virtual void handle_close(SessionData *sess, GateIStream &args) override {
        Serial::get() << "Client closed connection.\n";
        Handler<>::handle_close(sess, args);
    }
    virtual void handle_shutdown() override {
        Serial::get() << "Kernel wants to shut down.\n";
        run = false;
    }

private:
    int _count;
};

int main() {
    for(int i = 0; run && i < 20; ++i) {
        MyHandler hdl;
        srv = new Server<MyHandler>("srvtest-server", &hdl);
        if(Errors::occurred())
            break;
        env()->backend->workloop->run();
        delete srv;
    }
    return 0;
}
