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
#include <base/KIF.h>

#include <m3/ObjCap.h>
#include <m3/VPE.h>
#include <m3/com/GateStream.h>

namespace m3 {

/**
 * A session represents a connection between client and the server, that provides the used service.
 * Over the session, capabilities can be exchanged, e.g. to delegate a SendGate from server to
 * client in order to let the client send messages to the server.
 *
 * At construction, the server receives an OPEN event, that allows him to associate information with
 * this session. At destruction, the server receives a CLOSE event to perform cleanup.
 */
class Session : public ObjCap {
public:
    /**
     * Creates a session at service <name>, sending him <arg> as argument to the OPEN event.
     *
     * @param name the service name
     * @param arg the argument
     */
    explicit Session(const String &name, xfer_t arg = 0)
        : ObjCap(SESSION) {
        connect(name, arg);
    }

    /**
     * Attaches this object to the given session
     *
     * @param sel the capability selector of the session
     * @param flags whether capabilitly/selector should be kept on destruction or not
     */
    explicit Session(capsel_t sel, uint flags = ObjCap::KEEP_CAP | ObjCap::KEEP_SEL)
        : ObjCap(SESSION, sel, flags) {
    }

    /**
     * @return whether this session is connected
     */
    bool is_connected() const {
        return sel() != INVALID;
    }

    /**
     * Delegates the given object capability to the server.
     *
     * @param sel the capability
     */
    void delegate_obj(capsel_t sel) {
        KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, sel, 1);
        delegate(crd);
    }
    /**
     * Delegates the given capability range to the server with additional arguments and puts the
     * arguments from the server again into argcount and args.
     *
     * @param caps the capabilities
     * @param argcount the number of arguments
     * @param args the arguments to pass to the server
     * @return the error code
     */
    void delegate(const KIF::CapRngDesc &caps, size_t *argcount = nullptr, xfer_t *args = nullptr);

    /**
     * Obtains up to <count> capabilities from the server with additional arguments and puts the
     * arguments from the server again into argcount and args.
     *
     * @param count the number of capabilities
     * @param argcount the number of arguments
     * @param args the arguments to pass to the server
     * @return the received capabilities
     */
    KIF::CapRngDesc obtain(uint count, size_t *argcount = nullptr, xfer_t *args = nullptr);

private:
    void connect(const String &name, xfer_t arg);
};

}
