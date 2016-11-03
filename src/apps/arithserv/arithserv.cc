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

#include <base/stream/IStringStream.h>

#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>
#include <m3/stream/Standard.h>
#include <m3/com/GateStream.h>

using namespace m3;

enum ArithOp {
    CALC
};

class ArithRequestHandler : public RequestHandler<ArithRequestHandler, ArithOp, 1> {
public:
    explicit ArithRequestHandler() : RequestHandler<ArithRequestHandler, ArithOp, 1>() {
        add_operation(CALC, &ArithRequestHandler::calc);
    }

    void calc(GateIStream &is) {
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
        reply_vmsg(is, String(os.str()));
    }
};

int main() {
    Server<ArithRequestHandler> srv("arith", new ArithRequestHandler());
    if(Errors::occurred())
        exitmsg("Unable to register service 'arith'");

    env()->workloop()->run();
    return 0;
}
