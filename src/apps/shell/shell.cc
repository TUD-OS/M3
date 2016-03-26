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

#include <base/stream/IStringStream.h>

#include <m3/stream/Standard.h>
#include <m3/pipe/Pipe.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/Dir.h>
#include <m3/VPE.h>

#include "Args.h"
#include "Parser.h"

using namespace m3;

int main() {
    if(VFS::mount("/", new M3FS("m3fs")) != Errors::NO_ERROR) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Unable to mount filesystem\n");
    }

    cout << "========================\n";
    cout << "Welcome to the M3 shell!\n";
    cout << "========================\n";
    cout << "\n";

    while(1) {
        cout << "> ";
        cout.flush();

        // String input("echo * foo");
        // IStringStream is(input);
        // CmdList *list = get_command(&is);

        CmdList *list = get_command(&cin);
        if(!list)
            continue;

        // extract core type
        // String core;
        // if(strncmp(args[0], "CORE=", 5) == 0) {
        //     core = args[0] + 5;
        //     args++;
        //     argc--;
        // }

        for(size_t i = 0; i < list->count; ++i) {
            Args::prefix_path(list->cmds[i]->args);
            Args::expand(list->cmds[i]->args);
        }

        if(list->count == 1) {
            Command *cmd1 = list->cmds[0];
            if(cmd1->args->count == 1 && (strcmp(cmd1->args->args[0], "exit") == 0))
                break;

            VPE vpe(cmd1->args->args[0]);

            vpe.fds(*VPE::self().fds());
            vpe.obtain_fds();

            vpe.mountspace(*VPE::self().mountspace());
            vpe.obtain_mountspace();

            Errors::Code err;
            if((err = vpe.exec(cmd1->args->count, cmd1->args->args)) != Errors::NO_ERROR)
                cout << "Unable to execute '" << cmd1->args->args[0] << "': " << Errors::to_string(err) << "\n";

            int res = vpe.wait();
            if(res != 0)
                cout << "Program terminated with exit code " << res << "\n";
        }
        // TODO workaround because of the current limitations of the pipe
        else if(list->count == 2) {
            Command *cmd1 = list->cmds[0];
            Command *cmd2 = list->cmds[1];

            VPE reader(cmd2->args->args[0]);
            VPE writer(cmd1->args->args[0]);

            Pipe pipe(reader, writer, 0x1000);

            reader.fds()->set(STDIN_FD, VPE::self().fds()->get(pipe.reader_fd()));
            reader.fds()->set(STDOUT_FD, VPE::self().fds()->get(STDOUT_FD));
            reader.fds()->set(STDERR_FD, VPE::self().fds()->get(STDERR_FD));
            reader.obtain_fds();

            reader.mountspace(*VPE::self().mountspace());
            reader.obtain_mountspace();

            writer.fds()->set(STDIN_FD, VPE::self().fds()->get(STDIN_FD));
            writer.fds()->set(STDOUT_FD, VPE::self().fds()->get(pipe.writer_fd()));
            writer.fds()->set(STDERR_FD, VPE::self().fds()->get(STDERR_FD));
            writer.obtain_fds();

            writer.mountspace(*VPE::self().mountspace());
            writer.obtain_mountspace();

            Errors::Code err;
            if((err = writer.exec(cmd1->args->count, cmd1->args->args)) != Errors::NO_ERROR)
                cout << "Unable to execute '" << cmd1->args->args[0] << "': " << Errors::to_string(err) << "\n";
            if((err = reader.exec(cmd2->args->count, cmd2->args->args)) != Errors::NO_ERROR)
                cout << "Unable to execute '" << cmd2->args->args[0] << "': " << Errors::to_string(err) << "\n";

            pipe.close_writer();
            pipe.close_reader();

            int res = reader.wait();
            if(res != 0)
                cout << "Program terminated with exit code " << res << "\n";
            res = writer.wait();
            if(res != 0)
                cout << "Program terminated with exit code " << res << "\n";
        }
        else {
            cout << "Unsupported\n";
        }

        ast_cmds_destroy(list);
    }
    return 0;
}
