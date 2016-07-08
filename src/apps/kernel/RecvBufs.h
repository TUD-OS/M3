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

#include <m3/Common.h>
#include <m3/util/Math.h>
#include <m3/Subscriber.h>
#include <m3/Config.h>
#include <m3/DTU.h>
#include <m3/Errors.h>

#include "KDTU.h"
#include "KVPE.h"
#include "RBuf.h"

namespace m3 {

class RecvBufs {
    RecvBufs() = delete;

    enum Flags {
        F_ATTACHED  = 1 << (sizeof(int) * 8 - 1),
    };

public:
    static bool is_attached(size_t core, size_t epid) {
        RBuf &rbuf = get(core, epid);
        return rbuf.flags & F_ATTACHED;
    }

    static void subscribe(size_t core, size_t epid, const Subscriptions<bool>::callback_type &cb) {
        RBuf &rbuf = get(core, epid);
        assert(~rbuf.flags & F_ATTACHED);
        rbuf.waitlist.subscribe(cb);
    }

    static Errors::Code attach(KVPE &vpe, size_t epid, uintptr_t addr, int order, int msgorder, uint flags) {
        RBuf &rbuf = get(vpe.core(), epid);
        if(rbuf.flags & F_ATTACHED)
            return Errors::EXISTS;

        for(size_t i = 0; i < EP_COUNT; ++i) {
            if(i != epid) {
                RBuf &rb = get(vpe.core(), i);
                if((rb.flags & F_ATTACHED) &&
                    Math::overlap(rb.addr, rb.addr + (1UL << rb.order), addr, addr + (1UL << order)))
                    return Errors::INV_ARGS;
            }
        }

        rbuf.addr = addr;
        rbuf.order = order;
        rbuf.msgorder = msgorder;
        rbuf.flags = flags | F_ATTACHED;
        configure(vpe, epid, rbuf);
        notify(rbuf, true);
        return Errors::NO_ERROR;
    }

    static void detach(KVPE &vpe, size_t epid) {
        RBuf &rbuf = get(vpe.core(), epid);
        if(rbuf.flags & F_ATTACHED) {
            // TODO we have to make sure here that nobody can send to that EP anymore
            // BEFORE detaching it!
            rbuf.flags = 0;
            configure(vpe, epid, rbuf);
        }
        notify(rbuf, false);
    }

    static void get_vpe_rbufs(KVPE &vpe, RBuf (&bufs)[EP_COUNT]) {
        memcpy(&_rbufs[(vpe.core() - APP_CORES) * EP_COUNT], &bufs, EP_COUNT * sizeof(RBuf));
    }

    static void set_vpe_rbufs(KVPE &vpe, RBuf (&bufs)[EP_COUNT]) {
        memcpy(&bufs, &_rbufs[(vpe.core() - APP_CORES) * EP_COUNT], EP_COUNT * sizeof(RBuf));
        for(size_t i = 0; i < EP_COUNT; ++i) {
            RBuf &rbuf = get(vpe.core(), i);
            configure(vpe, i, rbuf);
            notify(rbuf, true);
        }
    }

private:
    static void configure(KVPE &vpe, size_t epid, RBuf &rbuf) {
        KDTU::get().config_recv_remote(vpe, epid,
            rbuf.addr, rbuf.order, rbuf.msgorder, rbuf.flags & ~F_ATTACHED, rbuf.flags & F_ATTACHED);
    }

    static void notify(RBuf &rbuf, bool success) {
        for(auto sub = rbuf.waitlist.begin(); sub != rbuf.waitlist.end(); ) {
            auto old = sub++;
            old->callback(success, nullptr);
            rbuf.waitlist.unsubscribe(&*old);
        }
    }
    static RBuf &get(size_t coreid, size_t epid) {
        return _rbufs[(coreid - APP_CORES) * EP_COUNT + epid];
    }

    static RBuf _rbufs[AVAIL_PES * EP_COUNT];
};

}
