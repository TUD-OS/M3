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

#include <thread/ThreadManager.h>

#include <cstring>

#include "cap/CapTable.h"
#include "cap/Capability.h"
#include "mem/AddrSpace.h"
#include "mem/SlabCache.h"
#include "pes/VPEDesc.h"
#include "DTUState.h"
#include "Types.h"

namespace kernel {

class ContextSwitcher;
class PEManager;
class VPECapability;
class VPEManager;

class VPE : public m3::SListItem, public SlabObject<VPE> {
    friend class ContextSwitcher;
    friend class PEManager;
    friend class VPECapability;
    friend class VPEManager;

    struct ServName : public m3::SListItem {
        explicit ServName(const m3::String &_name) : name(_name) {
        }
        m3::String name;
    };

public:
    static const uint16_t INVALID_ID    = 0xFFFF;

    static const cycles_t TIME_SLICE    = 1000000;

    static const int SYSC_MSGSIZE_ORD   = m3::nextlog2<512>::val;
    static const int SYSC_CREDIT_ORD    = SYSC_MSGSIZE_ORD;
    static const int NOTIFY_MSGSIZE_ORD = m3::nextlog2<64>::val;

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
        F_HASAPP      = 1 << 4,
        F_MUXABLE     = 1 << 5, // TODO temporary
        F_READY       = 1 << 6,
        F_WAITING     = 1 << 7,
    };

    explicit VPE(m3::String &&prog, peid_t peid, vpeid_t id, uint flags, epid_t ep = -1,
        capsel_t pfgate = m3::KIF::INV_SEL);
    VPE(const VPE &) = delete;
    VPE &operator=(const VPE &) = delete;
    ~VPE();

    vpeid_t id() const {
        return desc().id;
    }
    const m3::String &name() const {
        return _name;
    }

    const VPEDesc &desc() const {
        return _desc;
    }
    peid_t pe() const {
        return desc().pe;
    }
    void set_pe(peid_t pe) {
        _desc.pe = pe;
    }

    int pid() const {
        return _pid;
    }
    void set_pid(int pid) {
        _pid = pid;
    }

    bool is_daemon() const {
        return _flags & F_DAEMON;
    }
    bool has_app() const {
        return _flags & F_HASAPP;
    }
    State state() const {
        return _state;
    }

    AddrSpace *address_space() {
        return _as;
    }

    uintptr_t ep_addr() const {
        return _epaddr;
    }
    void set_ep_addr(uintptr_t addr) {
        _epaddr = addr;
        init();
    }

    Capability *ep_cap(epid_t ep) {
        return _epcaps[ep - m3::DTU::FIRST_FREE_EP];
    }
    void ep_cap(epid_t ep, Capability *cap) {
        _epcaps[ep - m3::DTU::FIRST_FREE_EP] = cap;
    }
    epid_t cap_ep(Capability *cap) {
        for(size_t i = 0; i < ARRAY_SIZE(_epcaps); ++i) {
            if(_epcaps[i] == cap)
                return i + m3::DTU::FIRST_FREE_EP;
        }
        return 0;
    }

    int exitcode() const {
        return _exitcode;
    }
    void wait_for_exit();

    bool is_waiting() const {
        return _flags & F_WAITING;
    }
    void start_wait() {
        assert(!(_flags & F_WAITING));
        _flags |= F_WAITING;
    }
    void stop_wait() {
        assert(_flags & F_WAITING);
        _flags &= ~F_WAITING;
    }

    CapTable &objcaps() {
        return _objcaps;
    }
    CapTable &mapcaps() {
        return _mapcaps;
    }

    SendGate &upcall_sgate() {
        return _upcsgate;
    }

    SendQueue &upcall_queue() {
        return _upcqueue;
    }

    void upcall(const void *msg, size_t size, bool onheap) {
        _upcqueue.send(&_upcsgate, msg, size, onheap);
    }
    void upcall_notify(m3::Errors::Code res, word_t event);

    void add_forward() {
        _pending_fwds++;
    }
    void rem_forward() {
        _pending_fwds--;
    }

    void start_app();
    void stop_app();
    void exit_app(int exitcode);

    void yield();

    bool migrate();
    bool resume(bool need_app = true, bool unblock = true);
    void wakeup();

    void invalidate_ep(epid_t ep);

    bool can_forward_msg(epid_t ep);
    void forward_msg(epid_t ep, peid_t pe, vpeid_t vpe);
    void forward_mem(epid_t ep, peid_t pe);

    m3::Errors::Code config_rcv_ep(epid_t ep, const RBufObject &obj);
    void config_snd_ep(epid_t ep, const MsgObject &obj);
    void config_mem_ep(epid_t ep, const MemObject &obj);

    void make_daemon();

    const m3::SList<ServName> &requirements() const {
        return _requires;
    }
    void add_requirement(const m3::String &name) {
        _requires.append(new ServName(name));
    }

    void set_args(size_t argc, char **argv) {
        _argc = argc;
        _argv = argv;
    }

private:
    int refcount() const {
        return _refs;
    }
    void ref() {
        _refs++;
    }
    void unref();

    void init();
    void init_memory();
    void load_app();

    void update_ep(epid_t ep);

    void notify_resume();

    void write_env_file(int pid, label_t label, epid_t ep);
    void activate_sysc_ep(void *addr);

    void free_reqs();

    VPEDesc _desc;
    uint _flags;
    int _refs;
    int _pid;
    State _state;
    int _exitcode;
    uint _pending_fwds;
    m3::String _name;
    CapTable _objcaps;
    CapTable _mapcaps;
    uint64_t _lastsched;
    Capability *_epcaps[EP_COUNT - m3::DTU::FIRST_FREE_EP];
    alignas(DTU_PKG_SIZE) DTUState _dtustate;
    SendGate _upcsgate;
    SendQueue _upcqueue;
    AddrSpace *_as;
    m3::SList<ServName> _requires;
    size_t _argc;
    char **_argv;
    uintptr_t _epaddr;
};

}
