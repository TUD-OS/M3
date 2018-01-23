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
#include <base/util/Time.h>

#include <m3/com/MemGate.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/File.h>
#include <m3/vfs/VFS.h>
#include <m3/VPE.h>

using namespace m3;

#define COUNT   20

#if !defined(DUMMY_BUF_SIZE)
#   define DUMMY_BUF_SIZE    8192
#endif

USED static char dummy[DUMMY_BUF_SIZE] = {'a'};

int main(int argc, char **) {
    // for the exec benchmark
    if(argc > 1) {
        Time::stop(1);
        return 0;
    }

    memset(dummy, 0, sizeof(dummy));

    cycles_t exec_time = 0;

    for(int i = 0; i < COUNT; ++i) {
        cycles_t start2 = Time::start(1);

        VPE vpe("hello");
        Errors::Code res = vpe.run([start2]() {
            cycles_t end = Time::stop(1);
            return end - start2;
        });
        if(res != Errors::NONE)
            exitmsg("VPE::run failed");

        int time = vpe.wait();
        exec_time += static_cast<cycles_t>(time);
    }

    cout << "Time for clone: " << (exec_time / COUNT) << " cycles\n";
    return 0;
}
