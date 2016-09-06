/**
* Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
* Economic rights: Technische Universität Dresden (Germany)
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

#include <m3/com/GateStream.h>
#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>

using namespace m3;

enum TestOp {
    TEST
};

class TestRequestHandler : public RequestHandler<TestRequestHandler, TestOp, 1> {
public:
    explicit TestRequestHandler() : RequestHandler<TestRequestHandler, TestOp, 1>(), _cnt() {
        add_operation(TEST, &TestRequestHandler::test);
    }

    virtual size_t credits() override {
        return Server<TestRequestHandler>::DEF_MSGSIZE;
    }

    void test(GateIStream &is) {
        reply_vmsg(is, _cnt++);
    }

private:
    int _cnt;
};

int main(int argc, char **argv) {
    const char *name = argc > 1 ? argv[1] : "test";
    Server<TestRequestHandler> srv(name, new TestRequestHandler());
    env()->workloop()->run();
    return 0;
}
