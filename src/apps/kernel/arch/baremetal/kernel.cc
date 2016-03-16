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

#include <m3/stream/Serial.h>
#include <m3/tracing/Tracing.h>
#include <m3/DTU.h>
#include <m3/Log.h>

#include "../../CapTable.h"
#include "../../PEManager.h"
#include "../../SyscallHandler.h"

using namespace m3;

int main(int argc, char *argv[]) {
    Serial &ser = Serial::get();
    if(argc < 2) {
        ser << "Usage: " << argv[0] << " <program>...\n";
        Machine::shutdown();
    }

    EVENT_TRACE_INIT_KERNEL();

    ser << "Initializing PEs...\n";

    PEManager::create();
    PEManager::get().load(argc - 1, argv + 1);

    env()->backend->workloop->run();

    EVENT_TRACE_FLUSH();

    ser << "Shutting down...\n";

    PEManager::destroy();

    Machine::shutdown();
}
