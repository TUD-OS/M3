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

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#define MSG_SIZE    256

//const char *socket_path = "./socket";
const char *socket_path = "\0hidden";

static void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    bool perf = false;
    struct sockaddr_un addr;
    char buf[MSG_SIZE];
    int fd, rc;

    if(argc > 1 && strcmp(argv[1], "--perf") == 0)
        perf = true;

    if((fd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1)
        error("socket");

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    unlink(socket_path);

    if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
        error("bind");

    if(perf) {
        while(recvfrom(fd, buf, sizeof(buf), 0, nullptr, nullptr) > 0)
            ;
    }
    else {
        while((rc = recvfrom(fd, buf, sizeof(buf), 0, nullptr, nullptr)) > 0)
            printf("read %u bytes: %.*s\n", rc, rc, buf);
        if(rc < 0)
            perror("recvfrom");
    }
    return 0;
}
