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

#include "cap/CapTable.h"
#include "mem/AddrSpace.h"
#include "mem/SlabCache.h"
#include "pes/VPEDesc.h"
#include "pes/VPEGroup.h"
#include "DTUState.h"
#include "Types.h"

namespace kernel {

class ContextSwitcher;
class PEManager;
class VPECapability;
class VPEGroup;
class VPEManager;

class VPE : public m3::SListItem, public SlabObject<VPE>, public m3::RefCounted {
    friend class ContextSwitcher;
    friend class PEManager;
    friend class VPECapability;
    friend class VPEGroup;
    friend class VPEManager;

    struct ServName : public m3::SListItem {
        explicit ServName(const m3::String &_name) : name(_name) {
        }
        m3::String name;
    };

public:
    static const uint16_t INVALID_ID    = 0xFFFF;
    static const epid_t INVALID_EP      = static_cast<epid_t>(-1);

    static cycles_t TIME_SLICE;
    static const cycles_t APP_YIELD     = 20000;
    static const cycles_t SRV_YIELD     = 1;

    static const int SYSC_MSGSIZE_ORD   = m3::nextlog2<512>::val;
    static const int SYSC_CREDIT_ORD    = SYSC_MSGSIZE_ORD;
    static const int NOTIFY_MSGSIZE_ORD = m3::nextlog2<64>::val;

    static void set_timeslice(cycles_t timeslice) {
        TIME_SLICE = timeslice;
    }

    enum State {
        RUNNING,
        SUSPENDED,
        RESUMING,
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
        F_NEEDS_INVAL = 1 << 8,
        F_FLUSHED     = 1 << 9,
    };

    explicit VPE(m3::String &&prog, peid_t peid, vpeid_t id, uint flags, epid_t sep = INVALID_EP,
                 epid_t rep = INVALID_EP, capsel_t sgate = m3::KIF::INV_SEL, VPEGroup *group = nullptr);
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

    bool is_daemon() const {
        return _flags & F_DAEMON;
    }
    bool is_idle() const {
        return _flags & F_IDLE;
    }
    bool has_app() const {
        return _flags & F_HASAPP;
    }
    bool is_on_pe() const {
        return state() == RUNNING || state() == RESUMING;
    }
    State state() const {
        return _state;
    }

    AddrSpace *address_space() {
        return _as;
    }
    const MainMemory::Allocation &recvbuf_copy() const {
        return _rbufcpy;
    }

    goff_t ep_addr() const {
        return _epaddr;
    }
    void set_ep_addr(goff_t addr) {
        _epaddr = addr;
        init();
    }

    int exitcode() const {
        return _exitcode;
    }
    static void wait_for_exit();

    bool is_waiting() const {
        return _flags & F_WAITING;
    }
    void start_wait() {
        assert(!(_flags & F_WAITING));
        _flags |= F_WAITING;
    }
    void stop_wait() {
        assert(_flags & F_WAITING);
        _flags ^= F_WAITING;
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

    cycles_t yield_time() const {
        return _services > 0 || !Platform::pe(pe()).is_programmable() ? SRV_YIELD : APP_YIELD;
    }
    void add_service() {
        _services++;
    }
    void rem_service() {
        _services--;
    }

    void needs_invalidate() {
        _flags |= F_NEEDS_INVAL;
    }
    void flush_cache();

    void start_app(int pid);
    void stop_app(int exitcode, bool self);

    void yield();

    bool migrate();
    bool resume(bool need_app = true, bool unblock = true);
    void wakeup();

    bool invalidate_ep(epid_t ep, bool cmd = false);

    bool can_forward_msg(epid_t ep);
    void forward_msg(epid_t ep, peid_t pe, vpeid_t vpe);
    void forward_mem(epid_t ep, peid_t pe);

    m3::Errors::Code config_rcv_ep(epid_t ep, RGateObject &obj);
    m3::Errors::Code config_snd_ep(epid_t ep, SGateObject &obj);
    m3::Errors::Code config_mem_ep(epid_t ep, const MGateObject &obj, goff_t off = 0);

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
    void init();
    void init_memory();
    void load_app();
    void exit_app(int exitcode);

    void update_ep(epid_t ep);

    void notify_resume();

    void free_reqs();

    VPEDesc _desc;
    uint _flags;
    int _pid;
    State _state;
    int _exitcode;
    VPEGroup *_group;
    uint _services;
    uint _pending_fwds;
    m3::String _name;
    CapTable _objcaps;
    CapTable _mapcaps;
    uint64_t _lastsched;
    size_t _rbufs_size;
    alignas(DTU_PKG_SIZE) DTUState _dtustate;
    SendGate _upcsgate;
    SendQueue _upcqueue;
    AddrSpace *_as;
    size_t _headers;
    MainMemory::Allocation _rbufcpy;
    m3::SList<ServName> _requires;
    size_t _argc;
    char **_argv;
    goff_t _epaddr;
};

}
