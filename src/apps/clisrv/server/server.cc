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

#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>
#include <m3/GateStream.h>
#include <m3/WorkLoop.h>
#include <m3/Log.h>

using namespace m3;

enum TestOp {
    TEST
};

class TestRequestHandler;

static Server<TestRequestHandler> *srv;

class TestRequestHandler : public RequestHandler<TestRequestHandler, TestOp, 1> {
public:
    explicit TestRequestHandler() : RequestHandler<TestRequestHandler, TestOp, 1>() {
        add_operation(TEST, &TestRequestHandler::test);
    }

    virtual size_t credits() override {
        return Server<TestRequestHandler>::DEF_MSGSIZE;
    }

    void test(RecvGate &gate, GateIStream &is) {
        String str;
        is >> str;
        char *resp = new char[str.length() + 1];
        for(size_t i = 0; i < str.length(); ++i)
            resp[str.length() - i - 1] = str[i];
        reply_vmsg(gate, String(resp,str.length()));
        delete[] resp;

        // pretend that we crash after some requests
        static int count = 0;
        if(++count == 6)
            srv->shutdown();
    }
};

int main() {
    srv = new Server<TestRequestHandler>("test", new TestRequestHandler());
    WorkLoop::get().run();
    return 0;
}
