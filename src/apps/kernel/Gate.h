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
#include <base/com/GateStream.h>
#include <base/util/Subscriber.h>
#include <base/Errors.h>

#include "DTU.h"

namespace kernel {

class VPE;

class RecvGate : public m3::Subscriptions<RecvGate&> {
public:
    explicit RecvGate(int ep, void *sess) : m3::Subscriptions<RecvGate&>(), _ep(ep), _sess(sess) {
    }

    size_t epid() const {
        return _ep;
    }
    template<class T>
    T *session() {
        return static_cast<T*>(_sess);
    }

    void notify_all() {
        for(auto it = _list.begin(); it != _list.end(); ) {
            auto old = it++;
            old->callback(*this, &*old);
        }
    }

    m3::Errors::Code reply_sync(const void *data, size_t len, size_t msgidx) {
        // TODO hack to fix the race-condition on T2. as soon as we've replied to the other core, he
        // might send us another message, which we might miss if we ACK this message after we've got
        // another one. so, ACK it now since the reply marks the end of the handling anyway.
#if defined(__t2__)
        m3::DTU::get().ack_message(epid());
#endif

        m3::DTU::get().wait_until_ready(_ep);
        m3::Errors::Code res = m3::DTU::get().reply(_ep, data, len, msgidx);
        m3::DTU::get().wait_until_ready(_ep);
        return res;
    }

private:
    int _ep;
    void *_sess;
};

class SendGate {
public:
    explicit SendGate(VPE &vpe, int ep, label_t label)
        : _vpe(vpe), _ep(ep), _label(label) {
    }

    m3::Errors::Code send(const void *data, size_t len, RecvGate *rgate) {
        DTU::get().send_to(_vpe, _ep, _label, data, len,
            reinterpret_cast<uintptr_t>(rgate), rgate->epid());
        return m3::Errors::NO_ERROR;
    }

private:
    VPE &_vpe;
    int _ep;
    label_t _label;
};

using GateOStream = m3::BaseGateOStream<RecvGate, SendGate>;
template<size_t SIZE>
using StaticGateOStream = m3::BaseStaticGateOStream<SIZE, RecvGate, SendGate>;
using AutoGateOStream = m3::BaseAutoGateOStream<RecvGate, SendGate>;
using GateIStream = m3::BaseGateIStream<RecvGate, SendGate>;

template<typename ... Args>
static inline auto create_vmsg(const Args& ... args) -> StaticGateOStream<m3::ostreamsize<Args...>()> {
    StaticGateOStream<m3::ostreamsize<Args...>()> os;
    os.vput(args...);
    return os;
}

template<typename... Args>
static inline m3::Errors::Code send_vmsg(SendGate &gate, RecvGate *rgate, const Args &... args) {
    EVENT_TRACER_send_vmsg();
    auto msg = kernel::create_vmsg(args...);
    return gate.send(msg.bytes(), msg.total, rgate);
}
template<typename... Args>
static inline m3::Errors::Code reply_vmsg(RecvGate &gate, const Args &... args) {
    EVENT_TRACER_reply_vmsg();
    return kernel::create_vmsg(args...).reply(gate);
}

}
