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

#pragma once

#include <m3/Common.h>
#include <m3/cap/VPE.h>
#include <m3/RecvBuf.h>
#include <m3/Log.h>
#include <sys/mman.h>

#include "../../MemoryMap.h"
#include "../../KDTU.h"

namespace m3 {

class MainMemory {
    explicit MainMemory()
            : _addr(mmap(0, DRAM_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0)),
              _size(DRAM_SIZE), _map(addr(), DRAM_SIZE),
              _rbuf(RecvBuf::create(VPE::self().alloc_ep(), 0,
                      RecvBuf::NO_HEADER | RecvBuf::NO_RINGBUF)) {
        // needs to be done manually in the kernel
        KDTU::get().config_recv_local(_rbuf.epid(),
            reinterpret_cast<uintptr_t>(_rbuf.addr()), _rbuf.order(), _rbuf.msgorder(),
            _rbuf.flags());

        if(_addr == MAP_FAILED)
            PANIC("mmap failed: " << strerror(errno));
        LOG(DEF, "Mapped " << (DRAM_SIZE / 1024 / 1024) << " MiB of main memory @ " << _addr);
    }

public:
    static MainMemory &get() {
        return _inst;
    }

    uintptr_t base() const {
        return addr();
    }
    uintptr_t addr() const {
        return reinterpret_cast<uintptr_t>(_addr);
    }
    size_t size() const {
        return _size;
    }
    size_t epid() const {
        return _rbuf.epid();
    }
    MemoryMap &map() {
        return _map;
    }

private:
    void *_addr;
    size_t _size;
    MemoryMap _map;
    m3::RecvBuf _rbuf;
    static MainMemory _inst;
};

}
