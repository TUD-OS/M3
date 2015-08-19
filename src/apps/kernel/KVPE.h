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

#pragma once

#include <m3/util/Util.h>
#include <m3/util/SList.h>
#include <m3/server/RequestHandler.h>
#include <m3/cap/MemGate.h>
#include <m3/Log.h>
#include <cstring>

#include "CapTable.h"
#include "RecvBufs.h"

namespace m3 {

class KVPE : public RequestSessionData {
public:
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

    explicit KVPE(String &&prog, int argc, char **argv, size_t id);
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
    void exit(int exitcode) {
        _state = DEAD;
        _exitcode = exitcode;
        for(auto it = _exitsubscr.begin(); it != _exitsubscr.end();) {
            auto cur = it++;
            cur->callback(exitcode, &*cur);
            _exitsubscr.unsubscribe(&*cur);
        }
    }

    void activate_sysc_chan(void *addr);
    Errors::Code xchg_chan(size_t cid, MsgCapability *oldcapobj, MsgCapability *newcapobj);

    int id() const {
        return _id;
    }
    int pid() const {
        return _pid;
    }
    int core() const {
        return _id + APP_CORES;
    }
    int state() const {
        return _state;
    }
    int exitcode() const {
        return _exitcode;
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
    const String &name() const {
        return _name;
    }
    CapTable &capabilities() {
        return _caps;
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

private:
    void write_env_file(int pid, label_t label, size_t cid);
    void activate_sysc_chan();

    void handle_args(int argc, char **argv) {
        for(int i = 0; i < argc; ++i) {
            if(strncmp(argv[i], "requires=", sizeof("requires=") - 1) == 0)
                _requires.append(new ServName(String(argv[i] + sizeof("requires=") - 1)));
        }
    }
    void free_deps() {
        for(auto it = _requires.begin(); it != _requires.end(); ) {
            auto old = it++;
            delete &*old;
        }
    }
    void detach_rbufs() {
        for(size_t c = 0; c < CHAN_COUNT; ++c)
            RecvBufs::detach(core(), c);
    }

    size_t _id;
    int _refs;
    int _pid;
    int _state;
    int _exitcode;
    String _name;
    CapTable _caps;
    MemGate _sepsgate;
    RecvGate _syscgate;
    RecvGate _srvgate;
    SList<ServName> _requires;
    Subscriptions<int> _exitsubscr;
};

}
