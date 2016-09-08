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
        explicit RBuf(epid_t _epid, uintptr_t _addr, int _order, int _msgorder, uint _flags)
            : epid(_epid), addr(_addr), order(_order), msgorder(_msgorder), flags(_flags) {
        }

        size_t size() const {
            return 1UL << order;
        }

        void configure(VPE &vpe, bool attach);

        epid_t epid;
        uintptr_t addr;
        int order;
        int msgorder;
        int flags;
    };

    struct Subscriber : public m3::SListItem {
        // TODO hack: we have to use m3::Subscriber<bool> here
        using callback_type = std::function<void(bool,m3::Subscriber<bool>*)>;

        callback_type callback;
        epid_t epid;

        explicit Subscriber(const callback_type &cb, epid_t epid)
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

    bool is_attached(epid_t epid) {
        return get(epid) != nullptr;
    }

    void subscribe(epid_t epid, const Subscriber::callback_type &cb) {
        _waits.append(new Subscriber(cb, epid));
    }


    m3::Errors::Code attach(VPE &vpe, epid_t epid, uintptr_t addr, int order, int msgorder, uint flags);
    void detach(VPE &vpe, epid_t epid);
    void detach_all(VPE &vpe, epid_t except);

private:
    void notify(epid_t epid, bool success);

    const RBuf *get(epid_t epid) const {
        return const_cast<RecvBufs*>(this)->get(epid);
    }
    RBuf *get(epid_t epid) {
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
