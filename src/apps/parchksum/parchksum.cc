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

#include <m3/cap/MemGate.h>
#include <m3/cap/SendGate.h>
#include <m3/cap/RecvGate.h>
#include <m3/cap/VPE.h>
#include <m3/stream/Serial.h>
#include <m3/GateStream.h>

using namespace m3;

struct Worker {
    MemGate submem;
    SendGate sgate;
    VPE vpe;

    Worker(RecvGate &rgate, MemGate &mem, size_t offset, size_t size)
            : submem(mem.derive(offset, size)),
              sgate(SendGate::create(SendGate::UNLIMITED, &rgate)), vpe("worker") {
        vpe.delegate(CapRngDesc(submem.sel(), 1));
    }
};

static const size_t VPE_COUNT  = 6;
static const size_t MEM_SIZE    = VPE_COUNT * 1024 * 1024;
static const size_t SUBMEM_SIZE = MEM_SIZE / VPE_COUNT;
static const size_t BUF_SIZE    = 4096;

int main() {
    RecvBuf rbuf = RecvBuf::create(VPE::self().alloc_ep(),
        nextlog2<VPE_COUNT * (DTU_PKG_SIZE + DTU::HEADER_SIZE)>::val, 0);
    RecvGate rgate = RecvGate::create(&rbuf);

    MemGate mem = MemGate::create_global(MEM_SIZE, MemGate::RW);

    // create worker
    Worker *worker[VPE_COUNT];
    for(size_t i = 0; i < VPE_COUNT; ++i)
        worker[i] = new Worker(rgate, mem, i * SUBMEM_SIZE, SUBMEM_SIZE);

    // write data into memory
    for(size_t i = 0; i < VPE_COUNT; ++i) {
        MemGate &vpemem = worker[i]->submem;
        worker[i]->vpe.run([&vpemem] {
            uint *buffer = new uint[BUF_SIZE / sizeof(uint)];
            size_t rem = SUBMEM_SIZE;
            size_t offset = 0;
            while(rem > 0) {
                for(size_t i = 0; i < BUF_SIZE / sizeof(uint); ++i)
                    buffer[i] = i;
                vpemem.write_sync(buffer, BUF_SIZE, offset);
                offset += BUF_SIZE;
                rem -= BUF_SIZE;
            }
            Serial::get() << "Memory initialization of " << SUBMEM_SIZE << " bytes finished\n";
            return 0;
        });
    }

    // wait for all workers
    for(size_t i = 0; i < VPE_COUNT; ++i)
        worker[i]->vpe.wait();

    // now build the checksum
    for(size_t i = 0; i < VPE_COUNT; ++i) {
        worker[i]->vpe.delegate(CapRngDesc(worker[i]->sgate.sel(), 1));
        MemGate &vpemem = worker[i]->submem;
        SendGate &vpegate = worker[i]->sgate;
        worker[i]->vpe.run([&vpemem, &vpegate] {
            uint *buffer = new uint[BUF_SIZE / sizeof(uint)];

            uint checksum = 0;
            size_t rem = SUBMEM_SIZE;
            size_t offset = 0;
            while(rem > 0) {
                vpemem.read_sync(buffer, BUF_SIZE, offset);
                for(size_t i = 0; i < BUF_SIZE / sizeof(uint); ++i)
                    checksum += buffer[i];
                offset += BUF_SIZE;
                rem -= BUF_SIZE;
            }

            Serial::get() << "Checksum for sub area finished\n";
            send_vmsg(vpegate, checksum);
            return 0;
        });
    }

    // reduce
    uint checksum = 0;
    for(size_t i = 0; i < VPE_COUNT; ++i) {
        uint vpechksum;
        receive_vmsg(rgate, vpechksum);
        checksum += vpechksum;
    }

    Serial::get() << "Checksum: " << checksum << "\n";

    for(size_t i = 0; i < VPE_COUNT; ++i) {
        worker[i]->vpe.wait();
        delete worker[i];
    }
    return 0;
}
