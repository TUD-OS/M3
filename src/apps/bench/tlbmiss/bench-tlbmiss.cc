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

#include <base/util/Time.h>
#include <base/KIF.h>

#include <m3/stream/Standard.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

using namespace m3;

static const size_t COUNT       = 9;
static const size_t PAGES       = 16;

int main() {
    const uintptr_t virt = 0x30000000;

    MemGate mgate = MemGate::create_global(PAGES * PAGE_SIZE, MemGate::RW);

    cycles_t xfer = 0;
    for(size_t i = 0; i < COUNT; ++i) {
        Syscalls::get().createmap(
            virt / PAGE_SIZE, VPE::self().sel(), mgate.sel(), 0, PAGES, MemGate::RW
        );

        alignas(8) char buf[8];
        for(size_t p = 0; p < PAGES; ++p) {
            cycles_t start = Time::start(0);
            VPE::self().mem().read(buf, sizeof(buf), virt + p * PAGE_SIZE);
            cycles_t end = Time::stop(0);
            xfer += end - start;
        }

        Syscalls::get().revoke(
            VPE::self().sel(), KIF::CapRngDesc(KIF::CapRngDesc::MAP, virt / PAGE_SIZE, PAGES), true
        );
    }

    cout << "per-xfer: " << (xfer / (COUNT * PAGES)) << "\n";
    return 0;
}
