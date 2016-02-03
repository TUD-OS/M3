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

#include <m3/tracing/Tracing.h>
#include <m3/Syscalls.h>
#include <m3/GateStream.h>
#include <m3/Errors.h>
#include <m3/Log.h>

namespace m3 {

Syscalls Syscalls::_inst INIT_PRIORITY(108);

Errors::Code Syscalls::finish(GateIStream &&reply) {
    if(reply.error())
        return reply.error();
    reply >> Errors::last;
    return Errors::last;
}

void Syscalls::noop() {
    send_receive_vmsg(_gate, NOOP);
}

Errors::Code Syscalls::activate(size_t ep, capsel_t oldcap, capsel_t newcap) {
    LOG(SYSC, "activate(ep=" << ep << ", oldcap=" << oldcap << ", newcap=" << newcap << ")");
    return finish(send_receive_vmsg(_gate, ACTIVATE, ep, oldcap, newcap));
}

Errors::Code Syscalls::createsrv(capsel_t gate, capsel_t srv, const String &name) {
    LOG(SYSC, "createsrv(gate=" << gate << ", srv=" << srv << ", name=" << name << ")");
    return finish(send_receive_vmsg(_gate, CREATESRV, gate, srv, name));
}

Errors::Code Syscalls::createsess(capsel_t vpe, capsel_t cap, const String &name, const GateOStream &args) {
    LOG(SYSC, "createsess(vpe=" << vpe << ", cap=" << cap << ", name=" << name
        << ", argc=" << args.total() << ")");
    AutoGateOStream msg(vostreamsize(
        ostreamsize<Operation, capsel_t, capsel_t, size_t>(), name.length(), args.total()));
    msg << CREATESESS << vpe << cap << name;
    msg.put(args);
    return finish(send_receive_msg(_gate, msg.bytes(), msg.total()));
}

Errors::Code Syscalls::creategate(capsel_t vpe, capsel_t dst, label_t label, size_t ep, word_t credits) {
    LOG(SYSC, "creategate(vpe=" << vpe << ", dst=" << dst << ", label=" << fmt(label, "#x")
        << ", ep=" << ep << ", credits=" << credits << ")");
    return finish(send_receive_vmsg(_gate, CREATEGATE, vpe, dst, label, ep, credits));
}

Errors::Code Syscalls::createmap(capsel_t vpe, capsel_t mem, capsel_t first, capsel_t pages, capsel_t dst, int perms) {
    LOG(SYSC, "createmap(vpe=" << vpe << ", mem=" << mem << ", first=" << first
        << ", pages=" << pages << ", dst=" << dst << ", perms=" << perms << ")");
    return finish(send_receive_vmsg(_gate, CREATEMAP, vpe, mem, first, pages, dst, perms));
}

Errors::Code Syscalls::createvpe(capsel_t vpe, capsel_t mem, const String &name, const String &core) {
    LOG(SYSC, "createvpe(vpe=" << vpe << ", mem=" << mem << ", name=" << name
        << ", core=" << core << ")");
    return finish(send_receive_vmsg(_gate, CREATEVPE, vpe, mem, name, core));
}

Errors::Code Syscalls::setpfgate(capsel_t vpe, capsel_t gate, size_t ep) {
    LOG(SYSC, "setpfgate(vpe=" << vpe << ", gate=" << gate << ", ep=" << ep << ")");
    return finish(send_receive_vmsg(_gate, SETPFGATE, vpe, gate, ep));
}

Errors::Code Syscalls::attachrb(capsel_t vpe, size_t ep, uintptr_t addr, int order, int msgorder, uint flags) {
    LOG(SYSC, "attachrb(vpe=" << vpe << ", ep=" << ep << ", addr=" << fmt(addr, "p")
        << ", size=" << fmt(1UL << order, "x") << ", msgsize=" << fmt(1UL << msgorder, "x")
        << ", flags=" << fmt(flags, "x") << ")");
    return finish(send_receive_vmsg(_gate, ATTACHRB, vpe, ep, addr, order, msgorder, flags));
}

Errors::Code Syscalls::detachrb(capsel_t vpe, size_t ep) {
    LOG(SYSC, "detachrb(vpe=" << vpe << ", ep=" << ep);
    return finish(send_receive_vmsg(_gate, DETACHRB, vpe, ep));
}

Errors::Code Syscalls::exchange(capsel_t vpe, const CapRngDesc &own, const CapRngDesc &other, bool obtain) {
    LOG(SYSC, "exchange(vpe=" << vpe << ", own=" << own << ", other=" << other
        << ", obtain=" << obtain << ")");
    return finish(send_receive_vmsg(_gate, EXCHANGE, vpe, own, other, obtain));
}

Errors::Code Syscalls::vpectrl(capsel_t vpe, VPECtrl op, int pid, int *exitcode) {
    LOG(SYSC, "vpectrl(vpe=" << vpe << ", op=" << op << ", pid=" << pid << ")");
    GateIStream &&reply = send_receive_vmsg(_gate, VPECTRL, vpe, op, pid);
    reply >> Errors::last;
    if(op == VCTRL_WAIT && Errors::last == Errors::NO_ERROR)
        reply >> *exitcode;
    return Errors::last;
}

Errors::Code Syscalls::delegate(capsel_t vpe, capsel_t sess, const CapRngDesc &crd) {
    LOG(SYSC, "delegate(vpe=" << vpe << ", sess=" << sess << ", crd=" << crd << ")");
    return finish(send_receive_vmsg(_gate, DELEGATE, vpe, sess, crd));
}

GateIStream Syscalls::delegate(capsel_t vpe, capsel_t sess, const CapRngDesc &crd, const GateOStream &args) {
    LOG(SYSC, "delegate(vpe=" << vpe << ", sess=" << sess << ", crd=" << crd << ", argc=" << args.total() << ")");
    AutoGateOStream msg(vostreamsize(ostreamsize<Operation, capsel_t, CapRngDesc>(), args.total()));
    msg << DELEGATE << vpe << sess << crd;
    msg.put(args);
    return send_receive_msg(_gate, msg.bytes(), msg.total());
}

Errors::Code Syscalls::obtain(capsel_t vpe, capsel_t sess, const CapRngDesc &crd) {
    LOG(SYSC, "obtain(vpe=" << vpe << ", sess=" << sess << ", crd=" << crd << ")");
    return finish(send_receive_vmsg(_gate, OBTAIN, vpe, sess, crd));
}

GateIStream Syscalls::obtain(capsel_t vpe, capsel_t sess, const CapRngDesc &crd, const GateOStream &args) {
    LOG(SYSC, "obtain(vpe=" << vpe << ", sess=" << sess << ", crd=" << crd << ", argc=" << args.total() << ")");
    AutoGateOStream msg(vostreamsize(ostreamsize<Operation, capsel_t, CapRngDesc>(), args.total()));
    msg << OBTAIN << vpe << sess << crd;
    msg.put(args);
    return send_receive_msg(_gate, msg.bytes(), msg.total());
}

Errors::Code Syscalls::reqmemat(capsel_t cap, uintptr_t addr, size_t size, int perms) {
    LOG(SYSC, "reqmem(cap=" << cap << ", addr=" << addr << ", size=" << size
        << ", perms=" << perms << ")");
    return finish(send_receive_vmsg(_gate, REQMEM, cap, addr, size, perms));
}

Errors::Code Syscalls::derivemem(capsel_t src, capsel_t dst, size_t offset, size_t size, int perms) {
    LOG(SYSC, "derivemem(src=" << src << ", dst=" << dst << ", off=" << offset
            << ", size=" << size << ", perms=" << perms << ")");
    return finish(send_receive_vmsg(_gate, DERIVEMEM, src, dst, offset, size, perms));
}

Errors::Code Syscalls::revoke(const CapRngDesc &crd) {
    LOG(SYSC, "revoke(crd=" << crd << ")");
    return finish(send_receive_vmsg(_gate, REVOKE, crd));
}

void Syscalls::exit(int exitcode) {
    LOG(SYSC, "exit(code=" << exitcode << ")");
    EVENT_TRACE_FLUSH();
    send_vmsg(_gate, EXIT, exitcode);
}

#if defined(__host__)
void Syscalls::init(void *sepregs) {
    LOG(SYSC, "init(addr=" << sepregs << ")");
    send_receive_vmsg(_gate, INIT, sepregs);
}
#endif

}
