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

#include <m3/cap/VPE.h>
#include <m3/vfs/VFS.h>
#include <m3/Log.h>

using namespace m3;

enum {
    MAX_ARG_COUNT   = 6,
    MAX_ARG_LEN     = 64,
};

static const char **parseArgs(const char *line, int *argc) {
    static char argvals[MAX_ARG_COUNT][MAX_ARG_LEN];
    static char *args[MAX_ARG_COUNT];
    size_t i = 0,j = 0;
    args[0] = argvals[0];
    while(*line) {
        if(Chars::isspace(*line)) {
            if(args[j][0]) {
                if(j + 2 >= MAX_ARG_COUNT)
                    break;
                args[j][i] = '\0';
                j++;
                i = 0;
                args[j] = argvals[j];
            }
        }
        else if(i < MAX_ARG_LEN)
            args[j][i++] = *line;
        line++;
    }
    *argc = j + 1;
    args[j][i] = '\0';
    args[j + 1] = NULL;
    return (const char**)args;
}

int main() {
    auto &ser = Serial::get();

    if(VFS::mount("/", new M3FS("m3fs")) != Errors::NO_ERROR)
        PANIC("Unable to mount filesystem\n");

    ser << "========================\n";
    ser << "Welcome to the M3 shell!\n";
    ser << "========================\n";
    ser << "\n";

    while(1) {
        ser << "> ";
        ser.flush();

        String line;
        ser >> line;

        VPE vpe("job");
        vpe.delegate_mounts();

        int argc;
        const char **args = parseArgs(line.c_str(), &argc);
        Errors::Code err;
        if((err = vpe.exec(argc, args)) != Errors::NO_ERROR)
            ser << "Unable to execute '" << args[0] << "': " << Errors::to_string(err) << "\n";
        int res = vpe.wait();
        if(res != 0)
            ser << "Program terminated with exit code " << res << "\n";
    }
    return 0;
}
