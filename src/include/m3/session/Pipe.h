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

#include <m3/session/ClientSession.h>
#include <m3/com/SendGate.h>
#include <m3/vfs/GenericFile.h>

namespace m3 {

class Pipe : public ClientSession {
public:
    explicit Pipe(const String &service, MemGate &memory, size_t memsize)
        : ClientSession(service, memsize) {
        delegate_obj(memory.sel());
    }

    GenericFile *create_channel(bool read, int flags = 0) {
        KIF::ExchangeArgs args;
        args.count = 1;
        args.vals[0] = read;
        KIF::CapRngDesc desc = obtain(2, &args);
        return new GenericFile(flags | (read ? FILE_R : FILE_W), desc.start());
    }
};

}
