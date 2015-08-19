/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/Syscalls.h>
#include <m3/GateStream.h>
#include <m3/Errors.h>
#include <m3/Log.h>

namespace m3 {

Syscalls Syscalls::_inst INIT_PRIORITY(108);

Errors::Code Syscalls::finish(GateIStream &&reply) {
    reply >> Errors::last;
    return Errors::last;
}

void Syscalls::noop() {
    send_receive_vmsg(_gate, NOOP);
}

Errors::Code Syscalls::activate(size_t chan, capsel_t oldcap, capsel_t newcap) {
    LOG(SYSC, "activate(chan=" << chan << ", oldcap=" << oldcap << ", newcap=" << newcap << ")");
    return finish(send_receive_vmsg(_gate, ACTIVATE, chan, oldcap, newcap));
}

Errors::Code Syscalls::createsrv(capsel_t gate, capsel_t srv, const String &name) {
    LOG(SYSC, "createsrv(gate=" << gate << ", srv=" << srv << ", name=" << name << ")");
    return finish(send_receive_vmsg(_gate, CREATESRV, gate, srv, name));
}

Errors::Code Syscalls::createsess(capsel_t cap, const String &name, const GateOStream &args) {
    LOG(SYSC, "createsess(cap=" << cap << ", name=" << name << ", argc=" << args.total() << ")");
    AutoGateOStream msg(vostreamsize(ostreamsize<Operation, capsel_t, size_t>(), name.length(), args.total()));
    msg << CREATESESS << cap << name;
    msg.put(args);
    return finish(send_receive_msg(_gate, msg.bytes(), msg.total()));
}

Errors::Code Syscalls::creategate(capsel_t vpe, capsel_t dst, label_t label, size_t chan, word_t credits) {
    LOG(SYSC, "creategate(vpe=" << vpe << ", dst=" << dst << ", label=" << fmt(label, "#x")
        << ", chan=" << chan << ", credits=" << credits << ")");
    return finish(send_receive_vmsg(_gate, CREATEGATE, vpe, dst, label, chan, credits));
}

Errors::Code Syscalls::createvpe(capsel_t vpe, capsel_t mem, const String &name, const String &core) {
    LOG(SYSC, "createvpe(vpe=" << vpe << ", mem=" << mem << ", name=" << name << ", core=" << core << ")");
    return finish(send_receive_vmsg(_gate, CREATEVPE, vpe, mem, name, core));
}

Errors::Code Syscalls::attachrb(capsel_t vpe, size_t chan, uintptr_t addr, size_t size, bool replies) {
    LOG(SYSC, "attachrb(vpe=" << vpe << ", chan=" << chan << ", addr=" << fmt(addr, "p")
        << ", size=" << fmt(size, "x") << " replies=" << replies << ")");
    return finish(send_receive_vmsg(_gate, ATTACHRB, vpe, chan, addr, size, replies));
}

Errors::Code Syscalls::detachrb(capsel_t vpe, size_t chan) {
    LOG(SYSC, "detachrb(vpe=" << vpe << ", chan=" << chan);
    return finish(send_receive_vmsg(_gate, DETACHRB, vpe, chan));
}

Errors::Code Syscalls::exchange(capsel_t vpe, const CapRngDesc &own, const CapRngDesc &other, bool obtain) {
    LOG(SYSC, "exchange(vpe=" << vpe << ", own=" << own << ", other=" << other << ", obtain=" << obtain << ")");
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

Errors::Code Syscalls::delegate(capsel_t sess, const CapRngDesc &crd) {
    LOG(SYSC, "delegate(sess=" << sess << ", crd=" << crd << ")");
    return finish(send_receive_vmsg(_gate, DELEGATE, sess, crd));
}

GateIStream Syscalls::delegate(capsel_t sess, const CapRngDesc &crd, const GateOStream &args) {
    LOG(SYSC, "delegate(sess=" << sess << ", crd=" << crd << ", argc=" << args.total() << ")");
    AutoGateOStream msg(vostreamsize(ostreamsize<Operation, capsel_t, CapRngDesc>(), args.total()));
    msg << DELEGATE << sess << crd;
    msg.put(args);
    return send_receive_msg(_gate, msg.bytes(), msg.total());
}

Errors::Code Syscalls::obtain(capsel_t sess, const CapRngDesc &crd) {
    LOG(SYSC, "obtain(sess=" << sess << ", crd=" << crd << ")");
    return finish(send_receive_vmsg(_gate, OBTAIN, sess, crd));
}

GateIStream Syscalls::obtain(capsel_t sess, const CapRngDesc &crd, const GateOStream &args) {
    LOG(SYSC, "obtain(sess=" << sess << ", crd=" << crd << ", argc=" << args.total() << ")");
    AutoGateOStream msg(vostreamsize(ostreamsize<Operation, capsel_t, CapRngDesc>(), args.total()));
    msg << OBTAIN << sess << crd;
    msg.put(args);
    return send_receive_msg(_gate, msg.bytes(), msg.total());
}

Errors::Code Syscalls::reqmemat(capsel_t cap, uintptr_t addr, size_t size, int perms) {
    LOG(SYSC, "reqmem(cap=" << cap << ", addr=" << addr << ", size=" << size << ", perms=" << perms << ")");
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
    return send_vmsg(_gate, EXIT, exitcode);
}

#if defined(__host__)
void Syscalls::init(void *sepregs) {
    LOG(SYSC, "init(addr=" << sepregs << ")");
    send_receive_vmsg(_gate, INIT, sepregs);
}
#endif

}
