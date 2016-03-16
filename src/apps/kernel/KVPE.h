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

#include <m3/col/SList.h>
#include <m3/cap/MemGate.h>
#include <m3/Log.h>
#include <cstring>

#include "CapTable.h"
#include "AddrSpace.h"

namespace m3 {

class KVPE {
    // use an object to set the VPE id at first and unset it at last
    struct VPEId {
        VPEId(int _id, int _core);
        ~VPEId();

        int id;
        int core;
    };

public:
    static const uint16_t INVALID_ID = 0xFFFF;

    enum State {
        RUNNING,
        DEAD
    };

    struct ServName : public SListItem {
        explicit ServName(const String &_name) : name(_name) {
        }
        String name;
    };

    static constexpr int SYSC_CREDIT_ORD    = nextlog2<512>::val;

    explicit KVPE(String &&prog, size_t id, bool bootmod, bool as, int ep = -1,
        capsel_t pfgate = ObjCap::INVALID);
    KVPE(const KVPE &) = delete;
    KVPE &operator=(const KVPE &) = delete;
    ~KVPE();

    int refcount() const {
        return _refs;
    }
    void ref() {
        _refs++;
    }
    void unref();

    void start(int argc, char **argv, int pid);
    void exit(int exitcode);

    void init();
    void activate_sysc_ep(void *addr);
    Errors::Code xchg_ep(size_t epid, MsgCapability *oldcapobj, MsgCapability *newcapobj);

    int id() const {
        return _id.id;
    }
    int pid() const {
        return _pid;
    }
    int core() const {
        return _id.core;
    }
    int state() const {
        return _state;
    }
    int exitcode() const {
        return _exitcode;
    }
    AddrSpace *address_space() {
        return _as;
    }
    void subscribe_exit(const Subscriptions<int>::callback_type &cb) {
        _exitsubscr.subscribe(cb);
    }
    void unsubscribe_exit(Subscriber<int> *sub) {
        _exitsubscr.unsubscribe(sub);
    }
    const SList<ServName> &requirements() const {
        return _requires;
    }
    void add_requirement(const String &name) {
        _requires.append(new ServName(name));
    }
    const String &name() const {
        return _name;
    }
    CapTable &objcaps() {
        return _objcaps;
    }
    CapTable &mapcaps() {
        return _mapcaps;
    }
    RecvGate &syscall_gate() {
        return _syscgate;
    }
    RecvGate &service_gate() {
        return _srvgate;
    }
    MemGate &seps_gate() {
        return _sepsgate;
    }
    void make_daemon() {
        _daemon = true;
    }

private:
    void init_memory(const char *name);
    void write_env_file(int pid, label_t label, size_t epid);
    void activate_sysc_ep();

    void free_reqs() {
        for(auto it = _requires.begin(); it != _requires.end(); ) {
            auto old = it++;
            delete &*old;
        }
    }
    void detach_rbufs();

    VPEId _id;
    bool _daemon;
    bool _bootmod;
    int _refs;
    int _pid;
    int _state;
    int _exitcode;
    String _name;
    CapTable _objcaps;
    CapTable _mapcaps;
    MemGate _sepsgate;
    RecvGate _syscgate;
    RecvGate _srvgate;
    AddrSpace *_as;
    SList<ServName> _requires;
    Subscriptions<int> _exitsubscr;
};

}
