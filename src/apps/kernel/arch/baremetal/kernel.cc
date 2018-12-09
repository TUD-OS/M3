/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/stream/IStringStream.h>
#include <base/tracing/Tracing.h>
#include <base/log/Kernel.h>

#include "mem/MainMemory.h"
#include "pes/PEManager.h"
#include "pes/VPEManager.h"
#include "SyscallHandler.h"
#include "WorkLoop.h"

using namespace kernel;

int main(int argc, char *argv[]) {
    if(argc < 3) {
        m3::Serial::get() << "Usage: " << argv[0] << " [-t=<timeslice>] -- <program>...\n";
        m3::Machine::shutdown();
    }

    EVENT_TRACE_INIT_KERNEL();

    KLOG(MEM, MainMemory::get());

    // create some worker threads
    m3::env()->workloop()->multithreaded(48);

    if(strncmp(argv[1], "-t=", 3) == 0) {
        VPE::set_timeslice(m3::IStringStream::read_from<cycles_t>(argv[1] + 3));
        argc -= 1;
        argv += 1;
    }

    SyscallHandler::init();
    PEManager::create();
    VPEManager::create();
    VPEManager::get().init(argc - 2, argv + 2);

    PEManager::get().init();

    KLOG(INFO, "Kernel is ready");

    m3::env()->workloop()->run();

    EVENT_TRACE_FLUSH();

    KLOG(INFO, "Shutting down");

    VPEManager::destroy();

    m3::Machine::shutdown();
}
