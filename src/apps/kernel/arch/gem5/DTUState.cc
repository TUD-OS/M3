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
#include <base/CPU.h>
#include <base/DTU.h>

#include "pes/VPE.h"
#include "pes/VPEManager.h"
#include "DTUState.h"
#include "DTU.h"
#include "Platform.h"

namespace kernel {

bool DTUState::was_idling() const {
    return _regs.get(m3::DTU::DtuRegs::MSG_CNT) == 0 &&
        (_regs.get(m3::DTU::DtuRegs::FEATURES) & m3::DTU::IRQ_WAKEUP);
}

cycles_t DTUState::get_idle_time() const {
    return _regs.get(m3::DTU::DtuRegs::IDLE_TIME);
}

void *DTUState::get_ep(epid_t ep) {
    return _regs._eps + ep * m3::DTU::EP_REGS;
}

void DTUState::move_rbufs(const VPEDesc &vpe, vpeid_t oldvpe, bool save) {
    VPE &vpeobj = VPEManager::get().vpe(vpe.id);
    VPEDesc memvpe(vpeobj.recvbuf_copy().pe(), VPE::INVALID_ID);
    size_t offset = vpeobj.recvbuf_copy().addr;

    for(epid_t ep = 0; ep < EP_COUNT; ++ep) {
        m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
        // receive EP and any slot occupied?
        if(static_cast<m3::DTU::EpType>(r[0] >> 61) == m3::DTU::EpType::RECEIVE && r[2]) {
            const uintptr_t addr = r[1];
            const size_t size = ((r[0] >> 16) & 0xFFFF) * ((r[0] >> 32) & 0xFFFF);
            if(save)
                DTU::get().copy_clear(memvpe, offset, vpe, addr, size, false);
            else
                DTU::get().copy_clear(VPEDesc(vpe.pe, oldvpe), addr, memvpe, offset, size, false);
            offset += size;
        }
    }
}

void DTUState::save(const VPEDesc &vpe) {
    DTU::get().read_mem(vpe, m3::DTU::BASE_ADDR, this, sizeof(*this));

    // copy the receive buffers, which have pending messages, to an external location
    if(!Platform::pe(vpe.pe).has_virtmem())
        move_rbufs(vpe, 0, true);
}

void DTUState::restore(const VPEDesc &vpe, vpeid_t vpeid) {
    // copy the receive buffers back to the SPM
    if(!Platform::pe(vpe.pe).has_virtmem())
        move_rbufs(VPEDesc(vpe.pe, vpeid), vpe.id, false);

    // re-enable pagefaults, if we have a valid pagefault EP (the abort operation disables it)
    // and unset COM_DISABLED and IRQ_WAKEUP
    m3::DTU::reg_t features = 0;
    if(_regs.get(m3::DTU::DtuRegs::PF_EP) != static_cast<epid_t>(-1))
        features = m3::DTU::StatusFlags::PAGEFAULTS;
    _regs.set(m3::DTU::DtuRegs::FEATURES, features);

    // similarly, set the vpeid again, because abort invalidates it
    _regs.set(m3::DTU::DtuRegs::VPE_ID, vpeid);

    // reset idle time and msg count; msg count will be recalculated from the EPs
    _regs.set(m3::DTU::DtuRegs::IDLE_TIME, 0);

    _regs.set(m3::DTU::DtuRegs::RW_BARRIER, Platform::rw_barrier(vpe.pe));

    m3::CPU::compiler_barrier();
    DTU::get().write_mem(vpe, m3::DTU::BASE_ADDR, this, sizeof(*this));
}

bool DTUState::invalidate(epid_t ep, bool check) {
    if(check) {
        m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
        if(static_cast<m3::DTU::EpType>(r[0] >> 61) == m3::DTU::EpType::SEND) {
            if(((r[1] >> 16) & 0xFFFF) != (r[1] & 0xFFFF))
                return false;
        }
    }

    memset(get_ep(ep), 0, sizeof(m3::DTU::reg_t) * m3::DTU::EP_REGS);
    return true;
}

void DTUState::invalidate_eps(epid_t first) {
    size_t total = sizeof(m3::DTU::reg_t) * m3::DTU::EP_REGS * (EP_COUNT - first);
    memset(get_ep(first), 0, total);
}

bool DTUState::can_forward_msg(epid_t ep) {
    m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
    return ((r[0] >> 16) & 0xFFFF) == VPE::INVALID_ID;
}

void DTUState::forward_msg(epid_t ep, peid_t pe, vpeid_t vpe) {
    m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
    r[0] &= ~(static_cast<m3::DTU::reg_t>(0xFFFF) << 16);
    r[0] |= vpe << 16;
    r[1] &= ~(static_cast<m3::DTU::reg_t>(0xFF) << 40);
    r[1] |= static_cast<m3::DTU::reg_t>(pe) << 40;
}

void DTUState::forward_mem(epid_t ep, peid_t pe) {
    m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
    r[2] &= ~(static_cast<m3::DTU::reg_t>(0xFF) << 4);
    r[2] |= pe << 4;
}

void DTUState::read_ep(const VPEDesc &vpe, epid_t ep) {
    DTU::get().read_ep_remote(vpe, ep, get_ep(ep));
}

void DTUState::config_recv(epid_t ep, uintptr_t buf, int order, int msgorder) {
    m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
    m3::DTU::reg_t bufSize = static_cast<m3::DTU::reg_t>(1) << (order - msgorder);
    m3::DTU::reg_t msgSize = static_cast<m3::DTU::reg_t>(1) << msgorder;
    r[0] = (static_cast<m3::DTU::reg_t>(m3::DTU::EpType::RECEIVE) << 61) |
            ((msgSize & 0xFFFF) << 32) | ((bufSize & 0xFFFF) << 16) | 0;
    r[1] = buf;
    r[2] = 0;
}

void DTUState::config_send(epid_t ep, label_t lbl, peid_t pe, vpeid_t vpe, epid_t dstep,
        size_t msgsize, word_t credits) {
    m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
    r[0] = (static_cast<m3::DTU::reg_t>(m3::DTU::EpType::SEND) << 61) |
            ((vpe & 0xFFFF) << 16) | (msgsize & 0xFFFF);
    r[1] = (static_cast<m3::DTU::reg_t>(pe & 0xFF) << 40) |
            (static_cast<m3::DTU::reg_t>(dstep & 0xFF) << 32) |
            (credits << 16) |
            (credits << 0);
    r[2] = lbl;
}

void DTUState::config_mem(epid_t ep, peid_t pe, vpeid_t vpe, uintptr_t addr, size_t size, int perm) {
    m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
    r[0] = (static_cast<m3::DTU::reg_t>(m3::DTU::EpType::MEMORY) << 61) | (size & 0x1FFFFFFFFFFFFFFF);
    r[1] = addr;
    r[2] = ((vpe & 0xFFFF) << 12) | ((pe & 0xFF) << 4) | (perm & 0x7);
}

bool DTUState::config_mem_cached(epid_t ep, peid_t pe, vpeid_t vpe) {
    m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
    m3::DTU::reg_t r0, r2;
    r0 = (static_cast<m3::DTU::reg_t>(m3::DTU::EpType::MEMORY) << 61) | 0x1FFFFFFFFFFFFFFF;
    r2 = ((vpe & 0xFFFF) << 12) | ((pe & 0xFF) << 4) | m3::DTU::RW;
    bool res = false;
    if(r0 != r[0]) {
        r[0] = r0;
        res = true;
    }
    if(r[1] != 0) {
        r[1] = 0;
        res = true;
    }
    if(r2 != r[2]) {
        r[2] = r2;
        res = true;
    }
    return res;
}

void DTUState::config_pf(gaddr_t rootpt, epid_t sep, epid_t rep) {
    uint features = 0;
    if(sep != static_cast<epid_t>(-1))
        features = static_cast<uint>(m3::DTU::StatusFlags::PAGEFAULTS);
    _regs.set(m3::DTU::DtuRegs::FEATURES, features);
    _regs.set(m3::DTU::DtuRegs::ROOT_PT, rootpt);
    _regs.set(m3::DTU::DtuRegs::PF_EP, sep | (rep << 8));
}

void DTUState::reset(uintptr_t addr) {
    m3::DTU::reg_t value = static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::RESET) | (addr << 3);
    _regs.set(m3::DTU::DtuRegs::EXT_CMD, value);
}

}
