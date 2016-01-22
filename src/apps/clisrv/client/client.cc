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

#include <m3/stream/Serial.h>
#include <m3/cap/Session.h>
#include <m3/cap/SendGate.h>
#include <m3/RecvBuf.h>
#include <m3/GateStream.h>

using namespace m3;

int main() {
    Serial &ser = Serial::get();

    for(int j = 0; j < 3; ++j) {
        String serv = "test";
        //ser << "Please enter the name of the service to use: ";
        //ser.flush();
        //ser >> serv;

        Session test(serv);
        if(Errors::last != Errors::NO_ERROR) {
            ser << "Unable to connect to '" << serv << "': " << Errors::to_string(Errors::last) << "\n";
            continue;
        }

        SendGate gate = SendGate::bind(test.obtain(1).start());

        String req,resp;
        for(int i = 0; i < 5; ++i) {
            //ser << "Please enter the string to send: ";
            //ser.flush();
            //ser >> req;
            req = "123456";
            if(req.length() == 0)
                break;

            // 0 = TEST
            GateIStream is = send_receive_vmsg(gate, 0, req);
            is >> resp;
            ser << "Got '" << resp << "'\n";
        }
    }
    return 0;
}
