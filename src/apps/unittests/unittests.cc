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

#include <m3/vfs/VFS.h>
#include <m3/stream/Standard.h>
#include <m3/VPE.h>

#include <test/TestSuiteContainer.h>

#if defined(__host__)
#include "suites/dtu/Commands.h"
#include "suites/dtu/Memory.h"
#endif
#include "suites/stream/Stream.h"
#include "suites/misc/BitField.h"
#include "suites/misc/Heap.h"
#include "suites/fs/FS.h"
#include "suites/fs2/FS2.h"

using namespace m3;

int main() {
    if(VFS::mount("/", "m3fs") != Errors::NONE) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Unable to mount m3fs as root-fs");
    }

    test::TestSuiteContainer con;

#if defined(__host__)
    con.add(new CommandsTestSuite());
    con.add(new MemoryTestSuite());
#endif
    // FIXME: FSTestSuite changes pat.bin, which is why we need to run FS2TestSuite first
    con.add(new FS2TestSuite());
    con.add(new FSTestSuite());
    con.add(new BitFieldTestSuite());
    con.add(new HeapTestSuite());
    con.add(new StreamTestSuite());

    uint32_t res = con.run();
    uint16_t total = res >> 16;
    uint16_t succ = res & 0xFFFF;

    cout << "---------------------------------------\n";
    cout << "\033[1mIn total: " << (total == succ ? "\033[1;32m" : "\033[1;31m")
            << succ << "\033[1m of " << total << " testsuites successfull\033[0;m\n";
    cout << "---------------------------------------\n";
    return 0;
}
