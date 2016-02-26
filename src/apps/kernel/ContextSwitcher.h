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

#include <m3/Config.h>
#include <m3/col/SList.h>

#include "KVPE.h"

namespace m3 {

#if defined(__t3__)

class ContextSwitcher {

    struct TMuxVPE : public SListItem {
        explicit TMuxVPE(KVPE *_vpe)
            : vpe(_vpe) {
        }

        KVPE *vpe;
    };

public:
    explicit ContextSwitcher(size_t core);

    ~ContextSwitcher() {
        while (_tmuxvpes.length() > 0) {
            delete _tmuxvpes.remove_first();
        }
    }

   void assign(KVPE *tmuxvpe, bool switch_now = false) {
        if (_tmuxvpes.length() == 0) {
            _tmuxvpes.append(new TMuxVPE(tmuxvpe));
            _tmuxvpeit = _tmuxvpes.begin();
            _currtmuxvpe = tmuxvpe;
        } else
            _tmuxvpes.insert(&*_tmuxvpeit, new TMuxVPE(tmuxvpe));

        if (switch_now)
            switch_next();
    }

    KVPE* switch_next() {
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

private:
    void switch_to(KVPE *to);
    void send_flags(DTU *dtu, uint64_t *flags);
    void recv_flags(DTU *dtu, uint64_t *flags);
    void reset_endpoints(KVPE *vpe, KVPE *next_vpe);
    void restore_endpoints(KVPE *vpe);

private:
    size_t _core;
    SList<TMuxVPE> _tmuxvpes;
    SListIterator<TMuxVPE> _tmuxvpeit;
    KVPE *_currtmuxvpe;
};

#endif

}
