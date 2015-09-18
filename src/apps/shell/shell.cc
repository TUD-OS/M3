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
#include <m3/vfs/Dir.h>
#include <m3/Log.h>

#include "Args.h"

using namespace m3;

int main() {
    auto &ser = Serial::get();

    if(VFS::mount("/", new M3FS("m3fs")) != Errors::NO_ERROR) {
        if(Errors::last != Errors::EXISTS)
            PANIC("Unable to mount filesystem\n");
    }

    ser << "========================\n";
    ser << "Welcome to the M3 shell!\n";
    ser << "========================\n";
    ser << "\n";

    while(1) {
        ser << "> ";
        ser.flush();

        String line;
        ser >> line;

        if(strcmp(line.c_str(), "quit") == 0 || strcmp(line.c_str(), "exit") == 0)
            break;

        int argc;
        char **args = Args::parse(line.c_str(), &argc);
        Errors::Code err;

        // prefix "/bin/" if necessary
        if(args[0][0] != '/' && strlen(args[0]) + 5 < Args::MAX_ARG_LEN) {
            memmove(args[0] + 5, args[0], strlen(args[0]) + 1);
            memcpy(args[0], "/bin/", 5);
        }

        // execute the command
        VPE vpe(args[0]);
        vpe.delegate_mounts();
        if((err = vpe.exec(argc, const_cast<const char**>(args))) != Errors::NO_ERROR)
            ser << "Unable to execute '" << args[0] << "': " << Errors::to_string(err) << "\n";
        int res = vpe.wait();
        if(res != 0)
            ser << "Program terminated with exit code " << res << "\n";
    }
    return 0;
}
