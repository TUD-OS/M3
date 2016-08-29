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
#include "WorkLoop.h"

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
            _vpes[id] = new VPE(m3::String(argv[i]), coreid, id, VPE::F_BOOTMOD);

            coreid++;
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

        // register pending item if necessary
        if(strcmp(argv[i], "idle") != 0 && _vpes[id]->requirements().length() > 0)
            _pending.append(new Pending(_vpes[id], end - i, argv + i));
        else
            _vpes[id]->set_ready();

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
            old->vpe->set_ready();
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

    UNUSED VPE *vpe = new VPE(std::move(name), i, id, 0, ep, pfgate, tmuxable);
    assert(vpe == _vpes[id]);

    _count++;
    return _vpes[id];
}

void VPEManager::start(vpeid_t id) {
    assert(_vpes[id]);

    PEManager::get().start_vpe(_vpes[id]);
}

void VPEManager::resume(vpeid_t id, const m3::Subscriptions<bool>::callback_type &cb) {
    assert(_vpes[id]);

    _vpes[id]->subscribe_resume(cb);
    PEManager::get().start_switch(_vpes[id]->core());
}

void VPEManager::remove(vpeid_t id) {
    assert(_vpes[id]);

    uint flags = _vpes[id]->flags();

    delete _vpes[id];
    _vpes[id] = nullptr;

    if(flags & VPE::F_DAEMON) {
        assert(_daemons > 0);
        _daemons--;
    }

    if(~flags & VPE::F_IDLE) {
        assert(_count > 0);
        _count--;
    }

    // if there are no VPEs left, we can stop everything
    if(used() == 0) {
        destroy();
        m3::env()->workloop()->stop();
    }
    // if there are only daemons left, start the shutdown-procedure
    else if(used() == daemons())
        shutdown();
}

}