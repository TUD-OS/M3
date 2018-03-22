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

#include <base/util/String.h>

namespace m3 {

class SharedMemory {
public:
    enum Op {
        CREATE,
        JOIN,
    };

    explicit SharedMemory(const String &name, size_t size, Op op);
    SharedMemory(SharedMemory &&o)
        : _fd(o._fd),
          _name(o._name),
          _addr(o._addr),
          _size(o._size) {
        o._addr = 0;
    }
    SharedMemory(const SharedMemory&) = delete;
    SharedMemory &operator=(const SharedMemory&) = delete;
    ~SharedMemory();

    void *addr() const {
        return _addr;
    }
    size_t size() const {
        return _size;
    }
    const m3::String &name() const {
        return _name;
    }

private:
    int _fd;
    String _name;
    void *_addr;
    size_t _size;
};

}
