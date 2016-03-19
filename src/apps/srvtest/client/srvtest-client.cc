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

#include <base/Common.h>
#include <base/stream/Serial.h>

#include <m3/session/Session.h>
#include <m3/VPE.h>

using namespace m3;

int main() {
    int failed = 0;
    for(int j = 0; j < 20; ++j) {
        Session sess("srvtest-server");
        if(Errors::occurred()) {
            Serial::get() << "Unable to create sess at srvtest-server. Retrying.\n";
            if(++failed == 20)
                break;
            --j;
            continue;
        }

        failed = 0;
        for(int i = 0; i < 10; ++i) {
            capsel_t sel = sess.obtain(1).start();
            VPE::self().free_cap(sel);

            if(Errors::last == Errors::INV_ARGS)
                break;
            if(Errors::last != Errors::NOT_SUP)
                Serial::get() << "Expected NOT_SUP, got " << Errors::to_string(Errors::last) << "\n";
        }
    }
    return 0;
}
