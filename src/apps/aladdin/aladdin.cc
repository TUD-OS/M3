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

#include <m3/accel/AladdinAccel.h>
#include <m3/com/MemGate.h>
#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>
#include <m3/session/Pager.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/VPE.h>

using namespace m3;

static const int REPEATS = 4;

static size_t step_size = 0;
static bool use_files = false;
static bool map_eager = false;

struct AccelWorkload {
    explicit AccelWorkload(PEISA isa) : alad(isa), msg(), iterations() {
    }

    void init();
    void run() {
        size_t count = 0;
        size_t per_step = step_size == 0 ? iterations : step_size;
        while(count < iterations) {
            msg.iterations = Math::min(iterations - count, per_step);
            alad.invoke(msg);
            count += msg.iterations;
        }
    }

    Aladdin alad;
    Aladdin::InvokeMessage msg;
    size_t iterations;
};

static const size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
static size_t offs[ARRAY_SIZE(sizes)] = {0};
static goff_t virt = 0x1000000;
static fd_t fds[16] = {0};
static size_t next_fd = 0;

static void reset() {
    virt = 0x1000000;
    for(size_t i = 0; i < ARRAY_SIZE(sizes); ++i)
        offs[i] = 0;
    for(size_t i = 0; i < next_fd; ++i)
        VFS::close(fds[i]);
    next_fd = 0;
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
        if(next_fd >= ARRAY_SIZE(fds))
            exitmsg("Not enough file slots");
        int perms = (prot & MemGate::W) ? FILE_RW : FILE_R;
        size_t off = 0;
        String filename = get_file(size, &off);
        fd_t fd = VFS::open(filename.c_str(), perms);
        if(fd == FileTable::INVALID)
            exitmsg("Unable to open '" << filename << "'");
        const GenericFile *file = static_cast<const GenericFile*>(VPE::self().fds()->get(fd));
        int flags = (prot & MemGate::W) ? Pager::MAP_SHARED : Pager::MAP_PRIVATE;
        alad._accel->pager()->map_ds(&virt, psize, prot, flags, file->sess(), off);
        fds[next_fd++] = fd;
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

static AccelWorkload *create_workload(const char *bench) {
    PEISA isa;
    if(strcmp(bench, "stencil") == 0)
        isa = PEISA::ACCEL_STE;
    else if(strcmp(bench, "md") == 0)
        isa = PEISA::ACCEL_MD;
    else if(strcmp(bench, "spmv") == 0)
        isa = PEISA::ACCEL_SPMV;
    else
        isa = PEISA::ACCEL_AFFT;
    return new AccelWorkload(isa);
}

void AccelWorkload::init() {
    msg.repeats = 1;

    switch(alad.isa()) {
        case PEISA::ACCEL_STE: {
            const size_t HEIGHT = 32;
            const size_t COLS = 32;
            const size_t ROWS = 64;
            const size_t SIZE = HEIGHT * ROWS * COLS * sizeof(uint32_t);
            const size_t ITERATIONS = (HEIGHT - 2) * (COLS - 2);

            msg.iterations = iterations = ITERATIONS;
            msg.array_count = 3;

            add(alad, SIZE, msg.arrays + 0, MemGate::R);                        // orig
            add(alad, SIZE, msg.arrays + 1, MemGate::W);                        // sol
            add(alad, 8, msg.arrays + 2, MemGate::R);                           // C
            break;
        }

        case PEISA::ACCEL_MD: {
            const size_t ATOMS = 1024;
            const size_t MAX_NEIGHBORS = 16;
            const size_t ATOM_SET = ATOMS * sizeof(double);

            msg.iterations = iterations = ATOMS;
            msg.array_count = 7;

            for(size_t i = 0; i < 3; ++i)
                add(alad, ATOM_SET, msg.arrays + i, MemGate::R);                // position_{x,y,z}
            for(size_t i = 0; i < 3; ++i)
                add(alad, ATOM_SET, msg.arrays + 3 + i, MemGate::W);            // force_{x,y,z}
            add(alad, ATOMS * MAX_NEIGHBORS * 4, msg.arrays + 6, MemGate::R);   // NC
            break;
        }

        case PEISA::ACCEL_SPMV: {
            const size_t NNZ = 39321;
            const size_t N = 2048;
            const size_t VEC_LEN = N * sizeof(double);

            msg.iterations = iterations = N;
            msg.array_count = 5;

            add(alad, NNZ * sizeof(double), msg.arrays + 0, MemGate::R);        // val
            add(alad, NNZ * sizeof(int32_t), msg.arrays + 1, MemGate::R);       // cols
            add(alad, (N + 1) * sizeof(int32_t), msg.arrays + 2, MemGate::R);   // rowDelimiters
            add(alad, VEC_LEN, msg.arrays + 3, MemGate::R);                     // vec
            add(alad, VEC_LEN, msg.arrays + 4, MemGate::W);                     // out
            break;
        }

        default:
        case PEISA::ACCEL_AFFT: {
            const size_t DATA_LEN = 16384;
            const size_t SIZE = DATA_LEN * sizeof(double);
            const size_t ITERS = (DATA_LEN / 512) * 11;

            msg.iterations = iterations = ITERS;
            msg.array_count = 4;

            add(alad, SIZE, msg.arrays + 0, MemGate::R);                       // in_x
            add(alad, SIZE, msg.arrays + 1, MemGate::R);                       // in_y
            add(alad, SIZE, msg.arrays + 2, MemGate::W);                       // out_x
            add(alad, SIZE, msg.arrays + 3, MemGate::W);                       // out_y
            break;
        }
    }
}

static void usage(const char *name) {
    Serial::get() << "Usage: " << name << " [-s <step_size>] [-f] [-e] [-m <mode>] (stencil|md|spmv|fft)\n";
    Serial::get() << "  -s: the step size (0 = unlimited)\n";
    Serial::get() << "  -f: use files for input and output\n";
    Serial::get() << "  -e: map all memory eagerly\n";
    Serial::get() << "  -m: the mode: 0 = default, 1 = two accels sequentially, 2 = two accels simultaneously\n";
    exit(1);
}

enum {
    MODE_DEFAULT        = 0,
    MODE_SEQUENCE       = 1,
    MODE_SIMULTANEOUS   = 2,
};

int main(int argc, char **argv) {
    int mode = MODE_DEFAULT;
    int opt;
    while((opt = CmdArgs::get(argc, argv, "s:fem:")) != -1) {
        switch(opt) {
            case 's': step_size = IStringStream::read_from<size_t>(CmdArgs::arg); break;
            case 'f': use_files = true; break;
            case 'e': map_eager = true; break;
            case 'm': mode = IStringStream::read_from<int>(CmdArgs::arg); break;
            default:
                usage(argv[0]);
        }
    }
    if(CmdArgs::ind >= argc)
        usage(argv[0]);

    const char *bench = argv[CmdArgs::ind];
    for(int i = 0; i < REPEATS; ++i) {
        cycles_t start = Time::start(0x1234);

        if(mode == MODE_DEFAULT) {
            AccelWorkload *wl = create_workload(bench);
            wl->init();
            wl->run();
            delete wl;
        }
        else if(mode == MODE_SEQUENCE) {
            AccelWorkload *wl1 = create_workload(bench);
            AccelWorkload *wl2 = create_workload(bench);
            wl1->init();
            wl2->init();
            wl1->msg.repeats = wl1->alad.isa() == PEISA::ACCEL_STE ? 40 : 20;
            wl2->msg.repeats = wl2->alad.isa() == PEISA::ACCEL_STE ? 40 : 20;
            wl1->run();
            wl2->run();
            delete wl2;
            delete wl1;
        }
        else {
            AccelWorkload *wl1 = create_workload(bench);
            AccelWorkload *wl2 = create_workload(bench);
            wl1->init();
            wl2->init();
            wl1->msg.repeats = wl1->alad.isa() == PEISA::ACCEL_STE ? 40 : 20;
            wl2->msg.repeats = wl2->alad.isa() == PEISA::ACCEL_STE ? 40 : 20;
            wl1->alad.start(wl1->msg);
            wl2->alad.start(wl2->msg);
            wl1->alad.wait();
            wl2->alad.wait();
            delete wl2;
            delete wl1;
        }

        cycles_t end = Time::stop(0x1234);
        cout << "Benchmark took " << (end - start) << " cycles\n";

        reset();
    }
    return 0;
}
