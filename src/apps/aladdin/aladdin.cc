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
#include <base/stream/Serial.h>
#include <base/stream/IStringStream.h>
#include <base/util/Time.h>
#include <base/CmdArgs.h>

#include <m3/stream/Standard.h>

#include <m3/com/MemGate.h>
#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>
#include <m3/session/Pager.h>
#include <m3/vfs/RegularFile.h>
#include <m3/vfs/VFS.h>
#include <m3/VPE.h>

using namespace m3;

static const int REPEATS = 2;

static size_t step_size = 0;
static bool use_files = false;
static bool map_eager = false;

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

    struct Array {
        uint64_t addr;
        uint64_t size;
    } PACKED;

    struct InvokeMessage {
        Array arrays[8];
        uint64_t array_count;
        uint64_t iterations;
    } PACKED;

    explicit Aladdin(PEISA isa)
        : _accel(new VPE("aladdin", PEDesc(PEType::COMP_EMEM, isa), "pager", true)),
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
        cycles_t start = Time::start(0x1234);

        size_t count = 0;
        size_t per_step = step_size == 0 ? iterations : step_size;
        while(count < iterations) {
            msg.iterations = Math::min(iterations - count, per_step);
            invoke(msg);
            count += msg.iterations;
        }

        cycles_t end = Time::stop(0x1234);
        cout << "Benchmark took " << (end - start) << " cycles\n";
    }

    VPE *_accel;
    capsel_t _lastmem;
    m3::RecvGate _rgate;
    m3::RecvGate _srgate;
    m3::SendGate _sgate;
};

static const size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
static size_t offs[ARRAY_SIZE(sizes)] = {0};
static uintptr_t virt = 0x1000000;

static void reset() {
    virt = 0x1000000;
    for(size_t i = 0; i < ARRAY_SIZE(sizes); ++i)
        offs[i] = 0;
}

static String get_file(size_t size, size_t *off) {
    // take care that we do not use the same file data twice.
    // (otherwise, we experience more cache hits than we should...)

    for(size_t i = 0; i < ARRAY_SIZE(sizes); ++i) {
        if(size < sizes[i] * 1024 - offs[i]) {
            OStringStream os;
            os << "/data/" << sizes[i] << "k.txt";
            *off = offs[i];
            offs[i] += size;
            return os.str();
        }
    }

    exitmsg("Unable to find file for data size " << size);
}

static void add(Aladdin &alad, size_t size, Aladdin::Array *a, int prot) {
    size_t psize = Math::round_up(size, PAGE_SIZE);

    if(use_files) {
        int perms = (prot & MemGate::W) ? FILE_RW : FILE_R;
        size_t off = 0;
        String filename = get_file(size, &off);
        fd_t fd = VFS::open(filename.c_str(), perms);
        if(fd == FileTable::INVALID)
            exitmsg("Unable to open '" << filename << "'");
        RegularFile *file = static_cast<RegularFile*>(VPE::self().fds()->get(fd));
        int flags = (prot & MemGate::W) ? Pager::MAP_SHARED : Pager::MAP_PRIVATE;
        alad._accel->pager()->map_ds(&virt, psize, prot, flags, *file->fs(), file->fd(), off);
    }
    else {
        MemGate *mem = new MemGate(MemGate::create_global(psize, prot));
        alad._accel->pager()->map_mem(&virt, *mem, psize, prot);
    }

    if(map_eager) {
        size_t off = 0;
        size_t pages = psize / PAGE_SIZE;
        while(pages > 0) {
            alad._accel->pager()->pagefault(virt + off, static_cast<uint>(prot));
            pages -= 1;
            off += PAGE_SIZE;
        }
    }

    a->addr = virt;
    a->size = size;
    virt += psize;
}

