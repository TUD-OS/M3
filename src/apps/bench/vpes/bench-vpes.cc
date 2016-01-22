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

#include <m3/Common.h>
#include <m3/stream/Serial.h>
#include <m3/cap/VPE.h>
#include <m3/cap/MemGate.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/File.h>
#include <m3/util/Sync.h>
#include <m3/util/Profile.h>
#include <m3/Log.h>

using namespace m3;

#define COUNT   1

static cycles_t exec_time = 0;

int main() {
    if(VFS::mount("/", new M3FS("m3fs")) < 0)
        PANIC("Mounting root-fs failed");

    {
        cycles_t start = Profile::start(0);
        VPE vpe("hello");
        cycles_t end = Profile::stop(0);
        Serial::get() << "Time for VPE-creation: " << (end - start) << " cycles\n";

        for(int i = 0; i < COUNT; ++i) {
            cycles_t start2 = Profile::start(1);
            Errors::Code res = vpe.run([start2]() {
                cycles_t end = Profile::stop(1);
                return end - start2;
            });
            if(res != Errors::NO_ERROR)
                PANIC("Unable to load /bin/init: " << Errors::to_string(res));

            int time = vpe.wait();
            exec_time += time;
        }
    }

    Serial::get() << "Time for run: " << (exec_time / COUNT) << " cycles\n";

    exec_time = 0;

    {
        VPE vpe("hello");
        for(int i = 0; i < COUNT; ++i) {
            cycles_t start = Profile::start(2);
            Errors::Code res = vpe.run([]() {
                return 0;
            });
            if(res != Errors::NO_ERROR)
                PANIC("Unable to load /bin/init: " << Errors::to_string(res));

            vpe.wait();
            cycles_t end = Profile::stop(2);
            exec_time += end - start;
        }
    }

    Serial::get() << "Time for run+wait: " << (exec_time / COUNT) << " cycles\n";

    exec_time = 0;

    {
        VPE vpe("hello");
        for(int i = 0; i < COUNT; ++i) {
            cycles_t start = Profile::start(3);
            const char *args[] = {"/bin/noop"};
            Errors::Code res = vpe.exec(ARRAY_SIZE(args), args);
            if(res != Errors::NO_ERROR)
                PANIC("Unable to load " << args[0] << ": " << Errors::to_string(res));

            vpe.wait();
            cycles_t end = Profile::stop(3);
            exec_time += end - start;
        }
    }

    Serial::get() << "Time for exec: " << (exec_time / COUNT) << " cycles\n";
    return 0;
}
