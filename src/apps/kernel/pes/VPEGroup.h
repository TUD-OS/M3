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

#include <base/util/Reference.h>

#include "mem/SlabCache.h"

namespace kernel {

class VPE;

class VPEGroup : public SlabObject<VPEGroup>, public m3::RefCounted {
    struct VPEItem : public m3::SListItem {
        explicit VPEItem(VPE *v) : m3::SListItem(), vpe(v) {
        }
        VPE *vpe;
    };

public:
    typedef m3::SList<VPEItem>::iterator iterator;

    explicit VPEGroup() : RefCounted(), vpes() {
    }
    ~VPEGroup();

    bool has_other_app(VPE *self) const;
    bool is_pe_used(peid_t pe) const;

    iterator begin() {
        return vpes.begin();
    }
    iterator end() {
        return vpes.end();
    }

    void add(VPE *v) {
        vpes.append(new VPEItem(v));
    }
    void remove(VPE *v) {
        VPEItem *i = vpes.remove_if([v](VPEItem *i) {
            return i->vpe == v;
        });
        delete i;
    }

private:
    m3::SList<VPEItem> vpes;
};

}
