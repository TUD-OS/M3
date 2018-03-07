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

#include <m3/server/RequestHandler.h>

class M3FSSession : public m3::RequestSessionData {
public:
    static constexpr size_t MSG_SIZE = 128;

    enum Type {
        META,
        FILE,
    };

    virtual ~M3FSSession() {
    }

    virtual Type type() const {
        // TODO not pure virtual because of stupid Handler stuff
        return META;
    }

    virtual void close() {
    }
};
