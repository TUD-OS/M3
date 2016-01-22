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

#include <m3/cap/Session.h>
#include <m3/cap/VPE.h>
#include <m3/service/Memory.h>
#include <m3/Syscalls.h>
#include <m3/Log.h>

using namespace m3;

int main() {
    Serial::get() << "Hello World!\n";

    uintptr_t virt = 0x100000;
    {
        Memory mem("memsrv", VPE::self());
        Syscalls::get().setpfgate(VPE::self().sel(), mem.gate().sel(), VPE::self().alloc_ep());

        Errors::Code res = mem.map(&virt, 0x4000, Memory::READ | Memory::WRITE, 0);
        if(res != Errors::NO_ERROR)
            PANIC("Unable to map memory:" << Errors::to_string(res));

        volatile uint64_t *pte = reinterpret_cast<volatile uint64_t*>(0x3ffff000);
        Serial::get() << fmt(*pte, "#0x", 16) << "\n";

        volatile int *addr = reinterpret_cast<volatile int*>(virt);
        for(size_t i = 0; i < (4 * PAGE_SIZE) / sizeof(int); ++i)
            addr[i] = i;

        Serial::get() << fmt(*pte, "#0x", 16) << "\n";
        Serial::get() << fmt(*pte, "#0x", 16) << "\n";

        res = mem.unmap(virt);
        if(res != Errors::NO_ERROR)
            PANIC("Unable to unmap memory:" << Errors::to_string(res));
    }

    volatile int *addr = reinterpret_cast<volatile int*>(virt);
    addr[0] = 1;

    Serial::get() << "Bye World!\n";
    return 0;
}
