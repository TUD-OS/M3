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
#include "SyscallHandler.h"

namespace kernel {

class RGateObject;
class VPE;
class VPEDesc;

class DTU {
    explicit DTU() : _ep(SyscallHandler::srvep() + 1) {
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
    void wakeup(const VPEDesc &vpe);
    void suspend(const VPEDesc &vpe);
    void inject_irq(const VPEDesc &vpe);
    void ext_request(const VPEDesc &vpe, uint64_t req);
    void flush_cache(const VPEDesc &vpe);

    void invtlb_remote(const VPEDesc &vpe);
    void invlpg_remote(const VPEDesc &vpe, goff_t virt);

    m3::Errors::Code inval_ep_remote(const VPEDesc &vpe, epid_t ep);
    void read_ep_remote(const VPEDesc &vpe, epid_t ep, void *regs);
    void write_ep_remote(const VPEDesc &vpe, epid_t ep, void *regs);
    void write_ep_local(epid_t ep);

    void mark_read_remote(const VPEDesc &vpe, epid_t ep, goff_t msg);

    m3::Errors::Code get_header(const VPEDesc &vpe, const RGateObject *obj, goff_t &msgaddr,
                                void *head);
    m3::Errors::Code set_header(const VPEDesc &vpe, const RGateObject *obj, goff_t &msgaddr,
                                const void *head);

    void recv_msgs(epid_t ep, uintptr_t buf, int order, int msgorder);

    void reply(epid_t ep, const void *msg, size_t size, size_t msgidx);

    m3::Errors::Code send_to(const VPEDesc &vpe, epid_t ep, label_t label, const void *msg,
                             size_t size, label_t replylbl, epid_t replyep,
                             uint64_t sender = static_cast<uint64_t>(-1));
    m3::Errors::Code reply_to(const VPEDesc &vpe, epid_t rep, label_t label, const void *msg,
                              size_t size, uint64_t sender);

    m3::Errors::Code try_write_mem(const VPEDesc &vpe, goff_t addr, const void *data, size_t size);
    m3::Errors::Code try_read_mem(const VPEDesc &vpe, goff_t addr, void *data, size_t size);

    void write_mem(const VPEDesc &vpe, goff_t addr, const void *data, size_t size) {
        if(try_write_mem(vpe, addr, data, size) != m3::Errors::NONE)
            PANIC("write failed");
    }
    void read_mem(const VPEDesc &vpe, goff_t addr, void *data, size_t size) {
        if(try_read_mem(vpe, addr, data, size) != m3::Errors::NONE)
            PANIC("read failed");
    }

    void copy_clear(const VPEDesc &dstvpe, goff_t dstaddr,
                    const VPEDesc &srcvpe, goff_t srcaddr,
                    size_t size, bool clear);

    void write_swstate(const VPEDesc &vpe, uint64_t flags, uint64_t notify);
    void write_swflags(const VPEDesc &vpe, uint64_t flags);
    void read_swflags(const VPEDesc &vpe, uint64_t *flags);

private:
#if defined(__gem5__)
    void do_set_vpeid(const VPEDesc &vpe, vpeid_t nid);
    void do_ext_cmd(const VPEDesc &vpe, m3::DTU::reg_t cmd);
#endif

    epid_t _ep;
    DTUState _state;
    static DTU _inst;
};

}
