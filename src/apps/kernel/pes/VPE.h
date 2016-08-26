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

#include <base/col/SList.h>
#include <base/KIF.h>

#include <cstring>

#include "com/RecvBufs.h"
#include "cap/CapTable.h"
#include "cap/Capability.h"
#include "mem/AddrSpace.h"
#include "mem/SlabCache.h"

namespace kernel {

// TODO move that to somewhere else
typedef size_t vpeid_t;

struct VPEDesc {
    explicit VPEDesc(int _core, vpeid_t _id) : core(_core), id(_id) {
    }

    int core;
    vpeid_t id;
};

class ContextSwitcher;
class RecvBufs;

class VPE : public SlabObject<VPE> {
    friend class ContextSwitcher;

public:
    static const uint16_t INVALID_ID = 0xFFFF;

    enum State {
        RUNNING,
        SUSPENDED,
        DEAD
    };

    enum Flags {
        F_BOOTMOD     = 1 << 0,
        F_DAEMON      = 1 << 1,
        F_IDLE        = 1 << 2,
        F_INIT        = 1 << 3,
        F_START       = 1 << 4,
    };

    struct ServName : public m3::SListItem {
        explicit ServName(const m3::String &_name) : name(_name) {
        }
        m3::String name;
    };

    static constexpr int SYSC_CREDIT_ORD    = m3::nextlog2<512>::val;

    explicit VPE(m3::String &&prog, int coreid, vpeid_t id, uint flags, int ep = -1,
        capsel_t pfgate = m3::KIF::INV_SEL, bool tmuxable = false);
    VPE(const VPE &) = delete;
    VPE &operator=(const VPE &) = delete;
    ~VPE();

    int refcount() const {
        return _refs;
    }
    void ref() {
        _refs++;
    }
    void unref();

    void start();
    void stop();
    void exit(int exitcode);

    void init();

    m3::DTU::reg_t *dtu_state() {
        return _dtu_state;
    }

    RecvBufs &rbufs() {
        return _rbufs;
    }

    void wakeup();

    void set_ready();

    void activate_sysc_ep(void *addr);
    m3::Errors::Code xchg_ep(size_t epid, MsgCapability *oldcapobj, MsgCapability *newcapobj);

    const VPEDesc &desc() const {
        return _desc;
    }
    vpeid_t id() const {
        return desc().id;
    }
    int pid() const {
        return _pid;
    }
    int core() const {
        return desc().core;
    }
    uint flags() const {
        return _flags;
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
    bool tmuxable() const {
        return _tmuxable;
    }
    void subscribe_exit(const m3::Subscriptions<int>::callback_type &cb) {
        _exitsubscr.subscribe(cb);
    }
    void unsubscribe_exit(m3::Subscriber<int> *sub) {
        _exitsubscr.unsubscribe(sub);
    }
    void subscribe_resume(const m3::Subscriptions<bool>::callback_type &cb) {
        _resumesubscr.subscribe(cb);
    }
    void unsubscribe_resume(m3::Subscriber<bool> *sub) {
        _resumesubscr.unsubscribe(sub);
    }
    const m3::SList<ServName> &requirements() const {
        return _requires;
    }
    void add_requirement(const m3::String &name) {
        _requires.append(new ServName(name));
    }
    const m3::String &name() const {
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
    void *eps() {
        return _eps;
    }
    void make_daemon();

    void invalidate_ep(int ep);
    void invalidate_eps(int first);

    void config_snd_ep(int ep, label_t label, int dstcore, int dstvpe, int dstep, size_t msgsize,
        word_t credits);
    void config_rcv_ep(int ep, uintptr_t buf, uint order, uint msgorder, int flags, bool valid);
    void config_mem_ep(int ep, int dstcore, int dstvpe, uintptr_t addr, size_t size, int perm);

private:
    m3::DTU::reg_t *ep_regs(int ep);

    void notify_resume() {
        // notify subscribers
        for(auto it = _resumesubscr.begin(); it != _resumesubscr.end();) {
            auto cur = it++;
            cur->callback(true, &*cur);
            _resumesubscr.unsubscribe(&*cur);
        }
    }

    void init_memory();
    void load_app(const char *name);

    void write_env_file(int pid, label_t label, size_t epid);
    void activate_sysc_ep();

    void free_reqs() {
        for(auto it = _requires.begin(); it != _requires.end(); ) {
            auto old = it++;
            delete &*old;
        }
    }

    VPEDesc _desc;
    uint _flags;
    int _refs;
    int _pid;
    int _state;
    int _exitcode;
    bool _tmuxable;
    uintptr_t _entry;
    m3::String _name;
    CapTable _objcaps;
    CapTable _mapcaps;
    alignas(DTU_PKG_SIZE) m3::DTU::reg_state_t _dtu_state;
    void *_eps;
    RecvGate _syscgate;
    RecvGate _srvgate;
    RecvBufs _rbufs;
    AddrSpace *_as;
    m3::SList<ServName> _requires;
    m3::Subscriptions<int> _exitsubscr;
    m3::Subscriptions<bool> _resumesubscr;
};

}
