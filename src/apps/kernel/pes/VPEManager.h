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

#pragma once

#include <base/PEDesc.h>

#include "pes/VPE.h"
#include "Platform.h"

namespace kernel {

class VPEManager {
    friend class VPE;
    friend class ContextSwitcher;

    struct Pending : public m3::SListItem {
        explicit Pending(VPE *_vpe) : vpe(_vpe) {
        }

        VPE *vpe;
    };

public:
    static const size_t MAX_VPES    = 1024;

    static void create() {
        _inst = new VPEManager();
    }
    static VPEManager &get() {
        return *_inst;
    }
    static void shutdown();
    static void destroy() {
        if(_inst) {
            delete _inst;
            _inst = nullptr;
        }
    }

private:
    explicit VPEManager();
    ~VPEManager();

public:
    void init(int argc, char **argv);

    VPE *create(m3::String &&name, const m3::PEDesc &pe, epid_t sep, epid_t rep,
                capsel_t sgate, uint flags = 0, VPEGroup *group = nullptr);

    void start_pending(ServiceList &serv);

    size_t used() const {
        return _count;
    }
    size_t daemons() const {
        return _daemons;
    }

    bool exists(vpeid_t id) {
        return id < MAX_VPES && _vpes[id];
    }

    VPE &vpe(vpeid_t id) {
        assert(exists(id));
        return *_vpes[id];
    }

    peid_t peof(vpeid_t id) {
        if(id == MAX_VPES)
            return Platform::kernel_pe();
        return vpe(id).pe();
    }

    VPE *vpe_by_pid(int pid);

private:
    vpeid_t get_id();

    void add(VPE *vpe);
    void remove(VPE *vpe);

    vpeid_t _next_id;
    VPE **_vpes;
    size_t _count;
    size_t _daemons;
    m3::SList<Pending> _pending;
    static bool _shutdown;
    static VPEManager *_inst;
};

}
