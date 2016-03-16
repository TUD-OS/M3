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
#include <m3/stream/IStringStream.h>
#include <m3/GateStream.h>
#include <m3/WorkLoop.h>
#include <m3/Log.h>

using namespace m3;

enum ArithOp {
    CALC
};

class ArithRequestHandler : public RequestHandler<ArithRequestHandler, ArithOp, 1> {
public:
    explicit ArithRequestHandler() : RequestHandler<ArithRequestHandler, ArithOp, 1>() {
        add_operation(CALC, &ArithRequestHandler::calc);
    }

    virtual size_t credits() override {
        return Server<ArithRequestHandler>::DEF_MSGSIZE;
    }

    void calc(RecvGate &gate, GateIStream &is) {
        String str;
        is >> str;

        int a,b,res = 0;
        char op;
        IStringStream istr(str);
        istr >> a >> op >> b;
        switch(op) {
            case '+':
                res = a + b;
                break;
            case '-':
                res = a - b;
                break;
            case '*':
                res = a * b;
                break;
            case '/':
                res = a / b;
                break;
        }

        OStringStream os;
        os << res;
        reply_vmsg(gate, String(os.str()));
    }
};

int main() {
    Server<ArithRequestHandler> srv("arith", new ArithRequestHandler());
    if(Errors::occurred())
        PANIC("Unable to register service 'arith'");

    env()->workloop()->run();
    return 0;
}
