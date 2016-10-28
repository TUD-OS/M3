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

#include <base/log/Kernel.h>

#include "pes/VPEManager.h"
#include "cap/Capability.h"
#include "cap/CapTable.h"

namespace kernel {

m3::OStream &operator<<(m3::OStream &os, const Capability &cc) {
    cc.print(os);
    return os;
}

MemObject::~MemObject() {
    // if it's not derived, it's always memory from mem-PEs
    if(!derived)
        MainMemory::get().free(pe, addr, size);
}

void SessionObject::close() {
    // only send the close message, if the service has not exited yet
    if(srv->vpe().has_app()) {
        KLOG(SERV, "Sending CLOSE message for ident " << m3::fmt(ident, "#x", 8)
            << " to " << srv->name());

        m3::KIF::Service::Close msg;
        msg.opcode = m3::KIF::Service::CLOSE;
        msg.sess = ident;
        ServiceList::get().send(srv, &msg, sizeof(msg), false);
    }
}
SessionObject::~SessionObject() {
    if(!servowned)
        close();
}

static void invalidate_ep(vpeid_t vpeid, Capability *cap) {
    VPE &vpe = VPEManager::get().vpe(vpeid);
    epid_t ep = vpe.cap_ep(cap);
    if(ep != 0) {
        vpe.ep_cap(ep, nullptr);
        vpe.invalidate_ep(ep);
        // wakeup the pe to give him the chance to notice that the endpoint was invalidated
        vpe.wakeup();
    }
}

m3::Errors::Code RBufCapability::revoke() {
    VPE &vpe = VPEManager::get().vpe(table()->id() - 1);
    vpe.rbufs().detach(vpe, &*obj);
    obj.unref();
    return m3::Errors::NO_ERROR;
}

m3::Errors::Code MsgCapability::revoke() {
    invalidate_ep(table()->id() - 1, this);
    obj.unref();
    return m3::Errors::NO_ERROR;
}

m3::Errors::Code MemCapability::revoke() {
    invalidate_ep(table()->id() - 1, this);
    obj.unref();
    return m3::Errors::NO_ERROR;
}

MapCapability::MapCapability(CapTable *tbl, capsel_t sel, uintptr_t _phys, uint _pages, uint _attr)
    : Capability(tbl, sel, MAP, _pages), phys(_phys), attr(_attr) {
    VPE &vpe = VPEManager::get().vpe(tbl->id() - 1);
    DTU::get().map_pages(vpe.desc(), sel << PAGE_BITS, phys, length, attr);
}

void MapCapability::remap(uintptr_t _phys, uint _attr) {
    phys = _phys;
    attr = _attr;
    VPE &vpe = VPEManager::get().vpe(table()->id() - 1);
    DTU::get().map_pages(vpe.desc(), sel() << PAGE_BITS, phys, length, attr);
}

m3::Errors::Code MapCapability::revoke() {
    VPE &vpe = VPEManager::get().vpe(table()->id() - 1);
    DTU::get().unmap_pages(vpe.desc(), sel() << PAGE_BITS, length);
    return m3::Errors::NO_ERROR;
}

m3::Errors::Code SessionCapability::revoke() {
    // if the server created that, we want to close it as soon as there are no clients using it anymore
    if(obj->servowned && obj->refcount() == 2)
        obj->close();
    obj.unref();
    return m3::Errors::NO_ERROR;
}

m3::Errors::Code ServiceCapability::revoke() {
    bool closing = inst->closing;
    inst->closing = true;
    // if we have childs, i.e. sessions, we need to send the close-message to the service first,
    // in which case there will be pending requests, which need to be handled first.
    if(inst->pending() > 0 || (child() != nullptr && !closing))
        return m3::Errors::MSGS_WAITING;
    return m3::Errors::NO_ERROR;
}

VPECapability::VPECapability(CapTable *tbl, capsel_t sel, VPE *p)
    : Capability(tbl, sel, VIRTPE), vpe(p) {
    p->ref();
}

VPECapability::VPECapability(const VPECapability &t) : Capability(t), vpe(t.vpe) {
    vpe->ref();
}

m3::Errors::Code VPECapability::revoke() {
    vpe->unref();
    return m3::Errors::NO_ERROR;
}

void Capability::print(m3::OStream &os) const {
    os << m3::fmt(table()->id(), 2) << " @ " << m3::fmt(sel(), 6);
    printInfo(os);
    child()->printChilds(os);
}

void RBufCapability::printInfo(m3::OStream &os) const {
    os << ": rbuf[refs=" << obj->refcount()
       << ", ep=" << obj->ep
       << ", addr=#" << m3::fmt(obj->addr, "0x", sizeof(label_t) * 2)
       << ", order=" << obj->order
       << ", msgorder=" << obj->msgorder << "]";
}

void MsgCapability::printInfo(m3::OStream &os) const {
    os << ": mesg[refs=" << obj->refcount()
       << ", dst=" << obj->rbuf->vpe << ":" << obj->rbuf->ep
       << ", lbl=" << m3::fmt(obj->label, "#0x", sizeof(label_t) * 2)
       << ", crd=#" << m3::fmt(obj->credits, "x") << "]";
}

void MemCapability::printInfo(m3::OStream &os) const {
    os << ": mem [refs=" << obj->refcount()
       << ", dst=" << obj->vpe << "@" << obj->pe
       << ", addr=" << m3::fmt(obj->addr, "#0x", sizeof(label_t) * 2)
       << ", size=" << m3::fmt(obj->size, "#0x", sizeof(label_t) * 2)
       << ", perms=#" << m3::fmt(obj->perms, "x") << "]";
}

void MapCapability::printInfo(m3::OStream &os) const {
    os << ": map [virt=#" << m3::fmt(sel() << PAGE_BITS, "x")
       << ", phys=#" << m3::fmt(phys, "x")
       << ", pages=" << length
       << ", attr=#" << m3::fmt(attr, "x") << "]";
}

void ServiceCapability::printInfo(m3::OStream &os) const {
    os << ": serv[name=" << inst->name() << "]";
}

void SessionCapability::printInfo(m3::OStream &os) const {
    os << ": sess[refs=" << obj->refcount()
        << ", serv=" << obj->srv->name()
        << ", ident=#" << m3::fmt(obj->ident, "x")
        << ", servowned=" << obj->servowned << "]";
}

void VPECapability::printInfo(m3::OStream &os) const {
    os << ": vpe [refs=" << vpe->refcount()
       << ", name=" << vpe->name() << "]";
}

void Capability::printChilds(m3::OStream &os, int layer) const {
    const Capability *n = this;
    while(n) {
        os << "\n";
        os << m3::fmt("", layer * 2) << " \\-";
        n->print(os);
        if(n->_child)
            n->_child->printChilds(os, layer + 1);
        n = n->_next;
    }
}

}
