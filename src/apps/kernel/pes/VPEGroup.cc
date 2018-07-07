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

#include "pes/VPEGroup.h"
#include "pes/VPE.h"

namespace kernel {

VPEGroup::~VPEGroup() {
    for(auto it = vpes.begin(); it != vpes.end(); ) {
        auto old = it++;
        old->vpe->_group = nullptr;
        delete &*old;
    }
}

bool VPEGroup::all_yielded() const {
    for(auto it = vpes.begin(); it != vpes.end(); ++it) {
        if(!it->vpe->has_yielded())
            return false;
    }
    return true;
}

bool VPEGroup::has_other_app(VPE *self) const {
    for(auto it = vpes.begin(); it != vpes.end(); ++it) {
        if(it->vpe != self && it->vpe->has_app())
            return true;
    }
    return false;
}

bool VPEGroup::is_pe_used(peid_t pe) const {
    for(auto it = vpes.begin(); it != vpes.end(); ++it) {
        if(it->vpe->pe() == pe)
            return true;
    }
    return false;
}

}
