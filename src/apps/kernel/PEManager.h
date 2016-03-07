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

#include <m3/WorkLoop.h>
#include <m3/stream/OStringStream.h>

#include "MainMemory.h"
#include "SyscallHandler.h"
#include "KVPE.h"
#include "KDTU.h"

namespace m3 {

class KVPE;

class PEManager {
    friend class KVPE;

    struct Pending : public SListItem {
        explicit Pending(KVPE *_vpe, int _argc, char **_argv)
            : vpe(_vpe), argc(_argc), argv(_argv) {
        }

        KVPE *vpe;
        int argc;
        char **argv;
    };

public:
    static void create() {
        _inst = new PEManager();
    }
    static PEManager &get() {
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
    explicit PEManager();
    ~PEManager();

public:
    void load(int argc, char **argv);

    KVPE *create(String &&name, const char *core, bool as, int ep, capsel_t pfgate);
    void remove(int id, bool daemon);

    const char *type(int id) const {
        return _petype[id];
    }
    size_t used() const {
        return _count;
    }
    size_t daemons() const {
        return _daemons;
    }
    bool exists(int id) {
        return id < (int)AVAIL_PES && _vpes[id];
    }
    KVPE &vpe(int id) {
        assert(_vpes[id]);
        return *_vpes[id];
    }

    void start_pending(ServiceList &serv);

private:
    void deprivilege_pes() {
        for(int i = 0; i < AVAIL_PES; ++i) {
            if(PE_MASK & (1 << i))
                KDTU::get().deprivilege(APP_CORES + i);
        }
    }

    bool core_matches(size_t i, const char *core) const;
    static m3::String path_to_name(const m3::String &path, const char *suffix);
    static m3::String fork_name(const m3::String &name);

    const char *_petype[AVAIL_PES];
    KVPE *_vpes[AVAIL_PES];
    size_t _count;
    size_t _daemons;
    SList<Pending> _pending;
    static bool _shutdown;
    static PEManager *_inst;
};

}
