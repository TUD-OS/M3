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

#include <base/log/Services.h>

#include <m3/server/Server.h>
#include <m3/server/RequestHandler.h>
#include <m3/session/Pipe.h>

#include "Session.h"

using namespace m3;

class PipeServiceHandler;
using base_class_t = RequestHandler<
    PipeServiceHandler, Pipe::MetaOp, Pipe::MetaOp::COUNT, PipeSessionData
>;

class PipeServiceHandler : public base_class_t {
public:
    explicit PipeServiceHandler() : base_class_t() {
        add_operation(Pipe::ATTACH, &PipeServiceHandler::attach);
        add_operation(Pipe::CLOSE, &PipeServiceHandler::close);
    }

    virtual size_t credits() override {
        return 64;
    }

    virtual PipeSessionData *handle_open(GateIStream &args) override {
        size_t size;
        args >> size;
        PipeSessionData *sess = add_session(new PipeSessionData(size));
        reply_vmsg(args, Errors::NO_ERROR, sess);
        return sess;
    }

    virtual void handle_obtain(PipeSessionData *sess, RecvBuf *rcvbuf, GateIStream &args,
            uint capcount) override {
        if(!sess->send_gate()) {
            base_class_t::handle_obtain(sess, rcvbuf, args, capcount);
            return;
        }
        if((sess->reader && sess->writer) || capcount != 1) {
            reply_vmsg(args, Errors::INV_ARGS);
            return;
        }

        if(sess->reader == nullptr) {
            sess->reader = new PipeReadHandler(sess);
            reply_vmsg(args, Errors::NO_ERROR, CapRngDesc(CapRngDesc::OBJ,
                sess->reader->sendgate().sel()));
        }
        else {
            sess->writer = new PipeWriteHandler(sess);
            reply_vmsg(args, Errors::NO_ERROR, CapRngDesc(CapRngDesc::OBJ,
                sess->writer->sendgate().sel()));
        }
    }

    void attach(GateIStream &is) {
        PipeSessionData *sess = is.gate().session<PipeSessionData>();

        bool reading;
        is >> reading;

        Errors::Code res = reading ? sess->reader->attach(sess) : sess->writer->attach(sess);
        reply_vmsg(is, res);
    }

    void close(GateIStream &is) {
        PipeSessionData *sess = is.gate().session<PipeSessionData>();

        bool reading;
        int id;
        is >> reading >> id;

        Errors::Code res = reading ? sess->reader->close(sess, id) : sess->writer->close(sess, id);
        reply_vmsg(is, res);
    }
};

int main() {
    Server<PipeServiceHandler> srv("pipe", new PipeServiceHandler());
    env()->workloop()->run();
    return 0;
}
