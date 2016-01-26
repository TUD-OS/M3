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

#include <test/TestSuite.h>
#include <m3/Log.h>

namespace test {

void TestSuite::run() {
    for(auto &t : _cases) {
        LOG(DEF, "  Testcase \"" << t.get_name() << "\"...");

        t.run();
        if(t.get_failed() == 0) {
            LOG(DEF, "  \033[0;32mSUCCEEDED!\033[0m");
            success();
        }
        else {
            LOG(DEF, "  \033[0;31mFAILED!\033[0m");
            failed();
        }
    }
}

}
