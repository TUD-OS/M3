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

#include <m3/com/Marshalling.h>
#include <m3/com/SendGate.h>
#include <m3/com/MemGate.h>
#include <m3/com/RecvGate.h>

#include <alloca.h>

namespace m3 {

/**
 * The gate stream classes provide an easy abstraction to marshall or unmarshall data when
 * communicating between VPEs. Therefore, if you want to combine multiple values into a single
 * message or extract multiple values from a message, this is the abstraction you might want to use.
 * If you already have the data to send, you should directly use the send method of SendGate. If
 * you don't want to extract values from a message but directly access the message, use the
 * data-field of the message you received.
 *
 * All classes work with (variadic) templates and are thus type-safe. Of course, that does not
 * relieve you from taking care that sender and receiver agree on the types of the values that are
 * exchanged via messaging.
 */

/**
 * The gate stream to marshall values into a message and send it over an endpoint. Thus, it "outputs"
 * values into a message.
 */
class GateOStream : public Marshaller {
public:
    explicit GateOStream(unsigned char *bytes, size_t total) : Marshaller(bytes, total) {
    }
    GateOStream(const GateOStream &) = default;
    GateOStream &operator=(const GateOStream &) = default;

    using Marshaller::put;

    /**
     * Puts all remaining items (the ones that haven't been read yet) of <is> into this GateOStream.
     *
     * @param is the GateIStream
     * @return *this
     */
    void put(const GateIStream &is);
};

/**
 * An implementation of GateOStream that hosts the message as a member. E.g. you can put an object
 * of this class on the stack, which would host the message on the stack.
 * In most cases, you don't want to use this class yourself, but the free standing convenience
 * functions below that automatically determine <SIZE>.
 *
 * @param SIZE the max. size of the message
 */
template<size_t SIZE>
class StaticGateOStream : public GateOStream {
public:
    explicit StaticGateOStream() : GateOStream(_bytes, SIZE) {
    }
    template<size_t SRCSIZE>
    StaticGateOStream(const StaticGateOStream<SRCSIZE> &os) : GateOStream(os) {
        static_assert(SIZE >= SRCSIZE, "Incompatible sizes");
        memcpy(_bytes, os._bytes, sizeof(os._bytes));
    }
    template<size_t SRCSIZE>
    StaticGateOStream &operator=(const StaticGateOStream<SRCSIZE> &os) {
        static_assert(SIZE >= SRCSIZE, "Incompatible sizes");
        GateOStream::operator=(os);
        if(&os != this)
            memcpy(_bytes, os._bytes, sizeof(os._bytes));
        return *this;
    }

private:
    alignas(DTU_PKG_SIZE) unsigned char _bytes[SIZE];
};

/**
 * An implementation of GateOStream that hosts the message on the stack by using alloca.
 */
class AutoGateOStream : public GateOStream {
public:
#if defined(__t2__) or defined(__t3__)
    // TODO alloca() uses movsp which causes an exception to be handled appropriately. since this
    // isn't that trivial to implement, we're using malloc instead.
    explicit AutoGateOStream(size_t size)
        : GateOStream(static_cast<unsigned char*>(Heap::alloc(Math::round_up(size, DTU_PKG_SIZE))),
                      Math::round_up(size, DTU_PKG_SIZE)) {
    }
    ~AutoGateOStream() {
        Heap::free(this->_bytes);
    }
#else
    ALWAYS_INLINE explicit AutoGateOStream(size_t size)
        : GateOStream(static_cast<unsigned char*>(alloca(Math::round_up(size, DTU_PKG_SIZE))),
                      Math::round_up(size, DTU_PKG_SIZE)) {
    }
#endif

    AutoGateOStream(AutoGateOStream &&os) : GateOStream(os) {
    }

    bool is_on_heap() const {
#if defined(__t2__) or defined(__t3__)
        return true;
#else
        return false;
#endif
    }

    /**
     * Claim the ownership of the data from this class. Thus, it will not free it.
     */
    void claim() {
        this->_bytes = nullptr;
    }
};

/**
 * The gate stream to unmarshall values from a message. Thus, it "inputs" values from a message
 * into variables.
 *
 * Note: unfortunately, we can't reuse the functionality of Unmarshaller here. It seems to be a
 * compiler-bug when building for Xtensa. The compiler generates wrong code when we initialize the
 * _length field to _msg->length.
 */
class GateIStream {
public:
    /**
     * Creates an object for the given message from <rgate>.
     *
     * @param rgate the receive gate
     * @param err the error code
     */
    explicit GateIStream(RecvGate &rgate, const DTU::Message *msg, Errors::Code err = Errors::NONE)
        : _err(err),
          _ack(true),
          _pos(0),
          _rgate(&rgate),
          _msg(msg) {
    }

