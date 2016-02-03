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

namespace m3 {

void KDTU::init() {
    // nothing to do
}

void KDTU::set_vpeid(int, int) {
    // nothing to do
}

void KDTU::unset_vpeid(int, int) {
    // nothing to do
}

void KDTU::wakeup(KVPE &vpe) {
    injectIRQ(vpe);
}

void KDTU::injectIRQ(KVPE &vpe) {
    // inject an IRQ
    uint64_t val = 1;
    Sync::memory_barrier();
    write_mem(vpe, IRQ_ADDR_EXTERN, &val, sizeof(val));
}

void KDTU::deprivilege(int) {
    // nothing to do
}

void KDTU::config_pf_remote(KVPE &, int, uint64_t) {
}

void KDTU::map_page(KVPE &, uintptr_t, uintptr_t, int) {
}

void KDTU::unmap_page(KVPE &, uintptr_t) {
}

void KDTU::invalidate_ep(KVPE &vpe, int ep) {
    alignas(DTU_PKG_SIZE) EPConf conf;
    memset(&conf, 0, sizeof(conf));
    Sync::memory_barrier();
    uintptr_t addr = CONF_GLOBAL + offsetof(CoreConf, eps) + ep * sizeof(EPConf);
    // TODO something is wrong here. This print statement helps to make it work in most cases, but
    // not always. It seems to be a timing problem, but I have no clue why this occurs.
    Serial::get() << "Invalidating EP " << ep << " on PE " << vpe.core() << "\n";
    write_mem(vpe, addr, &conf, sizeof(conf));
}

void KDTU::invalidate_eps(KVPE &vpe) {
    alignas(DTU_PKG_SIZE) CoreConf conf;
    memset(&conf, 0, sizeof(conf));
    conf.coreid = vpe.core();
    Sync::memory_barrier();
    write_mem(vpe, CONF_GLOBAL, &conf, sizeof(conf));
}

void KDTU::config_recv_local(int, uintptr_t, uint, uint, int) {
    // nothing to do; everything is always ready and fixed on T2 for receiving
}

void KDTU::config_recv_remote(KVPE &, int, uintptr_t, uint, uint, int, bool) {
    // nothing to do; everything is always ready and fixed on T2 for receiving
}

void KDTU::config_send(void *e, label_t label, int dstcore, int, int dstep, size_t, word_t credits) {
    EPConf *ep = reinterpret_cast<EPConf*>(e);
    ep->valid = 1;
    ep->dstcore = dstcore;
    ep->dstep = dstep;
    ep->label = label;
    ep->credits = credits;
}

void KDTU::config_send_local(int ep, label_t label, int dstcore, int dstvpe, int dstep,
        size_t msgsize, word_t credits) {
    config_send(coreconf()->eps + ep, label, dstcore, dstvpe, dstep, msgsize, credits);
}

void KDTU::config_send_remote(KVPE &vpe, int ep, label_t label, int dstcore, int dstvpe, int dstep,
        size_t msgsize, word_t credits) {
    alignas(DTU_PKG_SIZE) EPConf conf;
    config_send(&conf, label, dstcore, dstvpe, dstep, msgsize, credits);
    Sync::memory_barrier();
    uintptr_t epaddr = CONF_GLOBAL + offsetof(CoreConf, eps) + ep * sizeof(EPConf);
    write_mem(vpe, epaddr, &conf, sizeof(conf));
}

void KDTU::config_mem(void *e, int dstcore, int, uintptr_t addr, size_t size, int perm) {
    EPConf *ep = reinterpret_cast<EPConf*>(e);
    ep->valid = 1;
    ep->dstcore = dstcore;
    ep->dstep = 0;
    ep->label = addr | perm;
    ep->credits = size;
}

void KDTU::config_mem_local(int ep, int dstcore, int dstvpe, uintptr_t addr, size_t size) {
    config_mem(coreconf()->eps + ep, dstcore, dstvpe, addr, size, MemGate::RW);
}

void KDTU::config_mem_remote(KVPE &vpe, int ep, int dstcore, int dstvpe, uintptr_t addr, size_t size, int perm) {
    alignas(DTU_PKG_SIZE) EPConf conf;
    config_mem(&conf, dstcore, dstvpe, addr, size, perm);
    Sync::memory_barrier();
    uintptr_t epaddr = CONF_GLOBAL + offsetof(CoreConf, eps) + ep * sizeof(EPConf);
    write_mem(vpe, epaddr, &conf, sizeof(conf));
}

void KDTU::reply_to(KVPE &vpe, int ep, int, word_t, label_t label, const void *msg, size_t size) {
    config_send_local(_ep, label, vpe.core(), vpe.id(), ep, size + DTU::HEADER_SIZE, size + DTU::HEADER_SIZE);
    DTU::get().send(_ep, msg, size, 0, 0);
    DTU::get().wait_until_ready(_ep);
}

void KDTU::write_mem(KVPE &vpe, uintptr_t addr, const void *data, size_t size) {
    DTU::get().set_target(SLOT_NO, vpe.core(), addr);
    DTU::get().fire(SLOT_NO, DTU::WRITE, data, size);
}

}
