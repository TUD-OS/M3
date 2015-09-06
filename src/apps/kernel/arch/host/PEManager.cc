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

#include "../../PEManager.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <sstream>
#include <csignal>
#include <unistd.h>

namespace m3 {

PEManager::~PEManager() {
    for(size_t i = 0; i < AVAIL_PES; ++i) {
        if(_vpes[i]) {
            kill(_vpes[i]->pid(), SIGTERM);
            waitpid(_vpes[i]->pid(), nullptr, 0);
            _vpes[i]->unref();
        }
    }
}

String PEManager::fork_name(const String &name) {
    char buf[256];
    OStringStream nname(buf, sizeof(buf));
    size_t pos = strrchr(name.c_str(), '-') - name.c_str();
    nname << fmt(name.c_str(), 1, pos) << '-' << rand();
    return nname.str();
}

}
