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

#include <m3/com/MemGate.h>
#include <m3/server/Server.h>
#include <m3/server/RequestHandler.h>
#include <m3/session/Pipe.h>

#include "Session.h"

using namespace m3;

class PipeServiceHandler;
using base_class = RequestHandler<
    PipeServiceHandler, GenericFile::Operation, GenericFile::Operation::COUNT, PipeSession
>;

static Server<PipeServiceHandler> *srv;

class PipeServiceHandler : public base_class {
public:
    static constexpr size_t MSG_SIZE = 64;

    explicit PipeServiceHandler()
        : base_class(),
          _rgate(RecvGate::create(nextlog2<32 * MSG_SIZE>::val, nextlog2<MSG_SIZE>::val)) {
        add_operation(GenericFile::SEEK, &PipeServiceHandler::invalid_op);
        add_operation(GenericFile::STAT, &PipeServiceHandler::invalid_op);
        add_operation(GenericFile::READ, &PipeServiceHandler::read);
        add_operation(GenericFile::WRITE, &PipeServiceHandler::write);

        using std::placeholders::_1;
        _rgate.start(std::bind(&PipeServiceHandler::handle_message, this, _1));
    }

    virtual Errors::Code open(PipeSession **sess, capsel_t srv_sel, word_t arg) override {
        *sess = new PipeData(srv_sel, &_rgate, arg);
        return Errors::NONE;
    }

    virtual Errors::Code obtain(PipeSession *sess, KIF::Service::ExchangeData &data) override {
        if(data.caps != 2)
            return Errors::INV_ARGS;

        PipeChannel *nchan;
        if(sess->type() == PipeSession::META) {
            if(data.args.count != 1)
                return Errors::INV_ARGS;
            nchan = static_cast<PipeData*>(sess)->attach(srv->sel(), data.args.vals[0]);
        }
        else
            nchan = static_cast<PipeChannel*>(sess)->clone(srv->sel());
        data.caps = nchan->crd().value();
        return Errors::NONE;
    }

    virtual Errors::Code delegate(PipeSession *sess, KIF::Service::ExchangeData &data) override {
        if(sess->type() == PipeSession::META) {
            if(data.caps != 1 || data.args.count != 0 || static_cast<PipeData*>(sess)->memory)
                return Errors::INV_ARGS;

            capsel_t sel = VPE::self().alloc_sel();
            static_cast<PipeData*>(sess)->memory = new MemGate(MemGate::bind(sel));
            data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sel, data.caps).value();
        }
        else {
            if(data.caps != 1 || data.args.count != 0)
                return Errors::INV_ARGS;

            capsel_t sel = VPE::self().alloc_sel();
            static_cast<PipeChannel*>(sess)->set_ep(sel);
            data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sel, data.caps).value();
        }
        return Errors::NONE;
    }

    virtual Errors::Code close(PipeSession *sess) override {
        sess->close();
        delete sess;
        return Errors::NONE;
    }

    virtual void shutdown() override {
        _rgate.stop();
    }

    void invalid_op(GateIStream &is) {
        reply_vmsg(is, m3::Errors::NOT_SUP);
    }

    void read(m3::GateIStream &is) {
        PipeSession *sess = is.label<PipeSession*>();
        size_t submit;
        is >> submit;

        sess->read(is, submit);
    }

    void write(m3::GateIStream &is) {
        PipeSession *sess = is.label<PipeSession*>();
        size_t submit;
        is >> submit;

        sess->write(is, submit);
    }

private:
    RecvGate _rgate;
};

int main() {
    srv = new Server<PipeServiceHandler>("pipe", new PipeServiceHandler());
    env()->workloop()->multithreaded(4);
    env()->workloop()->run();
    delete srv;
    return 0;
}
