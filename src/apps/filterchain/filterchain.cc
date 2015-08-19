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
#include <m3/util/Random.h>
#include <m3/stream/Serial.h>
#include <m3/GateStream.h>

#include <unistd.h>

using namespace m3;

static const size_t MEM_SIZE    = 8 * 1024 * 1024;
static const size_t BUF_SIZE    = 4096;

int main() {
    Random::init(0x1234);

    MemGate mem = MemGate::create_global(MEM_SIZE, MemGate::RW);

    Serial::get() << "Initializing memory...\n";

    // init memory with random numbers
    uint *buffer = new uint[BUF_SIZE / sizeof(uint)];
    size_t rem = MEM_SIZE;
    size_t offset = 0;
    while(rem > 0) {
        for(size_t i = 0; i < BUF_SIZE / sizeof(uint); ++i)
            buffer[i] = Random::get();
        mem.write_sync(buffer, BUF_SIZE, offset);
        offset += BUF_SIZE;
        rem -= BUF_SIZE;
    }

    Serial::get() << "Starting filter chain...\n";

    // create receiver
    VPE t2("receiver");

    // create a gate the sender can send to (at the receiver)
    size_t rchan = t2.alloc_chan();
    SendGate gate = SendGate::create_for(t2, rchan);
    // use the buffer as the receive memory area at t2
    MemGate resmem = t2.mem().derive(reinterpret_cast<uintptr_t>(buffer), BUF_SIZE);

    t2.run([rchan] {
        RecvBuf rbuf = RecvBuf::create(rchan, nextlog2<512>::val, nextlog2<64>::val, 0);
        RecvGate rcvgate = RecvGate::create(&rbuf);
        size_t count, total = 0;
        int finished = 0;
        while(!finished) {
            GateIStream is = receive_vmsg(rcvgate, count, finished);

            Serial::get() << "Got " << count << " data items\n";

            reply_vmsg_on(is, 0);
            total += count;
        }
        Serial::get() << "Got " << total << " items in total\n";
        return 0;
    });

    VPE t1("sender");
    t1.delegate(CapRngDesc::All());
    t1.run([buffer, &mem, &gate, &resmem] {
        uint *result = new uint[BUF_SIZE / sizeof(uint)];
        size_t c = 0;

        size_t rem = MEM_SIZE;
        size_t offset = 0;
        while(rem > 0) {
            mem.read_sync(buffer, BUF_SIZE, offset);
            for(size_t i = 0; i < BUF_SIZE / sizeof(uint); ++i) {
                // condition that selects the data item
                if(buffer[i] % 10 == 0) {
                    result[c++] = buffer[i];
                    // if the result buffer is full, send it over to the receiver and notify him
                    if(c == BUF_SIZE / sizeof(uint)) {
                        resmem.write_sync(result, c * sizeof(uint), 0);
                        send_receive_vmsg(gate, c, 0);
                        c = 0;
                    }
                }
            }

            offset += BUF_SIZE;
            rem -= BUF_SIZE;
        }

        // any data left to send?
        if(c > 0) {
            resmem.write_sync(result, c * sizeof(uint), 0);
            send_receive_vmsg(gate, c, 1);
        }
        return 0;
    });

    t1.wait();
    t2.wait();

    Serial::get() << "Done.\n";
    return 0;
}
