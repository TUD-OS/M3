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
     * @param sel the desired selector
     */
    explicit Session(const String &name, xfer_t arg = 0, capsel_t sel = ObjCap::INVALID)
        : ObjCap(SESSION) {
        connect(name, arg, sel);
    }

    /**
     * Attaches this object to the given session
     *
     * @param sel the capability selector of the session
     * @param flags whether capabilitly/selector should be kept on destruction or not
     */
    explicit Session(capsel_t sel, uint flags = ObjCap::KEEP_CAP)
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
     * @return the error, if any
     */
    Errors::Code delegate_obj(capsel_t sel) {
        KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, sel, 1);
        return delegate(crd);
    }

    /**
     * Delegates the given capability range to the server with additional arguments and puts the
     * arguments from the server again into argcount and args.
     *
     * @param caps the capabilities
     * @param args the arguments to pass to the server
     * @return the error code
     */
    Errors::Code delegate(const KIF::CapRngDesc &caps, KIF::ExchangeArgs *args = nullptr) {
        return delegate_for(VPE::self(), caps, args);
    }

    /**
     * Delegates the given capability range of <vpe> to the server with additional arguments and
     * puts the arguments from the server again into argcount and args.
     *
     * @param vpe the vpe to do the delegate for
     * @param caps the capabilities
     * @param args the arguments to pass to the server
     * @return the error code
     */
    Errors::Code delegate_for(VPE &vpe, const KIF::CapRngDesc &caps, KIF::ExchangeArgs *args = nullptr);

    /**
     * Obtains up to <count> capabilities from the server with additional arguments and puts the
     * arguments from the server again into argcount and args.
     *
     * @param count the number of capabilities
     * @param args the arguments to pass to the server
     * @return the received capabilities
     */
    KIF::CapRngDesc obtain(uint count, KIF::ExchangeArgs *args = nullptr) {
        return obtain_for(VPE::self(), count, args);
    }

    /**
     * Obtains up to <count> capabilities from the server for <vpe> with additional arguments and
     * puts the arguments from the server again into argcount and args.
     *
     * @param vpe the vpe to do the obtain for
     * @param count the number of capabilities
     * @param args the arguments to pass to the server
     * @return the received capabilities
     */
    KIF::CapRngDesc obtain_for(VPE &vpe, uint count, KIF::ExchangeArgs *args = nullptr) {
        KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, vpe.alloc_sels(count), count);
        obtain_for(vpe, crd, args);
        return crd;
    }

    /**
     * Obtains up to <crd>.count() capabilities from the server for <vpe> with additional arguments and
     * puts the arguments from the server again into argcount and args.
     *
     * @param vpe the vpe to do the obtain for
     * @param crd the selectors to use
     * @param argcount the number of arguments
     * @param args the arguments to pass to the server
     * @return the error code
     */
    Errors::Code obtain_for(VPE &vpe, const KIF::CapRngDesc &crd, KIF::ExchangeArgs *args = nullptr);

private:
    void connect(const String &name, xfer_t arg, capsel_t sel);
};

}
