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

#include "pes/VPE.h"
#include "DTU.h"
#include "Gate.h"

namespace kernel {

m3::Errors::Code SendGate::send(const void *data, size_t len, epid_t rep, label_t label) {
    DTU::get().send_to(_vpe.desc(), _ep, _label, data, len, label, rep);
    return m3::Errors::NO_ERROR;
}

}
