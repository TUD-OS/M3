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

#include <base/Errors.h>
#include <base/KIF.h>

#include <m3/vfs/GenericFile.h>
#include <m3/ObjCap.h>
#include <m3/VPE.h>

namespace m3 {

class VTerm : public Session {
public:
    explicit VTerm(const String &name) : Session(name) {
    }

    GenericFile *create_channel(bool read) {
        capsel_t sels = VPE::self().alloc_sels(2);
        KIF::ExchangeArgs args;
        args.count = 1;
        args.vals[0] = read ? 0 : 1;
        obtain_for(VPE::self(), KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sels, 2), &args);
        return new GenericFile(sels);
    }
};

}
