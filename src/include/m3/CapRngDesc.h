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
#include <m3/stream/OStream.h>

namespace m3 {

/**
 * Describes a range of capabilities.
 */
class CapRngDesc {
public:
    enum Type {
        OBJ,
        MAP,
    };

    /**
     * Empty range
     */
    explicit CapRngDesc() : _type(OBJ), _start(), _count() {
    }
    /**
     * Creates a range from <start> to <start>+<count>-1.
     */
    explicit CapRngDesc(Type type, capsel_t start, uint count = 1)
        : _type(type), _start(start), _count(count) {
    }

    /**
     * @return the type of descriptor: OBJ or MAP
     */
    Type type() const {
        return _type;
    }
    /**
     * @return the start, i.e. the first capability
     */
    capsel_t start() const {
        return _start;
    }
    /**
     * @return the number of capabilities
     */
    uint count() const  {
        return _count;
    }

    friend OStream &operator <<(OStream &os, const CapRngDesc &crd) {
        os << "CRD[" << (crd._type == OBJ ? "OBJ" : "MAP") << ":"
           << crd.start() << ":" << crd.count() << "]";
        return os;
    }

private:
    Type _type;
    capsel_t _start;
    uint _count;
};

}
