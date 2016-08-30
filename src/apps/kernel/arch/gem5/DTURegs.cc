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
#include <base/DTU.h>

#include "pes/VPE.h"
#include "DTUState.h"
#include "DTU.h"

namespace kernel {

void *DTUState::get_ep(epid_t ep) {
    return _regs._eps + ep * m3::DTU::EP_REGS;
}

void DTUState::save(const VPEDesc &vpe) {
    DTU::get().read_mem(VPEDesc(vpe.pe, VPE::INVALID_ID), m3::DTU::BASE_ADDR, this, sizeof(*this));
}

void DTUState::restore(const VPEDesc &vpe, vpeid_t vpeid) {
    // re-enable pagefaults, if we have a valid pagefault EP (the abort operation disables it)
    m3::DTU::reg_t status = 0;
    if(_regs.get(m3::DTU::DtuRegs::PF_EP) != static_cast<m3::DTU::reg_t>(-1))
        status = m3::DTU::StatusFlags::PAGEFAULTS;
    _regs.set(m3::DTU::DtuRegs::STATUS, status);

    // similarly, set the vpeid again, because abort invalidates it
    _regs.set(m3::DTU::DtuRegs::VPE_ID, vpeid);

    m3::Sync::compiler_barrier();
    DTU::get().write_mem(vpe, m3::DTU::BASE_ADDR, this, sizeof(*this));
}

void DTUState::invalidate(epid_t ep) {
    memset(get_ep(ep), 0, sizeof(m3::DTU::reg_t) * m3::DTU::EP_REGS);
}

void DTUState::invalidate_eps(epid_t first) {
    size_t total = sizeof(m3::DTU::reg_t) * m3::DTU::EP_REGS * (EP_COUNT - first);
    memset(get_ep(first), 0, total);
}

void DTUState::config_recv(epid_t ep, uintptr_t buf, uint order, uint msgorder, int) {
    m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
    m3::DTU::reg_t bufSize = static_cast<m3::DTU::reg_t>(1) << (order - msgorder);
    m3::DTU::reg_t msgSize = static_cast<m3::DTU::reg_t>(1) << msgorder;
    r[0] = (static_cast<m3::DTU::reg_t>(m3::DTU::EpType::RECEIVE) << 61) |
            ((msgSize & 0xFFFF) << 32) | ((bufSize & 0xFFFF) << 16) | 0;
    r[1] = buf;
    r[2] = 0;
}

void DTUState::config_send(epid_t ep, label_t lbl, peid_t pe, vpeid_t vpe, epid_t dstep, size_t msgsize, word_t) {
    m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
    r[0] = (static_cast<m3::DTU::reg_t>(m3::DTU::EpType::SEND) << 61) |
            ((vpe & 0xFFFF) << 16) | (msgsize & 0xFFFF);
    // TODO hand out "unlimited" credits atm
    r[1] = ((pe & 0xFF) << 24) | ((dstep & 0xFF) << 16) | m3::DTU::CREDITS_UNLIM;
    r[2] = lbl;
}

void DTUState::config_mem(epid_t ep, peid_t pe, vpeid_t vpe, uintptr_t addr, size_t size, int perm) {
    m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
    r[0] = (static_cast<m3::DTU::reg_t>(m3::DTU::EpType::MEMORY) << 61) | (size & 0x1FFFFFFFFFFFFFFF);
    r[1] = addr;
    r[2] = ((vpe & 0xFFFF) << 12) | ((pe & 0xFF) << 4) | (perm & 0x7);
}

void DTUState::config_rwb(uintptr_t addr) {
    _regs.set(m3::DTU::DtuRegs::RW_BARRIER, addr);
}

void DTUState::config_pf(uint64_t rootpt, epid_t ep) {
    uint flags = 0;
    if(ep != static_cast<epid_t>(-1))
        flags = static_cast<uint>(m3::DTU::StatusFlags::PAGEFAULTS);
    _regs.set(m3::DTU::DtuRegs::STATUS, flags);
    _regs.set(m3::DTU::DtuRegs::ROOT_PT, rootpt);
    _regs.set(m3::DTU::DtuRegs::PF_EP, ep);
}

void DTUState::reset(uintptr_t addr) {
    m3::DTU::reg_t value = static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::RESET) | (addr << 3);
    _regs.set(m3::DTU::DtuRegs::EXT_CMD, value);
}

}
