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
#include <base/DTU.h>

#include "mem/MainMemory.h"

namespace kernel {

class AddrSpace {
public:
    explicit AddrSpace(vpeid_t vpeid, epid_t sep, capsel_t sgate, epid_t rep, capsel_t rgate);
    ~AddrSpace();

    epid_t sep() const {
        return _sep;
    }
    capsel_t sgate() const {
        return _sgate;
    }
    epid_t rep() const {
        return _rep;
    }
    capsel_t rgate() const {
        return _rgate;
    }

    gaddr_t root_pt() const {
        return _root;
    }

    vpeid_t _vpeid;
    epid_t _sep;
    epid_t _rep;
    capsel_t _sgate;
    capsel_t _rgate;
    gaddr_t _root;
};

}
