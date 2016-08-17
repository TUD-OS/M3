/*
 * Copyright (C) 2016, René Küttner <rene.kuettner@tu-dresden.de>
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

#include <base/Config.h>
#include <m3/RCTMux.h>

#include "../../pes/VPE.h"
#include "../../DTU.h"
#include "../../ContextSwitcher.h"

namespace kernel {

void ContextSwitcher::store_dtu_state(VPE *vpe) {
    DTU::get().get_regs_state(vpe->core(), &(vpe->dtu_state));
}

void ContextSwitcher::attach_storage(VPE *, VPE *) {
    // drop all current endpoints at remote dtu and attach
    // the storage memories
    // TODO
}

void ContextSwitcher::restore_dtu_state(VPE *vpe) {
    // restore dtu state
    // TODO
    DTU::get().set_regs_state(VPEDesc(vpe->core(), vpe->id()), &(vpe->dtu_state));
}

} /* namespace m3 */
