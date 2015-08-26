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

#include "../../MemoryMap.h"

namespace m3 {

class MainMemory {
public:
    // TODO temporary, until we have a DRAM
    explicit MainMemory() : _map(1, 0) {
    }

    static MainMemory &get() {
        return _inst;
    }

    uintptr_t base() const {
        return 0;
    }
    uintptr_t addr() const {
        return 0;
    }
    size_t size() const {
        return 0;
    }
    size_t channel() const {
        return 0;
    }
    MemoryMap &map() {
        return _map;
    }

private:
    MemoryMap _map;
    static MainMemory _inst;
};

}
