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
#endif

#include "Types.h"

namespace kernel {

class VPEDesc;

class DTUState {
    friend class DTU;

public:
    explicit DTUState() : _regs() {
    }

    void *get_ep(epid_t ep);
    void save(const VPEDesc &vpe);
    void restore(const VPEDesc &vpe, vpeid_t vpeid);

    void invalidate(epid_t ep);
    void invalidate_eps(epid_t first);

    void config_recv(epid_t ep, uintptr_t buf, uint order, uint msgorder, int flags);
    void config_send(epid_t ep, label_t lbl, peid_t core, vpeid_t vpe, epid_t dstep, size_t msgsize, word_t crd);
    void config_mem(epid_t ep, peid_t dstcore, vpeid_t dstvpe, uintptr_t addr, size_t size, int perm);

    void config_pf(uint64_t rootpt, epid_t ep);
    void config_rwb(uintptr_t addr);
    void reset(uintptr_t addr);

private:
    DTURegs _regs;
};

}
