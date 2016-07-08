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

#include <m3/Config.h>
#include <m3/RCTMux.h>
#include <m3/util/Sync.h>
#include <m3/Log.h>
#include <m3/col/Treap.h>

#include "KDTU.h"
#include "ContextSwitcher.h"

namespace m3 {

ContextSwitcher::ContextSwitcher(size_t core)
    : _core(core), _currtmuxvpe(nullptr)
{
    assert(core > 0);
    LOG(VPES, "Initialized context switcher for core " << core);
}

void ContextSwitcher::finalize_switch() {
    if (_currtmuxvpe) {
        restore_dtu_state(_currtmuxvpe);
        _currtmuxvpe->resume();
    }

    alignas(DTU_PKG_SIZE) uint64_t flags = 0;
    int vpeid = (_currtmuxvpe) ? _currtmuxvpe->id() : KVPE::INVALID_ID;
    send_flags(_core, vpeid, &flags);
}

void ContextSwitcher::send_flags(int core, int vpeid, const uint64_t *flags) {
    KDTU::get().write_mem_at(core, vpeid, RCTMUX_FLAGS, flags, sizeof(*flags));
}

void ContextSwitcher::recv_flags(int core, int vpeid, uint64_t *flags) {
    KDTU::get().read_mem_at(core, vpeid, RCTMUX_FLAGS, flags, sizeof(*flags));
}

void ContextSwitcher::switch_to(KVPE *to) {

    LOG(VPES, "TMux: Switching from " << _currtmuxvpe << " to " << to);

    if (_currtmuxvpe == to) {
        // nothing to be done
        LOG(VPES, "TMux: Ignoring switch request with old == new VPE");
        return;
    }

    if (_currtmuxvpe != nullptr) {

        // there is already a suspendable running, so we have to switch

        // -- Initialization --

        int core  = _currtmuxvpe->core();
        int vpeid = KVPE::INVALID_ID;

        _currtmuxvpe->suspend();
        KDTU::get().unset_vpeid(core, _currtmuxvpe->id());

        alignas(DTU_PKG_SIZE) uint64_t ctrl = RCTMUXCtrlFlag::STORE;
        recv_flags(core, vpeid, &ctrl);
        ctrl = RCTMUXCtrlFlag::STORE;
        if (to && to->state() == KVPE::SUSPENDED)
            ctrl |= RCTMUXCtrlFlag::RESTORE;
        send_flags(core, vpeid, &ctrl);

        // -- Storage --

        KDTU::get().injectIRQ(core, vpeid);

        // wait for rctmux to be initialized - this should only take a few
        // cycles so we can busy wait here; we limit the maximal amount
        // of cycles, though
        uint8_t timeout_counter = 1 << 7;
        while (--timeout_counter && !(ctrl & RCTMUX_FLAG_SIGNAL)) {
            recv_flags(core, vpeid, &ctrl);
        }

        if (!timeout_counter) {
            LOG(VPES, "TMux: rctmux at core " << _currtmuxvpe->core() << " timed out");
            // FIXME: how to handle this best? disable time multiplexing?
            return;
        }

        _currtmuxvpe->suspend();

        // store current dtu state)
        store_dtu_state(_currtmuxvpe);
        attach_storage(_currtmuxvpe, to);

        LOG(VPES, "FOOO: " << (uint64_t)_currtmuxvpe->address_space()->root_pt());

        if(to) {
            LOG(VPES, "FOO2: " << (uint64_t)to->address_space()->root_pt());
            restore_dtu_state(to);
            //KDTU::get().set_vpeid(core, to->id());
            //KDTU::get().config_pf_remote(*to, DTU::SYSC_EP);
            vpeid = to->id();
            //to->wakeup();
        }

        // 'finished' notification of RCTMux
        ctrl = (ctrl | RCTMUXCtrlFlag::SIGNAL) ^ RCTMUXCtrlFlag::SIGNAL;
        send_flags(core, vpeid, &ctrl);
    }

    _currtmuxvpe = to;
}

} /* namespace m3 */
