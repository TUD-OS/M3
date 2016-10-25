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

#include <base/Common.h>
#include <base/util/Util.h>
#include <base/WorkLoop.h>
#include <base/DTU.h>

namespace m3 {

class Env;

class RecvBuf {
    friend class Env;

public:
    static const size_t UNBOUND         = -1;

    class RecvBufWorkItem : public WorkItem {
    public:
        explicit RecvBufWorkItem(size_t epid) : _epid(epid) {
        }

        void epid(size_t id) {
            _epid = id;
        }

        virtual void work() override;

    private:
        size_t _epid;
    };

    class UpcallWorkItem : public WorkItem {
    public:
        virtual void work() override;
    };

private:
    static const int DEF_RBUF_ORDER     = 8;

    explicit RecvBuf(size_t epid, void *addr, int order, int msgorder, bool del)
        : _buf(reinterpret_cast<uint8_t*>(addr)), _order(order), _msgorder(msgorder),
          _epid(epid), _del(del), _workitem() {
        if(epid != UNBOUND)
            attach(epid);
    }

    static RecvBuf bindto(size_t epid, void *addr, int order) {
        return RecvBuf(epid, addr, order, order, false);
    }

public:
    static RecvBuf &syscall() {
        return _syscall;
    }
    static RecvBuf &upcall() {
        return _upcall;
    }
    static RecvBuf &def() {
        if(_default == nullptr)
            _default = new RecvBuf(RecvBuf::create(DTU::DEF_REP, DEF_RBUF_ORDER, DEF_RBUF_ORDER));
        return *_default;
    }

#if defined(__t2__)
    static RecvBuf create(size_t epid, int, unsigned) {
        return create(epid);
    }
    static RecvBuf create(size_t epid, int, int, unsigned) {
        return create(epid);
    }
    static RecvBuf create(size_t epid) {
        return RecvBuf(epid, reinterpret_cast<void*>(
            RECV_BUF_LOCAL + DTU::get().recvbuf_offset(env()->coreid, epid)),
            nextlog2<RECV_BUF_MSGSIZE>::val, nextlog2<RECV_BUF_MSGSIZE>::val, NONE);
    }
#else
    static RecvBuf create(size_t epid, int order) {
        return RecvBuf(epid, allocate(1UL << order), order, order, true);
    }
    static RecvBuf create(size_t epid, int order, int msgorder) {
        return RecvBuf(epid, allocate(1UL << order), order, msgorder, true);
    }
#endif

    RecvBuf(const RecvBuf&) = delete;
    RecvBuf &operator=(const RecvBuf&) = delete;
    RecvBuf(RecvBuf &&r) : _buf(r._buf), _order(r._order), _msgorder(r._msgorder),
            _epid(r._epid), _del(r._del), _workitem(r._workitem) {
        r._del = false;
        r._epid = UNBOUND;
        r._workitem = nullptr;
    }
    ~RecvBuf() {
        if(_del)
            free(_buf);
        detach();
    }

    void *addr() const {
        return _buf;
    }
    int order() const {
        return _order;
    }
    int msgorder() const {
        return _msgorder;
    }
    size_t epid() const {
        return _epid;
    }

#if !defined(__t2__)
    void setbuffer(void *addr, int order) {
        _buf = reinterpret_cast<uint8_t*>(addr);
        _order = order;
        if(_epid != UNBOUND)
            attach(_epid);
    }
#endif

    void attach(size_t i);
    void disable();
    void detach();

private:
    static uint8_t *allocate(size_t size);
    static void free(uint8_t *);

    uint8_t *_buf;
    int _order;
    int _msgorder;
    size_t _epid;
    bool _del;
    RecvBufWorkItem *_workitem;
    static RecvBuf _syscall;
    static RecvBuf _upcall;
    static RecvBuf *_default;
};

}
