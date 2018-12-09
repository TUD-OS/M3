/*
 * Copyright (C) 2015-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

namespace m3 {

template<class SESS>
class Handler {
public:
    typedef SESS session_type;

    virtual ~Handler() {
    }

    virtual Errors::Code open(SESS **sess, capsel_t, word_t) = 0;
    virtual Errors::Code obtain(SESS *, KIF::Service::ExchangeData &) {
        return Errors::NOT_SUP;
    }
    virtual Errors::Code delegate(SESS *, KIF::Service::ExchangeData &) {
        return Errors::NOT_SUP;
    }
    virtual Errors::Code close(SESS *sess) = 0;
    virtual void shutdown() {
    }
};

}
