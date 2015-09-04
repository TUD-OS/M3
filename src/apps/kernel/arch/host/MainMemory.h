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

#pragma once

#include <m3/Common.h>
#include <m3/cap/VPE.h>
#include <m3/RecvBuf.h>
#include <m3/Log.h>
#include <sys/mman.h>

#include "../../MemoryMap.h"

namespace m3 {

class MainMemory {
    static constexpr size_t MEM_SIZE = 1024 * 1024 * 64;

    explicit MainMemory()
            : _addr(mmap(0, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0)),
              _size(MEM_SIZE), _map(addr(), MEM_SIZE),
              _rbuf(RecvBuf::create(VPE::self().alloc_chan(), 0,
                      RecvBuf::NO_HEADER | RecvBuf::NO_RINGBUF)) {
        if(_addr == MAP_FAILED)
            PANIC("mmap failed: " << strerror(errno));
        LOG(DEF, "Mapped " << (MEM_SIZE / 1024 / 1024) << " MiB of main memory @ " << _addr);
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
    size_t channel() const {
        return _rbuf.chanid();
    }
    MemoryMap &map() {
        return _map;
    }

private:
    void *_addr;
    size_t _size;
    MemoryMap _map;
    // is used only to set the msgqid
    m3::RecvBuf _rbuf;
    static MainMemory _inst;
};

}
