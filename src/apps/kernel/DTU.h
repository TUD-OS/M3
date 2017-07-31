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

class RGateObject;
class VPE;
class VPEDesc;

class DTU {
    explicit DTU() : _ep(m3::DTU::UPCALL_REP) {
        init();
    }

public:
    static DTU &get() {
        return _inst;
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
    void wakeup(const VPEDesc &vpe, uintptr_t addr = 0);
    void suspend(const VPEDesc &vpe);
    void inject_irq(const VPEDesc &vpe);

    void config_pf_remote(const VPEDesc &vpe, gaddr_t rootpt, epid_t sep, epid_t rep);

    void set_rootpt_remote(const VPEDesc &vpe, gaddr_t rootpt);
    void invlpg_remote(const VPEDesc &vpe, uintptr_t virt);

    void map_pages(const VPEDesc &vpe, uintptr_t virt, gaddr_t phys, uint pages, int perm);
    void unmap_pages(const VPEDesc &vpe, uintptr_t virt, uint pages);
    void remove_pts(vpeid_t vpe);

    m3::Errors::Code inval_ep_remote(const VPEDesc &vpe, epid_t ep);
    void read_ep_remote(const VPEDesc &vpe, epid_t ep, void *regs);
    void write_ep_remote(const VPEDesc &vpe, epid_t ep, void *regs);
    void write_ep_local(epid_t ep);

    void mark_read_remote(const VPEDesc &vpe, epid_t ep, uintptr_t msg);

    void drop_msgs(epid_t ep, label_t label);

    m3::Errors::Code get_header(const VPEDesc &vpe, const RGateObject *obj, uintptr_t &msgaddr,
        m3::DTU::Header &head);
    m3::Errors::Code set_header(const VPEDesc &vpe, const RGateObject *obj, uintptr_t &msgaddr,
        const m3::DTU::Header &head);

    void recv_msgs(epid_t ep, uintptr_t buf, int order, int msgorder);

    void reply(epid_t ep, const void *msg, size_t size, size_t msgidx);

    void send_to(const VPEDesc &vpe, epid_t ep, label_t label, const void *msg, size_t size,
        label_t replylbl, epid_t replyep, uint64_t sender = static_cast<uint64_t>(-1));
    void reply_to(const VPEDesc &vpe, epid_t rep, label_t label, const void *msg, size_t size,
        uint64_t sender);

    m3::Errors::Code try_write_mem(const VPEDesc &vpe, uintptr_t addr, const void *data, size_t size);
    m3::Errors::Code try_read_mem(const VPEDesc &vpe, uintptr_t addr, void *data, size_t size);

    void write_mem(const VPEDesc &vpe, uintptr_t addr, const void *data, size_t size) {
        if(try_write_mem(vpe, addr, data, size) != m3::Errors::NONE)
            PANIC("write failed");
    }
    void read_mem(const VPEDesc &vpe, uintptr_t addr, void *data, size_t size) {
        if(try_read_mem(vpe, addr, data, size) != m3::Errors::NONE)
            PANIC("read failed");
    }

    void copy_clear(const VPEDesc &dstvpe, uintptr_t dstaddr,
                    const VPEDesc &srcvpe, uintptr_t srcaddr,
                    size_t size, bool clear);

    void cmpxchg_mem(const VPEDesc &vpe, uintptr_t addr, const void *data, size_t datasize,
        size_t off, size_t size);

    void write_swstate(const VPEDesc &vpe, uint64_t flags, uint64_t notify);
    void write_swflags(const VPEDesc &vpe, uint64_t flags);
    void read_swflags(const VPEDesc &vpe, uint64_t *flags);

private:
#if defined(__gem5__)
    bool create_pt(const VPEDesc &vpe, vpeid_t vpeid, uintptr_t virt, uintptr_t pteAddr,
        m3::DTU::pte_t pte, gaddr_t &phys, uint &pages, int perm, int level);
    bool create_ptes(const VPEDesc &vpe, vpeid_t vpeid, uintptr_t &virt, uintptr_t pteAddr,
        m3::DTU::pte_t pte, gaddr_t &phys, uint &pages, int perm);
    void remove_pts_rec(vpeid_t vpe, gaddr_t pt, uintptr_t virt, int level);
    uintptr_t get_pte_addr_mem(const VPEDesc &vpe, gaddr_t root, uintptr_t virt, int level);
    void do_inject_irq(const VPEDesc &vpe, uint64_t cmd);
    void do_set_vpeid(const VPEDesc &vpe, vpeid_t nid);
    void do_ext_cmd(const VPEDesc &vpe, m3::DTU::reg_t cmd);
    void mmu_cmd_remote(const VPEDesc &vpe, m3::DTU::reg_t arg);
    void clear_pt(gaddr_t);
#endif

    epid_t _ep;
    DTUState _state;
    static DTU _inst;
};

}
