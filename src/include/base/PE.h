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

#include <base/Common.h>

namespace m3 {

enum class PEType {
    COMP_IMEM   = 0,
    COMP_EMEM   = 1,
    MEM         = 2,
};

struct PE {
    typedef uint32_t value_t;

    explicit PE() : _value() {
    }
    explicit PE(value_t value) : _value(value) {
    }
    explicit PE(PEType type, size_t memsize = 0)
        : _value(static_cast<value_t>(type) | memsize) {
    }

    value_t value() const {
        return _value;
    }

    PEType type() const {
        return static_cast<PEType>(_value & 0x7);
    }
    size_t mem_size() const {
        return _value & ~0x7;
    }
    bool has_memory() const {
        return type() != PEType::COMP_EMEM;
    }
    bool has_cache() const {
        return type() == PEType::COMP_EMEM;
    }
    bool has_virtmem() const {
        return type() == PEType::COMP_EMEM;
    }

private:
    value_t _value;
} PACKED;

}
