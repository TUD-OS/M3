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

#include <base/Common.h>
#include <base/stream/IStringStream.h>
#include <base/Panic.h>
#include <base/CmdArgs.h>

#include <m3/session/M3FS.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/Dir.h>
#include <m3/vfs/VFS.h>

#include "common/traceplayer.h"
#include "platform.h"

using namespace m3;

static const size_t MAX_TMP_FILES   = 128;
static const bool VERBOSE           = 0;
static const uint META_EPS          = 4;

static void remove_rec(const char *path) {
    if(VERBOSE) cerr << "Unlinking " << path << "\n";
    if(VFS::unlink(path) == Errors::IS_DIR) {
        Dir::Entry e;
        char tmp[128];
        Dir dir(path);
        while(dir.readdir(e)) {
            if(strcmp(e.name, ".") == 0 || strcmp(e.name, "..") == 0)
                continue;

            OStringStream file(tmp, sizeof(tmp));
            file << path << "/" << e.name;
            remove_rec(file.str());
        }
        VFS::rmdir(path);
    }
}

static void cleanup() {
    Dir dir("/tmp");
    if(Errors::occurred())
        return;

    size_t x = 0;
    String *entries[MAX_TMP_FILES];

    if(VERBOSE) cerr << "Collecting files in /tmp\n";

    // remove all entries; we assume here that they are files
    Dir::Entry e;
    char path[128];
    while(dir.readdir(e)) {
        if(strcmp(e.name, ".") == 0 || strcmp(e.name, "..") == 0)
            continue;

        OStringStream file(path, sizeof(path));
        file << "/tmp/" << e.name;
        if(x > ARRAY_SIZE(entries))
            PANIC("Too few entries");
        entries[x++] = new String(file.str());
    }

    for(; x > 0; --x) {
        remove_rec(entries[x - 1]->c_str());
        delete entries[x - 1];
    }
}

static void usage(const char *name) {
    cerr << "Usage: " << name << " [-p <prefix>] [-n <iterations>] [-w] [-f <fs>]"
                              << " [-g <rgate selector>] [-l <loadgen>] [-i] [-d] <name>\n";
    exit(1);
}

int main(int argc, char **argv) {
    // defaults
    int num_iterations  = 1;
    bool keep_time      = true;
    bool make_ckpt      = false;
    bool wait           = false;
    bool stdio          = false;
    bool data           = false;
    const char *fs      = "m3fs";
    const char *prefix  = "";
    const char *loadgen = "";
    capsel_t rgate      = ObjCap::INVALID;
    epid_t rgate_ep     = EP_COUNT;

    int opt;
    while((opt = CmdArgs::get(argc, argv, "p:n:wf:g:l:id")) != -1) {
        switch(opt) {
            case 'p': prefix = CmdArgs::arg; break;
            case 'n': num_iterations = IStringStream::read_from<size_t>(CmdArgs::arg); break;
            case 'w': wait = true; break;
            case 'f': fs = CmdArgs::arg; break;
            case 'l': loadgen = CmdArgs::arg; break;
            case 'i': stdio = true; break;
            case 'd': data = true; break;
            case 'g': {
                String input(CmdArgs::arg);
                IStringStream is(input);
                is >> rgate >> rgate_ep;
                break;
            }
            default:
                usage(argv[0]);
        }
    }
    if(CmdArgs::ind >= argc)
        usage(argv[0]);

    Platform::init(argc, argv, loadgen);

    if(VFS::mount("/", "m3fs", fs) != Errors::NONE) {
        if(Errors::last != Errors::EXISTS)
            PANIC("Unable to mount root filesystem " << fs);
    }

    if(*prefix)
        VFS::mkdir(prefix, 0755);

    // pass some EP caps to m3fs (required for FILE_NOSESS)
    epid_t eps = VPE::self().alloc_ep();
    if(eps == EP_COUNT)
        PANIC("Unable to allocate EPs for meta session");
    for(uint i = 1; i < META_EPS; ++i) {
        if(VPE::self().alloc_ep() != eps + i)
           PANIC("Unable to allocate EPs for meta session");
    }
    if(VFS::delegate_eps("/", VPE::self().ep_to_sel(eps), META_EPS) != Errors::NONE)
        PANIC("Unable to delegate EPs to meta session");

    TracePlayer player(prefix);

    Trace *trace = Traces::get(argv[CmdArgs::ind]);
    if(!trace)
        PANIC("Trace '" << argv[CmdArgs::ind] << "' does not exist.");

    // touch all operations to make sure we don't get pagefaults in trace_ops arrary
    unsigned int numTraceOps = 0;
    trace_op_t *op = trace->trace_ops;
    while (op && op->opcode != INVALID_OP) {
        op++;
        if (op->opcode != WAITUNTIL_OP)
            numTraceOps++;
    }

    if(rgate != ObjCap::INVALID) {
        RecvGate rg = RecvGate::bind(rgate, 6, rgate_ep);
        {
            // tell the coordinator, that we are ready
            GateIStream msg = receive_msg(rg);
            reply_vmsg(msg, 1);
        }
        // wait until we should start
        receive_msg(rg);
    }

    // print parameters for reference
    cerr << "VPFS trace_bench started ["
         << "trace=" << argv[CmdArgs::ind] << ","
         << "n=" << num_iterations << ","
         << "wait=" << (wait ? "yes" : "no") << ","
         << "data=" << (data ? "yes" : "no") << ","
         << "stdio=" << (stdio ? "yes" : "no") << ","
         << "prefix=" << prefix << ","
         << "fs=" << fs << ","
         << "loadgen=" << loadgen << ","
         << "ops=" << numTraceOps
         << "]\n";

    for(int i = 0; i < num_iterations; ++i) {
        player.play(trace, wait, data, stdio, keep_time, make_ckpt);
        if(i + 1 < num_iterations)
            cleanup();
    }

    cerr << "VPFS trace_bench benchmark terminated\n";

    // done
    Platform::shutdown();
    return 0;
}
