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

#include <base/RCTMux.h>
#include <base/log/Kernel.h>
#include <base/col/Treap.h>

#include "DTU.h"
#include "pes/ContextSwitcher.h"
#include "pes/VPEManager.h"
#include "pes/VPE.h"
#include "Platform.h"
#include "WorkLoop.h"

namespace kernel {

static const char *stateNames[] = {
    "STATE_IDLE",
    "STATE_STORE",
    "STATE_SWITCH",
    "STATE_START",
    "STATE_DONE",
};

ContextSwitcher::ContextSwitcher(size_t core)
    : m3::SListItem(), _core(core), _state(STATE_IDLE), _vpes(), _it(), _idle(), _cur() {
    assert(core > 0);
    KLOG(VPES, "Initialized context switcher for core " << core);
}

void ContextSwitcher::send_flags(int vpeid, uint64_t flags) {
    alignas(DTU_PKG_SIZE) uint64_t ctrl = flags;
    DTU::get().write_mem(VPEDesc(_core, vpeid), RCTMUX_FLAGS, &ctrl, sizeof(ctrl));
}

void ContextSwitcher::recv_flags(int vpeid, uint64_t *flags) {
    DTU::get().read_mem(VPEDesc(_core, vpeid), RCTMUX_FLAGS, flags, sizeof(*flags));
}

VPE* ContextSwitcher::schedule() {
    if (_vpes.length() > 0) {
        _it++;
        if (_it == _vpes.end())
            _it = _vpes.begin();
        return _it->vpe;
    }

    return _idle;
}

void ContextSwitcher::init() {
    assert(_idle == nullptr);

    _idle = new VPE(m3::String("idle"), _core, VPEManager::get().get_id(),
        VPE::F_IDLE | VPE::F_INIT, -1, m3::KIF::INV_SEL, true);
}

bool ContextSwitcher::enqueue(VPE *vpe) {
    _vpes.append(new TMuxVPE(vpe));
    if(_vpes.length() == 1) {
        _it = _vpes.begin();
        return true;
    }
    return false;
}

bool ContextSwitcher::remove(VPE *vpe) {
    for(auto it = _vpes.begin(); it != _vpes.end(); ++it) {
        if(it->vpe == vpe) {
            _vpes.remove(&*it);
            if(_it == it)
                _it = _vpes.begin();
            break;
        }
    }

    if(_cur == vpe) {
        _cur->_state = VPE::DEAD;
        // increase the references until we are done with the VPE
        _cur->ref();
        return start_switch();
    }
    return false;
}

bool ContextSwitcher::start_switch() {
    // if there is a switch running, do nothing
    if(_state != STATE_IDLE)
        return false;

    // if no VPE is running, directly switch to a new VPE
    if (_cur == nullptr)
        _state = STATE_SWITCH;

    next_state();
    return true;
}

bool ContextSwitcher::continue_switch() {
    uint64_t flags = 0;
    // rctmux is expected to invalidate the VPE id after we've injected the IRQ
    recv_flags(_state == STATE_STORE ? VPE::INVALID_ID : _cur->id(), &flags);
    if(~flags & m3::RCTMuxCtrl::SIGNAL)
        return true;

    return !next_state();
}

bool ContextSwitcher::start_vpe() {
    assert(_state == STATE_IDLE);

    _state = STATE_START;
    return !next_state();
}

bool ContextSwitcher::next_state() {
    KLOG(VPES, "CtxSw[" << _core << "]: next; state=" << stateNames[static_cast<size_t>(_state)]
        << " (current=" << (_cur ? _cur->id() : 0) << ":"
                        << (_cur ? _cur->name().c_str() : "-") << ")");

    bool res = false;
    switch(_state) {
        case STATE_IDLE: {
            assert(_cur != nullptr);

            send_flags(_cur->id(), m3::RCTMuxCtrl::STORE);
            DTU::get().injectIRQ(_cur->desc());

            _state = STATE_STORE;
            break;
        }

        case STATE_STORE: {
            DTU::get().get_regs_state(_cur->core(), _cur->dtu_state());

            if(_cur->state() == VPE::DEAD) {
                _cur->unref();
                _cur = nullptr;
                if(!m3::env()->workloop()->has_items())
                    return true;
            }
            else
                _cur->_state = VPE::SUSPENDED;
            _state = STATE_SWITCH;
            // fall through
        }

        case STATE_SWITCH: {
            _cur = schedule();

            // make it running here, so that the PTEs are sent to the PE, if F_INIT is set
            _cur->_state = VPE::RUNNING;

            if(_cur->flags() & VPE::F_INIT)
                _cur->init_memory();
            if(_cur->flags() & VPE::F_BOOTMOD)
                _cur->load_app(_cur->name().c_str());

            DTU::get().reset(_cur->dtu_state(), _cur->_entry);

            VPEDesc vpe(_core, (_cur->flags() & VPE::F_INIT) ? _cur->id() : VPE::INVALID_ID);
            DTU::get().set_regs_state(vpe, _cur->id(), _cur->dtu_state());

            // fall through
        }

        case STATE_START: {
            uint64_t flags = 0;
            // it's the first start if we are initializing or starting
            if(_cur->flags() & (VPE::F_INIT | VPE::F_START))
                flags |= m3::RCTMuxCtrl::INIT;
            // there is an application to restore if we are either resuming an application (!INIT)
            // or if we are just starting it
            if(!(_cur->flags() & VPE::F_INIT) || (_cur->flags() & VPE::F_START))
                flags |= m3::RCTMuxCtrl::RESTORE | (static_cast<uint64_t>(_core) << 32);

            KLOG(VPES, "CtxSw[" << _core << "]: waking up PE with flags=" << m3::fmt(flags, "#x"));

            send_flags(_cur->id(), flags);
            DTU::get().wakeup(_cur->desc());
            _state = STATE_DONE;
            break;
        }

        case STATE_DONE: {
            // we have finished these phases now (if they were set)
            _cur->_flags &= ~(VPE::F_INIT | VPE::F_START);
            _cur->notify_resume();

            send_flags(_cur->id(), 0);
            _state = STATE_IDLE;
            res = true;
            break;
        }
    }

    KLOG(VPES, "CtxSw[" << _core << "]: done; state=" << stateNames[static_cast<size_t>(_state)]
        << " (current=" << (_cur ? _cur->id() : 0) << ":"
                        << (_cur ? _cur->name().c_str() : "-") << ")");

    return res;
}

} /* namespace m3 */
