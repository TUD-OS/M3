/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/Common.h>
#include <base/DTU.h>

#include "pes/VPE.h"
#include "DTUState.h"
#include "DTU.h"

namespace kernel {

bool DTUState::was_idling() const {
    // not supported
    return false;
}

cycles_t DTUState::get_idle_time() const {
    // not supported
    return 0;
}

void *DTUState::get_ep(epid_t ep) {
    return _regs._eps + ep * m3::DTU::EPS_RCNT;
}

void DTUState::save(const VPEDesc &, size_t) {
    // not supported
}

void DTUState::restore(const VPEDesc &, size_t, vpeid_t) {
    // not supported
}

void DTUState::enable_communication(const VPEDesc &) {
    // not supported
}

bool DTUState::invalidate(epid_t ep, bool) {
    memset(get_ep(ep), 0, sizeof(word_t) * m3::DTU::EPS_RCNT);
    return true;
}

void DTUState::invalidate_eps(epid_t first) {
    size_t total = sizeof(word_t) * m3::DTU::EPS_RCNT * (EP_COUNT - first);
    memset(get_ep(first), 0, total);
}

bool DTUState::can_forward_msg(epid_t) {
    // not supported
    return false;
}

void DTUState::forward_msg(epid_t, peid_t, vpeid_t) {
    // not supported
}

void DTUState::forward_mem(epid_t, peid_t) {
    // not supported
}

void DTUState::read_ep(const VPEDesc &vpe, epid_t ep) {
    DTU::get().read_ep_remote(vpe, ep, get_ep(ep));
}

void DTUState::config_recv(epid_t ep, goff_t buf, int order, int msgorder, uint) {
    word_t *regs = reinterpret_cast<word_t*>(get_ep(ep));
    regs[m3::DTU::EP_BUF_ADDR]       = buf;
    regs[m3::DTU::EP_BUF_ORDER]      = static_cast<word_t>(order);
    regs[m3::DTU::EP_BUF_MSGORDER]   = static_cast<word_t>(msgorder);
    regs[m3::DTU::EP_BUF_ROFF]       = 0;
    regs[m3::DTU::EP_BUF_WOFF]       = 0;
    regs[m3::DTU::EP_BUF_MSGCNT]     = 0;
    regs[m3::DTU::EP_BUF_UNREAD]     = 0;
    regs[m3::DTU::EP_BUF_OCCUPIED]   = 0;
}

void DTUState::config_send(epid_t ep, label_t lbl, peid_t pe, vpeid_t, epid_t dstep, size_t msgsize, word_t credits) {
    word_t *regs = reinterpret_cast<word_t*>(get_ep(ep));
    regs[m3::DTU::EP_LABEL]         = lbl;
    regs[m3::DTU::EP_PEID]          = pe;
    regs[m3::DTU::EP_EPID]          = dstep;
    regs[m3::DTU::EP_CREDITS]       = credits;
    regs[m3::DTU::EP_MSGORDER]      = static_cast<word_t>(m3::getnextlog2(msgsize));
}

void DTUState::config_mem(epid_t ep, peid_t pe, vpeid_t, goff_t addr, size_t size, int perms) {
    word_t *regs = reinterpret_cast<word_t*>(get_ep(ep));
    assert((addr & static_cast<goff_t>(perms)) == 0);
    regs[m3::DTU::EP_LABEL]         = addr | static_cast<uint>(perms);
    regs[m3::DTU::EP_PEID]          = pe;
    regs[m3::DTU::EP_EPID]          = 0;
    regs[m3::DTU::EP_CREDITS]       = size;
    regs[m3::DTU::EP_MSGORDER]      = 0;
}

bool DTUState::config_mem_cached(epid_t, peid_t, vpeid_t) {
    // unused
    return true;
}

void DTUState::config_pf(gaddr_t, epid_t, epid_t) {
    // not supported
}

void DTUState::reset(gaddr_t, bool) {
    // not supported
}

}
