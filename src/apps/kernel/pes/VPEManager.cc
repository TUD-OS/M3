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
#include <base/log/Kernel.h>
#include <base/Panic.h>

#include <string.h>

#include "pes/PEManager.h"
#include "pes/VPEManager.h"
#include "Platform.h"

namespace kernel {

bool VPEManager::_shutdown = false;
VPEManager *VPEManager::_inst;

VPEManager::VPEManager()
    : _next_id(0), _vpes(new VPE*[MAX_VPES]()), _count(), _daemons(), _pending() {
}

void VPEManager::load(int argc, char **argv) {
    int coreid = Platform::first_pe();
    for(int i = 0; i < argc; ++i) {
        if(strcmp(argv[i], "--") == 0)
            continue;

        vpeid_t id = get_id();
        assert(id != MAX_VPES);

        // for idle, don't create a VPE
        if(strcmp(argv[i], "idle")) {
            // allow multiple applications with the same name
            m3::OStringStream name;
            name << path_to_name(m3::String(argv[i]), nullptr).c_str() << "-" << coreid;
            _vpes[id] = new VPE(m3::String(name.str()), coreid, id, true);

            PEManager::get().add_vpe(coreid, _vpes[id]);
            _count++;

#if defined(__t3__)
            // VPEs started in t3 simulator are already running when loaded
            // via commandline, thus suspend them temporarily
            // FIXME: this feels like a dirty hack to me
            _vpes[id]->resume();
#endif
        }

        // find end of arguments
        bool karg = false;
        int j = i + 1, end = i + 1;
        for(; j < argc; ++j) {
            if(strcmp(argv[j], "daemon") == 0) {
                _vpes[id]->make_daemon();
                karg = true;
            }
            else if(strncmp(argv[j], "requires=", sizeof("requires=") - 1) == 0) {
                 _vpes[id]->add_requirement(argv[j] + sizeof("requires=") - 1);
                karg = true;
            }
            else if(strcmp(argv[j], "--") == 0)
                break;
            else if(karg)
                PANIC("Kernel argument before program argument");
            else
                end++;
        }

        // start it, or register pending item
        if(strcmp(argv[i], "idle") != 0) {
            if(_vpes[id]->requirements().length() == 0)
                _vpes[id]->start(end - i, argv + i, 0);
            else
                _pending.append(new Pending(_vpes[id], end - i, argv + i));
            coreid++;
        }

        i = j;
    }
}

void VPEManager::start_pending(ServiceList &serv) {
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

void VPEManager::shutdown() {
    if(_shutdown)
        return;

    _shutdown = true;
    ServiceList &serv = ServiceList::get();
    for(auto &s : serv) {
        m3::Reference<Service> ref(&s);
        AutoGateOStream msg(m3::ostreamsize<m3::KIF::Service::Command>());
        msg << m3::KIF::Service::SHUTDOWN;
        KLOG(SERV, "Sending SHUTDOWN message to " << ref->name());
        serv.send_and_receive(ref, msg.bytes(), msg.total());
        msg.claim();
    }
}

m3::String VPEManager::path_to_name(const m3::String &path, const char *suffix) {
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

vpeid_t VPEManager::get_id() {
    vpeid_t id = _next_id;
    for(; id < MAX_VPES && _vpes[id] != nullptr; ++id)
        ;
    if(id == MAX_VPES) {
        for(id = 0; id < MAX_VPES && _vpes[id] != nullptr; ++id)
            ;
    }
    if(id == MAX_VPES)
        return MAX_VPES;
    _next_id = id + 1;
    return id;
}

VPE *VPEManager::create(m3::String &&name, const m3::PEDesc &pe, int ep, capsel_t pfgate, bool tmuxable) {
    int i = PEManager::get().find_pe(pe, tmuxable);
    if(i == 0)
        return nullptr;

    // a pager without virtual memory support, doesn't work
    if(!Platform::pe(i).has_virtmem() && pfgate != m3::KIF::INV_SEL)
        return nullptr;

    vpeid_t id = get_id();
    if(id == MAX_VPES)
        return nullptr;

    _vpes[id] = new VPE(std::move(name), i, id, false, ep, pfgate, tmuxable);

    PEManager::get().add_vpe(i, _vpes[id]);

    _count++;
    return _vpes[id];
}

void VPEManager::remove(vpeid_t id, bool daemon) {
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
