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

#if defined(__gem5__)
#   include "arch/gem5/DTURegs.h"
#elif defined(__host__)
#   include "arch/host/DTURegs.h"
#endif

#include "Types.h"

namespace kernel {

class VPEDesc;

class DTUState {
    friend class DTU;

public:
    explicit DTUState() : _regs() {
    }

    bool was_idling() const;
    cycles_t get_idle_time() const;

    void *get_ep(epid_t ep);
    void save(const VPEDesc &vpe);
    void restore(const VPEDesc &vpe, vpeid_t vpeid);
    void enable_communication(const VPEDesc &vpe);

    bool invalidate(epid_t ep, bool check);
    void invalidate_eps(epid_t first);

    bool can_forward_msg(epid_t ep);
    void forward_msg(epid_t ep, peid_t pe, vpeid_t vpe);
    void forward_mem(epid_t ep, peid_t pe);

    size_t get_header_idx(epid_t ep, goff_t msgaddr);

    void read_ep(const VPEDesc &vpe, epid_t ep);

    void config_recv(epid_t ep, goff_t buf, int order, int msgorder, uint header);
    void config_send(epid_t ep, label_t lbl, peid_t pe, vpeid_t vpe, epid_t dstep, size_t msgsize, word_t crd);
    void config_mem(epid_t ep, peid_t pe, vpeid_t vpe, goff_t addr, size_t size, int perm);
    bool config_mem_cached(epid_t ep, peid_t pe, vpeid_t vpe);

    void config_pf(gaddr_t rootpt, epid_t sep, epid_t rep);
    void reset();

private:
    void move_rbufs(const VPEDesc &vpe, vpeid_t oldvpe, bool save);

    DTURegs _regs;
};

}
