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

#include <base/Common.h>
#include <base/col/SList.h>
#include <base/util/Math.h>
#include <base/Config.h>
#include <base/DTU.h>
#include <base/Errors.h>

#include "com/RBuf.h"
#include "DTU.h"
#include "Platform.h"

#include <functional>

namespace kernel {

class RecvBufs {
    struct RBuf : public m3::SListItem {
        explicit RBuf(size_t _epid, uintptr_t _addr, int _order, int _msgorder, uint _flags)
            : epid(_epid), addr(_addr), order(_order), msgorder(_msgorder), flags(_flags) {
        }

        void configure(VPE &vpe, bool attach);

        size_t epid;
        uintptr_t addr;
        int order;
        int msgorder;
        int flags;
    };

    struct Subscriber : public m3::SListItem {
        // TODO hack: we have to use m3::Subscriber<bool> here
        using callback_type = std::function<void(bool,m3::Subscriber<bool>*)>;

        callback_type callback;
        size_t epid;

        explicit Subscriber(const callback_type &cb, size_t epid)
            : m3::SListItem(), callback(cb), epid(epid) {
        }
    };

public:
    explicit RecvBufs() : _rbufs() {
    }

    ~RecvBufs() {
        while(_waits.length() > 0)
            delete _waits.remove_first();
        while(_rbufs.length() > 0)
            delete _rbufs.remove_first();
    }

    bool is_attached(size_t epid) {
        return get(epid) != nullptr;
    }

    void subscribe(size_t epid, const Subscriber::callback_type &cb) {
        _waits.append(new Subscriber(cb, epid));
    }

    m3::Errors::Code attach(VPE &vpe, size_t epid, uintptr_t addr, int order, int msgorder, uint flags) {
        RBuf *rbuf = get(epid);
        if(rbuf)
            return m3::Errors::EXISTS;

        for(auto it = _rbufs.begin(); it != _rbufs.end(); ++it) {
            if(it->epid == epid)
                return m3::Errors::EXISTS;

            if(m3::Math::overlap(it->addr, it->addr + (1UL << it->order), addr, addr + (1UL << order)))
                return m3::Errors::INV_ARGS;
        }

        rbuf = new RBuf(epid, addr, order, msgorder, flags);
        rbuf->configure(vpe, true);
        _rbufs.append(rbuf);
        notify(epid, true);
        return m3::Errors::NO_ERROR;
    }

    void detach(VPE &vpe, size_t epid) {
        RBuf *rbuf = get(epid);
        if(!rbuf)
            return;

        rbuf->configure(vpe, false);
        notify(epid, false);
        _rbufs.remove(rbuf);
        delete rbuf;
    }

    void detach_all(VPE &vpe, size_t except) {
        // TODO not nice
        for(size_t i = 0; i < EP_COUNT; ++i) {
            if(i == except)
                continue;
            detach(vpe, i);
        }
    }

private:
    void notify(size_t epid, bool success) {
        for(auto sub = _waits.begin(); sub != _waits.end(); ) {
            auto old = sub++;
            if(old->epid == epid) {
                old->callback(success, nullptr);
                _waits.remove(&*old);
                delete &*old;
            }
        }
    }

    const RBuf *get(size_t epid) const {
        return const_cast<RecvBufs*>(this)->get(epid);
    }
    RBuf *get(size_t epid) {
        for(auto it = _rbufs.begin(); it != _rbufs.end(); ++it) {
            if(it->epid == epid)
                return &*it;
        }
        return nullptr;
    }

    m3::SList<RBuf> _rbufs;
    m3::SList<Subscriber> _waits;
};

}
