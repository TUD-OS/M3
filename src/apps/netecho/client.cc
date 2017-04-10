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

#include <m3/session/NetworkManager.h>
#include <m3/stream/Standard.h>

using namespace m3;

int main(int argc, char **argv) {
    const char *message = "GTO____";
    if(argc == 2)
        message = argv[1];

    NetworkManager net("net0");

    InetSocket *socket = net.create(NetworkManager::SOCK_DGRAM);
    if(!socket)
        exitmsg("Socket creation failed.");
    cout << "Socket created.\n";

    if(socket->connect(IpAddr(192, 168, 112, 1), 1337) != Errors::NONE)
        exitmsg("Socket connect failed:" << Errors::to_string(Errors::last));

    while(true) {
        cout << "Sending message: " << message << "(" << strlen(message) + 1 << ")\n";
        ssize_t len = socket->send(message, strlen(message) + 1);
        cout << "Sent " << len << " bytes\n";

        cout << "Waiting for response...\n";
        char response[8];
        len = socket->recv(response, sizeof(response));
        cout << "Received response of length " << len << ": " << response << "\n";
    }
}
