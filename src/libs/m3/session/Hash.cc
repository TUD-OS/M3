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

#include <m3/com/GateStream.h>
#include <m3/session/Hash.h>

namespace m3 {

size_t Hash::get(Algorithm algo, const void *data, size_t len, void *res, size_t max) {
    assert(len <= BUF_SIZE);
    _mem.write_sync(data, len, 0);

    uint64_t count;
    GateIStream is = send_receive_vmsg(_send,
        static_cast<uint64_t>(algo),
        static_cast<uint64_t>(len)
    );
    is >> count;

    if(count == 0)
        return 0;
    memcpy(res, is.buffer() + sizeof(uint64_t), Math::min(max, static_cast<size_t>(count)));
    return count;
}

}
