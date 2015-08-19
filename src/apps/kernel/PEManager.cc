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

#include <m3/stream/OStringStream.h>
#include <string.h>

#include "PEManager.h"

namespace m3 {

PEManager *PEManager::_inst;

PEManager::PEManager(int argc, char **argv)
        : _petype(), _vpes(), _count(), _daemons() {
    size_t no = 0;
    for(int i = 0; i < argc; ++i) {
        if(strcmp(argv[i], "--") == 0)
            continue;

        // find end of arguments
        bool daemon = false;
        int end = i + 1;
        for(; end < argc; ++end) {
            if(strncmp(argv[end], "core=", 5) == 0)
                _petype[no] = argv[end] + 5;
            else if(strcmp(argv[end], "daemon") == 0)
                daemon = true;
            else if(strcmp(argv[end], "--") == 0)
                break;
        }

        // for idle, don't create a VPE
        if(strcmp(argv[i], "idle") != 0) {
            // allow multiple applications with the same name
            OStringStream name;
            name << path_to_name(String(argv[i]), nullptr).c_str() << "-" << no;
            _vpes[no] = new KVPE(String(name.str()), end - i, argv + i, no);
            if(_vpes[no]->requirements().length() == 0)
                _vpes[no]->start(end - i, argv + i, 0);
            else
                _pending.append(new Pending(_vpes[no], end - i, argv + i));
            _count++;
        }

        no++;
        i = end;
        if(daemon)
            _daemons++;
    }
}

void PEManager::shutdown() {
    ServiceList &serv = ServiceList::get();
    for(auto &s : serv) {
        Reference<Service> ref(&s);
        AutoGateOStream msg(ostreamsize<SyscallHandler::server_type::Command>());
        msg << m3::SyscallHandler::server_type::SHUTDOWN;
        serv.send_and_receive(ref, msg.bytes(), msg.total());
        msg.claim();
    }
}

String PEManager::path_to_name(const String &path, const char *suffix) {
    static char name[64];
    strncpy(name, path.c_str(), sizeof(name));
    name[sizeof(name) - 1] = '\0';
    OStringStream os;
    char *start = name;
    size_t len = strlen(name);
    for(ssize_t i = len - 1; i >= 0; --i) {
        if(name[i] == '/') {
            start = name + i + 1;
            break;
        }
    }

    os << start;
    if(suffix)
        os << "-" << suffix;
    return os.str();
}

bool PEManager::core_matches(size_t i, const char *core) const {
    if(core == NULL)
        return _petype[i] == NULL;
    if(_petype[i])
        return strcmp(_petype[i], core) == 0;
    return strcmp(core, "default") == 0;
}

KVPE *PEManager::create(String &&name, const char *core) {
    if(_count == AVAIL_PES)
        return nullptr;

    size_t i;
    for(i = 0; i < AVAIL_PES; ++i) {
        if(_vpes[i] == nullptr && core_matches(i, core))
            break;
    }
    if(i == AVAIL_PES)
        return nullptr;

    _vpes[i] = new KVPE(std::move(name), 0, nullptr, i);
    _count++;
    return _vpes[i];
}

}
