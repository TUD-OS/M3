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

#include <base/stream/Serial.h>
#include <base/tracing/Tracing.h>
#include <base/DTU.h>
#include <base/Log.h>
#include <base/WorkLoop.h>

#include "pes/PEManager.h"
#include "SyscallHandler.h"

using namespace kernel;

int main(int argc, char *argv[]) {
    m3::Serial &ser = m3::Serial::get();
    if(argc < 2) {
        ser << "Usage: " << argv[0] << " <program>...\n";
        m3::Machine::shutdown();
    }

    EVENT_TRACE_INIT_KERNEL();

    ser << "Initializing PEs...\n";

    PEManager::create();
    PEManager::get().load(argc - 1, argv + 1);

    m3::env()->workloop()->run();

    EVENT_TRACE_FLUSH();

    ser << "Shutting down...\n";

    PEManager::destroy();

    m3::Machine::shutdown();
}
