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

#include <m3/RCTMux.h>
#include <base/log/Kernel.h>
#include <base/col/Treap.h>

#include "DTU.h"
#include "pes/VPE.h"
#include "ContextSwitcher.h"

namespace kernel {

ContextSwitcher::ContextSwitcher(size_t core)
    : _core(core), _currtmuxvpe(nullptr)
{
    assert(core > 0);
    KLOG(VPES, "Initialized context switcher for core " << core);
}

void ContextSwitcher::finalize_switch() {
    if (_currtmuxvpe) {
        restore_dtu_state(_currtmuxvpe);
        _currtmuxvpe->resume();
    }

    alignas(DTU_PKG_SIZE) uint64_t flags = 0;
    int vpeid = (_currtmuxvpe) ? _currtmuxvpe->id() : VPE::INVALID_ID;
    send_flags(_core, vpeid, &flags);
}

void ContextSwitcher::send_flags(int core, int vpeid, const uint64_t *flags) {
    DTU::get().write_mem(VPEDesc(core, vpeid), RCTMUX_FLAGS, flags, sizeof(*flags));
}

void ContextSwitcher::recv_flags(int core, int vpeid, uint64_t *flags) {
    DTU::get().read_mem(VPEDesc(core, vpeid), RCTMUX_FLAGS, flags, sizeof(*flags));
}

void ContextSwitcher::switch_to(VPE *to) {

    KLOG(VPES, "TMux: Switching from " << _currtmuxvpe << " to " << to);

    if (_currtmuxvpe == to) {
        // nothing to be done
        KLOG(VPES, "TMux: Ignoring switch request with old == new VPE");
        return;
    }

    if (_currtmuxvpe != nullptr) {

        // there is already a suspendable running, so we have to switch

        // -- Initialization --

        int core  = _currtmuxvpe->core();
        int vpeid = VPE::INVALID_ID;

        _currtmuxvpe->suspend();
        DTU::get().unset_vpeid(VPEDesc(core, _currtmuxvpe->id()));

        alignas(DTU_PKG_SIZE) uint64_t ctrl = m3::RCTMUXCtrlFlag::STORE;
        recv_flags(core, vpeid, &ctrl);
        ctrl = m3::RCTMUXCtrlFlag::STORE;
        if (to && to->state() == VPE::SUSPENDED)
            ctrl |= m3::RCTMUXCtrlFlag::RESTORE;
        send_flags(core, vpeid, &ctrl);

        // -- Storage --

        DTU::get().injectIRQ(VPEDesc(core, vpeid));

        // wait for rctmux to be initialized - this should only take a few
        // cycles so we can busy wait here; we limit the maximal amount
        // of cycles, though
        uint8_t timeout_counter = 1 << 7;
        while (--timeout_counter && !(ctrl & RCTMUX_FLAG_SIGNAL)) {
            recv_flags(core, vpeid, &ctrl);
        }

        if (!timeout_counter) {
            KLOG(VPES, "TMux: rctmux at core " << _currtmuxvpe->core() << " timed out");
            // FIXME: how to handle this best? disable time multiplexing?
            return;
        }

        _currtmuxvpe->suspend();

        // store current dtu state)
        store_dtu_state(_currtmuxvpe);
        attach_storage(_currtmuxvpe, to);

        KLOG(VPES, "FOOO: " << (uint64_t)_currtmuxvpe->address_space()->root_pt());

        if(to) {
            KLOG(VPES, "FOO2: " << (uint64_t)to->address_space()->root_pt());
            restore_dtu_state(to);
            //DTU::get().set_vpeid(core, to->id());
            //DTU::get().config_pf_remote(*to, DTU::SYSC_EP);
            vpeid = to->id();
            //to->wakeup();
        }

        // 'finished' notification of RCTMux
        ctrl = (ctrl | m3::RCTMUXCtrlFlag::SIGNAL) ^ m3::RCTMUXCtrlFlag::SIGNAL;
        send_flags(core, vpeid, &ctrl);
    }

    _currtmuxvpe = to;
}

} /* namespace m3 */
