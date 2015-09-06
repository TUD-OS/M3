/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

namespace m3 {

void KDTU::wakeup(int core) {
    // inject an IRQ
    uint64_t val = 1;
    Sync::memory_barrier();
    write_mem(core, IRQ_ADDR_EXTERN, &val, sizeof(val));
}

void KDTU::deprivilege(int) {
    // nothing to do
}

void KDTU::invalidate_ep(int core, int ep) {
    alignas(DTU_PKG_SIZE) EPConf conf;
    memset(&conf, 0, sizeof(conf));
    Sync::memory_barrier();
    uintptr_t addr = CONF_GLOBAL + offsetof(CoreConf, eps) + ep * sizeof(EPConf);
    write_mem(core, addr, &conf, sizeof(conf));
}

void KDTU::invalidate_eps(int core) {
    alignas(DTU_PKG_SIZE) CoreConf conf;
    memset(&conf, 0, sizeof(conf));
    conf.coreid = core;
    Sync::memory_barrier();
    write_mem(core, CONF_GLOBAL, &conf, sizeof(conf));
}

void KDTU::config_recv_local(int, uintptr_t, uint, uint, int) {
    // nothing to do; everything is always ready and fixed on T2 for receiving
}

void KDTU::config_recv_remote(int, int, uintptr_t, uint, uint, int, bool) {
    // nothing to do; everything is always ready and fixed on T2 for receiving
}

void KDTU::config_send(void *e, label_t label, int dstcore, int dstep, size_t, word_t credits) {
    EPConf *ep = reinterpret_cast<EPConf*>(e);
    ep->dstcore = dstcore;
    ep->dstep = dstep;
    ep->label = label;
    ep->credits = credits;
}

void KDTU::config_send_local(int ep, label_t label, int dstcore, int dstep,
        size_t msgsize, word_t credits) {
    config_send(coreconf()->eps + ep, label, dstcore, dstep, msgsize, credits);
}

void KDTU::config_send_remote(int core, int ep, label_t label, int dstcore, int dstep,
        size_t msgsize, word_t credits) {
    alignas(DTU_PKG_SIZE) EPConf conf;
    config_send(&conf, label, dstcore, dstep, msgsize, credits);
    Sync::memory_barrier();
    uintptr_t epaddr = CONF_GLOBAL + offsetof(CoreConf, eps) + ep * sizeof(EPConf);
    write_mem(core, epaddr, &conf, sizeof(conf));
}

void KDTU::config_mem(void *e, int dstcore, uintptr_t addr, size_t size, int perm) {
    EPConf *ep = reinterpret_cast<EPConf*>(e);
    ep->dstcore = dstcore;
    ep->dstep = 0;
    ep->label = addr | perm;
    ep->credits = size;
}

void KDTU::config_mem_local(int ep, int coreid, uintptr_t addr, size_t size) {
    config_mem(coreconf()->eps + ep, coreid, addr, size, MemGate::RW);
}

void KDTU::config_mem_remote(int core, int ep, int dstcore, uintptr_t addr, size_t size, int perm) {
    alignas(DTU_PKG_SIZE) EPConf conf;
    config_mem(&conf, dstcore, addr, size, perm);
    Sync::memory_barrier();
    uintptr_t epaddr = CONF_GLOBAL + offsetof(CoreConf, eps) + ep * sizeof(EPConf);
    write_mem(core, epaddr, &conf, sizeof(conf));
}

void KDTU::reply_to(int core, int ep, int, word_t, label_t label, const void *msg, size_t size) {
    config_send_local(_ep, label, core, ep, size + DTU::HEADER_SIZE, size + DTU::HEADER_SIZE);
    DTU::get().send(_ep, msg, size, 0, 0);
    DTU::get().wait_until_ready(_ep);
}

void KDTU::write_mem(int core, uintptr_t addr, const void *data, size_t size) {
    DTU::get().set_target(SLOT_NO, core, addr);
    DTU::get().fire(SLOT_NO, DTU::WRITE, data, size);
}

}
