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

#include <base/Errors.h>

#include <m3/com/Gate.h>

namespace m3 {

class SendGate;

/**
 * A RecvGate can be used for receiving messages from somebody else and reply on these messages. It
 * is thus not backed by a capability (sel() will be INVALID).
 * In order to store the received messages, it has an associated RecvBuf. To receive messages over
 * this gate, you can subscribe this RecvGate to get notified as soon as a message arrived.
 *
 * Note also that you can bind a session object to a RecvGate. This allows you to associate data
 * with a gate. So, if you just want to receive messages, it suffices to create a RecvBuf, create
 * a RecvGate using that RecvBuf and give every sender a SendGate using the created RecvGate.
 * If you want to distinguish between the senders, you want to create a new RecvGate for every
 * sender.
 */
class RecvGate : public Gate, public Subscriptions<RecvGate&> {
    explicit RecvGate(RecvBuf *rcvbuf, void *sess)
        : Gate(RECV_GATE, INVALID, 0, rcvbuf->epid()), Subscriptions<RecvGate&>(),
          _rcvbuf(rcvbuf), _sess(sess) {
    }

public:
    /**
     * Creates a new receive-gate. Note that the receive-buffer has to be bound to an endpoint.
     *
     * @param rcvbuf the receive-buffer
     * @param sess optionally, a session bound to this gate
     */
    static RecvGate create(RecvBuf *rcvbuf, void *sess = nullptr) {
        assert(rcvbuf != nullptr);
        assert(rcvbuf->epid() != UNBOUND);
        return RecvGate(rcvbuf,sess);
    }

    RecvGate(RecvGate &&g)
        : Gate(Util::move(g)), Subscriptions<RecvGate&>(Util::move(g)), _rcvbuf(g._rcvbuf), _sess(g._sess) {
    }

    /**
     * @return the set receive-buffer
     */
    RecvBuf *buffer() {
        return _rcvbuf;
    }

    /**
     * @return the session that is associated with this gate
     */
    template<class T>
    T *session() {
        return static_cast<T*>(_sess);
    }
    template<class T>
    const T *session() const {
        return static_cast<const T*>(_sess);
    }

    /**
     * Waits until this endpoint has received a message. If <sgate> is given, it will stop if as
     * soon as it gets invalid and return the appropriate error.
     *
     * @param sgate the send-gate (optional), if waiting for a reply
     * @return the error code
     */
    Errors::Code wait(SendGate *sgate) const;

    /**
     * Calls all subscribers
     */
    void notify_all() {
        for(auto it = _list.begin(); it != _list.end(); ) {
            auto old = it++;
            old->callback(*this, &*old);
        }
    }

    /**
     * Performs the reply-operation with <data> of length <len> on message with index <msgidx>.
     * This requires that you have received a reply-capability with this message.
     * Synchronous means that it waits until the data has been sent, but NOT until a potential reply
     * has been received.
     *
     * @param data the data to send
     * @param len the length of the data
     * @param msgidx the index of the message to reply to
     * @return the error code or Errors::NO_ERROR
     */
    Errors::Code reply_sync(const void *data, size_t len, size_t msgidx) {
        Errors::Code res = reply_async(data, len, msgidx);
        wait_until_sent();
        return res;
    }
    Errors::Code reply_async(const void *data, size_t len, size_t msgidx) {
        // TODO hack to fix the race-condition on T2. as soon as we've replied to the other core, he
        // might send us another message, which we might miss if we ACK this message after we've got
        // another one. so, ACK it now since the reply marks the end of the handling anyway.
#if defined(__t2__)
        DTU::get().ack_message(epid());
#endif
        wait_until_sent();
        return DTU::get().reply(epid(), const_cast<void*>(data), len, msgidx);
    }

private:
    RecvBuf *_rcvbuf;
    void *_sess;
};

}
