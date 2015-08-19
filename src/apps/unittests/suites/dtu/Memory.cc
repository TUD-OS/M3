/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/GateStream.h>
#include <m3/Log.h>
#include "Memory.h"

using namespace m3;

void MemoryTestSuite::SyncTestCase::run() {
    static ulong data[4];

    MemGate gate = MemGate::bind(_mem.sel());

    Serial::get() << "-- Test read sync --\n";
    {
        write_vmsg(gate, 0, 1, 2, 3, 4);
        gate.read_sync(data, sizeof(data), 0);
        assert_int(data[0], 1);
        assert_int(data[1], 2);
        assert_int(data[2], 3);
        assert_int(data[3], 4);
    }
}

void MemoryTestSuite::DeriveTestCase::run() {
    static ulong test[6] = {0};
    MemGate gate = MemGate::bind(_mem.sel());
    write_vmsg(gate, 0, 1, 2, 3, 4);

    Serial::get() << "-- Test derive --\n";
    {
        gate.read_sync(test, sizeof(ulong) * 4, 0);

        assert_int(test[0], 1);
        assert_int(test[1], 2);
        assert_int(test[2], 3);
        assert_int(test[3], 4);
        assert_int(test[4], 0);

        MemGate sub = gate.derive(4 * sizeof(ulong), sizeof(ulong), MemGate::RWX);
        write_vmsg(sub, 0, 5);
        gate.read_sync(test, sizeof(ulong) * 5, 0);

        assert_int(test[0], 1);
        assert_int(test[1], 2);
        assert_int(test[2], 3);
        assert_int(test[3], 4);
        assert_int(test[4], 5);
    }

    Serial::get() << "-- Test wrong derive --\n";
    {
        MemGate sub = gate.derive(4 * sizeof(ulong), sizeof(ulong), MemGate::R);
        sub.read_sync(test, sizeof(ulong), 0);
        assert_int(test[0], 5);

        write_vmsg(sub, 0, 8);
        assert_true(DTU::get().get_cmd(DTU::CMD_CTRL) & DTU::CTRL_ERROR);
    }
}
