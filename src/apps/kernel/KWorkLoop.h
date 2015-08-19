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

#pragma once

#include <m3/Common.h>
#include <m3/WorkLoop.h>

#include "SyscallHandler.h"

#if defined(__host__)
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>

static int sigchilds = 0;

static void sigchild(int) {
    sigchilds++;
    signal(SIGCHLD, sigchild);
}

static void check_childs() {
    for(; sigchilds > 0; sigchilds--) {
        int status;
        pid_t pid = wait(&status);
        if(WIFEXITED(status)) {
            LOG(SYSC, "Child " << pid << " exited with status " << WEXITSTATUS(status));
        }
        else if(WIFSIGNALED(status)) {
            LOG(SYSC, "Child " << pid << " was killed by signal " << WTERMSIG(status));
        }
    }
}
#endif

namespace m3 {

class KWorkLoop {
    KWorkLoop() = delete;

public:
    static void run() {
#if defined(__host__)
        signal(SIGCHLD, sigchild);
#endif

        WorkLoop &wl = WorkLoop::get();
        ChanMng &chmng = ChanMng::get();
        SyscallHandler &sysch = SyscallHandler::get();
        int chan = sysch.chanid();
        int srvchan = sysch.srvchanid();
        while(wl.has_items()) {
            if(chmng.fetch_msg(chan)) {
                // we know the subscriber here, so optimize that a bit
                ChanMng::Message *msg = chmng.message(chan);
                RecvGate *rgate = reinterpret_cast<RecvGate*>(msg->label);
                sysch.handle_message(*rgate, nullptr);
                chmng.ack_message(chan);
            }
            if(chmng.fetch_msg(srvchan))
                chmng.notify(srvchan);

#if defined(__host__)
            check_childs();
#endif
        }
    }
};

}
