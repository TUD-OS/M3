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

#include <m3/Common.h>
#include <m3/cap/VPE.h>

namespace m3 {

class KDTU {
    explicit KDTU() : _ep(VPE::self().alloc_ep()) {
        EPMux::get().reserve(_ep);
    }

public:
    static KDTU &get() {
        return _inst;
    }

    void wakeup(int core);
    void deprivilege(int core);

    void invalidate_ep(int core, int ep);
    void invalidate_eps(int core);

    void config_recv_local(int ep, uintptr_t buf, uint order, uint msgorder, int flags);
    void config_recv_remote(int core, int ep, uintptr_t buf, uint order, uint msgorder, int flags, bool valid);

    void config_send_local(int ep, label_t label, int dstcore, int dstep, size_t msgsize, word_t credits);
    void config_send_remote(int core, int ep, label_t label, int dstcore, int dstep, size_t msgsize, word_t credits);

    void config_mem_local(int ep, int dstcore, uintptr_t addr, size_t size);
    void config_mem_remote(int core, int ep, int dstcore, uintptr_t addr, size_t size, int perm);

    void reply_to(int core, int ep, int crdep, word_t credits, label_t label, const void *msg, size_t size);

    void write_mem(int core, uintptr_t addr, const void *data, size_t size);

private:
    void config_recv(void *e, uintptr_t buf, uint order, uint msgorder, int flags);
    void config_send(void *e, label_t label, int dstcore, int dstep, size_t msgsize, word_t credits);
    void config_mem(void *e, int dstcore, uintptr_t addr, size_t size, int perm);

    int _ep;
    static KDTU _inst;
};

}
