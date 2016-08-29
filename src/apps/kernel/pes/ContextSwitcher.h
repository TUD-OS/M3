/*
 * Copyright (C) 2015, René Küttner <rene.kuettner@.tu-dresden.de>
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

#pragma once

#include <base/Config.h>
#include <base/col/SList.h>

#include "pes/VPE.h"

namespace kernel {

class ContextSwitcher : public m3::SListItem {

    struct TMuxVPE : public m3::SListItem {
        explicit TMuxVPE(VPE *_vpe)
            : vpe(_vpe) {
        }

        VPE *vpe;
    };

    enum State {
        // normal state, no context switch happening
        S_IDLE,

        // inject IRQ to store state
        S_STORE_WAIT,

        // on rctmux's ACK: save DTU state
        S_STORE_DONE,

        // schedule and reset
        S_SWITCH,

        // wakeup to start application
        S_RESTORE_WAIT,

        // on rctmux's ACK: finished restore phase
        S_RESTORE_DONE
    };

public:
    explicit ContextSwitcher(size_t core);

    ~ContextSwitcher() {
        while (_vpes.length() > 0) {
            delete _vpes.remove_first();
        }
    }

    void init();

    size_t core() const {
        return _core;
    }
    size_t count() const {
        return _vpes.length();
    }

    bool can_mux() const;

    bool enqueue(VPE *vpe);
    bool remove(VPE *vpe);
    bool start_vpe();
    bool start_switch();
    bool continue_switch();

private:
    VPE* schedule();

    bool next_state();

    void send_flags(int vpeid, uint64_t flags);
    void recv_flags(int vpeid, uint64_t *flags);

private:
    size_t _core;
    State _state;
    m3::SList<TMuxVPE> _vpes;
    m3::SList<TMuxVPE>::iterator _it;
    VPE *_idle;
    VPE *_cur;
};

}
