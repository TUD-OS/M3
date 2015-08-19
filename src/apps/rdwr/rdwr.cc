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

#include <m3/stream/Serial.h>
#include <m3/cap/SendGate.h>
#include <m3/cap/MemGate.h>
#include <m3/RecvBuf.h>
#include <m3/ChanMng.h>
#include <m3/GateStream.h>
#include <m3/DTU.h>
#include <stdarg.h>
#include <cstring>
#include <assert.h>

using namespace m3;

#define SIZE        64

alignas(DTU_PKG_SIZE) static unsigned int some_data[SIZE];
alignas(DTU_PKG_SIZE) static unsigned int some_data_ctrl[SIZE];

static void check_result() {
    Serial &ser = Serial::get();
    int errors = 0;
    for(size_t i = 0; i < ARRAY_SIZE(some_data); ++i) {
        if(some_data_ctrl[i] != some_data[i]) {
            ser << "received[" << i << "]: "
                << fmt(some_data_ctrl[i], "#0X", 8) << " != " << fmt(some_data[i], "#0X", 8) << "\n";
            errors++;
        }
    }
    if(errors == 0)
        ser << "Result correct!\n";
    else
        ser << "Result NOT correct!\n";
}

int main() {
    Serial &ser = Serial::get();

    for(size_t i = 0; i < ARRAY_SIZE(some_data); ++i)
        some_data[i] = i;
    memset(some_data_ctrl, 0, sizeof(some_data_ctrl));

    ser << "Requesting memory...\n";
    MemGate mem = MemGate::create_global(sizeof(some_data) * 8, MemGate::RW);

    ser << "Writing to it and reading it back...\n";
    mem.write_sync(some_data, sizeof(some_data), 0x10);
    mem.read_sync(some_data_ctrl, sizeof(some_data_ctrl), 0x10);

    check_result();

    ser << "Deriving memory...\n";
    MemGate submem = mem.derive(0x20, sizeof(some_data) + 0x10, MemGate::RWX);

    ser << "Writing to it and reading it back...\n";
    submem.write_sync(some_data, sizeof(some_data), 0x10);
    submem.read_sync(some_data_ctrl, sizeof(some_data_ctrl), 0x10);

    check_result();
    return 0;
}
