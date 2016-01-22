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

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#define MSG_KEY     1234
#define MSG_SIZE    256

static volatile bool stop = false;

static void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void sigterm(int) {
    stop = true;
}

int main(int argc, char **argv) {
    bool perf = false;
    char buf[MSG_SIZE];
    int id, rc = 0;

    if(argc > 1 && strcmp(argv[1], "--perf") == 0)
        perf = true;

    signal(SIGTERM, sigterm);

    if((id = msgget(MSG_KEY, IPC_CREAT | IPC_EXCL | 0777)) < 0) {
        if((id = msgget(MSG_KEY, 0)) < 0)
            error("msgget");
    }

    if(perf) {
        while(!stop) {
            if(msgrcv(id, buf, sizeof(buf), 0, 0) < 0)
                error("msgrcv");
        }
    }
    else {
        while(!stop && (rc = msgrcv(id, buf, sizeof(buf), 0, 0)) > 0)
            printf("read %u bytes: %.*s\n", rc, rc, buf);
        if(rc < 0)
            perror("msgrcv");
    }
    msgctl(id, IPC_RMID, nullptr);
    return 0;
}
