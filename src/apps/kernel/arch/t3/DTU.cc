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
#include <base/CPU.h>
#include <base/DTU.h>

#include "pes/VPE.h"
#include "DTU.h"

namespace kernel {

void DTU::init() {
    // nothing to do
}

int DTU::log_to_phys(int pe) {
    return pe;
}

void DTU::deprivilege(int) {
    // nothing to do
}

void DTU::set_vpeid(const VPEDesc &) {
    // nothing to do
}

void DTU::unset_vpeid(const VPEDesc &) {
    // nothing to do
}

void DTU::wakeup(const VPEDesc &vpe, uintptr_t) {
    // write the core id to the PE
    uint64_t id = vpe.core;
    m3::CPU::compiler_barrier();
    write_mem(vpe, RT_START, &id, sizeof(id));

    inject_irq(vpe);
}

void DTU::suspend(const VPEDesc &) {
}

void DTU::inject_irq(const VPEDesc &vpe) {
    // inject an IRQ
    uint64_t val = 1;
    m3::CPU::memory_barrier();
    write_mem(vpe, IRQ_ADDR_EXTERN, &val, sizeof(val));
}

void DTU::config_pf_remote(const VPEDesc &, gaddr_t, int) {
}

void DTU::map_pages(const VPEDesc &, uintptr_t, uintptr_t, uint, int) {
    // unsupported
}

void DTU::unmap_pages(const VPEDesc &, uintptr_t, uint) {
    // unsupported
}

void DTU::invalidate_ep(const VPEDesc &vpe, epid_t ep) {
    alignas(DTU_PKG_SIZE) uint64_t regs[EXTERN_CFG_SIZE_CREDITS_CMD + 1];
    memset(regs, 0, sizeof(regs));
    regs[OVERALL_SLOT_CFG] = (uint64_t)0xFFFFFFFF << 32;

    m3::CPU::memory_barrier();
    uintptr_t addr = m3::DTU::get().get_external_cmd_addr(ep, 0);
    write_mem(vpe, addr, regs, sizeof(regs));
}

void DTU::invalidate_eps(const VPEDesc &vpe, int first) {
    for(int i = first; i < EP_COUNT; ++i)
        invalidate_ep(vpe, i);
}

void DTU::config_recv_local(epid_t ep, uintptr_t buf, uint order, uint msgorder, int flags) {
    m3::DTU::get().configure_recv(ep, buf, order, msgorder, flags);
}

void DTU::config_recv_remote(const VPEDesc &, int, uintptr_t, uint, uint, int, bool) {
    // nothing to do; can only be done locally atm
}

void DTU::config_send(void *e, label_t label, int dstcore, int, epid_t dstep, size_t, word_t) {
    uint64_t *ep = reinterpret_cast<uint64_t*>(e);

    /* (1 << LOCAL_CFG_ADDRESS_FIFO_CMD) |
       (1 << TRANSFER_CFG_SIZE_STRIDE_REPEAT_CMD) |
       (1 << HEADER_CFG_REPLY_LABEL_SLOT_ENABLE_ADDR) |
       (1 << FIRE_CMD) |
       (1 << DEBUG_CMD) */
    /* TODO atm, we need to give him all permissions */

    ep[OVERALL_SLOT_CFG] = ((uint64_t)0xFFFFFFFF << 32) | label;
    ep[LOCAL_CFG_ADDRESS_FIFO_CMD] = 0;
    ep[TRANSFER_CFG_SIZE_STRIDE_REPEAT_CMD] = 0;
    ep[EXTERN_CFG_ADDRESS_MODULE_CHIP_CTA_INC_CMD] =
        (uint64_t)((1 << 24) | (0 << 16) | dstcore) << 32 | m3::DTU::get().get_slot_addr(dstep);
    // TODO hand out the real credits
    ep[EXTERN_CFG_SIZE_CREDITS_CMD] = (uint64_t)0xFFFFFFFF << 32;
}

void DTU::config_send_local(epid_t ep, label_t label, int dstcore, int dstvpe, epid_t dstep,
                            size_t msgsize, word_t credits) {
    config_send(m3::DTU::get().get_cmd_addr(ep, 0), label, dstcore, dstvpe, dstep, msgsize, credits);
}

void DTU::config_send_remote(const VPEDesc &vpe, epid_t ep, label_t label, int dstcore,
                             int dstvpe, epid_t dstep, size_t msgsize, word_t credits) {
    alignas(DTU_PKG_SIZE) uint64_t regs[EXTERN_CFG_SIZE_CREDITS_CMD + 1];
    config_send(regs, label, dstcore, dstvpe, dstep, msgsize, credits);

    m3::CPU::memory_barrier();
    uintptr_t epaddr = m3::DTU::get().get_external_cmd_addr(ep, 0);
    write_mem(vpe, epaddr, regs, sizeof(regs));
}

void DTU::config_mem(void *e, int dstcore, int, uintptr_t addr, size_t, int) {
    uint64_t *ep = reinterpret_cast<uint64_t*>(e);
    ep[OVERALL_SLOT_CFG] = ((uint64_t)0xFFFFFFFF << 32);
    ep[LOCAL_CFG_ADDRESS_FIFO_CMD] = 0;
    ep[TRANSFER_CFG_SIZE_STRIDE_REPEAT_CMD] = 0;
    ep[EXTERN_CFG_ADDRESS_MODULE_CHIP_CTA_INC_CMD] =
        (uint64_t)((1 << 24) | (1 << 16) | dstcore) << 32 | addr;
    // TODO hand out the real credits
    ep[EXTERN_CFG_SIZE_CREDITS_CMD] = (uint64_t)0xFFFFFFFF << 32;
}

void DTU::config_mem_local(epid_t ep, int dstcore, int dstvpe, uintptr_t addr, size_t size) {
    config_mem(m3::DTU::get().get_cmd_addr(ep, 0), dstcore, dstvpe, addr, size, m3::KIF::Perm::RW);
}

void DTU::config_mem_remote(const VPEDesc &vpe, epid_t ep, int dstcore, int dstvpe, uintptr_t addr,
                            size_t size, int perms) {
    alignas(DTU_PKG_SIZE) uint64_t regs[EXTERN_CFG_SIZE_CREDITS_CMD + 1];
    config_mem(regs, dstcore, dstvpe, addr, size, perms);

    m3::CPU::memory_barrier();
    uintptr_t epaddr = m3::DTU::get().get_external_cmd_addr(ep, 0);
    write_mem(vpe, epaddr, regs, sizeof(regs));
}

void DTU::send_to(const VPEDesc &vpe, epid_t ep, label_t label, const void *msg, size_t size,
                  label_t replylbl, epid_t replyep) {
    // TODO for some reason, we need to use a different EP here.
    epid_t tmpep = m3::DTU::FIRST_FREE_EP;
    config_send_local(tmpep, label, vpe.core, vpe.id, ep, size + m3::DTU::HEADER_SIZE,
                      size + m3::DTU::HEADER_SIZE);
    m3::DTU::get().send(tmpep, msg, size, replylbl, replyep);
    m3::DTU::get().wait_until_ready(tmpep);
}

void DTU::reply_to(const VPEDesc &vpe, epid_t ep, int, word_t, label_t label,
                   const void *msg, size_t size) {
    send_to(vpe, ep, label, msg, size, 0, 0);
}

void DTU::write_mem(const VPEDesc &vpe, uintptr_t addr, const void *data, size_t size) {
    config_mem_local(_ep, vpe.core, vpe.id, addr, size);
    m3::DTU::get().write(_ep, data, size, 0, 0);
}

void DTU::read_mem(const VPEDesc &vpe, uintptr_t addr, void *data, size_t size) {
    config_mem_local(_ep, vpe.core, vpe.id, addr, size);
    m3::DTU::get().read(_ep, data, size, 0, 0);
}

}
