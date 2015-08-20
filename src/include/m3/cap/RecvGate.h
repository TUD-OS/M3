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

#include <m3/cap/Gate.h>
#include <m3/Errors.h>

#if defined(TRACE_DEBUG)
#   include <m3/tracing/Tracing.h>
#endif

namespace m3 {

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
        : Gate(RECV_GATE, INVALID, 0, rcvbuf->chanid()), Subscriptions<RecvGate&>(),
          _rcvbuf(rcvbuf), _sess(sess) {
    }

public:
    /**
     * Creates a new receive-gate. Note that the receive-buffer has to be bound to a channel.
     *
     * @param rcvbuf the receive-buffer
     * @param sess optionally, a session bound to this gate
     */
    static RecvGate create(RecvBuf *rcvbuf, void *sess = nullptr) {
        assert(rcvbuf != nullptr);
        assert(rcvbuf->chanid() != UNBOUND);
        return RecvGate(rcvbuf,sess);
    }

    RecvGate(RecvGate &&c)
        : Gate(Util::move(c)), Subscriptions<RecvGate&>(Util::move(c)), _rcvbuf(c._rcvbuf), _sess(c._sess) {
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
     * Busy-waits until this channel has received a message.
     */
    void wait() const {
        while(!ChanMng::get().fetch_msg(chanid()))
            DTU::get().wait();
#if defined(TRACE_DEBUG)
        uint remote_core = ChanMng::get().message(chanid())->core;
        if((remote_core >= FIRST_PE_ID && remote_core < FIRST_PE_ID + MAX_CORES) ||
            remote_core == MEMORY_CORE) {
            Serial::get() << "RecvGate::wait: chan " << chanid()
                          << "  core: " << remote_core << "  timestamp: " << Profile::start() << "\n";
        }
#endif
    }

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
     */
    void reply_sync(const void *data, size_t len, size_t msgidx) {
        reply_async(data, len, msgidx);
        wait_until_sent();
    }
    void reply_async(const void *data, size_t len, size_t msgidx) {
        // TODO hack to fix the race-condition on T2. as soon as we've replied to the other core, he
        // might send us another message, which we might miss if we ACK this message after we've got
        // another one. so, ACK it now since the reply marks the end of the handling anyway.
#if defined(__t2__)
        ChanMng::get().ack_message(chanid());
#endif
        wait_until_sent();
        DTU::get().reply(chanid(), const_cast<void*>(data), len, msgidx);
    }

private:
    RecvBuf *_rcvbuf;
    void *_sess;
};

}
