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

#include <m3/Common.h>
#include <m3/util/Util.h>
#include <m3/WorkLoop.h>
#include <m3/DTU.h>

namespace m3 {

class RecvBuf {
public:
    static const size_t UNBOUND     = -1;

    enum {
        NONE        = 0,
        NO_RINGBUF  = DTU::FLAG_NO_RINGBUF,
        NO_HEADER   = DTU::FLAG_NO_HEADER,
        DELETE_BUF  = 1UL << (sizeof(unsigned) * 8 - 1) // internal
    };

    class RecvBufWorkItem : public WorkItem {
    public:
        explicit RecvBufWorkItem(size_t chanid) : _chanid(chanid) {
        }

        void chanid(size_t id) {
            _chanid = id;
        }

        virtual void work() override;

    private:
        size_t _chanid;
    };

private:
    explicit RecvBuf(size_t chanid, void *addr, int order, int msgorder, unsigned flags)
        : _buf(reinterpret_cast<uint8_t*>(addr)), _order(order), _msgorder(msgorder),
          _chanid(chanid), _flags(flags), _workitem() {
        if(chanid != UNBOUND)
            attach(chanid);
    }

public:
#if defined(__t2__)
    static RecvBuf create(size_t chanid, int, unsigned) {
        return create(chanid);
    }
    static RecvBuf create(size_t chanid, int, int, unsigned) {
        return create(chanid);
    }
    static RecvBuf create(size_t chanid) {
        return RecvBuf(chanid, reinterpret_cast<void*>(
            RECV_BUF_LOCAL + DTU::get().recvbuf_offset(coreid(), chanid)),
            nextlog2<RECV_BUF_MSGSIZE>::val, nextlog2<RECV_BUF_MSGSIZE>::val, NONE);
    }
#else
    static RecvBuf create(size_t chanid, int order, unsigned flags) {
        return RecvBuf(chanid, new uint8_t[1UL << order], order, order, flags | DELETE_BUF);
    }
    static RecvBuf create(size_t chanid, int order, int msgorder, unsigned flags) {
        return RecvBuf(chanid, new uint8_t[1UL << order], order, msgorder, flags | DELETE_BUF);
    }
#endif
    static RecvBuf bindto(size_t chanid, void *addr, int order, unsigned flags) {
        return RecvBuf(chanid, addr, order, order, flags);
    }
    static RecvBuf bindto(size_t chanid, void *addr, int order, int msgorder, unsigned flags) {
        return RecvBuf(chanid, addr, order, msgorder, flags);
    }

    RecvBuf(const RecvBuf&) = delete;
    RecvBuf &operator=(const RecvBuf&) = delete;
    RecvBuf(RecvBuf &&r) : _buf(r._buf), _order(r._order), _msgorder(r._msgorder),
            _chanid(r._chanid), _flags(r._flags) {
        r._flags &= ~DELETE_BUF;
    }
    ~RecvBuf() {
        if(_flags & DELETE_BUF)
            delete[] _buf;
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
    size_t chanid() const {
        return _chanid;
    }
    unsigned flags() const {
        return _flags & ~DELETE_BUF;
    }

#if !defined(__t2__)
    void setbuffer(void *addr, int order) {
        _buf = reinterpret_cast<uint8_t*>(addr);
        _order = order;
        if(_chanid != UNBOUND)
            attach(_chanid);
    }
#endif

    void attach(size_t i);
    void detach();

private:
    uint8_t *_buf;
    int _order;
    int _msgorder;
    size_t _chanid;
    unsigned _flags;
    RecvBufWorkItem *_workitem;
};

}
