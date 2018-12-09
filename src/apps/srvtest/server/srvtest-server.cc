/*
 * Copyright (C) 2015-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/Common.h>

#include <m3/server/Server.h>
#include <m3/session/ServerSession.h>
#include <m3/stream/Standard.h>

using namespace m3;

class MyHandler;

static Server<MyHandler> *srv;

class MyHandler : public Handler<ServerSession> {
public:
    MyHandler()
        : Handler<ServerSession>(),
          _count() {
    }

    virtual Errors::Code open(ServerSession **sess, capsel_t srv_sel, word_t) override {
        *sess = new ServerSession(srv_sel);
        return Errors::NONE;
    }
    virtual Errors::Code obtain(ServerSession *, KIF::Service::ExchangeData &) override {
        if(++_count == 5)
            srv->shutdown();
        return Errors::NOT_SUP;
    }
    virtual Errors::Code close(ServerSession *sess) override {
        cout << "Client closed connection.\n";
        delete sess;
        return Errors::NONE;
    }

private:
    int _count;
};

int main() {
    for(int i = 0; i < 10; ++i) {
        MyHandler hdl;
        srv = new Server<MyHandler>("srvtest-server", &hdl);
        if(Errors::occurred())
            break;
        env()->workloop()->run();
        delete srv;
    }
    return 0;
}
