/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include "pes/PEManager.h"
#include "pes/VPEManager.h"
#include "Platform.h"
#include "WorkLoop.h"

namespace kernel {

bool VPEManager::_shutdown = false;
VPEManager *VPEManager::_inst;

VPEManager::VPEManager()
    : _next_id(0),
      _vpes(new VPE*[MAX_VPES]()),
      _count(),
      _daemons(),
      _pending() {
}

void VPEManager::init(int argc, char **argv) {
    // TODO the required PE depends on the boot module, not the kernel PE
    m3::PEDesc pedesc = Platform::pe(Platform::kernel_pe());
    m3::PEDesc pedesc_emem(m3::PEType::COMP_EMEM, pedesc.isa(), pedesc.mem_size());
    m3::PEDesc pedesc_imem(m3::PEType::COMP_IMEM, pedesc.isa(), pedesc.mem_size());

    for(int i = 0; i < argc; ++i) {
        if(strcmp(argv[i], "--") == 0)
            continue;

        vpeid_t id = get_id();
        assert(id != MAX_VPES);

        // for idle, don't create a VPE
        if(strcmp(argv[i], "idle")) {
            // try to find a PE with the required ISA and external memory first
            peid_t peid = PEManager::get().find_pe(pedesc_emem, 0, 0, nullptr);
            if(peid == 0) {
                // if that failed, try to find a SPM PE
                peid = PEManager::get().find_pe(pedesc_imem, 0, 0, nullptr);
                if(peid == 0)
                    PANIC("Unable to find a free PE for boot module " << argv[i]);
            }

            // strip the path from the name
            const char *slash = strrchr(argv[i], '/');
            const char *name = slash ? slash + 1 : argv[i];
            _vpes[id] = new VPE(m3::String(name), peid, id, VPE::F_BOOTMOD);
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
                const char *req = argv[j] + sizeof("requires=") - 1;
                karg = true;
#if !defined(__gem5__)
                if(strcmp(req, "pager") == 0)
                    continue;
#endif
                _vpes[id]->add_requirement(req);
            }
            else if(strcmp(argv[j], "--") == 0)
                break;
            else if(karg)
                PANIC("Kernel argument before program argument");
            else
                end++;
        }

        // remember arguments
        _vpes[id]->set_args(static_cast<size_t>(end - i), argv + i);

        // register pending item if necessary
        if(strcmp(argv[i], "idle") != 0 && _vpes[id]->requirements().length() > 0)
            _pending.append(new Pending(_vpes[id]));
        else
            _vpes[id]->start_app(_vpes[id]->pid());

        i = j;
    }
}

void VPEManager::start_pending(const ServiceList &serv) {
    for(auto it = _pending.begin(); it != _pending.end(); ) {
        bool fullfilled = true;
        for(auto &r : it->vpe->requirements()) {
            if(!serv.contains(r.name)) {
                fullfilled = false;
                break;
            }
        }

        if(fullfilled) {
            auto old = it++;
            old->vpe->start_app(old->vpe->pid());
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
        KLOG(SERV, "Sending SHUTDOWN message to " << ref->name());

        m3::KIF::Service::Shutdown msg;
        msg.opcode = m3::KIF::Service::SHUTDOWN;
        ref->send_receive(&msg, sizeof(msg), false);
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

VPE *VPEManager::create(m3::String &&name, const m3::PEDesc &pe, epid_t sep, epid_t rep,
                        capsel_t sgate, uint flags, VPEGroup *group) {
    uint vflags = 0;
    if(flags & m3::KIF::VPEFlags::MUXABLE)
        vflags |= VPE::F_MUXABLE;
    if(flags & m3::KIF::VPEFlags::PINNED)
        vflags |= VPE::F_PINNED;

    peid_t i = PEManager::get().find_pe(pe, 0, vflags, group);
    if(i == 0)
        return nullptr;

    // a pager without virtual memory support, doesn't work
    if(!Platform::pe(i).has_virtmem() && sgate != m3::KIF::INV_SEL)
        return nullptr;

    vpeid_t id = get_id();
    if(id == MAX_VPES)
        return nullptr;

    // groups are implicitly pinned
    if(group)
        vflags |= VPE::F_PINNED;
    VPE *vpe = new VPE(m3::Util::move(name), i, id, vflags, sep, rep, sgate, group);
    assert(vpe == _vpes[id]);

    return vpe;
}

void VPEManager::add(VPE *vpe) {
    _vpes[vpe->id()] = vpe;

    if(~vpe->_flags & VPE::F_IDLE) {
        _count++;
        PEManager::get().add_vpe(vpe);
    }
}

void VPEManager::remove(VPE *vpe) {
    PEManager::get().remove_vpe(vpe);

    // do that afterwards, because some actions in the destructor might try to get the VPE
    _vpes[vpe->id()] = nullptr;

    if(vpe->_flags & VPE::F_IDLE)
        return;

    if(vpe->_flags & VPE::F_DAEMON) {
        assert(_daemons > 0);
        _daemons--;
    }

    assert(_count > 0);
    _count--;

    // if there are no VPEs left, we can stop everything
    if(used() == 0)
        m3::env()->workloop()->stop();
    // if there are only daemons left, start the shutdown-procedure
    else if(used() == daemons())
        shutdown();
}

}
