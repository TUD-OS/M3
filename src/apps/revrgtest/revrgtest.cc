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

#include <m3/stream/Standard.h>
#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>
#include <m3/com/GateStream.h>
#include <m3/VPE.h>

using namespace m3;

int main() {
    VPE child("test");

    {
        RecvGate rg = RecvGate::create(nextlog2<64>::val, nextlog2<64>::val);

        child.delegate_obj(rg.sel());
        child.fds(*VPE::self().fds());
        child.obtain_fds();

        child.run([&rg]() {
            SendGate sg = SendGate::create(&rg, 0, 64);
            int i = 0;
            while(1) {
                GateIStream is = send_receive_vmsg(sg, i, i + 1, i + 2);
                if(is.error() != Errors::NONE)
                    exitmsg("Communication failed");
                i += 3;
            }
            return 0;
        });

        for(int i = 0; i < 10; ++i) {
            int a, b, c;
            GateIStream is = receive_vmsg(rg, a, b, c);
            reply_vmsg(is, 0);

            cout << "Hello World " << a << " " << b << " " << c << "\n";
        }
    }

    child.wait();
    return 0;
}
