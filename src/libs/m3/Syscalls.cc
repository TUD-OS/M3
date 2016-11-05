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

#include <base/log/Lib.h>
#include <base/tracing/Tracing.h>
#include <base/Errors.h>
#include <base/Init.h>

#include <m3/com/GateStream.h>
#include <m3/Syscalls.h>

namespace m3 {

INIT_PRIO_SYSC Syscalls Syscalls::_inst;

DTU::Message *Syscalls::send_receive(const void *msg, size_t size) {
    DTU::get().send(_gate.ep(), msg, size, 0, m3::DTU::SYSC_REP);

    DTU::Message *reply;
    do {
        DTU::get().try_sleep(false);

        reply = DTU::get().fetch_msg(m3::DTU::SYSC_REP);
    }
    while(reply == nullptr);

    return reply;
}

Errors::Code Syscalls::send_receive_result(const void *msg, size_t size) {
    DTU::Message *reply = send_receive(msg, size);

    KIF::DefaultReply *rdata = reinterpret_cast<KIF::DefaultReply*>(reply->data);
    Errors::last = static_cast<Errors::Code>(rdata->error);

    DTU::get().mark_read(m3::DTU::SYSC_REP, reinterpret_cast<size_t>(reply));
    return Errors::last;
}

Errors::Code Syscalls::createsrv(capsel_t srv, capsel_t rgate, const String &name) {
    LLOG(SYSC, "createsrv(srv=" << srv << ", rgate=" << rgate << ", name=" << name << ")");

    KIF::Syscall::CreateSrv req;
    req.opcode = KIF::Syscall::CREATE_SRV;
    req.srv = srv;
    req.rgate = rgate;
    req.namelen = Math::min(name.length(), sizeof(req.name));
    memcpy(req.name, name.c_str(), req.namelen);
    size_t msgsize = sizeof(req) - sizeof(req.name) + req.namelen;
    return send_receive_result(&req, Math::round_up(msgsize, DTU_PKG_SIZE));
}

Errors::Code Syscalls::createsess(capsel_t cap, const String &name, word_t arg) {
    LLOG(SYSC, "createsess(cap=" << cap << ", name=" << name << ", arg=" << arg << ")");

    KIF::Syscall::CreateSess req;
    req.opcode = KIF::Syscall::CREATE_SESS;
    req.sess = cap;
    req.arg = arg;
    req.namelen = Math::min(name.length(), sizeof(req.name));
    memcpy(req.name, name.c_str(), req.namelen);
    size_t msgsize = sizeof(req) - sizeof(req.name) + req.namelen;
    return send_receive_result(&req, Math::round_up(msgsize, DTU_PKG_SIZE));
}

Errors::Code Syscalls::createsessat(capsel_t srv, capsel_t sess, word_t ident) {
    LLOG(SYSC, "createsessat(srv=" << srv << ", sess=" << sess << ", ident=" << fmt(ident, "0x") << ")");

    KIF::Syscall::CreateSessAt req;
    req.opcode = KIF::Syscall::CREATE_SESS_AT;
    req.srv = srv;
    req.sess = sess;
    req.ident = ident;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::creatergate(capsel_t rgate, int order, int msgorder) {
    LLOG(SYSC, "creatergate(rgate=" << rgate << ", size=" << fmt(1UL << order, "x")
        << ", msgsize=" << fmt(1UL << msgorder, "x") << ")");

    KIF::Syscall::CreateRGate req;
    req.opcode = KIF::Syscall::CREATE_RGATE;
    req.rgate = rgate;
    req.order = order;
    req.msgorder = msgorder;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::createsgate(capsel_t rgate, capsel_t dst, label_t label, word_t credits) {
    LLOG(SYSC, "createsgate(rgate=" << rgate << ", dst=" << dst << ", label=" << fmt(label, "#x")
        << ", credits=" << credits << ")");

    KIF::Syscall::CreateSGate req;
    req.opcode = KIF::Syscall::CREATE_SGATE;
    req.rgate = rgate;
    req.gate = dst;
    req.label = label;
    req.credits = credits;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::createmgateat(capsel_t cap, uintptr_t addr, size_t size, int perms) {
    LLOG(SYSC, "createmgate(cap=" << cap << ", addr=" << addr << ", size=" << size
        << ", perms=" << perms << ")");

    KIF::Syscall::CreateMGate req;
    req.opcode = KIF::Syscall::CREATE_MGATE;
    req.mem = cap;
    req.addr = addr;
    req.size = size;
    req.perms = perms;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::createmap(capsel_t vpe, capsel_t mem, capsel_t first, capsel_t pages, capsel_t dst, int perms) {
    LLOG(SYSC, "createmap(vpe=" << vpe << ", mem=" << mem << ", first=" << first
        << ", pages=" << pages << ", dst=" << dst << ", perms=" << perms << ")");

    KIF::Syscall::CreateMap req;
    req.opcode = KIF::Syscall::CREATE_MAP;
    req.vpe = vpe;
    req.mem = mem;
    req.first = first;
    req.pages = pages;
    req.dest = dst;
    req.perms = perms;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::createvpe(capsel_t vpe, capsel_t mem, const String &name, PEDesc &pe, capsel_t gate, epid_t ep, bool tmuxable) {
    LLOG(SYSC, "createvpe(vpe=" << vpe << ", mem=" << mem << ", name=" << name
        << ", type=" << static_cast<int>(pe.type()) << ", pfgate=" << gate <<
        ", pfep=" << ep << ", tmuxable=" << tmuxable << ")");

    KIF::Syscall::CreateVPE req;
    req.opcode = KIF::Syscall::CREATE_VPE;
    req.vpe = vpe;
    req.mem = mem;
    req.pe = pe.value();
    req.gate = gate;
    req.ep = ep;
    req.muxable = tmuxable;
    req.namelen = Math::min(name.length(), sizeof(req.name));
    memcpy(req.name, name.c_str(), req.namelen);

    size_t msgsize = sizeof(req) - sizeof(req.name) + req.namelen;
    DTU::Message *msg = send_receive(&req, Math::round_up(msgsize, DTU_PKG_SIZE));
    auto *reply = reinterpret_cast<KIF::Syscall::CreateVPEReply*>(msg->data);

    Errors::last = static_cast<Errors::Code>(reply->error);
    if(Errors::last == Errors::NO_ERROR)
        pe = PEDesc(reply->pe);

    DTU::get().mark_read(m3::DTU::SYSC_REP, reinterpret_cast<size_t>(reply));
    return Errors::last;
}

Errors::Code Syscalls::activate(capsel_t vpe, epid_t ep, capsel_t cap, uintptr_t addr) {
    LLOG(SYSC, "activate(vpe=" << vpe << ", ep=" << ep << ", cap=" << cap << ")");

    KIF::Syscall::Activate req;
    req.opcode = KIF::Syscall::ACTIVATE;
    req.vpe = vpe;
    req.ep = ep;
    req.cap = cap;
    req.addr = addr;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::vpectrl(capsel_t vpe, KIF::Syscall::VPEOp op, word_t *arg) {
    LLOG(SYSC, "vpectrl(vpe=" << vpe << ", op=" << op << ", arg=" << *arg << ")");

    KIF::Syscall::VPECtrl req;
    req.opcode = KIF::Syscall::VPE_CTRL;
    req.vpe = vpe;
    req.op = static_cast<word_t>(op);
    req.arg = *arg;

    DTU::Message *msg = send_receive(&req, sizeof(req));
    auto *reply = reinterpret_cast<KIF::Syscall::VPECtrlReply*>(msg->data);

    Errors::last = static_cast<Errors::Code>(reply->error);
    if(op == KIF::Syscall::VCTRL_WAIT && Errors::last == Errors::NO_ERROR)
        *arg = reply->exitcode;

    DTU::get().mark_read(m3::DTU::SYSC_REP, reinterpret_cast<size_t>(reply));
    return Errors::last;
}

Errors::Code Syscalls::derivemem(capsel_t src, capsel_t dst, size_t offset, size_t size, int perms) {
    LLOG(SYSC, "derivemem(src=" << src << ", dst=" << dst << ", off=" << offset
            << ", size=" << size << ", perms=" << perms << ")");

    KIF::Syscall::DeriveMem req;
    req.opcode = KIF::Syscall::DERIVE_MEM;
    req.src = src;
    req.dst = dst;
    req.offset = offset;
    req.size = size;
    req.perms = perms;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::exchange(capsel_t vpe, const KIF::CapRngDesc &own, capsel_t other, bool obtain) {
    LLOG(SYSC, "exchange(vpe=" << vpe << ", own=" << own << ", other=" << other
        << ", obtain=" << obtain << ")");

    KIF::Syscall::Exchange req;
    req.opcode = KIF::Syscall::EXCHANGE;
    req.vpe = vpe;
    req.own = own.value();
    req.other = other;
    req.obtain = obtain;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::exchangesess(capsel_t sess, const KIF::CapRngDesc &crd,
        size_t *argcount, word_t *args, bool obtain) {
    KIF::Syscall::ExchangeSess req;
    req.opcode = obtain ? KIF::Syscall::OBTAIN : KIF::Syscall::DELEGATE;
    req.sess = sess;
    req.caps = crd.value();
    req.argcount = 0;
    if(argcount) {
        assert(*argcount <= ARRAY_SIZE(req.args));
        req.argcount = *argcount;
        for(size_t i = 0; i < *argcount; ++i)
            req.args[i] = args[i];
    }

    DTU::Message *msg = send_receive(&req, sizeof(req));
    auto *reply = reinterpret_cast<KIF::Syscall::ExchangeSessReply*>(msg->data);

    Errors::last = static_cast<Errors::Code>(reply->error);
    if(Errors::last == Errors::NO_ERROR && argcount) {
        *argcount = reply->argcount;
        for(size_t i = 0; i < *argcount; ++i)
            args[i] = reply->args[i];
    }

    DTU::get().mark_read(m3::DTU::SYSC_REP, reinterpret_cast<size_t>(reply));
    return Errors::last;
}

Errors::Code Syscalls::delegate(capsel_t sess, const KIF::CapRngDesc &crd,
        size_t *argcount, word_t *args) {
    LLOG(SYSC, "delegate(sess=" << sess << ", crd=" << crd << ")");
    return exchangesess(sess, crd, argcount, args, false);
}

Errors::Code Syscalls::obtain(capsel_t sess, const KIF::CapRngDesc &crd,
        size_t *argcount, word_t *args) {
    LLOG(SYSC, "obtain(sess=" << sess << ", crd=" << crd << ")");
    return exchangesess(sess, crd, argcount, args, true);
}

Errors::Code Syscalls::revoke(capsel_t vpe, const KIF::CapRngDesc &crd, bool own) {
    LLOG(SYSC, "revoke(vpe=" << vpe << ", crd=" << crd << ", own=" << own << ")");

    KIF::Syscall::Revoke req;
    req.opcode = KIF::Syscall::REVOKE;
    req.vpe = vpe;
    req.crd = crd.value();
    req.own = own;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::forwardmsg(capsel_t cap, const void *msg, size_t len, epid_t rep, label_t rlabel, void *event) {
    LLOG(SYSC, "forwardmsg(cap=" << cap << ", msg=" << msg << ", len=" << len << ", rep=" << rep
        << ", rlabel=" << fmt(rlabel, "0x") << ", event=" << event << ")");

    KIF::Syscall::ForwardMsg req;
    req.opcode = KIF::Syscall::FORWARD_MSG;
    req.cap = cap;
    req.repid = rep;
    req.rlabel = rlabel;
    req.event = reinterpret_cast<word_t>(event);
    req.len = len;
    memcpy(req.msg, msg, Math::min(sizeof(req.msg), len));
    size_t msgsize = sizeof(req) - sizeof(req.msg) + req.len;
    return send_receive_result(&req, Math::round_up(msgsize, DTU_PKG_SIZE));
}

Errors::Code Syscalls::forwardmem(capsel_t cap, void *data, size_t len, size_t offset, uint flags, void *event) {
    LLOG(SYSC, "forwardmem(cap=" << cap << ", data=" << data << ", len=" << len
        << ", offset=" << offset << ", flags=" << fmt(flags, "0x") << ", event=" << event << ")");

    KIF::Syscall::ForwardMem req;
    req.opcode = KIF::Syscall::FORWARD_MEM;
    req.cap = cap;
    req.offset = offset;
    req.flags = flags;
    req.event = reinterpret_cast<word_t>(event);
    req.len = len;
    if(flags & KIF::Syscall::ForwardMem::WRITE)
        memcpy(req.data, data, Math::min(sizeof(req.data), len));

    DTU::Message *msg = send_receive(&req, sizeof(req) - sizeof(req.data) + req.len);
    auto *reply = reinterpret_cast<KIF::Syscall::ForwardMemReply*>(msg->data);

    Errors::last = static_cast<Errors::Code>(reply->error);
    if(Errors::last == Errors::NO_ERROR && (~flags & KIF::Syscall::ForwardMem::WRITE))
        memcpy(data, reply->data, len);

    DTU::get().mark_read(m3::DTU::SYSC_REP, reinterpret_cast<size_t>(reply));
    return Errors::last;
}

Errors::Code Syscalls::forwardreply(capsel_t cap, const void *msg, size_t len, uintptr_t msgaddr, void *event) {
    LLOG(SYSC, "forwardreply(cap=" << cap << ", msg=" << msg << ", len=" << len
        << ", msgaddr=" << (void*)msgaddr << ", event=" << event << ")");

    KIF::Syscall::ForwardReply req;
    req.opcode = KIF::Syscall::FORWARD_REPLY;
    req.cap = cap;
    req.msgaddr = msgaddr;
    req.event = reinterpret_cast<word_t>(event);
    req.len = len;
    memcpy(req.msg, msg, Math::min(sizeof(req.msg), len));
    size_t msgsize = sizeof(req) - sizeof(req.msg) + req.len;
    return send_receive_result(&req, Math::round_up(msgsize, DTU_PKG_SIZE));
}

Errors::Code Syscalls::noop() {
    KIF::Syscall::Noop req;
    req.opcode = KIF::Syscall::NOOP;
    return send_receive_result(&req, sizeof(req));
}

// the USED seems to be necessary, because the libc calls it and LTO removes it otherwise
USED void Syscalls::exit(int exitcode) {
    LLOG(SYSC, "exit(code=" << exitcode << ")");

    EVENT_TRACE_FLUSH();

    KIF::Syscall::VPECtrl req;
    req.opcode = KIF::Syscall::VPE_CTRL;
    req.vpe = 0;
    req.op = KIF::Syscall::VCTRL_STOP;
    req.arg = exitcode;
    DTU::get().send(_gate.ep(), &req, sizeof(req), 0, m3::DTU::SYSC_REP);
}

}
