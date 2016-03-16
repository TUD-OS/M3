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

#include <m3/Common.h>
#include <m3/Log.h>

#include "Capability.h"
#include "CapTable.h"
#include "KDTU.h"

namespace m3 {

// don't inherit from EnvBackend here. we'll do that in the arch-specific EnvBackends and use
// this class to implement switch_ep
class KEPMux {
public:
    static void switch_ep(size_t id, capsel_t, capsel_t newcap) {
        if(newcap != ObjCap::INVALID) {
            MsgCapability *c = static_cast<MsgCapability*>(
                CapTable::kernel_table().get(newcap, Capability::MSG));
            assert(c != nullptr);

            // TODO we need the max msg size
            KDTU::get().config_send_local(id,
                c->obj->label, c->obj->core, c->obj->vpe, c->obj->epid,
                c->obj->credits, c->obj->credits);

            LOG(IPC, "Kernel programs ep[" << id << "] to "
                << "core=" << c->obj->core << ", ep=" << c->obj->epid
                << ", lbl=" << fmt(c->obj->label, "#0x", sizeof(label_t) * 2)
                << ", credits=" << fmt(c->obj->credits, "#x"));
        }
    }
};

}
