/*
 * Copyright (C) 2016, René Küttner <rene.kuettner@tu-dresden.de>
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

#include <m3/Config.h>
#include <m3/RCTMux.h>

#include "../../KVPE.h"
#include "../../KDTU.h"
#include "../../ContextSwitcher.h"

namespace m3 {

void ContextSwitcher::store_dtu_state(KVPE *vpe) {
    // on t3 we cannot access the dtu state from remote currently
    // therefore we try to restore endpoints from the caps table
    // in restore_dtu_state() below

    // since we cannot store/restore important state like the
    // read/write positions of ringbuffers, preemptive switching
    // cannot be supported on t3
}

void ContextSwitcher::attach_storage(KVPE *curr_vpe, KVPE *next_vpe) {
    // drop all current endpoints at remote dtu and attach
    // the storage memories
    LOG(VPES, "Detaching endpoints of " << curr_vpe);

    // skip sysc+def_recvchan
    for (int i = 2; i < EP_COUNT; ++i) {
        switch(i) {
            case RCTMUX_STORE_EP:
                curr_vpe->xchg_ep(RCTMUX_STORE_EP, nullptr,
                    static_cast<MsgCapability*>(
                        curr_vpe->objcaps().get(2, Capability::MEM)));
                break;
            case RCTMUX_RESTORE_EP:
                if (next_vpe) {
                    curr_vpe->xchg_ep(RCTMUX_RESTORE_EP, nullptr,
                        static_cast<MsgCapability*>(
                            next_vpe->objcaps().get(2, Capability::MEM)));
                }
                break;
            default:
                curr_vpe->xchg_ep(i, nullptr, nullptr);
                break;
        }
    }
}

void ContextSwitcher::restore_dtu_state(KVPE *vpe) {
    // try to reconfigure EPs from caps table
    LOG(VPES, "Attaching endpoints of " << vpe);
    vpe->objcaps().activate_msgcaps();
}

} /* namespace m3 */
