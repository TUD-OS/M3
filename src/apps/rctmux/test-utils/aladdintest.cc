/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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
#include <base/stream/IStringStream.h>
#include <base/util/Time.h>
#include <base/CmdArgs.h>

#include <m3/accel/AladdinAccel.h>
#include <m3/com/MemGate.h>
#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>
#include <m3/session/Pager.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/VPE.h>

using namespace m3;

static const int REPEATS = 24;

static void add(Aladdin &alad, goff_t *virt, size_t size, Aladdin::Array *a, int prot) {
    size_t psize = Math::round_up(size, PAGE_SIZE);

    MemGate *mem = new MemGate(MemGate::create_global(psize, prot));
    alad._accel->pager()->map_mem(virt, *mem, psize, prot);

    size_t off = 0;
    size_t pages = psize / PAGE_SIZE;
    while(pages > 0) {
        alad._accel->pager()->pagefault(*virt + off, static_cast<uint>(prot));
        pages -= 1;
        off += PAGE_SIZE;
    }

    a->addr = *virt;
    a->size = size;
    *virt += psize;
}

int main() {
    Aladdin alad1(PEISA::ACCEL_STE);
    Aladdin alad2(PEISA::ACCEL_STE);

    const size_t HEIGHT = 32;
    const size_t COLS = 32;
    const size_t ROWS = 64;
    const size_t SIZE = HEIGHT * ROWS * COLS * sizeof(uint32_t);

    Aladdin::InvokeMessage msg;
    msg.array_count = 3;
    msg.iterations = 1;

    goff_t virt = 0x1000000;
    add(alad1, &virt, SIZE, msg.arrays + 0, MemGate::R);                        // orig
    add(alad1, &virt, SIZE, msg.arrays + 1, MemGate::W);                        // sol
    add(alad1, &virt, 8, msg.arrays + 2, MemGate::R);                           // C

    virt = 0x1000000;
    add(alad2, &virt, SIZE, msg.arrays + 0, MemGate::R);                        // orig
    add(alad2, &virt, SIZE, msg.arrays + 1, MemGate::W);                        // sol
    add(alad2, &virt, 8, msg.arrays + 2, MemGate::R);                           // C

    cycles_t total = 0;
    for(int i = 0; i < REPEATS; ++i) {
        alad1.invoke(msg);

        cycles_t start = Time::start(0x1234);
        alad2.invoke(msg);
        cycles_t end = Time::stop(0x1234);
        total += end - start;
    }

    cout << "Time: " << (total / REPEATS) << " cycles\n";
    return 0;
}
