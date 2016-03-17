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

#include <m3/Common.h>
#include <m3/util/Sync.h>
#include <m3/DTU.h>

#include "../../KDTU.h"
#include "../../KVPE.h"

namespace kernel {

void KDTU::init() {
    // nothing to do
}

void KDTU::deprivilege(int) {
    // unsupported
}

void KDTU::set_vpeid(int, int) {
    // unsupported
}

void KDTU::unset_vpeid(int, int) {
    // unsupported
}

void KDTU::wakeup(KVPE &vpe) {
    // first, invalidate all endpoints to start fresh
    invalidate_eps(vpe);

    // write the core id to the PE
    uint64_t id = vpe.core();
    m3::Sync::compiler_barrier();
    write_mem(vpe, RT_START, &id, sizeof(id));

    // configure syscall endpoint again
    config_send_remote(vpe, m3::DTU::SYSC_EP, reinterpret_cast<label_t>(&vpe.syscall_gate()),
        KERNEL_CORE, KERNEL_CORE, m3::DTU::SYSC_EP,
        1 << KVPE::SYSC_CREDIT_ORD, 1 << KVPE::SYSC_CREDIT_ORD);

    injectIRQ(vpe);
}

void KDTU::suspend(KVPE &) {
    // nothing to do
}

void KDTU::injectIRQ(KVPE &vpe) {
    // inject an IRQ
    uint64_t val = 1;
    m3::Sync::memory_barrier();
    write_mem(vpe, IRQ_ADDR_EXTERN, &val, sizeof(val));
}

void KDTU::config_pf_remote(KVPE &, int) {
    // unsupported
}

void KDTU::map_page(KVPE &, uintptr_t, uintptr_t, int) {
    // unsupported
}

void KDTU::unmap_page(KVPE &, uintptr_t) {
    // unsupported
}

void KDTU::invalidate_ep(KVPE &vpe, int ep) {
    alignas(DTU_PKG_SIZE) m3::EPConf conf;
    memset(&conf, 0, sizeof(conf));
    m3::Sync::memory_barrier();
    uintptr_t addr = EPS_START + ep * sizeof(m3::EPConf);
    write_mem(vpe, addr, &conf, sizeof(conf));
}

void KDTU::invalidate_eps(KVPE &vpe) {
    alignas(DTU_PKG_SIZE) char eps[EPS_SIZE];
    memset(eps, 0, sizeof(eps));
    m3::Sync::memory_barrier();
    write_mem(vpe, EPS_START, eps, sizeof(eps));
}

void KDTU::config_recv_local(int, uintptr_t, uint, uint, int) {
    // nothing to do; everything is always ready and fixed on T2 for receiving
}

void KDTU::config_recv_remote(KVPE &, int, uintptr_t, uint, uint, int, bool) {
    // nothing to do; everything is always ready and fixed on T2 for receiving
}

void KDTU::config_send(void *e, label_t label, int dstcore, int, int dstep, size_t, word_t credits) {
    m3::EPConf *ep = reinterpret_cast<m3::EPConf*>(e);
    ep->valid = 1;
    ep->dstcore = dstcore;
    ep->dstep = dstep;
    ep->label = label;
    ep->credits = credits;
}

void KDTU::config_send_local(int ep, label_t label, int dstcore, int dstvpe, int dstep,
        size_t msgsize, word_t credits) {
    config_send(m3::eps() + ep, label, dstcore, dstvpe, dstep, msgsize, credits);
}

void KDTU::config_send_remote(KVPE &vpe, int ep, label_t label, int dstcore, int dstvpe, int dstep,
        size_t msgsize, word_t credits) {
    alignas(DTU_PKG_SIZE) m3::EPConf conf;
    config_send(&conf, label, dstcore, dstvpe, dstep, msgsize, credits);
    m3::Sync::memory_barrier();
    uintptr_t epaddr = EPS_START + ep * sizeof(m3::EPConf);
    write_mem(vpe, epaddr, &conf, sizeof(conf));
}

void KDTU::config_mem(void *e, int dstcore, int, uintptr_t addr, size_t size, int perm) {
    m3::EPConf *ep = reinterpret_cast<m3::EPConf*>(e);
    ep->valid = 1;
    ep->dstcore = dstcore;
    ep->dstep = 0;
    ep->label = addr | perm;
    ep->credits = size;
}

void KDTU::config_mem_local(int ep, int dstcore, int dstvpe, uintptr_t addr, size_t size) {
    config_mem(m3::eps() + ep, dstcore, dstvpe, addr, size, m3::MemGate::RW);
}

void KDTU::config_mem_remote(KVPE &vpe, int ep, int dstcore, int dstvpe, uintptr_t addr, size_t size, int perm) {
    alignas(DTU_PKG_SIZE) m3::EPConf conf;
    config_mem(&conf, dstcore, dstvpe, addr, size, perm);
    m3::Sync::memory_barrier();
    uintptr_t epaddr = EPS_START + ep * sizeof(m3::EPConf);
    write_mem(vpe, epaddr, &conf, sizeof(conf));
}

void KDTU::reply_to(KVPE &vpe, int ep, int, word_t, label_t label, const void *msg, size_t size) {
    config_send_local(_ep, label, vpe.core(), vpe.id(), ep, size + m3::DTU::HEADER_SIZE,
        size + m3::DTU::HEADER_SIZE);
    m3::DTU::get().send(_ep, msg, size, 0, 0);
    m3::DTU::get().wait_until_ready(_ep);
}

void KDTU::write_mem(KVPE &vpe, uintptr_t addr, const void *data, size_t size) {
    m3::DTU::get().set_target(SLOT_NO, vpe.core(), addr);
    m3::DTU::get().fire(SLOT_NO, m3::DTU::WRITE, data, size);
}

}
