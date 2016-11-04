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

#include <m3/com/Gate.h>

#include <functional>

namespace m3 {

class Env;
class GateIStream;
class SendGate;
class VPE;

class RecvGate : public Gate {
    friend class Env;

public:
    class RecvGateWorkItem : public WorkItem {
    public:
        explicit RecvGateWorkItem(RecvGate *buf) : _buf(buf) {
        }

        virtual void work() override;

    protected:
        RecvGate *_buf;
    };

private:
    enum {
        FREE_BUF    = 1,
        FREE_EP     = 2,
    };

    explicit RecvGate(VPE &vpe, capsel_t cap, int order, uint flags)
        : Gate(RECV_GATE, cap, flags),
          _vpe(vpe),
          _buf(),
          _order(order),
          _free(FREE_BUF),
          _handler(),
          _workitem() {
    }
    explicit RecvGate(VPE &vpe, capsel_t cap, epid_t ep, void *buf, int order, int msgorder, uint flags);

public:
    using msghandler_t = std::function<void(GateIStream&)>;

    static RecvGate &syscall() {
        return _syscall;
    }
    static RecvGate &upcall() {
        return _upcall;
    }
    static RecvGate &def() {
        return _default;
    }

    static RecvGate create(int order, int msgorder);
    static RecvGate create(capsel_t cap, int order, int msgorder);

    static RecvGate create_for(VPE &vpe, int order, int msgorder);
    static RecvGate create_for(VPE &vpe, capsel_t cap, int order, int msgorder);

    static RecvGate bind(capsel_t cap, int order);

    RecvGate(const RecvGate&) = delete;
    RecvGate &operator=(const RecvGate&) = delete;
    RecvGate(RecvGate &&r)
            : Gate(Util::move(r)), _vpe(r._vpe), _buf(r._buf), _order(r._order),
              _free(r._free), _handler(r._handler), _workitem(r._workitem) {
        r._free = 0;
        r._workitem = nullptr;
    }
    ~RecvGate();

    const void *addr() const {
        return _buf;
    }

    void activate();
    void activate(epid_t ep);
    void activate(epid_t ep, uintptr_t addr);
    void deactivate();

    /**
     * Start to listen for received messages
     *
     * @param handler the handler to call for received messages
     */
    void start(msghandler_t handler);

    /**
     * Stop to listen for received messages
     */
    void stop();

    /**
     * Waits until this endpoint has received a message. If <sgate> is given, it will stop if as
     * soon as it gets invalid and return the appropriate error.
     *
     * @param sgate the send-gate (optional), if waiting for a reply
     * @param msg will be set to the fetched message
     * @return the error code
     */
    Errors::Code wait(SendGate *sgate, const DTU::Message **msg);

    /**
     * Performs the reply-operation with <data> of length <len> on message with index <msgidx>.
     * This requires that you have received a reply-capability with this message.
     *
     * @param data the data to send
     * @param len the length of the data
     * @param msgidx the index of the message to reply to
     * @return the error code or Errors::NO_ERROR
     */
    Errors::Code reply(const void *data, size_t len, size_t msgidx);

private:
    static void *allocate(epid_t ep, size_t size);
    static void free(void *);

    VPE &_vpe;
    void *_buf;
    int _order;
    uint _free;
    msghandler_t _handler;
    RecvGateWorkItem *_workitem;
    static RecvGate _syscall;
    static RecvGate _upcall;
    static RecvGate _default;
};

}
