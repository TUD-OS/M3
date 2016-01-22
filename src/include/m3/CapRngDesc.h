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
    /**
     * @return the complete range of capabilities
     */
    static CapRngDesc All();

    /**
     * Empty range
     */
    explicit CapRngDesc() : _start(), _count() {
    }
    /**
     * Creates a range from <start> to <start>+<count>-1.
     */
    explicit CapRngDesc(capsel_t start, uint count = 1) : _start(start), _count(count) {
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

    /**
     * Free's the selectors and revokes the capabilities in this range
     */
    void free_and_revoke() {
        revoke();
        free();
    }
    /**
     * Revokes the capabilities in this range
     */
    void revoke();
    /**
     * Free's the selectors in this range
     */
    void free();

    friend OStream &operator <<(OStream &os, const CapRngDesc &crd) {
        os << "CRD[" << crd.start() << ":" << crd.count() << "]";
        return os;
    }

private:
    capsel_t _start;
    uint _count;
};

}
