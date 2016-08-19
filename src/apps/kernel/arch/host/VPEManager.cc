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

#include <sys/types.h>
#include <sys/wait.h>
#include <sstream>
#include <csignal>
#include <unistd.h>

#include "pes/VPEManager.h"
#include "Platform.h"

namespace kernel {

VPEManager::~VPEManager() {
    for(size_t i = 0; i < MAX_VPES; ++i) {
        if(_vpes[i]) {
            kill(_vpes[i]->pid(), SIGTERM);
            waitpid(_vpes[i]->pid(), nullptr, 0);
            _vpes[i]->unref();
        }
    }
}

m3::String VPEManager::fork_name(const m3::String &name) {
    char buf[256];
    m3::OStringStream nname(buf, sizeof(buf));
    size_t pos = strrchr(name.c_str(), '-') - name.c_str();
    nname << m3::fmt(name.c_str(), 1, pos) << '-' << rand();
    return nname.str();
}

}
