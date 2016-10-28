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

#include <thread/ThreadManager.h>

#include "cap/Capability.h"
#include "DTU.h"
#include "Platform.h"

#include <functional>

namespace kernel {

class RecvBufs {
    struct RBuf : public m3::SListItem {
        explicit RBuf(const RBufObject *_obj)
            : obj(_obj) {
        }

        size_t size() const {
            return 1UL << obj->order;
        }

        void configure(VPE &vpe, bool attach);

        const RBufObject *obj;
    };

public:
    explicit RecvBufs() : _rbufs() {
    }

    ~RecvBufs() {
        while(_rbufs.length() > 0)
            delete _rbufs.remove_first();
    }

    m3::Errors::Code get_header(VPE &vpe, const RBufObject *obj, uintptr_t &msgaddr, m3::DTU::Header &head);
    m3::Errors::Code set_header(VPE &vpe, const RBufObject *obj, uintptr_t &msgaddr, const m3::DTU::Header &head);

    m3::Errors::Code attach(VPE &vpe, const RBufObject *obj);
    void detach(VPE &vpe, const RBufObject *obj);

private:
    uintptr_t get_msgaddr(RBuf *rbuf, uintptr_t msgaddr);

    void notify(const RBufObject *obj) {
        m3::ThreadManager::get().notify(obj);
    }

    const RBuf *get(const RBufObject *obj) const {
        return const_cast<RecvBufs*>(this)->get(obj);
    }
    RBuf *get(const RBufObject *obj) {
        for(auto it = _rbufs.begin(); it != _rbufs.end(); ++it) {
            if(it->obj == obj)
                return &*it;
        }
        return nullptr;
    }

    m3::SList<RBuf> _rbufs;
};

}
