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

#include <m3/cap/Cap.h>
#include <m3/GateStream.h>
#include <m3/CapRngDesc.h>
#include <m3/Errors.h>

namespace m3 {

/**
 * A session represents a connection between client and the server, that provides the used service.
 * Over the session, capabilities can be exchanged, e.g. to delegate a SendGate from server to
 * client in order to let the client send messages to the server.
 *
 * At construction, the server receives an OPEN event, that allows him to associate information with
 * this session. At destruction, the server receives a CLOSE event to perform cleanup.
 */
class Session : public Cap {
public:
    /**
     * Creates a session at service <name>, sending him <args> as arguments to the OPEN event.
     *
     * @param name the service name
     * @param args the arguments
     */
    explicit Session(const String &name, const GateOStream &args = StaticGateOStream<1>())
        : Cap(SESSION) {
        connect(name, args);
    }

    /**
     * Attaches this object to the given session
     *
     * @param sel the capability selector of the session
     */
    explicit Session(capsel_t sel) : Cap(SESSION, sel) {
    }

    /**
     * @return whether this session is connected
     */
    bool is_connected() const {
        return sel() != INVALID;
    }

    /**
     * Delegates the given capability range to the server.
     *
     * @param crd the capabilities
     */
    void delegate(const CapRngDesc &crd);
    /**
     * Delegates the given capability range to the server with additional arguments.
     *
     * @param crd the capabilities
     * @param args the arguments to pass to the server
     * @return the GateIStream with the received values
     */
    GateIStream delegate(const CapRngDesc &crd, const GateOStream &args);

    /**
     * Obtains up to <count> capabilities from the server
     *
     * @param count the number of capabilities
     * @return the received range
     */
    CapRngDesc obtain(uint count);
    /**
     * Obtains up to <count> capabilities from the server with additional arguments
     *
     * @param count the number of capabilities
     * @param crd will be set to the received range
     * @param args the arguments to pass to the server
     * @return the GateIStream with the received values
     */
    GateIStream obtain(uint count, CapRngDesc &crd, const GateOStream &args);

private:
    void connect(const String &name, const GateOStream &args);
};

}
