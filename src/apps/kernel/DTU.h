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
#include <m3/DTU.h>

namespace kernel {

class VPE;

class DTU {
    explicit DTU() : _next_ep(m3::DTU::FIRST_FREE_EP), _ep(alloc_ep()) {
        init();
    }

public:
    static DTU &get() {
        return _inst;
    }

    int alloc_ep() {
        return _next_ep++;
    }

    void init();

    void set_vpeid(int core, int vpe);
    void unset_vpeid(int core, int vpe);
    void deprivilege(int core);

    void wakeup(VPE &vpe);
    void suspend(VPE &vpe);
    void injectIRQ(VPE &vpe);

    void config_pf_remote(VPE &vpe, int ep);
    void map_page(VPE &vpe, uintptr_t virt, uintptr_t phys, int perm);
    void unmap_page(VPE &vpe, uintptr_t virt);

    void invalidate_ep(VPE &vpe, int ep);
    void invalidate_eps(VPE &vpe);

    void config_recv_local(int ep, uintptr_t buf, uint order, uint msgorder, int flags);
    void config_recv_remote(VPE &vpe, int ep, uintptr_t buf, uint order, uint msgorder, int flags, bool valid);

    void config_send_local(int ep, label_t label, int dstcore, int dstvpe, int dstep, size_t msgsize, word_t credits);
    void config_send_remote(VPE &vpe, int ep, label_t label, int dstcore, int dstvpe, int dstep, size_t msgsize, word_t credits);

    void config_mem_local(int ep, int dstcore, int dstvpe, uintptr_t addr, size_t size);
    void config_mem_remote(VPE &vpe, int ep, int dstcore, int dstvpe, uintptr_t addr, size_t size, int perm);

    void send_to(VPE &vpe, int ep, label_t label, const void *msg, size_t size, label_t replylbl, int replyep);
    void reply_to(VPE &vpe, int ep, int crdep, word_t credits, label_t label, const void *msg, size_t size);

    void write_mem(VPE &vpe, uintptr_t addr, const void *data, size_t size);
    void write_mem_at(int core, int vpe, uintptr_t addr, const void *data, size_t size);
#if defined(__gem5__)
    void read_mem_at(int core, int vpe, uintptr_t addr, void *data, size_t size);
#endif

    void cmpxchg_mem(VPE &vpe, uintptr_t addr, const void *data, size_t datasize, size_t off, size_t size);

private:
#if defined(__gem5__)
    void do_set_vpeid(size_t core, int oldVPE, int newVPE);
    void do_ext_cmd(VPE &vpe, m3::DTU::reg_t cmd);
    void clear_pt(uintptr_t pt);
    void disable_pfs(VPE &vpe);
#endif

    void config_recv(void *e, uintptr_t buf, uint order, uint msgorder, int flags);
    void config_send(void *e, label_t label, int dstcore, int dstvpe, int dstep, size_t msgsize, word_t credits);
    void config_mem(void *e, int dstcore, int dstvpe, uintptr_t addr, size_t size, int perm);

    int _next_ep;
    int _ep;
    static DTU _inst;
};

}
