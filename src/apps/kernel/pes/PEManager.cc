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

#include <base/stream/OStringStream.h>

#include <string.h>

#include "pes/PEManager.h"

namespace kernel {

bool PEManager::_shutdown = false;
PEManager *PEManager::_inst;

PEManager::PEManager() : _petype(), _vpes(), _count(), _daemons() {
    deprivilege_pes();
}

void PEManager::load(int argc, char **argv) {
    bool as = false;
#if defined(__gem5__)
    as = true;
#endif

    size_t no = 0;
    for(int i = 0; i < argc; ++i) {
        if(strcmp(argv[i], "--") == 0)
            continue;

        // find next usable PE
        while((PE_MASK & (1 << no)) == 0)
            no++;

        // for idle, don't create a VPE
        if(strcmp(argv[i], "idle") != 0) {
            // allow multiple applications with the same name
            m3::OStringStream name;
            name << path_to_name(m3::String(argv[i]), nullptr).c_str() << "-" << no;
            _vpes[no] = new VPE(m3::String(name.str()), no, true, as);
            _count++;
        }

        // find end of arguments
        bool daemon = false;
        int end = i + 1;
        for(; end < argc; ++end) {
            if(strncmp(argv[end], "core=", 5) == 0)
                _petype[no] = argv[end] + 5;
            else if(strcmp(argv[end], "daemon") == 0) {
                daemon = true;
                _vpes[no]->make_daemon();
            }
            else if(strncmp(argv[end], "requires=", sizeof("requires=") - 1) == 0)
                 _vpes[no]->add_requirement(argv[end] + sizeof("requires=") - 1);
            else if(strcmp(argv[end], "--") == 0)
                break;
        }

        // start it, or register pending item
        if(strcmp(argv[i], "idle") != 0) {
            if(_vpes[no]->requirements().length() == 0)
                _vpes[no]->start(end - i, argv + i, 0);
            else
                _pending.append(new Pending(_vpes[no], end - i, argv + i));
        }

        no++;
        i = end;
        if(daemon)
            _daemons++;
    }
}

void PEManager::start_pending(ServiceList &serv) {
    for(auto it = _pending.begin(); it != _pending.end(); ) {
        bool fullfilled = true;
        for(auto &r : it->vpe->requirements()) {
            if(!serv.find(r.name)) {
                fullfilled = false;
                break;
            }
        }

        if(fullfilled) {
            auto old = it++;
            old->vpe->start(old->argc, old->argv, 0);
            _pending.remove(&*old);
            delete &*old;
        }
        else
            it++;
    }
}

void PEManager::shutdown() {
    if(_shutdown)
        return;

    _shutdown = true;
    ServiceList &serv = ServiceList::get();
    for(auto &s : serv) {
        m3::Reference<Service> ref(&s);
        AutoGateOStream msg(m3::ostreamsize<m3::KIF::Service::Command>());
        msg << m3::KIF::Service::SHUTDOWN;
        serv.send_and_receive(ref, msg.bytes(), msg.total());
        msg.claim();
    }
}

m3::String PEManager::path_to_name(const m3::String &path, const char *suffix) {
    static char name[256];
    strncpy(name, path.c_str(), sizeof(name));
    name[sizeof(name) - 1] = '\0';
    m3::OStringStream os;
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
    if(core == nullptr)
        return _petype[i] == nullptr;
    if(_petype[i])
        return strcmp(_petype[i], core) == 0;
    return strcmp(core, "default") == 0;
}

VPE *PEManager::create(m3::String &&name, const char *core, bool as, int ep, capsel_t pfgate) {
    if(_count == AVAIL_PES)
        return nullptr;

    size_t i;
    for(i = 0; i < AVAIL_PES; ++i) {
        if((PE_MASK & (1 << i)) && _vpes[i] == nullptr && core_matches(i, core))
            break;
    }
    if(i == AVAIL_PES)
        return nullptr;

    _vpes[i] = new VPE(std::move(name), i, false, as, ep, pfgate);
    _count++;
    return _vpes[i];
}

void PEManager::remove(int id, bool daemon) {
    assert(_vpes[id]);
    delete _vpes[id];
    _vpes[id] = nullptr;

    if(daemon) {
        assert(_daemons > 0);
        _daemons--;
    }

    assert(_count > 0);
    _count--;
}

}
