/*
 * Copyright (C) 2017, Georg Kotheimer <georg.kotheimer@mailbox.tu-dresden.de>
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

#include <base/util/Time.h>

#include <m3/session/NetworkManager.h>
#include <m3/stream/Standard.h>

using namespace m3;

int main() {
    NetworkManager net("net0");

    InetSocket *socket = net.create(NetworkManager::SOCK_DGRAM);
    if(!socket)
        exitmsg("Socket creation failed.");

    if(socket->connect(IpAddr(192, 168, 112, 1), 1337) != Errors::NONE)
        exitmsg("Socket connect failed:" << Errors::to_string(Errors::last));

    union {
        uint8_t raw[1024];
        cycles_t time;
    } request;

    union {
        uint8_t raw[1024];
        cycles_t time;
    } response;

    size_t samples = 15;
    size_t packet_size = 8;

    size_t warmup = 5;
    cout << "Warmup...\n";
    while(warmup--) {
        socket->send(request.raw, 8);
        socket->recv(response.raw, 8);
    }
    cout << "Warmup done.\n";

    cout << "Benchmark...\n";
    size_t sample = 0;
    while(true) {
        cycles_t start = Time::start(0);

        request.time = start;
        ssize_t send_len = socket->send(request.raw, packet_size);
        ssize_t recv_len = socket->recv(response.raw, packet_size);

        cycles_t stop = Time::stop(0);

        if(static_cast<size_t>(send_len) != packet_size)
            exitmsg("Send failed.");

        if(static_cast<size_t>(recv_len) != packet_size || start != response.time)
            exitmsg("Receive failed.");

        cout << "RTT (" << packet_size << "b): " << stop - start << " cycles / " << (stop - start) / 3e6f << " ms (@3GHz) \n";

        packet_size *= 2;
        if(packet_size > sizeof(request)) {
            sample++;
            if(sample >= samples)
                 break;

            packet_size = 8;
        }
    }

    cout << "Benchmark done.\n";

    socket->close();
    delete socket;

    return 0;
}
