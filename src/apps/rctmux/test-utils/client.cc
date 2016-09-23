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

#include <base/Common.h>

#include <m3/com/SendGate.h>
#include <m3/session/Session.h>
#include <m3/stream/Standard.h>

using namespace m3;

enum TestOp {
    TEST
};

int main(int, char *argv[]) {
    // the kernel does not block us atm until the service is available
    // so try to connect until it's available
    Session *sess = nullptr;
    while(sess == nullptr) {
        sess = new Session(argv[2]);
        if(sess->is_connected())
            break;

        for(volatile int x = 0; x < 10000; ++x)
            ;
        delete sess;
        sess = nullptr;
    }

    {
        SendGate sgate = SendGate::bind(sess->obtain(1).start());

        cout << argv[1] << ": Starting test...\n";

        for(int i = 0; i < 200; ++i) {
            int res;
            GateIStream reply = send_receive_vmsg(sgate, TEST);
            reply >> res;
            cout << argv[1] << ": Got " << res << " from " << argv[2] << "\n";
        }

        cout << argv[1] << ": Test finished.\n";
    }

    delete sess;

    return 0;
}
