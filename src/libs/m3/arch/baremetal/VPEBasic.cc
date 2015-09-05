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

#include <m3/cap/VPE.h>
#include <m3/Syscalls.h>
#include <m3/Log.h>
#include <m3/ELF.h>
#include <stdlib.h>

namespace m3 {

void VPE::init_state() {
    delete _eps;
    delete _caps;
    Heap::free(_mounts);

    _caps = *reinterpret_cast<BitField<CAP_TOTAL>**>(BOOT_CAPS);
    if(_caps == nullptr)
        _caps = new BitField<CAP_TOTAL>();

    _eps = *reinterpret_cast<BitField<EP_COUNT>**>(BOOT_EPS);
    if(_eps == nullptr)
        _eps = new BitField<EP_COUNT>();

    _mounts = *reinterpret_cast<void**>(BOOT_MOUNTS);
    _mountlen = *reinterpret_cast<size_t*>(BOOT_MOUNTLEN);
}

}
