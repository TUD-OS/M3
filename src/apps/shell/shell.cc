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

#include <base/log/Lib.h>
#include <base/stream/IStringStream.h>
#include <base/util/Time.h>

#include <m3/stream/Standard.h>
#include <m3/pipe/AccelPipeReader.h>
#include <m3/pipe/AccelPipeWriter.h>
#include <m3/pipe/IndirectPipe.h>
#include <m3/vfs/Dir.h>
#include <m3/vfs/VFS.h>
#include <m3/VPE.h>

#include <accel/stream/StreamAccel.h>

#include "Args.h"
#include "Parser.h"
#include "Vars.h"

using namespace m3;
using namespace accel;

static const size_t ABUF_SIZE  = 8192;
static const size_t ACOMP_TIME = 8192;

static const size_t PIPE_SHM_SIZE   = 512 * 1024;

static struct {
    const char *name;
    PEDesc pe;
} petypes[] = {
    /* COMP_IMEM */  {"imem",  PEDesc(PEType::COMP_IMEM, PEISA::NONE)},
    /* COMP_EMEM */  {"emem",  PEDesc(PEType::COMP_EMEM, PEISA::NONE)},
    /* MEM       */  {"mem",   PEDesc(PEType::MEM, PEISA::NONE)},
};

static struct {
    const char *name;
    PEISA isa;
} isas[] = {
    {"SHA",     PEISA::ACCEL_SHA},
    {"FFT",     PEISA::ACCEL_FFT},
    {"TOUP",    PEISA::ACCEL_TOUP},
};

static PEDesc get_pe_type(const char *name) {
    for(size_t i = 0; i < ARRAY_SIZE(petypes); ++i) {
        if(strcmp(name, petypes[i].name) == 0)
            return petypes[i].pe;
    }
    return VPE::self().pe();
}

static char **build_args(Command *cmd) {
    char **res = new char*[cmd->args->count + 1];
    for(size_t i = 0; i < cmd->args->count; ++i)
        res[i] = (char*)expr_value(cmd->args->args[i]);
    res[cmd->args->count] = nullptr;
    return res;
}

#if defined(__gem5__)
static Errors::Code exec_accel_chain(CmdList *list, VPE **vpes, size_t start, size_t end) {
    RecvGate rgate = RecvGate::create(nextlog2<8 * 64>::val, nextlog2<64>::val);
    rgate.activate();

    size_t num = end - start + 1;
    StreamAccel::ChainMember *chain[num];

    // create chain
    auto vpe_n = vpes[end];
    chain[num - 1] = new StreamAccel::ChainMember(vpe_n, StreamAccel::getRBAddr(*vpe_n),
        StreamAccel::RB_SIZE, rgate, num - 1);

    for(ssize_t i = static_cast<ssize_t>(num) - 2; i >= 0; --i) {
        auto vpe_i = vpes[start + static_cast<size_t>(i)];
        chain[i] = new StreamAccel::ChainMember(vpe_i, StreamAccel::getRBAddr(*vpe_i),
            StreamAccel::RB_SIZE, chain[i + 1]->rgate, static_cast<label_t>(i));
    }

    // connect them
    for(auto *m : chain) {
        m->send_caps();
        m->activate_recv();
    }

    // start VPEs
    size_t i = start;
    for(auto *m : chain) {
        m->activate_send();
        if(m->vpe->pe().is_programmable()) {
            m->vpe->fds()->set(STDIN_FD, new AccelPipeReader());
            m->vpe->fds()->set(STDOUT_FD, new AccelPipeWriter());
            m->vpe->obtain_fds();

            char **args = build_args(list->cmds[i]);
            m->vpe->exec(static_cast<int>(list->cmds[i]->args->count), const_cast<const char**>(args));
            delete[] args;

            if(Errors::last != Errors::NONE) {
                errmsg("Unable to execute '" << expr_value(list->cmds[i]->args->args[0]) << "'");
                return Errors::last;
            }
        }
        else
            m->vpe->start();
        i += 1;
    }

    // send init
    uintptr_t bufaddr[num];
    for(size_t i = 0; i < num - 1; ++i)
        bufaddr[i] = chain[i]->init(ABUF_SIZE, ABUF_SIZE, ABUF_SIZE / 2, ACOMP_TIME);
    bufaddr[num - 1] = chain[num - 1]->init(ABUF_SIZE,
        static_cast<size_t>(-1), static_cast<size_t>(-1), ACOMP_TIME);

    // connect memory EPs
    for(size_t i = 0; i < num - 1; ++i) {
        MemGate *buf = new MemGate(chain[i + 1]->vpe->mem().derive(bufaddr[i + 1], ABUF_SIZE));
        buf->activate_for(*chain[i]->vpe, StreamAccel::EP_OUTPUT);
    }

    // handle beginning and end of chain
    File *in = chain[0]->vpe->fds()->get(STDIN_FD);
    File *out = chain[num - 1]->vpe->fds()->get(STDOUT_FD);

    Errors::Code res = StreamAccel::executeChain(rgate, in, out, *chain[0], *chain[num - 1]);

    // destroy chain
    for(auto *c : chain) {
        // don't destroy the VPE in ~ChainMember; we'll destroy the VPE later
        c->vpe = nullptr;
        delete c;
    }

    return res;
}
#endif

