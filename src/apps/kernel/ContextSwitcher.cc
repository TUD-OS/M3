/*
 * Copyright (C) 2015, René Küttner <rene.kuettner@tu-dresden.de>
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

#if defined(__t3__)

#include <m3/Config.h>
#include <m3/arch/t3/RCTMux.h>
#include <m3/DTU.h>
#include <m3/util/Sync.h>
#include <m3/Log.h>
#include <m3/col/Treap.h>

#include "ContextSwitcher.h"

namespace m3 {

ContextSwitcher::ContextSwitcher(size_t core)
    : _core(core), _currtmuxvpe(nullptr)
{
    assert(core > 0);
    LOG(VPES, "Initialized context switcher for core " << core);
}

void ContextSwitcher::finalize_switch() {
    restore_endpoints(_currtmuxvpe);
    _currtmuxvpe->resume();

    alignas(DTU_PKG_SIZE) uint64_t flags = 0;
    send_flags(&(DTU::get()), &flags);
}

void ContextSwitcher::send_flags(DTU *dtu, uint64_t *flags) {
    dtu->configure_mem(DTU::DEF_RECVEP, _core,
        RCTMUX_FLAGS_GLOBAL, sizeof(*flags));
    Sync::memory_barrier();
    dtu->write(DTU::DEF_RECVEP, flags, sizeof(*flags), 0);
    dtu->wait_for_mem_cmd();
}

void ContextSwitcher::recv_flags(DTU *dtu, uint64_t *flags) {
    dtu->configure_mem(DTU::DEF_RECVEP, _core,
        RCTMUX_FLAGS_GLOBAL, sizeof(*flags));
    dtu->read(DTU::DEF_RECVEP, flags, sizeof(*flags), 0);
    dtu->wait_for_mem_cmd();
}

void ContextSwitcher::reset_endpoints(KVPE *vpe, KVPE *next_vpe) {

    LOG(VPES, "Detaching endpoints of " << vpe);

    // skip sysc+def_recvchan
    for (int i = 2; i < EP_COUNT; ++i) {
        switch(i) {
            case RCTMUX_STORE_EP:
                vpe->xchg_ep(RCTMUX_STORE_EP, nullptr,
                    static_cast<MsgCapability*>(
                        vpe->objcaps().get(2, Capability::MEM)));
                break;
            case RCTMUX_RESTORE_EP:
                if (next_vpe) {
                    vpe->xchg_ep(RCTMUX_RESTORE_EP, nullptr,
                        static_cast<MsgCapability*>(
                            next_vpe->objcaps().get(2, Capability::MEM)));
                }
                break;
            default:
                LOG(VPES, "Detaching EP " << i);
                vpe->xchg_ep(i, nullptr, nullptr);
                break;
        }
    }
}

void ContextSwitcher::restore_endpoints(KVPE *vpe) {

    LOG(VPES, "Attaching endpoints of " << vpe);
#if defined(__t3__)
    vpe->objcaps().activate_msgcaps();
#endif
}

void ContextSwitcher::switch_to(KVPE *to) {

    if (_currtmuxvpe == to) {
        // nothing to be done
        LOG(VPES, "TMux: Ignoring switch request with old == new VPE");
        return;
    }

    if (_currtmuxvpe != nullptr) {

        // there is already a suspendable running, so we have to switch
        LOG(VPES, "TMux: Switching from " << _currtmuxvpe << " to " << to);

        DTU &dtu = DTU::get();

        // -- Initialization --

        alignas(DTU_PKG_SIZE) uint64_t ctrl = RCTMUXCtrlFlag::SWITCHREQ
            | RCTMUXCtrlFlag::STORE;
        if (to && to->state() == KVPE::SUSPENDED)
            ctrl |= RCTMUXCtrlFlag::RESTORE;
        send_flags(&dtu, &ctrl);

        // -- Storage --

        LOG(VPES, "TMux: Waking up rctmux at core " << _core);
        _currtmuxvpe->wakeup();

        // wait for rctmux to be initialized - this should only take a few
        // cycles so we can busy wait here; we limit the maximal amount
        // of cycles, though
        uint8_t timeout_counter = 1 << 6;
        while (--timeout_counter && (ctrl & RCTMUX_FLAG_SIGNAL)) {
            recv_flags(&dtu, &ctrl);
        }

        if (!timeout_counter) {
            LOG(VPES, "TMux: rctmux at core " << to->core() << " timed out");
            // FIXME: how to handle this best? disable time multiplexing?
            return;
        }

        _currtmuxvpe->suspend();

        // attach the memories for storage/restoration
        reset_endpoints(_currtmuxvpe, to);

        // we activate the new sysc chan here to it for
        // the 'finished' notification of RCTMux
        if (to)
            to->activate_sysc_ep();

        ctrl |= RCTMUXCtrlFlag::STORAGE_ATTACHED;
        send_flags(&dtu, &ctrl);
    }

    _currtmuxvpe = to;
}

}

#endif
