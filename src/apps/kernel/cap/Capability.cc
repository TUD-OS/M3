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
#include "DTU.h"

namespace kernel {

m3::OStream &operator<<(m3::OStream &os, const Capability &cc) {
    cc.print(os);
    return os;
}

GateObject::~GateObject() {
    for(auto user = epuser.begin(); user != epuser.end(); ) {
        auto old = user++;
        VPE &vpe = VPEManager::get().vpe(old->ep->vpe);
        vpe.invalidate_ep(old->ep->ep);
        old->ep->gate = nullptr;
        // wakeup the pe to give him the chance to notice that the endpoint was invalidated
        vpe.wakeup();
        delete &*old;
    }
}

RGateObject::~RGateObject() {
    m3::ThreadManager::get().notify(reinterpret_cast<event_t>(this));
}

MGateObject::~MGateObject() {
    // if it's not derived, it's always memory from mem-PEs
    if(!derived)
        MainMemory::get().free(pe, addr, size);
}

void SessObject::close() {
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

EPObject::~EPObject() {
    if(gate != nullptr)
        gate->remove_ep(this);
}

MapCapability::MapCapability(CapTable *tbl, capsel_t sel, gaddr_t _phys, uint _pages, int _attr)
    : Capability(tbl, sel, MAP, _pages),
      phys(_phys),
      attr(_attr) {
    VPE &vpe = VPEManager::get().vpe(tbl->id() - 1);
    vpe.address_space()->map_pages(vpe.desc(), sel << PAGE_BITS, phys, length, attr);
}

void MapCapability::remap(gaddr_t _phys, int _attr) {
    phys = _phys;
    attr = _attr;
    VPE &vpe = VPEManager::get().vpe(table()->id() - 1);
    vpe.address_space()->map_pages(vpe.desc(), sel() << PAGE_BITS, phys, length, attr);
}

void MapCapability::revoke() {
    VPE &vpe = VPEManager::get().vpe(table()->id() - 1);
    vpe.address_space()->unmap_pages(vpe.desc(), sel() << PAGE_BITS, length);
}

void SessCapability::revoke() {
    // the server's session cap is directly derived from the service. if the server revokes it,
    // disable further close-messages to the server
    if(parent()->type == SERV)
        obj->invalid = true;
    else if(!obj->invalid && obj->refcount() == 2)
        obj->close();
}

void ServCapability::revoke() {
    // first, reset the receive buffer: make all slots not-occupied
    if(obj->rgate()->activated())
        obj->vpe().config_rcv_ep(obj->rgate()->ep, *obj->rgate());
    // now, abort everything in the sendqueue
    obj->abort();
}

void Capability::print(m3::OStream &os) const {
    os << m3::fmt(table()->id(), 2) << " @ " << m3::fmt(sel(), 6);
    printInfo(os);
    if(_child)
      _child->printChilds(os);
}

void RGateCapability::printInfo(m3::OStream &os) const {
    os << ": rgate[refs=" << obj->refcount()
       << ", ep=" << obj->ep
       << ", addr=#" << m3::fmt(obj->addr, "0x", sizeof(label_t) * 2)
       << ", order=" << obj->order
       << ", msgorder=" << obj->msgorder
       << ", eps=";
    obj->print_eps(os);
    os << "]";
}

void SGateCapability::printInfo(m3::OStream &os) const {
    os << ": sgate[refs=" << obj->refcount()
       << ", dst=" << obj->rgate->vpe << ":" << obj->rgate->ep
       << ", lbl=" << m3::fmt(obj->label, "#0x", sizeof(label_t) * 2)
       << ", crd=#" << m3::fmt(obj->credits, "x")
       << ", eps=";
    obj->print_eps(os);
    os << "]";
}

void MGateCapability::printInfo(m3::OStream &os) const {
    os << ": mgate[refs=" << obj->refcount()
       << ", dst=" << obj->vpe << "@" << obj->pe
       << ", addr=" << m3::fmt(obj->addr, "#0x", sizeof(label_t) * 2)
       << ", size=" << m3::fmt(obj->size, "#0x", sizeof(label_t) * 2)
       << ", perms=#" << m3::fmt(obj->perms, "x")
       << ", eps=";
    obj->print_eps(os);
    os << "]";
}

void MapCapability::printInfo(m3::OStream &os) const {
    os << ": map  [virt=#" << m3::fmt(sel() << PAGE_BITS, "x")
       << ", phys=#" << m3::fmt(phys, "x")
       << ", pages=" << length
       << ", attr=#" << m3::fmt(attr, "x") << "]";
}

void ServCapability::printInfo(m3::OStream &os) const {
    os << ": serv [name=" << obj->name() << "]";
}

void SessCapability::printInfo(m3::OStream &os) const {
    os << ": sess [refs=" << obj->refcount()
        << ", serv=" << obj->srv->name()
        << ", ident=#" << m3::fmt(obj->ident, "x")
        << ", invalid=" << obj->invalid << "]";
}

void EPCapability::printInfo(m3::OStream &os) const {
    os << ": ep  [refs=" << obj->refcount()
        << ", vpe=" << obj->vpe
        << ", ep=" << obj->ep << "]";
}

void VPEGroupCapability::printInfo(m3::OStream &os) const {
    os << ": vgrp [refs=" << obj->refcount() << "]";
}

void VPECapability::printInfo(m3::OStream &os) const {
    os << ": vpe  [refs=" << obj->refcount()
       << ", name=" << obj->name() << "]";
}

void Capability::printChilds(m3::OStream &os, size_t layer) const {
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
