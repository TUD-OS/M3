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

#include <test/TestSuite.h>

namespace test {

void TestSuite::run() {
    for(auto &t : _cases) {
        m3::cout << "  Testcase \"" << t.get_name() << "\"...\n";

        t.run();
        if(t.get_failed() == 0) {
            m3::cout << "  \033[0;32mSUCCEEDED!\033[0m\n";
            success();
        }
        else {
            m3::cout << "  \033[0;31mFAILED!\033[0m\n";
            failed();
        }
    }
}

}
