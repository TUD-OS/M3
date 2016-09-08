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
#include <m3/com/RecvGate.h>

namespace m3 {

class Syscalls;
class EnvUserBackend;
class VPE;

/**
 * A SendGate can only be used for sending messages. To do that, it needs to be backed by a
 * msg-capability. You can either create a SendGate for one of your own endpoints and delegate it
 * to somebody else in order to allow him to send messages to you. Or you can bind a SendGate to
 * a msg-capability you've received from somebody else.
 *
 * In order to receive responses on the messages you send, a SendGate has an associated RecvGate.
 * By default, this is the default receive gate that is used for all replies. But you can specify
 * a different one, if desired.
 */
class SendGate : public Gate {
    friend class Syscalls;
    friend class EnvUserBackend;

    explicit SendGate(capsel_t cap, uint capflags, RecvGate *rcvgate, size_t epid = UNBOUND)
        : Gate(SEND_GATE, cap, capflags, epid),
            _rcvgate(rcvgate == nullptr ? &RecvGate::def() : rcvgate) {
    }

public:
    static const word_t UNLIMITED   = KIF::UNLIM_CREDITS;

    /**
     * Creates a new send-gate for your own VPE. That is, a gate is created for one of your endpoints
     * that you can delegate to somebody else so that he can send messages to you.
     *
     * @param credits the credits to assign to this gate
     * @param rcvgate the receive-gate to which the messages should be sent
     * @param sel the selector to use (if != INVALID, the selector is NOT freed on destruction)
     */
    static SendGate create(word_t credits = UNLIMITED, RecvGate *rcvgate = nullptr, capsel_t sel = INVALID);

    /**
     * Creates a new send-gate for the given VPE. That is, a gate is created for one of the endpoints
     * of the given VPE.
     *
     * @param vpe the VPE for whose endpoints the gate should be created
     * @param dstep the destination endpoint id
     * @param label the label
     * @param credits the credits to assign to this gate
     * @param rcvgate the receive-gate to which the replies should be sent
     * @param sel the selector to use (if != INVALID, the selector is NOT freed on destruction)
     */
    static SendGate create_for(const VPE &vpe, size_t dstep, label_t label = 0,
        word_t credits = UNLIMITED, RecvGate *rcvgate = nullptr, capsel_t sel = INVALID);

    /**
     * Binds this gate for sending to the given msg-capability. Typically, you've received the
     * cap from somebody else.
     *
     * @param cap the capability
     * @param rcvbuf the receive-buffer
     * @param flags the flags to control whether cap/selector are kept (default: both)
     */
    static SendGate bind(capsel_t cap, RecvGate *rcvgate = nullptr,
            uint flags = ObjCap::KEEP_CAP | ObjCap::KEEP_SEL) {
        return SendGate(cap, flags, rcvgate);
    }

    SendGate(SendGate &&g) : Gate(Util::move(g)), _rcvgate(g._rcvgate) {
    }

    /**
     * @return the gate to receive the replies from when sending a message over this gate
     */
    RecvGate *receive_gate() {
        return _rcvgate;
    }
    /**
     * Sets the receive-gate to receive replies on.
     *
     * @param rcvgate the new receive gate
     */
    void receive_gate(RecvGate *rcvgate) {
        _rcvgate = rcvgate;
    }

    /**
     * Performs the send-operation with <data> of length <len>.
     *
     * @param data the data to send
     * @param len the length of the data
     * @return the error code or Errors::NO_ERROR
     */
    Errors::Code send(const void *data, size_t len) {
        Errors::Code res = async_cmd(SEND, const_cast<void*>(data), len, 0, 0,
            _rcvgate->label(), _rcvgate->epid());
        wait_until_sent();
        return res;
    }

private:
    RecvGate *_rcvgate;
};

}
