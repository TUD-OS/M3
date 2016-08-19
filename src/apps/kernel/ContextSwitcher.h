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

class ContextSwitcher {

    struct TMuxVPE : public m3::SListItem {
        explicit TMuxVPE(VPE *_vpe)
            : vpe(_vpe) {
        }

        VPE *vpe;
    };

public:
    size_t count() const {
        return _tmuxvpes.length();
    }

    void enqueue(VPE *tmuxvpe) {
        _tmuxvpes.append(new TMuxVPE(tmuxvpe));
    }

    explicit ContextSwitcher(size_t core);

    ~ContextSwitcher() {
        while (_tmuxvpes.length() > 0) {
            delete _tmuxvpes.remove_first();
        }
    }

   void assign(VPE *tmuxvpe, bool switch_now = false) {
        if (_tmuxvpes.length() == 0) {
            _tmuxvpes.append(new TMuxVPE(tmuxvpe));
            _tmuxvpeit = _tmuxvpes.begin();
            _currtmuxvpe = tmuxvpe;
        } else
            _tmuxvpes.insert(&*_tmuxvpeit, new TMuxVPE(tmuxvpe));

        if (switch_now)
            switch_next();
    }

    VPE* switch_next() {
        if (_tmuxvpes.length() > 0) {
            _tmuxvpeit++;
            if (_tmuxvpeit == _tmuxvpes.end())
                _tmuxvpeit = _tmuxvpes.begin();
            switch_to(_tmuxvpeit->vpe);
            return _tmuxvpeit->vpe;
        }

        return nullptr;
    }

    void finalize_switch();

    size_t core() {
        return _core;
    }

    void switch_to(VPE *to);
 
private:
    void send_flags(int core, int vpeid, const uint64_t *flags);
    void recv_flags(int core, int vpeid, uint64_t *flags);
    void store_dtu_state(VPE *vpe);
    void attach_storage(VPE *curr_vpe, VPE *next_vpe);
    void restore_dtu_state(VPE *vpe);

private:
    size_t _core;
    m3::SList<TMuxVPE> _tmuxvpes;
    m3::SList<TMuxVPE>::iterator _tmuxvpeit;
    VPE *_currtmuxvpe;
};

}
