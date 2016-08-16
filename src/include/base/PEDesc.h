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

/**
 * The different types of PEs
 */
enum class PEType {
    // Compute PE with internal memory
    COMP_IMEM   = 0,
    // Compute PE with external memory, i.e., with cache
    COMP_EMEM   = 1,
    // memory PE
    MEM         = 2,
};

/**
 * The different ISAs
 */
enum class PEISA {
    NONE        = 0,
    X86         = 1,
    XTENSA      = 2,
    ACCEL_HASH  = 3,
};

/**
 * Describes a PE
 */
struct PEDesc {
    typedef uint32_t value_t;

    /**
     * Default constructor
     */
    explicit PEDesc() : _value() {
    }
    /**
     * Creates a PE description from the given descriptor word
     */
    explicit PEDesc(value_t value) : _value(value) {
    }
    /**
     * Creates a PE description of given type, ISA and memory size
     */
    explicit PEDesc(PEType type, PEISA isa, size_t memsize = 0)
        : _value(static_cast<value_t>(type) | (static_cast<value_t>(isa) << 3) | memsize) {
    }

    /**
     * @return the raw descriptor word
     */
    value_t value() const {
        return _value;
    }

    /**
     * @return the type of PE
     */
    PEType type() const {
        return static_cast<PEType>(_value & 0x7);
    }
    /**
     * @return the isa of the PE
     */
    PEISA isa() const {
        return static_cast<PEISA>((_value >> 3) & 0x3);
    }
    /**
     * @return if the PE has a core that is programmable
     */
    bool is_programmable() const {
        return isa() == PEISA::X86 || isa() == PEISA::XTENSA;
    }

    /**
     * @return the memory size (for type() == COMP_IMEM | MEM)
     */
    size_t mem_size() const {
        return _value & ~0xFFF;
    }
    /**
     * @return true if the PE has internal memory
     */
    bool has_memory() const {
        return type() != PEType::COMP_EMEM;
    }
    /**
     * @return true if the PE has a cache, i.e., external memory
     */
    bool has_cache() const {
        return type() == PEType::COMP_EMEM;
    }
    /**
     * @return true if the PE has virtual memory support
     */
    bool has_virtmem() const {
        return type() == PEType::COMP_EMEM;
    }

private:
    value_t _value;
} PACKED;

}
