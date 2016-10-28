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

#include <m3/ObjCap.h>

namespace m3 {

class Env;
class VPE;

class RecvBuf : public ObjCap {
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
    enum {
        FREE_BUF    = 1,
        FREE_EP     = 2,
    };

    explicit RecvBuf(VPE &vpe, capsel_t cap, int order, uint flags)
        : ObjCap(RECV_BUF, cap, flags),
          _vpe(vpe),
          _buf(),
          _order(order),
          _epid(UNBOUND),
          _free(FREE_BUF),
          _workitem() {
    }
    explicit RecvBuf(VPE &vpe, capsel_t cap, size_t epid, void *buf, int order, int msgorder, uint flags);

public:
    static RecvBuf &syscall() {
        return _syscall;
    }
    static RecvBuf &upcall() {
        return _upcall;
    }
    static RecvBuf &def() {
        return _default;
    }

    static RecvBuf create(int order, int msgorder);
    static RecvBuf create(capsel_t cap, int order, int msgorder);

    static RecvBuf create_for(VPE &vpe, int order, int msgorder);
    static RecvBuf create_for(VPE &vpe, capsel_t cap, int order, int msgorder);

    static RecvBuf bind(capsel_t cap, int order);

    RecvBuf(const RecvBuf&) = delete;
    RecvBuf &operator=(const RecvBuf&) = delete;
    RecvBuf(RecvBuf &&r)
            : ObjCap(Util::move(r)), _vpe(r._vpe), _buf(r._buf), _order(r._order), _epid(r._epid),
              _free(r._free), _workitem(r._workitem) {
        r._free = 0;
        r._epid = UNBOUND;
        r._workitem = nullptr;
    }
    ~RecvBuf();

    const void *addr() const {
        return _buf;
    }
    size_t epid() const {
        return _epid;
    }

    void activate();
    void activate(size_t epid);
    void activate(size_t epid, uintptr_t addr);
    void disable();
    void deactivate();

private:
    static void *allocate(size_t epid, size_t size);
    static void free(void *);

    VPE &_vpe;
    void *_buf;
    int _order;
    size_t _epid;
    uint _free;
    RecvBufWorkItem *_workitem;
    static RecvBuf _syscall;
    static RecvBuf _upcall;
    static RecvBuf _default;
};

}
