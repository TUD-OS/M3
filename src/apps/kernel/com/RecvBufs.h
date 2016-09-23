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

public:
    explicit RecvBufs() : _rbufs() {
    }

    ~RecvBufs() {
        while(_rbufs.length() > 0)
            delete _rbufs.remove_first();
    }

    bool is_attached(epid_t epid) {
        return get(epid) != nullptr;
    }

    void wait_for(epid_t epid) {
        m3::ThreadManager::get().wait_for(get_event(epid));
    }

    m3::Errors::Code reply_target(VPE &vpe, epid_t epid, uintptr_t msgaddr, vpeid_t *id);
    m3::Errors::Code activate_reply(VPE &vpe, VPE &dest, epid_t epid, uintptr_t msgaddr);

    m3::Errors::Code attach(VPE &vpe, epid_t epid, uintptr_t addr, int order, int msgorder, uint flags);
    void detach(VPE &vpe, epid_t epid);
    void detach_all(VPE &vpe, epid_t except);

private:
    void *get_event(epid_t epid) {
        // TODO in theory, the pointer could need more than 32 bits
        word_t event = reinterpret_cast<word_t>(this) | (epid << 32);
        return reinterpret_cast<void*>(event);
    }

    void notify(epid_t epid) {
        m3::ThreadManager::get().notify(get_event(epid));
    }

    m3::Errors::Code get_header(VPE &vpe, epid_t epid, uintptr_t &msgaddr, m3::DTU::Header &head);

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
};

}
