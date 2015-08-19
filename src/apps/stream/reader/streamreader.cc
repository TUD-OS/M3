/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/cap/Session.h>
#include <m3/cap/VPE.h>
#include <m3/util/Sync.h>
#include <m3/Syscalls.h>
#include <m3/GateStream.h>
#include <m3/WorkLoop.h>
#include <m3/Log.h>

#include <unistd.h>
#include <fcntl.h>

using namespace m3;

template<typename T>
class StreamReader {
public:
    explicit StreamReader(RecvGate &gate) : _gate(gate), _msgcnt(rep_reg(DTU::REP_MSGCNT)) {
    }

    T read() {
        while(1) {
            size_t msgcnt = rep_reg(DTU::REP_MSGCNT);
            if(msgcnt != _msgcnt) {
                _msgcnt = msgcnt;
                break;
            }
            DTU::get().wait();
        }

        word_t roff = rep_reg(DTU::REP_ROFF);
        word_t ord = rep_reg(DTU::REP_ORDER);
        T val = *reinterpret_cast<T*>(rep_reg(DTU::REP_ADDR) + (roff & ((1UL << ord) - 1)));
        Sync::compiler_barrier();
        roff = (roff + sizeof(T)) & ((1UL << (ord + 1)) - 1);
        rep_reg(DTU::REP_ROFF, roff);
        return val;
    }

private:
    word_t rep_reg(size_t off) const {
        return DTU::get().get_rep(_gate.chanid(), off);
    }
    void rep_reg(size_t off, word_t val) {
        DTU::get().set_rep(_gate.chanid(), off, val);
    }

    RecvGate &_gate;
    size_t _msgcnt;
};

int main() {
    Session qtest("streamer");
    RecvBuf rcvbuf = RecvBuf::create(VPE::self().alloc_chan(),
            nextlog2<256>::val, RecvBuf::NO_HEADER);
    RecvGate rgate = RecvGate::create(&rcvbuf);
    SendGate sgate = SendGate::create(SendGate::UNLIMITED, &rgate);
    qtest.delegate(CapRngDesc(sgate.sel()));
    StreamReader<int> str(rgate);
    while(1) {
        int val = str.read();
        LOG(DEF, "Got val=" << val);
    }
    return 0;
}
