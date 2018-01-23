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

#define COUNT   4

static cycles_t exec_time = 0;

int main() {
    {
        for(int i = 0; i < COUNT; ++i) {
            cycles_t start = Time::start(0);
            VPE vpe("hello");
            exec_time += Time::stop(0) - start;
        }

        cout << "Time for VPE-creation: " << (exec_time / COUNT) << " cycles\n";
    }

    exec_time = 0;

    {
        for(int i = 0; i < COUNT; ++i) {
            VPE vpe("hello");
            cycles_t start2 = Time::start(1);
            Errors::Code res = vpe.run([start2]() {
                cycles_t end = Time::stop(1);
                return end - start2;
            });
            if(res != Errors::NONE)
                exitmsg("VPE::run failed");

            int time = vpe.wait();
            exec_time += static_cast<cycles_t>(time);
        }
    }

    cout << "Time for run: " << (exec_time / COUNT) << " cycles\n";

    exec_time = 0;

    {
        for(int i = 0; i < COUNT; ++i) {
            VPE vpe("hello");
            cycles_t start = Time::start(2);
            Errors::Code res = vpe.run([]() {
                return 0;
            });
            if(res != Errors::NONE)
                exitmsg("VPE::run failed");

            vpe.wait();
            cycles_t end = Time::stop(2);
            exec_time += end - start;
        }
    }

    cout << "Time for run+wait: " << (exec_time / COUNT) << " cycles\n";

    exec_time = 0;

    {
        VPE vpe("hello");
        for(int i = 0; i < COUNT; ++i) {
            cycles_t start = Time::start(3);
            Errors::Code res = vpe.run([]() {
                return 0;
            });
            if(res != Errors::NONE)
                exitmsg("VPE::run failed");

            vpe.wait();
            cycles_t end = Time::stop(3);
            exec_time += end - start;
        }
    }

    cout << "Time for multi-run+wait: " << (exec_time / COUNT) << " cycles\n";

    exec_time = 0;

    {
        for(int i = 0; i < COUNT; ++i) {
            VPE vpe("hello");
            cycles_t start = Time::start(4);
            const char *args[] = {"/bin/noop"};
            Errors::Code res = vpe.exec(ARRAY_SIZE(args), args);
            if(res != Errors::NONE)
                exitmsg("Unable to load " << args[0]);

            vpe.wait();
            cycles_t end = Time::stop(4);
            exec_time += end - start;
        }
    }

    cout << "Time for exec: " << (exec_time / COUNT) << " cycles\n";
    return 0;
}
