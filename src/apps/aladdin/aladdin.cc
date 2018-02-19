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

#include <base/Common.h>

#include <m3/stream/Standard.h>

#include <m3/com/MemGate.h>
#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>
#include <m3/session/Pager.h>
#include <m3/VPE.h>

using namespace m3;

class Aladdin {
public:
    static const uint RBUF          = 2;
    static const uint RECV_EP       = 7;
    static const uint MEM_EP        = 8;
    static const uint DATA_EP       = 9;
    static const size_t RB_SIZE     = 256;

    static const size_t BUF_SIZE    = 1024;
    static const size_t BUF_ADDR    = 0x8000;
    static const size_t STATE_SIZE  = 1024;
    static const size_t STATE_ADDR  = BUF_ADDR - STATE_SIZE;
    static const size_t RBUF_ADDR   = RECVBUF_SPACE + SYSC_RBUF_SIZE + UPCALL_RBUF_SIZE + DEF_RBUF_SIZE;

    struct InvokeMessage {
        struct {
            uint64_t addr;
            uint64_t size;
        } PACKED arrays[8];
        uint64_t array_count;
        uint64_t iterations;
    } PACKED;

    explicit Aladdin(PEISA isa)
        : _accel(new VPE("aladdin", PEDesc(PEType::COMP_EMEM, isa), "pager")),
          _lastmem(ObjCap::INVALID),
          _rgate(RecvGate::create(nextlog2<256>::val, nextlog2<256>::val)),
          _srgate(RecvGate::create_for(*_accel, getnextlog2(RB_SIZE), getnextlog2(RB_SIZE))),
          _sgate(SendGate::create(&_srgate, 0, RB_SIZE, &_rgate)) {
        // has to be activated
        _rgate.activate();

        if(_accel->pager()) {
            uintptr_t virt = STATE_ADDR;
            _accel->pager()->map_anon(&virt, STATE_SIZE + BUF_SIZE, Pager::Prot::RW, 0);
        }

        _accel->delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _srgate.sel(), 1), RBUF);
        _srgate.activate(RECV_EP, RBUF_ADDR);
        _accel->start();
    }
    ~Aladdin() {
        delete _accel;
    }

    uint64_t invoke(const InvokeMessage &msg) {
        GateIStream is = send_receive_msg(_sgate, &msg, sizeof(msg));
        uint64_t res;
        is >> res;
        return res;
    }

    void run(InvokeMessage &msg, size_t iterations) {
        size_t count = 0;
        size_t per_step = 1;
        while(count < iterations) {
            msg.iterations = Math::min(iterations - count, per_step);
            invoke(msg);
            count += msg.iterations;
        }
    }

    VPE *_accel;
    capsel_t _lastmem;
    m3::RecvGate _rgate;
    m3::RecvGate _srgate;
    m3::SendGate _sgate;
};

