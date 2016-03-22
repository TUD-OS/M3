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

#include <base/log/Kernel.h>
#include <base/Panic.h>

#include "MainMemory.h"

namespace kernel {

MainMemory::MainMemory()
        : _addr(mmap(0, DRAM_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0)),
          _size(DRAM_SIZE), _map(addr(), DRAM_SIZE) {
    DTU::get().config_recv_local(DTU::get().alloc_ep(), 0, 0, 0,
        m3::DTU::FLAG_NO_HEADER | m3::DTU::FLAG_NO_RINGBUF);

    if(_addr == MAP_FAILED)
        PANIC("mmap failed: " << strerror(errno));
    KLOG(MEM, "Mapped " << (DRAM_SIZE / 1024 / 1024) << " MiB of main memory @ " << _addr);
}

}