static void run(const char *bench) {
    if(strcmp(bench, "stencil") == 0) {
        Aladdin alad(PEISA::ACCEL_STE);

        const size_t HEIGHT = 32;
        const size_t COLS = 32;
        const size_t ROWS = 64;
        const size_t SIZE = HEIGHT * ROWS * COLS * sizeof(uint32_t);
        const size_t ITERATIONS = (HEIGHT - 2) * (COLS - 2);

        Aladdin::InvokeMessage msg;
        msg.array_count = 3;
        add(alad, SIZE, msg.arrays + 0, MemGate::R);                        // orig
        add(alad, SIZE, msg.arrays + 1, MemGate::W);                        // sol
        add(alad, 8, msg.arrays + 2, MemGate::R);                           // C

        alad.run(msg, ITERATIONS);
    }
    else if(strcmp(bench, "md") == 0) {
        Aladdin alad(PEISA::ACCEL_MD);

        const size_t ATOMS = 1024;
        const size_t MAX_NEIGHBORS = 16;
        const size_t ATOM_SET = ATOMS * sizeof(double);

        Aladdin::InvokeMessage msg;
        msg.array_count = 7;
        for(size_t i = 0; i < 3; ++i)
            add(alad, ATOM_SET, msg.arrays + i, MemGate::R);                // position_{x,y,z}
        for(size_t i = 0; i < 3; ++i)
            add(alad, ATOM_SET, msg.arrays + 3 + i, MemGate::W);            // force_{x,y,z}
        add(alad, ATOMS * MAX_NEIGHBORS * 4, msg.arrays + 6, MemGate::R);   // NC

        alad.run(msg, ATOMS);
    }
    else if(strcmp(bench, "spmv") == 0) {
        Aladdin alad(PEISA::ACCEL_SPMV);

        const size_t NNZ = 39321;
        const size_t N = 2048;
        const size_t VEC_LEN = N * sizeof(double);

        Aladdin::InvokeMessage msg;
        msg.array_count = 5;
        add(alad, NNZ * sizeof(double), msg.arrays + 0, MemGate::R);        // val
        add(alad, NNZ * sizeof(int32_t), msg.arrays + 1, MemGate::R);       // cols
        add(alad, (N + 1) * sizeof(int32_t), msg.arrays + 2, MemGate::R);   // rowDelimiters
        add(alad, VEC_LEN, msg.arrays + 3, MemGate::R);                     // vec
        add(alad, VEC_LEN, msg.arrays + 4, MemGate::W);                     // out

        alad.run(msg, N);
    }
    else if(strcmp(bench, "fft") == 0) {
        Aladdin alad(PEISA::ACCEL_AFFT);

        const size_t DATA_LEN = 16384;
        const size_t SIZE = DATA_LEN * sizeof(double);
        const size_t ITERS = (DATA_LEN / 512) * 11;

        Aladdin::InvokeMessage msg;
        msg.array_count = 2;
        add(alad, SIZE, msg.arrays + 0, MemGate::RW);                       // work_x
        add(alad, SIZE, msg.arrays + 1, MemGate::RW);                       // work_y

        alad.run(msg, ITERS);
    }
}

static void usage(const char *name) {
    Serial::get() << "Usage: " << name << " [-s <step_size>] [-f] [-e] (stencil|md|spmv|fft)\n";
    Serial::get() << "  -s: the step size (0 = unlimited)\n";
    Serial::get() << "  -f: use files for input and output\n";
    Serial::get() << "  -e: map all memory eagerly\n";
    exit(1);
}

int main(int argc, char **argv) {
    int opt;
    while((opt = CmdArgs::get(argc, argv, "s:fe")) != -1) {
        switch(opt) {
            case 's': step_size = IStringStream::read_from<size_t>(CmdArgs::arg); break;
            case 'f': use_files = true; break;
            case 'e': map_eager = true; break;
            default:
                usage(argv[0]);
        }
    }
    if(CmdArgs::ind >= argc)
        usage(argv[0]);

    const char *bench = argv[CmdArgs::ind];
    for(int i = 0; i < REPEATS; ++i) {
        run(bench);
        reset();
    }
    return 0;
}
