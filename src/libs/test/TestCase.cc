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

#include <test/TestCase.h>

namespace test {

void TestCase::do_assert(const Assert& a) {
    if(!a) {
        m3::cout << "  \033[0;31mAssert failed\033[0m in " << a.get_file()
                 << ", line " << a.get_line() << ": " << a.get_desc() << "\n";
        failed();
    }
    else
        success();
}

}
