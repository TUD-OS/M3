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

#include <m3/com/GateStream.h>
#include <m3/session/Session.h>
#include <m3/stream/Standard.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

#include <unistd.h>
#include <fcntl.h>

using namespace m3;

template<typename T>
class StreamReader {
public:
    explicit StreamReader(RecvGate &gate) : _gate(gate), _msgcnt(ep_reg(DTU::EP_BUF_MSGCNT)) {
    }

    T read() {
        while(1) {
            size_t msgcnt = ep_reg(DTU::EP_BUF_MSGCNT);
            if(msgcnt != _msgcnt) {
                _msgcnt = msgcnt;
                break;
            }
            DTU::get().sleep();
        }

        word_t roff = ep_reg(DTU::EP_BUF_ROFF);
        word_t ord = ep_reg(DTU::EP_BUF_ORDER);
        T val = *reinterpret_cast<T*>(ep_reg(DTU::EP_BUF_ADDR) + (roff & ((1UL << ord) - 1)));
        Sync::compiler_barrier();
        roff = (roff + sizeof(T)) & ((1UL << (ord + 1)) - 1);
        ep_reg(DTU::EP_BUF_ROFF, roff);
        return val;
    }

private:
    word_t ep_reg(size_t off) const {
        return DTU::get().get_ep(_gate.epid(), off);
    }
    void ep_reg(size_t off, word_t val) {
        DTU::get().set_ep(_gate.epid(), off, val);
    }

    RecvGate &_gate;
    size_t _msgcnt;
};

int main() {
    Session qtest("streamer");
    RecvBuf rcvbuf = RecvBuf::create(VPE::self().alloc_ep(),
            nextlog2<256>::val, RecvBuf::NO_HEADER);
    RecvGate rgate = RecvGate::create(&rcvbuf);
    SendGate sgate = SendGate::create(SendGate::UNLIMITED, &rgate);
    qtest.delegate_obj(sgate.sel());
    StreamReader<int> str(rgate);
    while(1) {
        int val = str.read();
        cout << "Got val=" << val << "\n";
    }
    return 0;
}
