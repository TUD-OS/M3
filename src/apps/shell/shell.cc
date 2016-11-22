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
#include <base/util/Profile.h>

#include <m3/stream/Standard.h>
#include <m3/pipe/IndirectPipe.h>
#include <m3/vfs/Dir.h>
#include <m3/vfs/VFS.h>
#include <m3/VPE.h>

#include "Args.h"
#include "Parser.h"

using namespace m3;

static struct {
    const char *name;
    PEDesc pe;
} petypes[] = {
    /* COMP_IMEM */ {"imem", PEDesc(PEType::COMP_IMEM, PEISA::NONE)},
    /* COMP_EMEM */ {"emem", PEDesc(PEType::COMP_EMEM, PEISA::NONE)},
    /* MEM       */ {"mem",  PEDesc(PEType::MEM, PEISA::NONE)},
};

static PEDesc get_pe_type(const char *name) {
    for(size_t i = 0; i < ARRAY_SIZE(petypes); ++i) {
        if(strcmp(name, petypes[i].name) == 0)
            return petypes[i].pe;
    }
    return VPE::self().pe();
}

static bool execute(CmdList *list, bool muxed) {
    VPE *vpes[MAX_CMDS] = {nullptr};
    IndirectPipe *pipes[MAX_CMDS] = {nullptr};

    fd_t infd = STDIN_FD;
    fd_t outfd = STDOUT_FD;
    for(size_t i = 0; i < list->count; ++i) {
        Command *cmd = list->cmds[i];

        PEDesc pe = VPE::self().pe();
        for(size_t i = 0; i < cmd->vars->count; ++i) {
            if(strcmp(cmd->vars->vars[i].name, "PE") == 0) {
                pe = get_pe_type(cmd->vars->vars[i].value);
                // use the current ISA for comp-PEs
                // TODO we could let the user specify the ISA
                if(pe.type() != PEType::MEM)
                    pe = PEDesc(pe.type(), VPE::self().pe().isa(), pe.mem_size());
                break;
            }
        }

        vpes[i] = new VPE(cmd->args->args[0], pe, nullptr, muxed);
        if(Errors::last != Errors::NONE) {
            errmsg("Unable to create VPE for " << cmd->args->args[0]);
            break;
        }

        // I/O redirection is only supported at the beginning and end
        if((i + 1 < list->count && cmd->redirs->fds[STDOUT_FD]) ||
            (i > 0 && cmd->redirs->fds[STDIN_FD])) {
            errmsg("Invalid I/O redirection");
            break;
        }

        if(i == 0) {
            if(cmd->redirs->fds[STDIN_FD]) {
                infd = VFS::open(cmd->redirs->fds[STDIN_FD], FILE_R);
                if(infd == FileTable::INVALID) {
                    errmsg("Unable to open " << cmd->redirs->fds[STDIN_FD]);
                    break;
                }
            }
            vpes[i]->fds()->set(STDIN_FD, VPE::self().fds()->get(infd));
        }
        else
            vpes[i]->fds()->set(STDIN_FD, VPE::self().fds()->get(pipes[i - 1]->reader_fd()));

        if(i + 1 == list->count) {
            if(cmd->redirs->fds[STDOUT_FD]) {
                outfd = VFS::open(cmd->redirs->fds[STDOUT_FD], FILE_W | FILE_CREATE | FILE_TRUNC);
                if(outfd == FileTable::INVALID) {
                    errmsg("Unable to open " << cmd->redirs->fds[STDOUT_FD]);
                    break;
                }
            }
            vpes[i]->fds()->set(STDOUT_FD, VPE::self().fds()->get(outfd));
        }
        else {
            pipes[i] = new IndirectPipe(64 * 1024);
            vpes[i]->fds()->set(STDOUT_FD, VPE::self().fds()->get(pipes[i]->writer_fd()));
        }

        vpes[i]->fds()->set(STDERR_FD, VPE::self().fds()->get(STDERR_FD));
        vpes[i]->obtain_fds();

        vpes[i]->mountspace(*VPE::self().mountspace());
        vpes[i]->obtain_mountspace();

        Errors::Code err;
        if((err = vpes[i]->exec(static_cast<int>(cmd->args->count), cmd->args->args)) != Errors::NONE) {
            errmsg("Unable to execute '" << cmd->args->args[0] << "'");
            break;
        }

        if(i > 0) {
            pipes[i - 1]->close_reader();
            pipes[i - 1]->close_writer();
        }
    }

    for(size_t i = 0; i < list->count; ++i) {
        if(vpes[i]) {
            int res = vpes[i]->wait();
            if(res != 0)
                cerr << "Program terminated with exit code " << res << "\n";
            delete vpes[i];
        }
    }
    for(size_t i = 0; i < list->count; ++i)
        delete pipes[i];
    if(infd != STDIN_FD)
        VFS::close(infd);
    if(outfd != STDOUT_FD)
        VFS::close(outfd);
    return true;
}

int main(int argc, char **argv) {
    if(VFS::mount("/", new M3FS("m3fs")) != Errors::NONE) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Unable to mount filesystem\n");
    }

    bool muxed = argc > 1 && strcmp(argv[1], "1") == 0;

    if(argc > 2) {
        OStringStream os;
        for(int i = 2; i < argc; ++i)
            os << argv[i] << " ";

        String input(os.str(), os.length());
        IStringStream is(input);
        CmdList *list = get_command(&is);
        if(!list)
            exitmsg("Unable to parse command '" << input << "'");

        for(size_t i = 0; i < list->count; ++i) {
            Args::prefix_path(list->cmds[i]->args);
            Args::expand(list->cmds[i]->args);
        }

        cycles_t start = Profile::start(0x1234);
        execute(list, muxed);
        cycles_t end = Profile::stop(0x1234);
        cerr << "Execution took " << (end - start) << " cycles\n";
        return 0;
    }

    cout << "========================\n";
    cout << "Welcome to the M3 shell!\n";
    cout << "========================\n";
    cout << "\n";

    while(!cin.eof()) {
        cout << "$ ";
        cout.flush();

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

        if(!execute(list, muxed))
            break;

        ast_cmds_destroy(list);
    }
    return 0;
}
