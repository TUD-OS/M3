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

Errors::Code Syscalls::createsrv(capsel_t dst, capsel_t rgate, const String &name) {
    LLOG(SYSC, "createsrv(dst=" << dst << ", rgate=" << rgate << ", name=" << name << ")");

    KIF::Syscall::CreateSrv req;
    req.opcode = KIF::Syscall::CREATE_SRV;
    req.dst_sel = dst;
    req.rgate_sel = rgate;
    req.namelen = Math::min(name.length(), sizeof(req.name));
    memcpy(req.name, name.c_str(), req.namelen);
    size_t msgsize = sizeof(req) - sizeof(req.name) + req.namelen;
    return send_receive_result(&req, Math::round_up(msgsize, DTU_PKG_SIZE));
}

Errors::Code Syscalls::createsess(capsel_t dst, capsel_t srv, word_t ident) {
    LLOG(SYSC, "createsessat(dst=" << dst << ", srv=" << srv << ", ident=" << fmt(ident, "0x") << ")");

    KIF::Syscall::CreateSess req;
    req.opcode = KIF::Syscall::CREATE_SESS;
    req.dst_sel = dst;
    req.srv_sel = srv;
    req.ident = ident;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::creatergate(capsel_t dst, int order, int msgorder) {
    LLOG(SYSC, "creatergate(dst=" << dst << ", size=" << fmt(1UL << order, "x")
        << ", msgsize=" << fmt(1UL << msgorder, "x") << ")");

    KIF::Syscall::CreateRGate req;
    req.opcode = KIF::Syscall::CREATE_RGATE;
    req.dst_sel = dst;
    req.order = static_cast<xfer_t>(order);
    req.msgorder = static_cast<xfer_t>(msgorder);
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::createsgate(capsel_t dst, capsel_t rgate, label_t label, word_t credits) {
    LLOG(SYSC, "createsgate(dst=" << dst << ", rgate=" << rgate << ", label=" << fmt(label, "#x")
        << ", credits=" << credits << ")");

    KIF::Syscall::CreateSGate req;
    req.opcode = KIF::Syscall::CREATE_SGATE;
    req.dst_sel = dst;
    req.rgate_sel = rgate;
    req.label = label;
    req.credits = credits;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::createmgate(capsel_t dst, goff_t addr, size_t size, int perms) {
    LLOG(SYSC, "createmgate(dst=" << dst << ", addr=" << addr << ", size=" << size
        << ", perms=" << perms << ")");

    KIF::Syscall::CreateMGate req;
    req.opcode = KIF::Syscall::CREATE_MGATE;
    req.dst_sel = dst;
    req.addr = addr;
    req.size = size;
    req.perms = static_cast<xfer_t>(perms);
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::createmap(capsel_t dst, capsel_t vpe, capsel_t mgate, capsel_t first,
                                 capsel_t pages, int perms) {
    LLOG(SYSC, "createmap(dst=" << dst << ", vpe=" << vpe << ", mgate=" << mgate
        << ", first=" << first << ", pages=" << pages << ", perms=" << perms << ")");

    KIF::Syscall::CreateMap req;
    req.opcode = KIF::Syscall::CREATE_MAP;
    req.dst_sel = dst;
    req.vpe_sel = vpe;
    req.mgate_sel = mgate;
    req.first = first;
    req.pages = pages;
    req.perms = static_cast<xfer_t>(perms);
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::createvpegrp(capsel_t dst) {
    LLOG(SYSC, "createvpegrp(dst=" << dst << ")");

    KIF::Syscall::CreateVPEGrp req;
    req.opcode = KIF::Syscall::CREATE_VPEGRP;
    req.dst_sel = dst;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::createvpe(const KIF::CapRngDesc &dst, capsel_t sgate, const String &name,
                                 PEDesc &pe, epid_t sep, epid_t rep, bool tmuxable, capsel_t group) {
    LLOG(SYSC, "createvpe(dst=" << dst << ", sgate=" << sgate
        << ", name=" << name << ", type=" << static_cast<int>(pe.type())
        << ", sep=" << sep << ", rep=" << rep << ", tmuxable=" << tmuxable
        << ", group=" << group << ")");

    KIF::Syscall::CreateVPE req;
    req.opcode = KIF::Syscall::CREATE_VPE;
    req.dst_crd = dst.value();
    req.sgate_sel = sgate;
    req.pe = pe.value();
    req.sep = sep;
    req.rep = rep;
    req.muxable = tmuxable;
    req.group_sel = group;
    req.namelen = Math::min(name.length(), sizeof(req.name));
    memcpy(req.name, name.c_str(), req.namelen);

    size_t msgsize = sizeof(req) - sizeof(req.name) + req.namelen;
    DTU::Message *msg = send_receive(&req, Math::round_up(msgsize, DTU_PKG_SIZE));
    auto *reply = reinterpret_cast<KIF::Syscall::CreateVPEReply*>(msg->data);

    Errors::last = static_cast<Errors::Code>(reply->error);
    if(Errors::last == Errors::NONE)
        pe = PEDesc(reply->pe);

    DTU::get().mark_read(m3::DTU::SYSC_REP, reinterpret_cast<size_t>(reply));
    return Errors::last;
}

Errors::Code Syscalls::activate(capsel_t ep, capsel_t gate, goff_t addr) {
    LLOG(SYSC, "activate(ep=" << ep << ", gate=" << gate << ")");

    KIF::Syscall::Activate req;
    req.opcode = KIF::Syscall::ACTIVATE;
    req.ep_sel = ep;
    req.gate_sel = gate;
    req.addr = addr;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::vpectrl(capsel_t vpe, KIF::Syscall::VPEOp op, xfer_t arg) {
    LLOG(SYSC, "vpectrl(vpe=" << vpe << ", op=" << op << ", arg=" << arg << ")");

    KIF::Syscall::VPECtrl req;
    req.opcode = KIF::Syscall::VPE_CTRL;
    req.vpe_sel = vpe;
    req.op = static_cast<xfer_t>(op);
    req.arg = arg;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::vpewait(const capsel_t *vpes, size_t count, capsel_t *vpe, int *exitcode) {
    LLOG(SYSC, "vpewait(vpes=" << vpes << ")");

    KIF::Syscall::VPEWait req;
    req.opcode = KIF::Syscall::VPE_WAIT;
    req.vpe_count = count;
    for(size_t i = 0; i < count; ++i)
        req.sels[i] = vpes[i];

    DTU::Message *msg = send_receive(&req, sizeof(req));
    auto *reply = reinterpret_cast<KIF::Syscall::VPEWaitReply*>(msg->data);

    Errors::last = static_cast<Errors::Code>(reply->error);
    if(Errors::last == Errors::NONE) {
        *vpe = reply->vpe_sel;
        *exitcode = reply->exitcode;
    }

    DTU::get().mark_read(m3::DTU::SYSC_REP, reinterpret_cast<size_t>(reply));
    return Errors::last;
}

Errors::Code Syscalls::derivemem(capsel_t dst, capsel_t src, goff_t offset, size_t size, int perms) {
    LLOG(SYSC, "derivemem(dst=" << dst << ", src=" << src << ", off=" << offset
            << ", size=" << size << ", perms=" << perms << ")");

    KIF::Syscall::DeriveMem req;
    req.opcode = KIF::Syscall::DERIVE_MEM;
    req.dst_sel = dst;
    req.src_sel = src;
    req.offset = offset;
    req.size = size;
    req.perms = static_cast<xfer_t>(perms);
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::opensess(capsel_t dst, const String &name, xfer_t arg) {
    LLOG(SYSC, "opensess(dst=" << dst << ", name=" << name << ", arg=" << arg << ")");

    KIF::Syscall::OpenSess req;
    req.opcode = KIF::Syscall::OPEN_SESS;
    req.dst_sel = dst;
    req.arg = arg;
    req.namelen = Math::min(name.length(), sizeof(req.name));
    memcpy(req.name, name.c_str(), req.namelen);
    size_t msgsize = sizeof(req) - sizeof(req.name) + req.namelen;
    return send_receive_result(&req, Math::round_up(msgsize, DTU_PKG_SIZE));
}

Errors::Code Syscalls::exchange(capsel_t vpe, const KIF::CapRngDesc &own, capsel_t other, bool obtain) {
    LLOG(SYSC, "exchange(vpe=" << vpe << ", own=" << own << ", other=" << other
        << ", obtain=" << obtain << ")");

    KIF::Syscall::Exchange req;
    req.opcode = KIF::Syscall::EXCHANGE;
    req.vpe_sel = vpe;
    req.own_crd = own.value();
    req.other_sel = other;
    req.obtain = obtain;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::exchangesess(capsel_t vpe, capsel_t sess, const KIF::CapRngDesc &crd,
                                    KIF::ExchangeArgs *args, bool obtain) {
    KIF::Syscall::ExchangeSess req;
    req.opcode = obtain ? KIF::Syscall::OBTAIN : KIF::Syscall::DELEGATE;
    req.vpe_sel = vpe;
    req.sess_sel = sess;
    req.crd = crd.value();
    if(args)
        memcpy(&req.args, args, sizeof(*args));
    else
        req.args.count = 0;

    DTU::Message *msg = send_receive(&req, sizeof(req));
    auto *reply = reinterpret_cast<KIF::Syscall::ExchangeSessReply*>(msg->data);

    Errors::last = static_cast<Errors::Code>(reply->error);
    if(Errors::last == Errors::NONE && args)
        memcpy(args, &reply->args, sizeof(*args));

    DTU::get().mark_read(m3::DTU::SYSC_REP, reinterpret_cast<size_t>(reply));
    return Errors::last;
}

Errors::Code Syscalls::delegate(capsel_t vpe, capsel_t sess, const KIF::CapRngDesc &crd,
                                KIF::ExchangeArgs *args) {
    LLOG(SYSC, "delegate(vpe=" << vpe << ", sess=" << sess << ", crd=" << crd << ")");
    return exchangesess(vpe, sess, crd, args, false);
}

Errors::Code Syscalls::obtain(capsel_t vpe, capsel_t sess, const KIF::CapRngDesc &crd,
                              KIF::ExchangeArgs *args) {
    LLOG(SYSC, "obtain(vpe=" << vpe << ", sess=" << sess << ", crd=" << crd << ")");
    return exchangesess(vpe, sess, crd, args, true);
}

Errors::Code Syscalls::revoke(capsel_t vpe, const KIF::CapRngDesc &crd, bool own) {
    LLOG(SYSC, "revoke(vpe=" << vpe << ", crd=" << crd << ", own=" << own << ")");

    KIF::Syscall::Revoke req;
    req.opcode = KIF::Syscall::REVOKE;
    req.vpe_sel = vpe;
    req.crd = crd.value();
    req.own = own;
    return send_receive_result(&req, sizeof(req));
}

Errors::Code Syscalls::forwardmsg(capsel_t sgate, capsel_t rgate, const void *msg, size_t len,
                                  label_t rlabel, event_t event) {
    LLOG(SYSC, "forwardmsg(sgate=" << sgate << ", rgate=" << rgate << ", msg=" << msg
        << ", len=" << len << ", rlabel=" << fmt(rlabel, "0x") << ", event=" << fmt(event, "0x") << ")");

    KIF::Syscall::ForwardMsg req;
    req.opcode = KIF::Syscall::FORWARD_MSG;
    req.sgate_sel = sgate;
    req.rgate_sel = rgate;
    req.rlabel = rlabel;
    req.event = event;
    req.len = len;
    memcpy(req.msg, msg, Math::min(sizeof(req.msg), len));
    size_t msgsize = sizeof(req) - sizeof(req.msg) + req.len;
    return send_receive_result(&req, Math::round_up(msgsize, DTU_PKG_SIZE));
}

Errors::Code Syscalls::forwardmem(capsel_t mgate, void *data, size_t len, goff_t offset,
                                  uint flags, event_t event) {
    LLOG(SYSC, "forwardmem(mgate=" << mgate << ", data=" << data << ", len=" << len
        << ", offset=" << offset << ", flags=" << fmt(flags, "0x") << ", event=" << fmt(event, "0x") << ")");

    KIF::Syscall::ForwardMem req;
    req.opcode = KIF::Syscall::FORWARD_MEM;
    req.mgate_sel = mgate;
    req.offset = offset;
    req.flags = flags;
    req.event = event;
    req.len = len;
    if(flags & KIF::Syscall::ForwardMem::WRITE)
        memcpy(req.data, data, Math::min(sizeof(req.data), len));

    DTU::Message *msg = send_receive(&req, sizeof(req) - sizeof(req.data) + req.len);
    auto *reply = reinterpret_cast<KIF::Syscall::ForwardMemReply*>(msg->data);

    Errors::last = static_cast<Errors::Code>(reply->error);
    if(Errors::last == Errors::NONE && (~flags & KIF::Syscall::ForwardMem::WRITE))
        memcpy(data, reply->data, len);

    DTU::get().mark_read(m3::DTU::SYSC_REP, reinterpret_cast<size_t>(reply));
    return Errors::last;
}

Errors::Code Syscalls::forwardreply(capsel_t rgate, const void *msg, size_t len,
                                    goff_t msgaddr, event_t event) {
    LLOG(SYSC, "forwardreply(rgate=" << rgate << ", msg=" << msg << ", len=" << len
        << ", msgaddr=" << (void*)msgaddr << ", event=" << fmt(event, "0x") << ")");

    KIF::Syscall::ForwardReply req;
    req.opcode = KIF::Syscall::FORWARD_REPLY;
    req.rgate_sel = rgate;
    req.msgaddr = msgaddr;
    req.event = event;
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
    req.vpe_sel = 0;
    req.op = KIF::Syscall::VCTRL_STOP;
    req.arg = static_cast<xfer_t>(exitcode);
    DTU::get().send(_gate.ep(), &req, sizeof(req), 0, m3::DTU::SYSC_REP);
}

}
