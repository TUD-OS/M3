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

#include <base/Common.h>
#include <base/RCTMux.h>

#include "pes/VPEManager.h"
#include "DTU.h"

namespace kernel {

void DTU::init() {
    // nothing to do
}

peid_t log_to_phys(peid_t pe) {
    return pe;
}

void DTU::deprivilege(peid_t) {
    // unsupported
}

void DTU::set_vpeid(const VPEDesc &) {
    // unsupported
}

void DTU::unset_vpeid(const VPEDesc &) {
    // unsupported
}

cycles_t DTU::get_time() {
    // unsupported
    return 0;
}

void DTU::wakeup(const VPEDesc &) {
    // nothing to do
}

void DTU::suspend(const VPEDesc &) {
    // nothing to do
}

void DTU::injectIRQ(const VPEDesc &) {
    // unsupported
}

void DTU::config_rwb_remote(const VPEDesc &, uintptr_t) {
    // unsupported
}

void DTU::config_pf_remote(const VPEDesc &, gaddr_t, epid_t) {
    // unsupported
}

void DTU::map_pages(const VPEDesc &, uintptr_t, uintptr_t, uint, int) {
    // unsupported
}

void DTU::unmap_pages(const VPEDesc &, uintptr_t, uint) {
    // unsupported
}

m3::Errors::Code DTU::inval_ep_remote(const VPEDesc &vpe, epid_t ep) {
    word_t regs[m3::DTU::EPS_RCNT];
    memset(regs, 0, sizeof(regs));
    // TODO detect if credits are outstanding
    write_ep_remote(vpe, ep, regs);
    return m3::Errors::NONE;
}

void DTU::read_ep_remote(const VPEDesc &vpe, epid_t ep, void *regs) {
    uintptr_t eps = reinterpret_cast<uintptr_t>(VPEManager::get().vpe(vpe.id).ep_addr());
    uintptr_t addr = eps + ep * m3::DTU::EPS_RCNT * sizeof(word_t);
    read_mem(vpe, addr, regs, m3::DTU::EPS_RCNT * sizeof(word_t));
}

void DTU::write_ep_remote(const VPEDesc &vpe, epid_t ep, void *regs) {
    uintptr_t eps = reinterpret_cast<uintptr_t>(VPEManager::get().vpe(vpe.id).ep_addr());
    uintptr_t addr = eps + ep * m3::DTU::EPS_RCNT * sizeof(word_t);
    write_mem(vpe, addr, regs, m3::DTU::EPS_RCNT * sizeof(word_t));
}

void DTU::write_ep_local(epid_t ep) {
    uintptr_t eps = reinterpret_cast<uintptr_t>(m3::DTU::get().ep_regs());
    uintptr_t addr = eps + ep * m3::DTU::EPS_RCNT * sizeof(word_t);
    memcpy(reinterpret_cast<void*>(addr), _state.get_ep(ep), m3::DTU::EPS_RCNT * sizeof(word_t));
}

void DTU::mark_read_remote(const VPEDesc &, epid_t, uintptr_t) {
    // not supported
}

void DTU::drop_msgs(epid_t ep, label_t label) {
    word_t *regs = reinterpret_cast<word_t*>(_state.get_ep(ep));
    // we assume that the one that used the label can no longer send messages. thus, if there are
    // no messages yet, we are done.
    if(regs[m3::DTU::EP_BUF_MSGCNT] == 0)
        return;

    uintptr_t base = regs[m3::DTU::EP_BUF_ADDR];
    int order = regs[m3::DTU::EP_BUF_ORDER];
    int msgorder = regs[m3::DTU::EP_BUF_MSGORDER];
    word_t unread = regs[m3::DTU::EP_BUF_UNREAD];
    int max = 1UL << (order - msgorder);
    for(int i = 0; i < max; ++i) {
        if(unread & (1UL << i)) {
            m3::DTU::Message *msg = reinterpret_cast<m3::DTU::Message*>(
                base + (static_cast<uintptr_t>(i) << msgorder));
            if(msg->label == label)
                m3::DTU::get().mark_read(ep, reinterpret_cast<uintptr_t>(msg));
        }
    }
}

m3::Errors::Code get_header(const VPEDesc &, const RGateObject *, uintptr_t &, m3::DTU::Header &) {
    // unused
    return m3::Errors::NONE;
}

m3::Errors::Code set_header(const VPEDesc &, const RGateObject *, uintptr_t &, const m3::DTU::Header &) {
    // unused
    return m3::Errors::NONE;
}

void DTU::recv_msgs(epid_t ep, uintptr_t buf, int order, int msgorder) {
    _state.config_recv(ep, buf, order, msgorder);
    write_ep_local(ep);
}

void DTU::reply(epid_t ep, const void *msg, size_t size, size_t msgidx) {
    m3::DTU::get().reply(ep, msg, size, msgidx);
}

void DTU::send_to(const VPEDesc &vpe, epid_t ep, label_t label, const void *msg, size_t size,
        label_t replylbl, epid_t replyep, uint64_t) {
    m3::DTU::get().configure(_ep, label, vpe.pe, ep, size + m3::DTU::HEADER_SIZE);
    m3::DTU::get().send(_ep, msg, size, replylbl, replyep);
}

void DTU::reply_to(const VPEDesc &, epid_t, label_t, const void *, size_t, uint64_t) {
    // UNUSED
}

m3::Errors::Code DTU::try_write_mem(const VPEDesc &vpe, uintptr_t addr, const void *data, size_t size) {
    m3::DTU::get().configure(_ep, addr | m3::KIF::Perm::RWX, vpe.pe, 0, size);
    m3::DTU::get().write(_ep, data, size, 0, 0);
    return m3::Errors::NONE;
}

m3::Errors::Code DTU::try_read_mem(const VPEDesc &vpe, uintptr_t addr, void *data, size_t size) {
    m3::DTU::get().configure(_ep, addr | m3::KIF::Perm::RWX, vpe.pe, 0, size);
    m3::DTU::get().read(_ep, data, size, 0, 0);
    return m3::Errors::NONE;
}

void DTU::copy_clear(const VPEDesc &, uintptr_t, const VPEDesc &, uintptr_t, size_t, bool) {
    // not supported
}

void DTU::cmpxchg_mem(const VPEDesc &vpe, uintptr_t addr, const void *data, size_t datasize,
    size_t off, size_t size) {
    m3::DTU::get().configure(_ep, (addr + off) | m3::KIF::Perm::RWX, vpe.pe, 0, datasize);
    m3::DTU::get().cmpxchg(_ep, data, datasize, 0, size);
    m3::DTU::get().wait_for_mem_cmd();
}

void DTU::write_swstate(const VPEDesc &, uint64_t, uint64_t) {
}

void DTU::write_swflags(const VPEDesc &, uint64_t) {
}

void DTU::read_swflags(const VPEDesc &, uint64_t *flags) {
    // we are always immediately finished here
    *flags = m3::RCTMuxCtrl::SIGNAL;
}

}
