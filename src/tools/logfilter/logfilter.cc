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
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <err.h>

enum State {
    ST_DEF,
    ST_DTUOP
};

int main(int argc, char **argv) {
    if(argc < 2) {
        fprintf(stderr, "Usage: %s <fifo>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if(mknod(argv[1], S_IFIFO | 0600, 0) == -1)
        err(1, "mknod(%s) failed", argv[1]);

    long count = 0;
    State state = ST_DEF;
    std::ifstream f(argv[1]);
    if(!f)
        err(1, "fopen failed");
    while(!f.eof()) {
        std::string line;
        std::getline(f, line);

        bool print = state == ST_DTUOP;
        if(line.find("---------------------------------") != std::string::npos) {
            state = (state == ST_DEF) ? ST_DTUOP : ST_DEF;
            print = true;
        }
        else if(line.find("last packet of Msg received") != std::string::npos)
            print = true;
        else if(line.find("REPLY_CAP_RESP_CMD") != std::string::npos)
            print = true;
        else if(line.find("DMA-DEBUG-MESSAGE") != std::string::npos)
            print = true;
        else if(line.find("ERROR") != std::string::npos)
            print = true;
        else if(line.find("WARNING") != std::string::npos)
            print = true;

        if(print)
            std::cout << line << "\n";
        else if((++count % 10000) == 0)
            std::cout << line << std::endl;
    }

    if(unlink(argv[1]) == -1)
        warn("unlink of '%s' failed", argv[1]);
    return 0;
}
