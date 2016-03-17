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

#include <m3/cap/SendGate.h>
#include <m3/cap/MemGate.h>
#include <m3/cap/RecvGate.h>
#include <m3/GateStreamBase.h>

namespace m3 {

using GateOStream = BaseGateOStream<RecvGate, SendGate>;
template<size_t SIZE>
using StaticGateOStream = BaseStaticGateOStream<SIZE, RecvGate, SendGate>;
using AutoGateOStream = BaseAutoGateOStream<RecvGate, SendGate>;
using GateIStream = BaseGateIStream<RecvGate, SendGate>;

/**
 * All these methods send the given data; either over <gate> or as an reply to the first not
 * acknowledged message in <gate> or as a reply on a GateIStream.
 *
 * @param gate the gate to send to
 * @param data the message data
 * @param len the message length
 * @return the error code or Errors::NO_ERROR
 */
static inline Errors::Code send_msg(SendGate &gate, const void *data, size_t len) {
    EVENT_TRACER_send_msg();
    return gate.send(data, len);
}
static inline Errors::Code reply_msg(RecvGate &gate, const void *data, size_t len) {
    EVENT_TRACER_reply_msg();
    return gate.reply_sync(data, len, DTU::get().get_msgoff(gate.epid(), &gate));
}
static inline Errors::Code reply_msg_on(const GateIStream &is, const void *data, size_t len) {
    EVENT_TRACER_reply_msg_on();
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
 * @return the error code or Errors::NO_ERROR
 */
template<typename... Args>
static inline Errors::Code send_vmsg(SendGate &gate, const Args &... args) {
    EVENT_TRACER_send_vmsg();
    auto msg = create_vmsg(args...);
    return gate.send(msg.bytes(), msg.total());
}
template<typename... Args>
static inline Errors::Code reply_vmsg(RecvGate &gate, const Args &... args) {
    EVENT_TRACER_reply_vmsg();
    return create_vmsg(args...).reply(gate);
}
template<typename... Args>
static inline Errors::Code reply_vmsg_on(const GateIStream &is, const Args &... args) {
    EVENT_TRACER_reply_vmsg_on();
    return is.reply(create_vmsg(args...));
}

/**
 * Puts a message of the appropriate size, depending on the types of <args>, on the
 * stack, copies the values into it and writes it to <gate> at <offset>.
 *
 * @param gate the memory gate
 * @param offset the offset to write to
 * @param args the arguments to marshall
 * @return the error code or Errors::NO_ERROR
 */
template<typename... Args>
static inline Errors::Code write_vmsg(MemGate &gate, size_t offset, const Args &... args) {
    EVENT_TRACER_write_vmsg();
    auto os = create_vmsg(args...);
    return gate.write_sync(os.bytes(), os.total(), offset);
}

/**
 * Receives a message from <gate> and returns an GateIStream to unmarshall the message. Note that
 * the GateIStream object acknowledges the message on destruction.
 *
 * @param gate the gate to receive the message from
 * @return the GateIStream
 */
static inline GateIStream receive_msg(RecvGate &gate) {
    EVENT_TRACER_receive_msg();
    Errors::Code err = gate.wait(nullptr);
    return GateIStream(gate, err, true);
}
/**
 * Receives a message from <gate> and unmarshalls the message into <args>. Note that
 * the GateIStream object acknowledges the message on destruction.
 *
 * @param gate the gate to receive the message from
 * @param args the arguments to unmarshall to
 * @return the GateIStream, e.g. to read further values or to reply
 */
template<typename... Args>
static inline GateIStream receive_vmsg(RecvGate &gate, Args &... args) {
    EVENT_TRACER_receive_vmsg();
    Errors::Code err = gate.wait(nullptr);
    GateIStream is(gate, err, true);
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
    Errors::Code err = gate.receive_gate()->wait(&gate);
    return GateIStream(*gate.receive_gate(), err, true);
}

/**
 * Convenience methods that combine send_msg()/send_vmsg() and receive_msg().
 */
static inline GateIStream send_receive_msg(SendGate &gate, const void *data, size_t len) {
    EVENT_TRACER_send_receive_msg();
    send_msg(gate, data, len);
    return receive_reply(gate);
}
template<typename... Args>
static inline GateIStream send_receive_vmsg(SendGate &gate, const Args &... args) {
    EVENT_TRACER_send_receive_vmsg();
    send_vmsg(gate, args...);
    return receive_reply(gate);
}

}