int main(int argc, char **argv) {
    if(argc != 2)
        exitmsg("Usage: " << argv[0] << " (stencil|md|spmv|fft)");

    if(strcmp(argv[1], "stencil") == 0) {
        Aladdin alad(PEISA::ACCEL_STE);

        const size_t HEIGHT = 32;
        const size_t COLS = 32;
        const size_t ROWS = 64;
        const size_t SIZE = HEIGHT * ROWS * COLS * sizeof(uint32_t);
        const size_t ITERATIONS = 1 + (HEIGHT - 2) * (COLS - 2);

        uintptr_t dataAddr = 0x1000000;
        alad._accel->pager()->map_anon(&dataAddr, SIZE * 2, Pager::Prot::RW, 0);
        uintptr_t filAddr = dataAddr + SIZE * 2;
        alad._accel->pager()->map_anon(&filAddr, 8, Pager::Prot::RW, 0);

        Aladdin::InvokeMessage msg;
        msg.array_count = 3;
        msg.arrays[0].addr = dataAddr;            msg.arrays[0].size = SIZE;
        msg.arrays[1].addr = dataAddr + SIZE;     msg.arrays[1].size = SIZE;
        msg.arrays[2].addr = filAddr;             msg.arrays[2].size = 8;

        alad.run(msg, ITERATIONS);
    }
    else if(strcmp(argv[1], "md") == 0) {
        Aladdin alad(PEISA::ACCEL_MD);

        const size_t ATOMS = 4096;
        const size_t MAX_NEIGHBORS = 16;
        const size_t ATOM_SET = ATOMS * sizeof(double);

        uintptr_t posAddr = 0x1000000;
        alad._accel->pager()->map_anon(&posAddr, ATOM_SET * 3, Pager::Prot::RW, 0);
        uintptr_t forceAddr = 0x2000000;
        alad._accel->pager()->map_anon(&forceAddr, ATOM_SET * 3, Pager::Prot::RW, 0);
        uintptr_t neighAddr = 0x3000000;
        alad._accel->pager()->map_anon(&neighAddr, ATOMS * MAX_NEIGHBORS * 4, Pager::Prot::RW, 0);

        Aladdin::InvokeMessage msg;
        msg.array_count = 7;
        for(size_t i = 0; i < 3; ++i) {
            msg.arrays[i].addr = posAddr + ATOM_SET * i;
            msg.arrays[i].size = ATOM_SET;
        }
        for(size_t i = 0; i < 3; ++i) {
            msg.arrays[3 + i].addr = forceAddr + ATOM_SET * i;
            msg.arrays[3 + i].size = ATOM_SET;
        }
        msg.arrays[6].addr = neighAddr;
        msg.arrays[6].size = ATOMS * MAX_NEIGHBORS * 4;

        alad.run(msg, ATOMS);
    }
    else if(strcmp(argv[1], "spmv") == 0) {
        Aladdin alad(PEISA::ACCEL_SPMV);

        const size_t NNZ = 39321;
        const size_t N = 2048;
        const size_t VEC_LEN = N * sizeof(double);

        uintptr_t valAddr = 0x1000000;
        alad._accel->pager()->map_anon(&valAddr, NNZ * sizeof(double), Pager::Prot::RW, 0);
        uintptr_t colsAddr = 0x2000000;
        alad._accel->pager()->map_anon(&colsAddr, NNZ * sizeof(int32_t), Pager::Prot::RW, 0);
        uintptr_t rowDelimAddr = 0x3000000;
        alad._accel->pager()->map_anon(&rowDelimAddr, (N + 1) * sizeof(int32_t), Pager::Prot::RW, 0);
        uintptr_t vecOutAddr = 0x4000000;
        alad._accel->pager()->map_anon(&vecOutAddr, VEC_LEN * 2, Pager::Prot::RW, 0);

        Aladdin::InvokeMessage msg;
        msg.array_count = 5;
        msg.arrays[0].addr = valAddr;               msg.arrays[0].size = NNZ * sizeof(double);
        msg.arrays[1].addr = colsAddr;              msg.arrays[1].size = NNZ * sizeof(int32_t);
        msg.arrays[2].addr = rowDelimAddr;          msg.arrays[2].size = (N + 1) * sizeof(int32_t);
        msg.arrays[3].addr = vecOutAddr;            msg.arrays[3].size = VEC_LEN;
        msg.arrays[4].addr = vecOutAddr + VEC_LEN;  msg.arrays[4].size = VEC_LEN;

        alad.run(msg, N);
    }
    else if(strcmp(argv[1], "fft") == 0) {
        Aladdin alad(PEISA::ACCEL_AFFT);

        const size_t DATA_LEN = 16384;
        const size_t SIZE = DATA_LEN * sizeof(double);
        const size_t ITERS = (DATA_LEN / 512) * 11;

        uintptr_t dataAddr = 0x1000000;
        alad._accel->pager()->map_anon(&dataAddr, SIZE * 2, Pager::Prot::RW, 0);

        Aladdin::InvokeMessage msg;
        msg.array_count = 2;
        msg.arrays[0].addr = dataAddr;               msg.arrays[0].size = SIZE;
        msg.arrays[1].addr = dataAddr + SIZE;        msg.arrays[1].size = SIZE;

        alad.run(msg, ITERS);
    }
    return 0;
}
