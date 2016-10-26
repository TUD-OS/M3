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

#include <base/log/Kernel.h>
#include <base/Panic.h>

#include <unistd.h>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cerrno>

#include "pes/VPEManager.h"
#include "pes/VPE.h"
#include "SyscallHandler.h"

namespace kernel {

void VPE::init() {
    if(ep_addr() == 0)
        return;

    // configure notify endpoint
    MsgObject mobj(reinterpret_cast<label_t>(&syscall_gate()),
        Platform::kernel_pe(), VPEManager::MAX_VPES, m3::DTU::SYSC_SEP,
        1 << SYSC_MSGSIZE_ORD, 1 << SYSC_MSGSIZE_ORD);
    config_snd_ep(m3::DTU::NOTIFY_SEP, mobj);
}

void VPE::load_app() {
    if(_pid == 0) {
        _pid = fork();
        if(_pid < 0)
            PANIC("fork");
        if(_pid == 0) {
            write_env_file(getpid(), reinterpret_cast<label_t>(&_syscgate),
                SyscallHandler::get().epid());
            char **childargs = new char*[_argc + 1];
            size_t i = 0, j = 0;
            for(; i < _argc; ++i) {
                if(strncmp(_argv[i], "pe=", 5) == 0)
                    continue;
                else if(strcmp(_argv[i], "daemon") == 0)
                    continue;
                else if(strncmp(_argv[i], "requires=", sizeof("requires=") - 1) == 0)
                    continue;

                childargs[j++] = _argv[i];
            }
            childargs[j] = nullptr;
            execv(childargs[0], childargs);
            KLOG(VPES, "VPE creation failed: " << strerror(errno));
            exit(1);
        }
    }
    else {
        write_env_file(_pid, reinterpret_cast<label_t>(&_syscgate),
            SyscallHandler::get().epid());
    }

    KLOG(VPES, "Started VPE '" << _name << "' [pid=" << _pid << "]");
}

void VPE::init_memory() {
    load_app();
}

void VPE::write_env_file(pid_t pid, label_t label, epid_t epid) {
    char tmpfile[64];
    snprintf(tmpfile, sizeof(tmpfile), "/tmp/m3/%d", pid);
    std::ofstream of(tmpfile);
    of << m3::env()->shm_prefix().c_str() << "\n";
    of << pe() << "\n";
    of << label << "\n";
    of << epid << "\n";
    of << (1 << SYSC_CREDIT_ORD) << "\n";
}

VPE::~VPE() {
    KLOG(VPES, "Deleting VPE '" << _name << "' [id=" << id() << "]");
    _state = DEAD;
    free_reqs();
    // revoke all caps first because we might need the sepsgate for that
    _objcaps.revoke_all();
}

}
