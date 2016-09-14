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
    DTU::get().send(_gate.epid(), msg, size, _rlabel, _rep);

    DTU::Message *reply;
    do {
        DTU::get().try_sleep(false);

        reply = DTU::get().fetch_msg(_rep);
    }
    while(reply == nullptr);

    return reply;
}

Errors::Code Syscalls::send_receive_result(const void *msg, size_t size) {
    DTU::Message *reply = send_receive(msg, size);

    KIF::Syscall::DefaultReply *rdata = reinterpret_cast<KIF::Syscall::DefaultReply*>(reply->data);
    Errors::last = static_cast<Errors::Code>(rdata->error);

    DTU::get().mark_read(_rep, reinterpret_cast<size_t>(reply));
    return Errors::last;
}

Errors::Code Syscalls::noop() {
    KIF::Syscall::Noop req;
    req.opcode = KIF::Syscall::NOOP;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::activate(size_t ep, capsel_t oldcap, capsel_t newcap) {
    LLOG(SYSC, "activate(ep=" << ep << ", oldcap=" << oldcap << ", newcap=" << newcap << ")");

    KIF::Syscall::Activate req;
    req.opcode = KIF::Syscall::ACTIVATE;
    req.ep = ep;
    req.old_sel = oldcap;
    req.new_sel = newcap;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::activatereply(size_t ep, uintptr_t msgaddr) {
    LLOG(SYSC, "activate(ep=" << ep << ", oldcap=" << (void*)msgaddr << ")");

    KIF::Syscall::ActivateReply req;
    req.opcode = KIF::Syscall::ACTIVATEREPLY;
    req.ep = ep;
    req.msg_addr = msgaddr;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::createsrv(capsel_t gate, capsel_t srv, const String &name) {
    LLOG(SYSC, "createsrv(gate=" << gate << ", srv=" << srv << ", name=" << name << ")");

    KIF::Syscall::CreateSrv req;
    req.opcode = KIF::Syscall::CREATESRV;
    req.gate = gate;
    req.srv = srv;
    req.namelen = Math::min(name.length(), sizeof(req.name));
    memcpy(req.name, name.c_str(), req.namelen);
    return send_receive_result(&req, sizeof(req) - sizeof(req.name) + req.namelen);
}

Errors::Code Syscalls::createsessat(capsel_t srv, capsel_t sess, word_t ident) {
    LLOG(SYSC, "createsessat(srv=" << srv << ", sess=" << sess << ", ident=" << fmt(ident, "0x") << ")");

    KIF::Syscall::CreateSessAt req;
    req.opcode = KIF::Syscall::CREATESESSAT;
    req.srv = srv;
    req.sess = sess;
    req.ident = ident;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::createsess(capsel_t vpe, capsel_t cap, const String &name, word_t arg) {
    LLOG(SYSC, "createsess(vpe=" << vpe << ", cap=" << cap << ", name=" << name
        << ", arg=" << arg << ")");

    KIF::Syscall::CreateSess req;
    req.opcode = KIF::Syscall::CREATESESS;
    req.vpe = vpe;
    req.sess = cap;
    req.arg = arg;
    req.namelen = Math::min(name.length(), sizeof(req.name));
    memcpy(req.name, name.c_str(), req.namelen);
    return send_receive_result(&req, sizeof(req) - sizeof(req.name) + req.namelen);
}

Errors::Code Syscalls::creategate(capsel_t vpe, capsel_t dst, label_t label, size_t ep, word_t credits) {
    LLOG(SYSC, "creategate(vpe=" << vpe << ", dst=" << dst << ", label=" << fmt(label, "#x")
        << ", ep=" << ep << ", credits=" << credits << ")");

    KIF::Syscall::CreateGate req;
    req.opcode = KIF::Syscall::CREATEGATE;
    req.vpe = vpe;
    req.gate = dst;
    req.label = label;
    req.ep = ep;
    req.credits = credits;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::createmap(capsel_t vpe, capsel_t mem, capsel_t first, capsel_t pages, capsel_t dst, int perms) {
    LLOG(SYSC, "createmap(vpe=" << vpe << ", mem=" << mem << ", first=" << first
        << ", pages=" << pages << ", dst=" << dst << ", perms=" << perms << ")");

    KIF::Syscall::CreateMap req;
    req.opcode = KIF::Syscall::CREATEMAP;
    req.vpe = vpe;
    req.mem = mem;
    req.first = first;
    req.pages = pages;
    req.dest = dst;
    req.perms = perms;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::createvpe(capsel_t vpe, capsel_t mem, const String &name, PEDesc &pe, capsel_t gate, size_t ep, bool tmuxable) {
    LLOG(SYSC, "createvpe(vpe=" << vpe << ", mem=" << mem << ", name=" << name
        << ", type=" << static_cast<int>(pe.type()) << ", pfgate=" << gate <<
        ", pfep=" << ep << ", tmuxable=" << tmuxable << ")");

    KIF::Syscall::CreateVPE req;
    req.opcode = KIF::Syscall::CREATEVPE;
    req.vpe = vpe;
    req.mem = mem;
    req.pe = pe.value();
    req.gate = gate;
    req.ep = ep;
    req.muxable = tmuxable;
    req.namelen = Math::min(name.length(), sizeof(req.name));
    memcpy(req.name, name.c_str(), req.namelen);

    DTU::Message *msg = send_receive(&req, sizeof(req) - sizeof(req.name) + req.namelen);
    auto *reply = reinterpret_cast<KIF::Syscall::CreateVPEReply*>(msg->data);

    Errors::last = static_cast<Errors::Code>(reply->error);
    if(Errors::last == Errors::NO_ERROR)
        pe = PEDesc(reply->pe);

    DTU::get().mark_read(_rep, reinterpret_cast<size_t>(reply));
    return Errors::last;
}

Errors::Code Syscalls::attachrb(capsel_t vpe, size_t ep, uintptr_t addr, int order, int msgorder, uint flags) {
    LLOG(SYSC, "attachrb(vpe=" << vpe << ", ep=" << ep << ", addr=" << fmt(addr, "p")
        << ", size=" << fmt(1UL << order, "x") << ", msgsize=" << fmt(1UL << msgorder, "x")
        << ", flags=" << fmt(flags, "x") << ")");

    KIF::Syscall::AttachRB req;
    req.opcode = KIF::Syscall::ATTACHRB;
    req.vpe = vpe;
    req.ep = ep;
    req.addr = addr;
    req.order = order;
    req.msgorder = msgorder;
    req.flags = flags;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::detachrb(capsel_t vpe, size_t ep) {
    LLOG(SYSC, "detachrb(vpe=" << vpe << ", ep=" << ep << ")");

    KIF::Syscall::DetachRB req;
    req.opcode = KIF::Syscall::DETACHRB;
    req.vpe = vpe;
    req.ep = ep;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::exchange(capsel_t vpe, const KIF::CapRngDesc &own, const KIF::CapRngDesc &other, bool obtain) {
    LLOG(SYSC, "exchange(vpe=" << vpe << ", own=" << own << ", other=" << other
        << ", obtain=" << obtain << ")");

    KIF::Syscall::Exchange req;
    req.opcode = KIF::Syscall::EXCHANGE;
    req.vpe = vpe;
    req.own = own.value();
    req.other = other.value();
    req.obtain = obtain;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::vpectrl(capsel_t vpe, KIF::Syscall::VPEOp op, int pid, int *exitcode) {
    LLOG(SYSC, "vpectrl(vpe=" << vpe << ", op=" << op << ", pid=" << pid << ")");

    KIF::Syscall::VPECtrl req;
    req.opcode = KIF::Syscall::VPECTRL;
    req.vpe = vpe;
    req.op = static_cast<word_t>(op);
    req.pid = pid;

    DTU::Message *msg = send_receive(&req, sizeof(req));
    auto *reply = reinterpret_cast<KIF::Syscall::VPECtrlReply*>(msg->data);

    Errors::last = static_cast<Errors::Code>(reply->error);
    if(op == KIF::Syscall::VCTRL_WAIT && Errors::last == Errors::NO_ERROR)
        *exitcode = reply->exitcode;

    DTU::get().mark_read(_rep, reinterpret_cast<size_t>(reply));
    return Errors::last;
}

Errors::Code Syscalls::exchangesess(capsel_t vpe, capsel_t sess, const KIF::CapRngDesc &crd,
        size_t *argcount, word_t *args, bool obtain) {
    KIF::Syscall::ExchangeSess req;
    req.opcode = obtain ? KIF::Syscall::OBTAIN : KIF::Syscall::DELEGATE;
    req.vpe = vpe;
    req.sess = sess;
    req.caps = crd.value();
    assert(*argcount <= ARRAY_SIZE(req.args));
    req.argcount = *argcount;
    for(size_t i = 0; i < *argcount; ++i)
        req.args[i] = args[i];

    DTU::Message *msg = send_receive(&req, sizeof(req));
    auto *reply = reinterpret_cast<KIF::Syscall::ExchangeSessReply*>(msg->data);

    Errors::last = static_cast<Errors::Code>(reply->error);
    if(Errors::last == Errors::NO_ERROR) {
        *argcount = reply->argcount;
        for(size_t i = 0; i < *argcount; ++i)
            args[i] = reply->args[i];
    }

    DTU::get().mark_read(_rep, reinterpret_cast<size_t>(reply));
    return Errors::last;
}

Errors::Code Syscalls::delegate(capsel_t vpe, capsel_t sess, const KIF::CapRngDesc &crd,
        size_t *argcount, word_t *args) {
    LLOG(SYSC, "delegate(vpe=" << vpe << ", sess=" << sess << ", crd=" << crd << ")");
    return exchangesess(vpe, sess, crd, argcount, args, false);
}

Errors::Code Syscalls::obtain(capsel_t vpe, capsel_t sess, const KIF::CapRngDesc &crd,
        size_t *argcount, word_t *args) {
    LLOG(SYSC, "obtain(vpe=" << vpe << ", sess=" << sess << ", crd=" << crd << ")");
    return exchangesess(vpe, sess, crd, argcount, args, true);
}

Errors::Code Syscalls::reqmemat(capsel_t cap, uintptr_t addr, size_t size, int perms) {
    LLOG(SYSC, "reqmem(cap=" << cap << ", addr=" << addr << ", size=" << size
        << ", perms=" << perms << ")");

    KIF::Syscall::ReqMem req;
    req.opcode = KIF::Syscall::REQMEM;
    req.mem = cap;
    req.addr = addr;
    req.size = size;
    req.perms = perms;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::derivemem(capsel_t src, capsel_t dst, size_t offset, size_t size, int perms) {
    LLOG(SYSC, "derivemem(src=" << src << ", dst=" << dst << ", off=" << offset
            << ", size=" << size << ", perms=" << perms << ")");

    KIF::Syscall::DeriveMem req;
    req.opcode = KIF::Syscall::DERIVEMEM;
    req.src = src;
    req.dst = dst;
    req.offset = offset;
    req.size = size;
    req.perms = perms;
    return send_receive_result(&req, sizeof(req));
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

// the USED seems to be necessary, because the libc calls it and LTO removes it otherwise
USED void Syscalls::exit(int exitcode) {
    LLOG(SYSC, "exit(code=" << exitcode << ")");

    EVENT_TRACE_FLUSH();

    KIF::Syscall::Exit req;
    req.opcode = KIF::Syscall::EXIT;
    req.exitcode = exitcode;
    DTU::get().send(_gate.epid(), &req, sizeof(req), _rlabel, _rep);
}

}
