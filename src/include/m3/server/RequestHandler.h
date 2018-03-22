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

#include <base/tracing/Tracing.h>
#include <base/Errors.h>

#include <m3/com/GateStream.h>
#include <m3/server/Handler.h>

namespace m3 {

template<typename CLS, typename OP, size_t OPCNT, class SESS>
class RequestHandler : public Handler<SESS> {
    template<class HDL>
    friend class Server;

    using handler_func = void (CLS::*)(GateIStream &is);

public:
    explicit RequestHandler()
        : Handler<SESS>(),
        _callbacks() {
    }

    void add_operation(OP op, handler_func func) {
        _callbacks[op] = func;
    }

    void handle_message(GateIStream &msg) {
        EVENT_TRACER_Service_request();
        OP op;
        msg >> op;
        if(static_cast<size_t>(op) < sizeof(_callbacks) / sizeof(_callbacks[0])) {
            (static_cast<CLS*>(this)->*_callbacks[op])(msg);
            return;
        }

        reply_error(msg, Errors::INV_ARGS);
    }

private:
    handler_func _callbacks[OPCNT];
};

}
