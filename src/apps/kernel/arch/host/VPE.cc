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
    if(eps() == 0)
        return;

    // configure notify endpoint
    config_snd_ep(m3::DTU::NOTIFY_SEP, reinterpret_cast<label_t>(&syscall_gate()),
        Platform::kernel_pe(), VPEManager::MAX_VPES,
        m3::DTU::NOTIFY_SEP, 1 << NOTIFY_MSGSIZE_ORD, -1);
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

m3::Errors::Code VPE::xchg_ep(epid_t epid, MsgCapability *oldcapobj, MsgCapability *newcapobj) {
    // there is no point in writing to the EPs, if we are already destructing the VPE
    if(_state == DEAD)
        return m3::Errors::NO_ERROR;

    // set registers for caps
    word_t regs[m3::DTU::EPS_RCNT * 2];
    memset(regs, 0, sizeof(regs));
    MsgCapability *co[] = {oldcapobj, newcapobj};
    for(size_t i = 0; i < 2; ++i) {
        if(co[i]) {
            m3::DTU::get().configure(regs, i, co[i]->obj->label, co[i]->obj->pe,
                co[i]->obj->epid, co[i]->obj->credits);
        }
    }

    KLOG(EPS, "Setting ep " << epid << " of VPE " << id() << " to "
        << (newcapobj ? newcapobj->sel() : -1));

    if(newcapobj) {
        // now do the compare-exchange
        DTU::get().cmpxchg_mem(desc(), reinterpret_cast<uintptr_t>(_eps), regs, sizeof(regs),
            epid * m3::DTU::EPS_RCNT * sizeof(word_t), sizeof(regs) / 2);
        return m3::Errors::NO_ERROR;
    }

    // if we should just invalidate it, we don't have to do a cmpxchg
    uintptr_t addr = reinterpret_cast<uintptr_t>(_eps) + epid * m3::DTU::EPS_RCNT * sizeof(word_t);
    DTU::get().write_mem(desc(), addr, regs + m3::DTU::EPS_RCNT, sizeof(regs) / 2);
    return m3::Errors::NO_ERROR;
}

VPE::~VPE() {
    KLOG(VPES, "Deleting VPE '" << _name << "' [id=" << id() << "]");
    _state = DEAD;
    free_reqs();
    // revoke all caps first because we might need the sepsgate for that
    _objcaps.revoke_all();
}

}
