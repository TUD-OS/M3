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
#include <base/DTU.h>
#include <base/Panic.h>

#include "DTUState.h"

namespace kernel {

class VPE;
class VPEDesc;

class DTU {
    explicit DTU() : _next_ep(m3::DTU::FIRST_FREE_EP), _ep(alloc_ep()) {
        init();
    }

public:
    static DTU &get() {
        return _inst;
    }

    epid_t alloc_ep() {
        return _next_ep++;
    }

    DTUState &state() {
        return _state;
    }

    void init();

    peid_t log_to_phys(peid_t pe);

    void deprivilege(peid_t pe);

    void set_vpeid(const VPEDesc &vpe);
    void unset_vpeid(const VPEDesc &vpe);

    cycles_t get_time();
    void wakeup(const VPEDesc &vpe);
    void suspend(const VPEDesc &vpe);
    void injectIRQ(const VPEDesc &vpe);

    void config_rwb_remote(const VPEDesc &vpe, uintptr_t addr);
    void config_pf_remote(const VPEDesc &vpe, uint64_t rootpt, epid_t ep);

    void map_pages(const VPEDesc &vpe, uintptr_t virt, uintptr_t phys, uint pages, int perm);
    void unmap_pages(const VPEDesc &vpe, uintptr_t virt, uint pages);

    void read_ep_remote(const VPEDesc &vpe, epid_t ep, void *regs);
    void write_ep_remote(const VPEDesc &vpe, epid_t ep, void *regs);
    void write_ep_local(epid_t ep);

    void recv_msgs(epid_t ep, uintptr_t buf, uint order, uint msgorder);

    void reply(epid_t ep, const void *msg, size_t size, size_t msgidx);

    void send_to(const VPEDesc &vpe, epid_t ep, label_t label, const void *msg, size_t size,
        label_t replylbl, epid_t replyep, uint32_t sender = -1);
    void reply_to(const VPEDesc &vpe, epid_t ep, epid_t crdep, word_t credits, label_t label,
        const void *msg, size_t size);

    m3::Errors::Code try_write_mem(const VPEDesc &vpe, uintptr_t addr, const void *data, size_t size);
    m3::Errors::Code try_read_mem(const VPEDesc &vpe, uintptr_t addr, void *data, size_t size);

    void write_mem(const VPEDesc &vpe, uintptr_t addr, const void *data, size_t size) {
        if(try_write_mem(vpe, addr, data, size) != m3::Errors::NO_ERROR)
            PANIC("write failed");
    }
    void read_mem(const VPEDesc &vpe, uintptr_t addr, void *data, size_t size) {
        if(try_read_mem(vpe, addr, data, size) != m3::Errors::NO_ERROR)
            PANIC("read failed");
    }

    void cmpxchg_mem(const VPEDesc &vpe, uintptr_t addr, const void *data, size_t datasize,
        size_t off, size_t size);

    void write_swstate(const VPEDesc &vpe, uint64_t flags, uint64_t notify);
    void write_swflags(const VPEDesc &vpe, uint64_t flags);
    void read_swflags(const VPEDesc &vpe, uint64_t *flags);

private:
#if defined(__gem5__)
    bool create_pt(const VPEDesc &vpe, uintptr_t virt, uintptr_t pteAddr,
        m3::DTU::pte_t pte, int perm);
    bool create_ptes(const VPEDesc &vpe, uintptr_t &virt, uintptr_t pteAddr, m3::DTU::pte_t pte,
        uintptr_t &phys, uint &pages, int perm);
    uintptr_t get_pte_addr_mem(const VPEDesc &vpe, uint64_t root, uintptr_t virt, int level);
    void do_set_vpeid(const VPEDesc &vpe, vpeid_t nid);
    void do_ext_cmd(const VPEDesc &vpe, m3::DTU::reg_t cmd);
    void clear_pt(uintptr_t pt);
#endif

    epid_t _next_ep;
    epid_t _ep;
    DTUState _state;
    static DTU _inst;
};

}