    // don't do the ack twice. thus, copies never ack.
    GateIStream(const GateIStream &is)
        : _err(is._err),
          _ack(),
          _pos(is._pos),
          _rgate(is._rgate),
          _msg(is._msg) {
    }
    GateIStream &operator=(const GateIStream &is) {
        if(this != &is) {
            _err = is._err;
            _ack = false;
            _pos = is._pos;
            _rgate = is._rgate;
            _msg = is._msg;
        }
        return *this;
    }
    GateIStream &operator=(GateIStream &&is) {
        if(this != &is) {
            _err = is._err;
            _ack = is._ack;
            _pos = is._pos;
            _rgate = is._rgate;
            _msg = is._msg;
            is._ack = 0;
        }
        return *this;
    }
    GateIStream(GateIStream &&is)
        : _err(is._err),
          _ack(is._ack),
          _pos(is._pos),
          _rgate(is._rgate),
          _msg(is._msg) {
        is._ack = 0;
    }
    ~GateIStream() {
        finish();
    }

    /**
     * @return the error that occurred (or Errors::NONE)
     */
    Errors::Code error() const {
        return _err;
    }
    /**
     * @return the receive gate
     */
    RecvGate &rgate() {
        return *_rgate;
    }
    /**
     * @return the message (header + payload)
     */
    const DTU::Message &message() const {
        return *_msg;
    }
    /**
     * @return the label of the message
     */
    template<typename T>
    T label() const {
        return (T)_msg->label;
    }
    /**
     * @return the current position, i.e. the offset of the unread data
     */
    size_t pos() const {
        return _pos;
    }
    /**
     * @return the length of the message in bytes
     */
    size_t length() const {
#if defined(__t3__)
        return _msg->length * DTU_PKG_SIZE;
#else
        return _msg->length;
#endif
    }
    /**
     * @return the remaining bytes to read
     */
    size_t remaining() const {
        return length() - _pos;
    }
    /**
     * @return the message payload
     */
    const unsigned char *buffer() const {
        return _msg->data;
    }

    /**
     * Replies the message constructed by <os> to this message
     *
     * @param os the GateOStream hosting the message to reply
     * @return the error code or Errors::NONE
     */
    Errors::Code reply(const GateOStream &os) {
        return reply(os.bytes(), os.total());
    }
    /**
     * Replies the given message to this one
     *
     * @param data the message data
     * @param len the length of the message
     * @return the error code or Errors::NONE
     */
    Errors::Code reply(const void *data, size_t len) {
        Errors::Code res = _rgate->reply(data, len, DTU::get().get_msgoff(_rgate->ep(), _msg));
        // it's already acked
        _ack = false;
        return res;
    }

    void ignore(size_t bytes) {
        _pos += bytes;
    }

    /**
     * Pulls the given values out of this stream
     *
     * @param val the value to write to
     * @param args the other values to write to
     */
    template<typename T, typename... Args>
    void vpull(T &val, Args &... args) {
        *this >> val;
        vpull(args...);
    }

    /**
     * Pulls a value into <value>.
     *
     * @param value the value to write to
     * @return *this
     */
    template<typename T>
    GateIStream & operator>>(T &value) {
        assert(_pos + sizeof(T) <= length());
        value = (T)*reinterpret_cast<const xfer_t*>(_msg->data + _pos);
        _pos += Math::round_up(sizeof(T), sizeof(xfer_t));
        return *this;
    }
    GateIStream & operator>>(String &value) {
        assert(_pos + sizeof(xfer_t) <= length());
        size_t len = *reinterpret_cast<const xfer_t*>(_msg->data + _pos);
        _pos += sizeof(xfer_t);
        assert(_pos + len <= length());
        value.reset(reinterpret_cast<const char*>(_msg->data + _pos), len);
        _pos += Math::round_up(len, sizeof(xfer_t));
        return *this;
    }

    /**
     * Disables acknowledgement of the message. That is, it will be marked as read, but you have
     * to ack the message on your own via DTU::get().mark_acked(<ep>).
     */
    void claim() {
        _ack = false;
    }

    /**
     * Finishes this message, i.e. moves the read-position in the ringbuffer forward. If
     * acknowledgement has not been disabled (see claim), it will be acked.
     */
    void finish() {
        if(_ack) {
            DTU::get().mark_read(_rgate->ep(), DTU::get().get_msgoff(_rgate->ep(), _msg));
            _ack = false;
        }
    }

private:
    // needed as recursion-end
    void vpull() {
    }

