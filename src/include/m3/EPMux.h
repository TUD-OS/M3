/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <m3/Common.h>
#include <m3/Config.h>
#include <assert.h>

namespace m3 {

class Gate;
class VPE;
class RecvBuf;

class EPSwitcher {
public:
    virtual ~EPSwitcher() {
    }

    virtual void switch_ep(size_t victim, capsel_t oldcap, capsel_t newcap);
};

class EPMux {
    friend class Gate;
    friend class VPE;
    friend class RecvBuf;

public:
    explicit EPMux();

    static EPMux &get() {
        return _inst;
    }

    void set_epswitcher(EPSwitcher *epsw) {
        delete _epsw;
        _epsw = epsw;
    }

    void reserve(size_t i);
    void switch_to(Gate *gate);
    void switch_cap(Gate *gate, capsel_t newcap);
    void reset();

private:
    void remove(Gate *gate, bool unarm);
    size_t select_victim();

    size_t _next_victim;
    EPSwitcher *_epsw;
    Gate *_gates[EP_COUNT];
    static EPMux _inst;
};

}
