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

#include <base/Common.h>

namespace m3 {

/**
 * The kernel interface
 */
struct KIF {
    KIF() = delete;

    /**
     * Represents an invalid selector
     */
    static const capsel_t INV_SEL       = 0xFFFF;

    /**
     * Represents unlimited credits
     */
    static const word_t UNLIM_CREDITS   = -1;

    /**
     * The permissions for MemGate
     */
    struct Perm {
        static const int R = 1;
        static const int W = 2;
        static const int X = 4;
        static const int RW = R | W;
        static const int RWX = R | W | X;
    };

    /**
     * System calls
     */
    struct Syscall {
        enum Operation {
            PAGEFAULT = 0,  // sent by the DTU if the PF handler is not reachable
            CREATESRV,
            CREATESESS,
            CREATESESSAT,
            CREATEGATE,
            CREATEVPE,
            CREATEMAP,
            ATTACHRB,
            DETACHRB,
            EXCHANGE,
            VPECTRL,
            DELEGATE,
            OBTAIN,
            ACTIVATE,
            REQMEM,
            DERIVEMEM,
            REVOKE,
            EXIT,
            NOOP,
            IDLE,
            COUNT
        };

        enum VPECtrl {
            VCTRL_START,
            VCTRL_STOP,
            VCTRL_WAIT,
        };
    };

    /**
     * Service calls
     */
    struct Service {
        enum Command {
            OPEN,
            OBTAIN,
            DELEGATE,
            CLOSE,
            SHUTDOWN
        };
    };
};

}