    Errors::Code _err;
    bool _ack;
    size_t _pos;
    RecvGate *_rgate;
    const DTU::Message *_msg;
};

inline void GateOStream::put(const GateIStream &is) {
    assert(fits(_bytecount, is.remaining()));
    memcpy(const_cast<unsigned char*>(bytes()) + _bytecount, is.buffer() + is.pos(), is.remaining());
    _bytecount += is.remaining();
}

static inline Errors::Code reply_error(GateIStream &is, m3::Errors::Code error) {
    KIF::DefaultReply reply;
    reply.error = error;
    return is.reply(&reply, sizeof(reply));
}

/**
 * All these methods send the given data; either over <gate> or as an reply to the first not
 * acknowledged message in <gate> or as a reply on a GateIStream.
 *
 * @param gate the gate to send to
 * @param data the message data
 * @param len the message length
 * @return the error code or Errors::NONE
 */
static inline Errors::Code send_msg(SendGate &gate, const void *data, size_t len) {
    EVENT_TRACER_send_msg();
    return gate.send(data, len);
}
static inline Errors::Code reply_msg(GateIStream &is, const void *data, size_t len) {
    EVENT_TRACER_reply_msg();
    return is.reply(data, len);
}

/**
 * Creates a StaticGateOStream for the given arguments.
 *
 * @return the stream
 */
template<typename ... Args>
static inline auto create_vmsg(const Args& ... args) -> StaticGateOStream<ostreamsize<Args...>()> {
    StaticGateOStream<ostreamsize<Args...>()> os;
    os.vput(args...);
    return os;
}

/**
 * All these methods put a message of the appropriate size, depending on the types of <args>, on the
 * stack, copies the values into it and sends it; either over <gate> or as an reply to the first not
 * acknowledged message in <gate> or as a reply on a GateIStream.
 *
 * @param gate the gate to send to
 * @param args the arguments to put into the message
 * @return the error code or Errors::NONE
 */
template<typename... Args>
static inline Errors::Code send_vmsg(SendGate &gate, const Args &... args) {
    EVENT_TRACER_send_vmsg();
    auto msg = create_vmsg(args...);
    return gate.send(msg.bytes(), msg.total());
}
template<typename... Args>
static inline Errors::Code reply_vmsg(GateIStream &is, const Args &... args) {
    EVENT_TRACER_reply_vmsg();
    return is.reply(create_vmsg(args...));
}

/**
 * Puts a message of the appropriate size, depending on the types of <args>, on the
 * stack, copies the values into it and writes it to <gate> at <offset>.
 *
 * @param gate the memory gate
 * @param offset the offset to write to
 * @param args the arguments to marshall
 * @return the error code or Errors::NONE
 */
template<typename... Args>
static inline Errors::Code write_vmsg(MemGate &gate, size_t offset, const Args &... args) {
    EVENT_TRACER_write_vmsg();
    auto os = create_vmsg(args...);
    return gate.write(os.bytes(), os.total(), offset);
}

/**
 * Receives a message from <gate> and returns an GateIStream to unmarshall the message. Note that
 * the GateIStream object acknowledges the message on destruction.
 *
 * @param rgate the gate to receive the message from
 * @return the GateIStream
 */
static inline GateIStream receive_msg(RecvGate &rgate) {
    EVENT_TRACER_receive_msg();
    const DTU::Message *msg;
    Errors::Code err = rgate.wait(nullptr, &msg);
    return GateIStream(rgate, msg, err);
}
/**
 * Receives a message from <gate> and unmarshalls the message into <args>. Note that
 * the GateIStream object acknowledges the message on destruction.
 *
 * @param rgate the gate to receive the message from
 * @param args the arguments to unmarshall to
 * @return the GateIStream, e.g. to read further values or to reply
 */
template<typename... Args>
static inline GateIStream receive_vmsg(RecvGate &rgate, Args &... args) {
    EVENT_TRACER_receive_vmsg();
    const DTU::Message *msg;
    Errors::Code err = rgate.wait(nullptr, &msg);
    GateIStream is(rgate, msg, err);
    is.vpull(args...);
    return is;
}

/**
 * Receives the reply for a message sent over <gate> and returns an GateIStream to unmarshall the
 * message. Note that the GateIStream object acknowledges the message on destruction.
 * The difference to receive_v?msg() is, that it
 *
 * @param gate the gate to receive the message from
 * @return the GateIStream
 */
static inline GateIStream receive_reply(SendGate &gate) {
    EVENT_TRACER_receive_msg();
    const DTU::Message *msg;
    Errors::Code err = gate.reply_gate()->wait(&gate, &msg);
    return GateIStream(*gate.reply_gate(), msg, err);
}

/**
 * Convenience methods that combine send_msg()/send_vmsg() and receive_msg().
 */
static inline GateIStream send_receive_msg(SendGate &gate, const void *data, size_t len) {
    send_msg(gate, data, len);
    return receive_reply(gate);
}
template<typename... Args>
static inline GateIStream send_receive_vmsg(SendGate &gate, const Args &... args) {
    send_vmsg(gate, args...);
    return receive_reply(gate);
}

}