static PEDesc get_pedesc(const VarList &vars, const char *path) {
    FStream f(path, FILE_R | FILE_X);
    if(f.bad())
        return VPE::self().pe();

    // accelerator description file?
    if(f.read() == '@' && f.read() == '=') {
        char line[128];
        f.getline(line, sizeof(line));
        for(size_t i = 0; i < ARRAY_SIZE(isas); ++i) {
            if(strcmp(isas[i].name, line) == 0)
                return PEDesc(PEType::COMP_IMEM, isas[i].isa);
        }
    }

    for(size_t i = 0; i < vars.count; ++i) {
        if(strcmp(vars.vars[i].name, "PE") == 0) {
            PEDesc pe = get_pe_type(expr_value(vars.vars[i].value));
            // use the current ISA for comp-PEs
            // TODO we could let the user specify the ISA
            if(pe.type() != PEType::MEM)
                pe = PEDesc(pe.type(), VPE::self().pe().isa(), pe.mem_size());
            break;
        }
    }
    return VPE::self().pe();
}

static void execute_assignment(CmdList *list) {
    Command *cmd = list->cmds[0];

    for(size_t i = 0; i < cmd->vars->count; ++i) {
        Var *v = cmd->vars->vars + i;
        Vars::get().set(v->name, expr_value(v->value));
    }
}

static bool execute_pipeline(CmdList *list, bool muxed) {
    VPE *vpes[MAX_CMDS] = {nullptr};
    IndirectPipe *pipes[MAX_CMDS] = {nullptr};
    MemGate *mems[MAX_CMDS] = {nullptr};
    PEDesc descs[MAX_CMDS];

    // find accelerator chain
    size_t chain_start = MAX_CMDS;
    size_t chain_end = MAX_CMDS;
    for(size_t i = 0; i < list->count; ++i) {
        if(list->cmds[i]->args->count == 0) {
            errmsg("Command has no arguments");
            return true;
        }

        descs[i] = get_pedesc(*list->cmds[i]->vars, expr_value(list->cmds[i]->args->args[0]));
        if(!descs[i].is_programmable()) {
            if(chain_start == MAX_CMDS)
                chain_start = i;
            chain_end = i;
        }
    }

    fd_t infd = STDIN_FD;
    fd_t outfd = STDOUT_FD;
    for(size_t i = 0; i < list->count; ++i) {
        Command *cmd = list->cmds[i];

        vpes[i] = new VPE(expr_value(cmd->args->args[0]), descs[i], nullptr, muxed);
        if(Errors::last != Errors::NONE) {
            errmsg("Unable to create VPE for " << expr_value(cmd->args->args[0]));
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
        else if(!(i > chain_start && i <= chain_end))
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
        else if(!(i + 1 > chain_start && i + 1 <= chain_end)) {
            mems[i] = new MemGate(MemGate::create_global(PIPE_SHM_SIZE, MemGate::RW));
            pipes[i] = new IndirectPipe(*mems[i], PIPE_SHM_SIZE);
            vpes[i]->fds()->set(STDOUT_FD, VPE::self().fds()->get(pipes[i]->writer_fd()));
        }

        if(!(i >= chain_start && i <= chain_end)) {
            vpes[i]->fds()->set(STDERR_FD, VPE::self().fds()->get(STDERR_FD));
            vpes[i]->obtain_fds();

            vpes[i]->mounts(*VPE::self().mounts());
            vpes[i]->obtain_mounts();

            char **args = build_args(cmd);
            vpes[i]->exec(static_cast<int>(cmd->args->count), const_cast<const char**>(args));
            delete[] args;

            if(Errors::last != Errors::NONE) {
                errmsg("Unable to execute '" << expr_value(cmd->args->args[0]) << "'");
                break;
            }
        }

        if(i > 0 && pipes[i - 1]) {
            if(vpes[i]->pe().is_programmable())
                pipes[i - 1]->close_reader();
            if(vpes[i - 1]->pe().is_programmable())
                pipes[i - 1]->close_writer();
        }
    }

    // if there is an accelerator chain, we need to handle the beginning and end
#if defined(__gem5__)
    if(chain_start != MAX_CMDS && Errors::last == Errors::NONE) {
        Errors::Code res = exec_accel_chain(list, vpes, chain_start, chain_end);
        if(res != Errors::NONE)
            errmsg("Unable to execute accelerator pipeline");

        for(size_t i = 1; i < list->count; ++i) {
            if(pipes[i - 1]) {
                if(!vpes[i]->pe().is_programmable())
                    pipes[i - 1]->close_reader();
                if(!vpes[i - 1]->pe().is_programmable())
                    pipes[i - 1]->close_writer();
            }
        }
    }
#endif

    for(size_t i = 0; i < list->count; ++i) {
        if(vpes[i]) {
            if(vpes[i]->pe().is_programmable()) {
                int res = vpes[i]->wait();
                if(res != 0)
                    cerr << "Program terminated with exit code " << res << "\n";
            }
            delete vpes[i];
        }
    }
    for(size_t i = 0; i < list->count; ++i) {
        delete mems[i];
        delete pipes[i];
    }
    if(infd != STDIN_FD && infd != FileTable::INVALID)
        VFS::close(infd);
    if(outfd != STDOUT_FD && outfd != FileTable::INVALID)
        VFS::close(outfd);
    return true;
}

static bool execute(CmdList *list, bool muxed) {
    for(size_t i = 0; i < list->count; ++i) {
        Args::prefix_path(list->cmds[i]->args);
        Args::expand(list->cmds[i]->args);
    }

    bool res = true;
    if(list->count == 1 && list->cmds[0]->args->count == 0)
        execute_assignment(list);
    else
        res = execute_pipeline(list, muxed);
    return res;
}

int main(int argc, char **argv) {
    if(VFS::mount("/", "m3fs") != Errors::NONE) {
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

        cycles_t start = Time::start(0x1234);
        execute(list, muxed);
        cycles_t end = Time::stop(0x1234);

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

        if(!execute(list, muxed))
            break;

        ast_cmds_destroy(list);
    }
    return 0;
}
