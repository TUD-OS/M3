/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
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

#include <base/Common.h>
#include <base/Errors.h>

namespace kernel {

class VPE;
class SendGate;

class SendGate {
public:
    explicit SendGate(VPE &vpe, epid_t ep, label_t label)
        : _vpe(vpe),
          _ep(ep),
          _label(label) {
    }

    VPE &vpe() {
        return _vpe;
    }

    void send(const void *data, size_t len, epid_t rep, label_t label);

private:
    VPE &_vpe;
    epid_t _ep;
    label_t _label;
};

}
