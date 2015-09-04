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

#pragma once

#include <m3/Common.h>
#include <m3/Config.h>
#include <assert.h>

namespace m3 {

class ChanMng;
class Gate;
class VPE;
class RecvBuf;

class ChanMngBase {
    friend class Gate;
    friend class VPE;
    friend class RecvBuf;

public:
#if defined(__t3__) or defined(__gem5__)
    static const int MEM_CHAN       = 0;    // unused
    static const int SYSC_CHAN      = 0;
    static const int DEF_RECVCHAN   = 1;
#else
    static const int MEM_CHAN       = 0;
    static const int SYSC_CHAN      = 1;
    static const int DEF_RECVCHAN   = 2;
#endif

    explicit ChanMngBase() {
    }

    static ChanMng &get() {
        return _inst;
    }

    bool fetch_msg(size_t id);
    bool uses_header(size_t) const;
    void notify(size_t id);
    void ack_message(size_t id);

private:
    static ChanMng _inst;
};

}

#if defined(__host__)
#   include <m3/arch/host/ChanMng.h>
#elif defined(__t2__)
#   include <m3/arch/t2/ChanMng.h>
#elif defined(__t3__)
#   include <m3/arch/t3/ChanMng.h>
#elif defined(__gem5__)
#   include <m3/arch/gem5/ChanMng.h>
#else
#   error "Unsupported target"
#endif
