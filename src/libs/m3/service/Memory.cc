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

#include <m3/service/Memory.h>
#include <m3/GateStream.h>

namespace m3 {

Errors::Code Memory::map(uintptr_t *virt, size_t len, int prot, int flags) {
    GateIStream reply = send_receive_vmsg(_gate, MAP, *virt, len, prot, flags);
    Errors::Code res;
    reply >> res;
    if(res != Errors::NO_ERROR)
        return res;
    reply >> *virt;
    return Errors::NO_ERROR;
}

Errors::Code Memory::unmap(uintptr_t virt) {
    GateIStream reply = send_receive_vmsg(_gate, UNMAP, virt);
    Errors::Code res;
    reply >> res;
    return res;
}

}
