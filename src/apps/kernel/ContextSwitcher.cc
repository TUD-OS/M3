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
    restore_dtu_state(_currtmuxvpe);
    _currtmuxvpe->resume();

    alignas(DTU_PKG_SIZE) uint64_t flags = 0;
    send_flags(*_currtmuxvpe, &flags);
}

void ContextSwitcher::send_flags(KVPE &vpe, const uint64_t *flags) {
    KDTU::get().write_mem(vpe, RCTMUX_FLAGS_GLOBAL, flags, sizeof(*flags));
}

void ContextSwitcher::recv_flags(KVPE &vpe, uint64_t *flags) {
    KDTU::get().read_mem(vpe, RCTMUX_FLAGS_GLOBAL, flags, sizeof(*flags));
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

        // -- Initialization --

        alignas(DTU_PKG_SIZE) uint64_t ctrl = RCTMUXCtrlFlag::STORE;
        if (to && to->state() == KVPE::SUSPENDED)
            ctrl |= RCTMUXCtrlFlag::RESTORE;
        send_flags(*to, &ctrl);

        // -- Storage --

        LOG(VPES, "TMux: Waking up rctmux at core " << _core);
        _currtmuxvpe->wakeup();

        // wait for rctmux to be initialized - this should only take a few
        // cycles so we can busy wait here; we limit the maximal amount
        // of cycles, though
        uint8_t timeout_counter = 1 << 7;
        while (--timeout_counter && !(ctrl & RCTMUX_FLAG_SIGNAL)) {
            recv_flags(*to, &ctrl);
        }

        if (!timeout_counter) {
            LOG(VPES, "TMux: rctmux at core " << to->core() << " timed out");
            // FIXME: how to handle this best? disable time multiplexing?
            return;
        }

        _currtmuxvpe->suspend();

        // store current dtu state)
        store_dtu_state(_currtmuxvpe);
        attach_storage(_currtmuxvpe, to);

        // (re)activate the sysc chan for the
        // 'finished' notification of RCTMux
        if (to)
            to->activate_sysc_ep();

        ctrl ^= RCTMUXCtrlFlag::SIGNAL;
        send_flags(*to, &ctrl);
    }

    _currtmuxvpe = to;
}

} /* namespace m3 */
