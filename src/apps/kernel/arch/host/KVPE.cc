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

#include <m3/Log.h>

#include "../../PEManager.h"
#include "../../SyscallHandler.h"
#include "../../KVPE.h"

#include <unistd.h>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cerrno>

namespace m3 {

void KVPE::start(int argc, char **argv, int pid) {
    // when exiting, the program will release one reference
    ref();
    _state = RUNNING;
    if(pid == 0) {
        _pid = fork();
        if(_pid < 0)
            PANIC("fork");
        if(_pid == 0) {
            write_env_file(getpid(), _syscgate.label(), SyscallHandler::get().epid());
            char **childargs = new char*[argc + 1];
            int i = 0, j = 0;
            for(; i < argc; ++i) {
                if(strncmp(argv[i], "core=", 5) == 0)
                    continue;
                else if(strcmp(argv[i], "daemon") == 0)
                    continue;
                else if(strncmp(argv[i], "requires=", sizeof("requires=") - 1) == 0)
                    continue;

                childargs[j++] = argv[i];
            }
            childargs[j] = nullptr;
            execv(childargs[0], childargs);
            LOG(VPES, "VPE creation failed: " << strerror(errno));
        }
        else
            LOG(VPES, "Started VPE '" << _name << "' [pid=" << _pid << "]");
    }
    else {
        _pid = pid;
        write_env_file(_pid, _syscgate.label(), SyscallHandler::get().epid());
        LOG(VPES, "Started VPE '" << _name << "' [pid=" << _pid << "]");
    }
}

void KVPE::activate_sysc_ep(void *addr) {
    uintptr_t iaddr = reinterpret_cast<uintptr_t>(addr);
    // if we execute multiple programs in a VPE in a row, we already have our memory-cap
    MemCapability *mcap = static_cast<MemCapability*>(
        CapTable::kernel_table().get(_sepsgate.sel(), Capability::MEM));
    if(mcap == nullptr) {
        size_t len = DTU::EPS_RCNT * EP_COUNT * sizeof(word_t);
        mcap = new MemCapability(&CapTable::kernel_table(), _sepsgate.sel(),
            iaddr, len, MemGate::X | MemGate::W, core(), id(), 0);
        CapTable::kernel_table().set(_sepsgate.sel(), mcap);
    }
    else
        mcap->obj->label = iaddr | MemGate::X | MemGate::W;
}

void KVPE::write_env_file(pid_t pid, label_t label, size_t epid) {
    char tmpfile[64];
    snprintf(tmpfile, sizeof(tmpfile), "/tmp/m3/%d", pid);
    std::ofstream of(tmpfile);
    of << Config::get().shm_prefix().c_str() << "\n";
    of << core() << "\n";
    of << label << "\n";
    of << epid << "\n";
    of << (1 << SYSC_CREDIT_ORD) << "\n";
}

Errors::Code KVPE::xchg_ep(size_t epid, MsgCapability *oldcapobj, MsgCapability *newcapobj) {
    // set registers for caps
    word_t regs[DTU::EPS_RCNT * 2];
    memset(regs, 0, sizeof(regs));
    MsgCapability *co[] = {oldcapobj, newcapobj};
    for(size_t i = 0; i < 2; ++i) {
        if(co[i]) {
            DTU::get().configure(regs, i, co[i]->obj->label, co[i]->obj->core,
                co[i]->obj->epid, co[i]->obj->credits);
        }
    }

    if(newcapobj) {
        // now do the compare-exchange
        return seps_gate().cmpxchg_sync(regs, sizeof(regs), epid * DTU::EPS_RCNT * sizeof(word_t));
    }

    // if we should just invalidate it, we don't have to do a cmpxchg
    return seps_gate().write_sync(regs + DTU::EPS_RCNT,
        sizeof(regs) / 2, epid * DTU::EPS_RCNT * sizeof(word_t));
}

KVPE::~KVPE() {
    LOG(VPES, "Deleting VPE '" << _name << "' [id=" << id() << "]");
    SyscallHandler::get().remove_session(this);
    detach_rbufs();
    free_reqs();

    // revoke all caps first because we might need the sepsgate for that
    _objcaps.revoke_all();
    // now remove the sepsgate from our cap-table
    CapTable::kernel_table().unset(_sepsgate.sel());
}

}
