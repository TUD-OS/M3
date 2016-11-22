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

#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#define MSG_KEY     1234
#define MSG_COUNT   1000000
#define MSG_SIZE    256

static void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    bool perf = false;
    // damn, we can't send arbitrary data but really have to use this type-header or at least
    // something != 0
    char buf[MSG_SIZE] = "foobar";
    int id;
    ssize_t rc, res;

    if(argc > 1 && strcmp(argv[1], "--perf") == 0)
        perf = true;

    if((id = msgget(MSG_KEY, 0)) < 0)
        error("msgget");

    if(perf) {
        for(int i = 0; i < MSG_COUNT; ++i) {
            if(msgsnd(id, buf, sizeof(buf), 0) < 0)
                error("msgsnd");
        }
    }
    else {
        while((rc = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
            if((res = msgsnd(id, buf, static_cast<size_t>(rc), 0)) != 0)
                perror("msgsnd");
        }
    }
    return 0;
}
