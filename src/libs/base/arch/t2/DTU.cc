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

#include <base/util/Sync.h>
#include <base/log/Lib.h>
#include <base/tracing/Tracing.h>
#include <base/DTU.h>
#include <base/Init.h>
#include <base/KIF.h>
#include <base/Panic.h>

namespace m3 {

INIT_PRIO_DTU DTU DTU::inst;

size_t DTU::regs[2][6] = {
    // CM
    {
        /* REG_TARGET */    2,
        /* REG_REM_ADDR */  4,
        /* REG_LOC_ADDR */  6,
        /* REG_TYPE */      8,
        /* REG_SIZE */      10,
        /* REG_STATUS */    10,
    },

    // PEs
    {
        /* REG_TARGET */    0,
        /* REG_REM_ADDR */  4,
        /* REG_LOC_ADDR */  6,
        /* REG_TYPE */      2,
        /* REG_SIZE */      8,
        /* REG_STATUS */    10,
    }
};

void DTU::reset() {
    memset((void*)RECV_BUF_LOCAL,0,EP_COUNT * RECV_BUF_MSGSIZE * MAX_CORES);
    memset(_pos, 0, sizeof(_pos));
    memset(_last, 0, sizeof(_last));
}

bool DTU::fetch_msg(epid_t ep) {
    // simple way to achieve fairness here. otherwise we might choose the same client all the time
    int end = MAX_CORES;
retry:
    for(int i = _pos[ep]; i < end; ++i) {
        volatile Message *msg = reinterpret_cast<volatile Message*>(
            RECV_BUF_LOCAL + DTU::get().recvbuf_offset(i, ep));
        if(msg->length != 0) {
            LLOG(IPC, "Fetched msg @ " << (void*)msg << " over ep " << ep);
            EVENT_TRACE_MSG_RECV(msg->core, msg->length,
                ((uint)msg - RECV_BUF_GLOBAL) >> TRACE_ADDR2TAG_SHIFT);
            assert(_last[ep] == nullptr);
            _last[ep] = const_cast<Message*>(msg);
            _pos[ep] = i + 1;
            return true;
        }
    }
    if(_pos[ep] != 0) {
        end = _pos[ep];
        _pos[ep] = 0;
        goto retry;
    }
    return false;
}

Errors::Code DTU::send(epid_t ep, const void *msg, size_t size, label_t replylbl, epid_t reply_ep) {
    EPConf *cfg = conf(ep);
    assert(cfg->valid);
    uintptr_t destaddr = RECV_BUF_GLOBAL + recvbuf_offset(env()->coreid, cfg->dstep);
    LLOG(DTU, "-> " << fmt(size, 4) << "b to " << cfg->dstcore << ":" << cfg->dstep
        << " from " << msg << " with lbl=" << fmt(cfg->label, "#0x", sizeof(label_t) * 2));

    EVENT_TRACE_MSG_SEND(cfg->dstcore, size, ((uint)destaddr - RECV_BUF_GLOBAL) >> TRACE_ADDR2TAG_SHIFT);

    // first send data to ensure that everything has already arrived if the receiver notices
    // an arrival
    set_target(SLOT_NO, cfg->dstcore, destaddr + sizeof(Header));
    fire(SLOT_NO, WRITE, msg, size);
    wait_until_ready(SLOT_NO);

    // now send header
    alignas(DTU_PKG_SIZE) DTU::Header head;
    head.length = size;
    head.label = cfg->label;
    head.replylabel = replylbl;
    head.has_replycap = 1;
    head.core = env()->coreid;
    head.epid = reply_ep;
    set_target(SLOT_NO, cfg->dstcore, destaddr);
    Sync::memory_barrier();
    fire(SLOT_NO, WRITE, &head, sizeof(head));

    return Errors::NO_ERROR;
}

Errors::Code DTU::reply(epid_t ep, const void *msg, size_t size, size_t msgidx) {
    DTU::Message *orgmsg = message_at(ep, msgidx);
    uintptr_t destaddr = RECV_BUF_GLOBAL + recvbuf_offset(env()->coreid, orgmsg->epid);
    LLOG(DTU, ">> " << fmt(size, 4) << "b to " << orgmsg->core << ":" << orgmsg->epid
        << " from " << msg << " with lbl=" << fmt(orgmsg->replylabel, "#0x", sizeof(label_t) * 2));

    EVENT_TRACE_MSG_SEND(orgmsg->core, size, ((uint)destaddr - RECV_BUF_GLOBAL) >> TRACE_ADDR2TAG_SHIFT);

    // first send data to ensure that everything has already arrived if the receiver notices
    // an arrival
    set_target(SLOT_NO, orgmsg->core, destaddr + sizeof(Header));
    fire(SLOT_NO, WRITE, msg, size);
    wait_until_ready(SLOT_NO);

    // now send header
    alignas(DTU_PKG_SIZE) DTU::Header head;
    head.length = size;
    head.label = orgmsg->replylabel;
    head.has_replycap = 0;
    head.core = env()->coreid;
    set_target(SLOT_NO, orgmsg->core, destaddr);
    Sync::memory_barrier();
    fire(SLOT_NO, WRITE, &head, sizeof(head));

    return Errors::NO_ERROR;
}

Errors::Code DTU::check_rw_access(uintptr_t base, size_t len, size_t off, size_t size,
        int perms, int type) const {
    uintptr_t srcaddr = base + off;
    if(!(perms & type) || srcaddr < base || srcaddr + size < srcaddr ||  srcaddr + size > base + len) {
        // PANIC("No permission to " << (type == KIF::Perm::R ? "read from" : "write to")
        //         << " " << fmt(srcaddr, "p") << ".." << fmt(srcaddr + size, "p") << "\n"
        //         << "Allowed is: " << fmt(base, "p") << ".." << fmt(base + len, "p")
        //         << " with " << fmt(perms, "#x"));
        return Errors::NO_PERM;
    }
    return Errors::NO_ERROR;
}

Errors::Code DTU::read(epid_t ep, void *msg, size_t size, size_t off, uint) {
    EPConf *cfg = conf(ep);
    assert(cfg->valid);
    uintptr_t base = cfg->label & ~KIF::Perm::RWX;
    size_t len = cfg->credits;
    uintptr_t srcaddr = base + off;
    LLOG(DTU, "Reading " << size << "b from " << cfg->dstcore << " @ " << fmt(srcaddr, "p"));

    EVENT_TRACE_MEM_READ(cfg->dstcore, size);

    // mark the end
    reinterpret_cast<unsigned char*>(msg)[size - 1] = 0xFF;

    Errors::Code res = check_rw_access(base, len, off, size, cfg->label & KIF::Perm::RWX, KIF::Perm::R);
    if(res != Errors::NO_ERROR)
        return res;

    set_target(SLOT_NO, cfg->dstcore, srcaddr);
    fire(SLOT_NO, READ, msg, size);

    // wait until the size-register has been decremented to 0
    size_t rem;
    while((rem = get_remaining(ep)) > 0)
        ;

    // TODO how long should we wait?
    for(volatile size_t i = 0; i < 1000; ++i) {
        // stop if the end has been overwritten
        if(reinterpret_cast<unsigned char*>(msg)[size - 1] != 0xFF)
            break;
    }

    return Errors::NO_ERROR;
}

Errors::Code DTU::write(epid_t ep, const void *msg, size_t size, size_t off, uint) {
    EPConf *cfg = conf(ep);
    assert(cfg->valid);
    uintptr_t base = cfg->label & ~KIF::Perm::RWX;
    size_t len = cfg->credits;
    uintptr_t destaddr = base + off;
    LLOG(DTU, "Writing " << size << "b to " << cfg->dstcore << " @ " << fmt(destaddr, "p"));

    EVENT_TRACE_MEM_WRITE(cfg->dstcore, size);

    Errors::Code res = check_rw_access(base, len, off, size, cfg->label & KIF::Perm::RWX, KIF::Perm::W);
    if(res != Errors::NO_ERROR)
        return res;

    set_target(SLOT_NO, cfg->dstcore, destaddr);
    fire(SLOT_NO, WRITE, msg, size);

    // wait until the size-register has been decremented to 0
    // TODO why is this required?
    size_t rem;
    while((rem = get_remaining(ep)) > 0)
        ;

    return Errors::NO_ERROR;
}

}
