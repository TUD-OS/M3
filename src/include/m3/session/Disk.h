/*
 * Copyright (C) 2018, Sebastian Reimers <sebastian.reimers@mailbox.tu-dresden.de>
 * Copyright (C) 2017, Lukas Landgraf <llandgraf317@gmail.com>
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/log/Services.h>

#include <m3/com/MemGate.h>
#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>
#include <m3/session/ClientSession.h>

#include <fs/internal.h>

#include <thread/ThreadManager.h>

#include <limits>

namespace m3 {

class Disk : public ClientSession {
public:
    static constexpr size_t MSG_SIZE = 128;

    enum Operation {
        READ,
        WRITE,
        COUNT
    };

    explicit Disk(const char *name)
        : ClientSession(name, 0, VPE::self().alloc_sels(2)),
          _rgate(RecvGate::create(nextlog2<MSG_SIZE * 8>::val, nextlog2<MSG_SIZE>::val)),
          _sgate(obtain_sgate()) {
        _rgate.activate();
        _rgate.start([](GateIStream &is) {
            ThreadManager::get().notify(is.label<event_t>() & (std::numeric_limits<event_t>::max() >> 1));
        });
    };

    const RecvGate &rgate() const {
        return _rgate;
    }

    SendGate &sgate() {
        return _sgate;
    }

    Errors::Code read(blockno_t cap, blockno_t bno, size_t len, size_t blocksize, goff_t off = 0) {
        return send_vmsg(_sgate, bno, READ, cap, bno, len, blocksize, off);
    }

    Errors::Code write(blockno_t cap, blockno_t bno, size_t len, size_t blocksize, goff_t off = 0) {
        return send_vmsg(_sgate, bno, WRITE, cap, bno, len, blocksize, off);
    }

private:
    SendGate obtain_sgate() {
        KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, sel() + 1);
        obtain_for(VPE::self(), crd);
        return SendGate::bind(crd.start(), &_rgate);
    }

    template<typename... Args>
    static inline Errors::Code send_vmsg(SendGate &gate, blockno_t bno, const Args &... args) {
        EVENT_TRACER_send_vmsg();
        auto msg = create_vmsg(args...);
        // just assume there won't be 2^31 previous wait events
        event_t label = bno | (1U << 31);

        Errors::Code e = gate.send(msg.bytes(), msg.total(), label);
        if(ThreadManager::get().sleeping_count()) {
            ThreadManager::get().wait_for(label);
        }
        // if there are no sleeping threads just wait for the answer
        else {
            GateIStream reply = receive_reply(gate);
            reply >> e;
        }
        return e;
    }

    RecvGate _rgate;
    SendGate _sgate;
};

}
