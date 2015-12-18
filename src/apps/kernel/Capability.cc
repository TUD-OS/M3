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

#include <m3/Log.h>

#include "Capability.h"
#include "CapTable.h"
#include "PEManager.h"

namespace m3 {

OStream &operator<<(OStream &os, const Capability &cc) {
    cc.print(os);
    return os;
}

MemObject::~MemObject() {
    if(core == MEMORY_CORE && !derived) {
        uintptr_t addr = label & ~MemGate::RWX;
        MainMemory::get().map().free(addr, credits);
        LOG(KSYSC, "Free'd " << (credits / 1024) << " KiB of memory @ " << fmt(addr, "p"));
    }
}

SessionObject::~SessionObject() {
    AutoGateOStream msg(ostreamsize<SyscallHandler::server_type::Command, word_t>());
    msg << SyscallHandler::server_type::CLOSE << ident;
    LOG(KSYSC, "Sending CLOSE message for ident " << fmt(ident, "#x", 8) << " to " << srv->name());
    ServiceList::get().send_and_receive(srv, msg.bytes(), msg.total());
    msg.claim();
}

Errors::Code MsgCapability::revoke() {
    if(localepid != -1) {
        KVPE &vpe = PEManager::get().vpe(table()->id() - 1);
        LOG(IPC, "Invalidating ep " << localepid << " of VPE " << vpe.id() << "@" << vpe.core());
        vpe.xchg_ep(localepid, nullptr, nullptr);
    }
    obj.unref();
    return Errors::NO_ERROR;
}

MapCapability::MapCapability(CapTable *tbl, capsel_t sel, uintptr_t _phys, uint _attr)
    : Capability(tbl, sel, MAP), phys(_phys), attr(_attr) {
    KVPE &vpe = PEManager::get().vpe(tbl->id() - 1);
    KDTU::get().map_page(vpe.core(), sel << PAGE_BITS, phys, attr);
}

Errors::Code MapCapability::revoke() {
    KVPE &vpe = PEManager::get().vpe(table()->id() - 1);
    KDTU::get().unmap_page(vpe.core(), sel() << PAGE_BITS);
    return Errors::NO_ERROR;
}

Errors::Code SessionCapability::revoke() {
    obj.unref();
    return Errors::NO_ERROR;
}

Errors::Code ServiceCapability::revoke() {
    bool closing = inst->closing;
    inst->closing = true;
    // if we have childs, i.e. sessions, we need to send the close-message to the service first,
    // in which case there will be pending requests, which need to be handled first.
    if(inst->pending() > 0 || (child() != nullptr && !closing))
        return Errors::INV_ARGS;
    return Errors::NO_ERROR;
}

VPECapability::VPECapability(CapTable *tbl, capsel_t sel, KVPE *p)
    : Capability(tbl, sel, VPE), vpe(p) {
    p->ref();
}

VPECapability::VPECapability(const VPECapability &t) : Capability(t), vpe(t.vpe) {
    vpe->ref();
}

Errors::Code VPECapability::revoke() {
    vpe->unref();
    // TODO reset core and release it (make it free to use for others)
    return Errors::NO_ERROR;
}

void MsgCapability::print(OStream &os) const {
    os << fmt(sel(), 2) << ": mesg[refs=" << obj->refcount()
       << ", curep=" << localepid
       << ", dst=" << obj->core << ":" << obj->epid
       << ", lbl=" << fmt(obj->label, "#0x", sizeof(label_t) * 2)
       << ", crd=#" << fmt(obj->credits, "x") << "]";
}

void MemCapability::print(OStream &os) const {
    os << fmt(sel(), 2) << ": mem [refs=" << obj->refcount()
       << ", curep=" << localepid
       << ", dst=" << obj->core << ":" << obj->epid << ", lbl=" << fmt(obj->label, "#x")
       << ", crd=#" << fmt(obj->credits, "x") << "]";
}

void MapCapability::print(OStream &os) const {
    os << fmt(sel(), 2) << ": map [virt=#" << fmt(sel() << PAGE_BITS, "x")
       << ", phys=#" << fmt(phys, "x")
       << ", attr=#" << fmt(attr, "x") << "]";
}

void ServiceCapability::print(OStream &os) const {
    os << fmt(sel(), 2) << ": serv[name=" << inst->name() << "]";
}

void SessionCapability::print(OStream &os) const {
    os << fmt(sel(), 2) << ": sess[refs=" << obj->refcount()
        << ", serv=" << obj->srv->name()
        << ", ident=#" << fmt(obj->ident, "x") << "]";
}

void VPECapability::print(OStream &os) const {
    os << fmt(sel(), 2) << ": vpe [refs=" << vpe->refcount()
       << ", name=" << vpe->name() << "]";
}

}
