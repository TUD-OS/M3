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

#include <base/stream/OStringStream.h>
#include <base/PEDesc.h>

#include "pes/PEManager.h"
#include "pes/VPE.h"
#include "DTU.h"
#include "Platform.h"
#include "SyscallHandler.h"

namespace kernel {

class VPEManager {
    friend class VPE;

    struct Pending : public m3::SListItem {
        explicit Pending(VPE *_vpe, int _argc, char **_argv)
            : vpe(_vpe), argc(_argc), argv(_argv) {
        }

        VPE *vpe;
        int argc;
        char **argv;
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
    void load(int argc, char **argv);

    VPE *create(m3::String &&name, const m3::PEDesc &pe, int ep, capsel_t pfgate, bool tmuxable = false);
    void remove(vpeid_t id, bool daemon);

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

    void start_pending(ServiceList &serv);

private:
    vpeid_t get_id();

    static m3::String path_to_name(const m3::String &path, const char *suffix);
    static m3::String fork_name(const m3::String &name);

    vpeid_t _next_id;
    VPE **_vpes;
    size_t _count;
    size_t _daemons;
    m3::SList<Pending> _pending;
    static bool _shutdown;
    static VPEManager *_inst;
};

}
