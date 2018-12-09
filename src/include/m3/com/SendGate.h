/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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
 * A SendGate is used to send messages to a RecvGate. To receive replies for the sent messages,
 * it has an associated RecvGate. You can either create a SendGate for a RecvGate and delegate it
 * to somebody else in order to allow him to send messages to this RecvGate. Or you can bind a
 * SendGate to a capability you've received from somebody else.
 */
class SendGate : public Gate {
    friend class Syscalls;
    friend class EnvUserBackend;

    explicit SendGate(capsel_t cap, uint capflags, RecvGate *replygate, epid_t ep = UNBOUND)
        : Gate(SEND_GATE, cap, capflags, ep),
          _replygate(replygate == nullptr ? &RecvGate::def() : replygate) {
    }

public:
    static const word_t UNLIMITED   = KIF::UNLIM_CREDITS;

    /**
     * Creates a new send gate for the given receive gate.
     *
     * @param rgate the destination receive gate
     * @param label the label
     * @param credits the credits to assign to this gate
     * @param replygate the receive gate to which the replies should be sent
     * @param sel the selector to use (if != INVALID, the selector is NOT freed on destruction)
     */
    static SendGate create(RecvGate *rgate, label_t label = 0, word_t credits = UNLIMITED,
                           RecvGate *replygate = nullptr, capsel_t sel = INVALID);

    /**
     * Binds this send gate to the given capability. Typically, received from somebody else.
     *
     * @param sel the capability selector
     * @param replygate the receive gate to which the replies should be sent
     * @param flags the flags to control whether cap/selector are kept (default: both)
     */
    static SendGate bind(capsel_t sel, RecvGate *replygate = nullptr, uint flags = ObjCap::KEEP_CAP) {
        return SendGate(sel, flags, replygate);
    }

    SendGate(SendGate &&g)
        : Gate(Util::move(g)),
          _replygate(g._replygate) {
    }

    /**
     * @return the gate to receive the replies from when sending a message over this gate
     */
    RecvGate *reply_gate() {
        return _replygate;
    }
    /**
     * Sets the receive gate to receive replies on.
     *
     * @param rgate the new receive gate
     */
    void reply_gate(RecvGate *rgate) {
        _replygate = rgate;
    }

    /**
     * Activates this gate for <vpe> at EP <ep>.
     *
     * @param vpe the VPE to activate it for
     * @param ep the ep id
     * @return the result
     */
    Errors::Code activate_for(VPE &vpe, epid_t ep);

    /**
     * Sends <data> of length <len> to the associated RecvGate.
     *
     * @param data the data to send
     * @param len the length of the data
     * @param reply_label the reply label to set
     * @return the error code or Errors::NONE
     */
    Errors::Code send(const void *data, size_t len, label_t reply_label = 0);

private:
    RecvGate *_replygate;
};

}
