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
#include <m3/vfs/GenericFile.h>

using namespace m3;

static constexpr size_t MSG_SIZE = 64;
static constexpr size_t BUF_SIZE = 256;

class VTermSession {
public:
    explicit VTermSession(RecvGate &rgate)
        : active(false), writing(false), ep(ObjCap::INVALID), sess(ObjCap::INVALID),
          sgate(SendGate::create(&rgate, reinterpret_cast<label_t>(this), MSG_SIZE)),
          mem(MemGate::create_global(BUF_SIZE, MemGate::RW)) {
    }
    explicit VTermSession(RecvGate &rgate, capsel_t srv, capsel_t caps)
        : active(false), writing(false), ep(ObjCap::INVALID), sess(caps + 0, 0),
          sgate(SendGate::create(&rgate, reinterpret_cast<label_t>(this), MSG_SIZE, nullptr, caps + 1)),
          mem(MemGate::create_global(BUF_SIZE, MemGate::RW)) {
        Syscalls::get().createsessat(sess.sel(), srv, reinterpret_cast<word_t>(this));
    }

    bool active;
    bool writing;
    capsel_t ep;
    Session sess;
    SendGate sgate;
    MemGate mem;
};

class VTermHandler;
using base_class = RequestHandler<
    VTermHandler, GenericFile::Operation, GenericFile::Operation::COUNT, VTermSession
>;

static Server<VTermHandler> *srv;

class VTermHandler : public base_class {
public:

    explicit VTermHandler()
        : base_class(),
          _rgate(RecvGate::create(nextlog2<32 * MSG_SIZE>::val, nextlog2<MSG_SIZE>::val)) {
        add_operation(GenericFile::SEEK, &VTermHandler::invalid_op);
        add_operation(GenericFile::STAT, &VTermHandler::invalid_op);
        add_operation(GenericFile::READ, &VTermHandler::read);
        add_operation(GenericFile::WRITE, &VTermHandler::write);

        using std::placeholders::_1;
        _rgate.start(std::bind(&VTermHandler::handle_message, this, _1));
    }

    virtual Errors::Code open(VTermSession **sess, word_t) override {
        *sess = new VTermSession(_rgate);
        return Errors::NONE;
    }

    virtual Errors::Code obtain(VTermSession *sess, KIF::Service::ExchangeData &data) override {
        if(data.args.count != 0 || (data.caps != 1 && data.caps != 2))
            return Errors::INV_ARGS;

        if(data.caps == 1)
            data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sess->sgate.sel(), 1).value();
        else {
            capsel_t caps = VPE::self().alloc_caps(2);
            auto nsess = new VTermSession(_rgate, srv->sel(), caps);
            data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, nsess->sess.sel(), 2).value();
        }
        return Errors::NONE;
    }

    virtual Errors::Code delegate(VTermSession *sess, KIF::Service::ExchangeData &data) override {
        if(data.caps != 1 || data.args.count != 0)
            return Errors::INV_ARGS;

        sess->ep = VPE::self().alloc_cap();
        data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sess->ep, 1).value();
        return Errors::NONE;
    }

    virtual Errors::Code close(VTermSession *sess) override {
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
        VTermSession *sess = is.label<VTermSession*>();

        SLOG(VTERM, fmt((word_t)sess, "p") << " vterm::read()");

        Errors::last = Errors::NONE;

        char buf[BUF_SIZE];
        ssize_t count = Machine::read(buf, sizeof(buf));
        sess->mem.write(buf, static_cast<size_t>(count), 0);

        if(!sess->active) {
            Syscalls::get().activate(sess->ep, sess->mem.sel(), 0);
            sess->active = true;
        }

        sess->writing = false;

        if(Errors::last != Errors::NONE)
            reply_error(is, Errors::last);
        else
            reply_vmsg(is, Errors::NONE, 0, count);
    }

    void write(m3::GateIStream &is) {
        VTermSession *sess = is.label<VTermSession*>();
        size_t submit;
        is >> submit;

        if(submit > BUF_SIZE) {
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        SLOG(VTERM, fmt((word_t)sess, "p") << " vterm::write(submit="
            << submit << ", writing=" << sess->writing << ")");

        if(sess->writing) {
            char buf[BUF_SIZE];
            size_t amount = submit ? submit : BUF_SIZE;
            sess->mem.read(buf, amount, 0);
            Machine::write(buf, amount);
        }

        if(!sess->active) {
            Syscalls::get().activate(sess->ep, sess->mem.sel(), 0);
            sess->active = true;
        }

        sess->writing = submit == 0;

        if(Errors::last != Errors::NONE)
            reply_error(is, Errors::last);
        else if(submit > 0)
            reply_vmsg(is, Errors::NONE, BUF_SIZE);
        else
            reply_vmsg(is, Errors::NONE, static_cast<size_t>(0), BUF_SIZE);
    }

private:
    RecvGate _rgate;
};

int main() {
    srv = new Server<VTermHandler>("vterm", new VTermHandler());
    env()->workloop()->run();
    delete srv;
    return 0;
}
