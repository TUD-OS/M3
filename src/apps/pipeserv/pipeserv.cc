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

    virtual Errors::Code handle_open(PipeSessionData **sess, word_t arg) override {
        *sess = new PipeSessionData(arg);
        return Errors::NO_ERROR;
    }

    virtual Errors::Code handle_obtain(PipeSessionData *sess, RecvBuf *rcvbuf,
            KIF::Service::ExchangeData &data) override {
        if(!sess->send_gate())
            return base_class_t::handle_obtain(sess, rcvbuf, data);

        if((sess->reader && sess->writer) || data.argcount != 0 || data.caps != 1)
            return Errors::INV_ARGS;

        if(sess->reader == nullptr) {
            sess->reader = new PipeReadHandler(sess);
            KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, sess->reader->sendgate().sel());
            data.caps = crd.value();
        }
        else {
            sess->writer = new PipeWriteHandler(sess);
            KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, sess->writer->sendgate().sel());
            data.caps = crd.value();
        }
        sess->init();
        return Errors::NO_ERROR;
    }

    void attach(GateIStream &is) {
        PipeSessionData *sess = is.gate().session<PipeSessionData>();

        bool reading;
        is >> reading;

        Errors::Code res = reading ? sess->reader->attach(sess) : sess->writer->attach(sess);
        reply_error(is, res);
    }

    void close(GateIStream &is) {
        PipeSessionData *sess = is.gate().session<PipeSessionData>();

        bool reading;
        int id;
        is >> reading >> id;

        Errors::Code res = reading ? sess->reader->close(sess, id) : sess->writer->close(sess, id);
        reply_error(is, res);
    }
};

int main() {
    Server<PipeServiceHandler> srv("pipe", new PipeServiceHandler());
    env()->workloop()->multithreaded(4);
    env()->workloop()->run();
    return 0;
}
