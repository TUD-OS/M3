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

#include <base/Errors.h>
#include <base/KIF.h>

#include <m3/session/ClientSession.h>
#include <m3/com/MemGate.h>
#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>
#include <m3/VPE.h>

namespace m3 {

class LoadGen : public ClientSession {
public:
    class Channel {
    public:
        explicit Channel(capsel_t sels, size_t memsize)
            : _off(),
              _rem(),
              _rgate(RecvGate::create(nextlog2<64>::val, nextlog2<64>::val)),
              _sgate(SendGate::create(&_rgate, 0, 64, nullptr, sels + 0)),
              _mgate(MemGate::create_global(memsize, MemGate::RW, sels + 1)),
              _is(_rgate, nullptr) {
            _rgate.activate();
        }

        void wait() {
            _is = receive_msg(_rgate);
            _is >> _rem;
            _off = 0;
        }

        size_t pull(void *, size_t size) {
            size_t amount = Math::min(size, _rem);
            if(amount == 0) {
                _off = 0;
                return 0;
            }
            if(size > 2)
                CPU::compute(size / 2);
            // _mgate.read(buf, amount, _off);
            _off += amount;
            _rem -= amount;
            return amount;
        }

        void push(const void *, size_t size) {
            // TODO allow larger replies than our mgate
            if(size > 4)
                CPU::compute(size / 4);
            // _mgate.write(buf, size, _off);
            _off += size;
        }

        void reply() {
            reply_vmsg(_is, RESPONSE, _off);
        }

    private:
        size_t _off;
        size_t _rem;
        RecvGate _rgate;
        SendGate _sgate;
        MemGate _mgate;
        GateIStream _is;
    };

    enum Operation {
        START,
        RESPONSE,
        COUNT
    };

    explicit LoadGen(const String &name)
        : ClientSession(name),
          _sgate(SendGate::bind(obtain(1).start())) {
    }

    void start(uint count) {
        send_receive_vmsg(_sgate, START, count);
    }

    Channel *create_channel(size_t memsize) {
        capsel_t sels = VPE::self().alloc_sels(2);
        Channel *chan = new Channel(sels, memsize);
        KIF::ExchangeArgs args;
        args.count = 0;
        delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sels, 2), &args);
        return chan;
    }

private:
    SendGate _sgate;
};

}
