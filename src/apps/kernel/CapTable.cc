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

#include "CapTable.h"
#include "PEManager.h"

namespace m3 {

CapTable CapTable::_kcaps(0);

void CapTable::revoke_all() {
    Capability *c;
    // TODO it might be better to do that in a different order, because it is more expensive to
    // remove a node that has two childs (it requires a rotate). Thus, it would be better to start
    // with leaf nodes.
    while((c = static_cast<Capability*>(_caps.remove_root())) != nullptr)
        revoke(c);
}

#if defined(__t3__)
void CapTable::activate_msgcaps() {

    // FIXME
    // this is somewhat ugly code since we cant iterate over all capabilites
    // inside a Treap so we remove the root item one by one and insert
    // them again at the end
    // note: this code is not thread-safe(!)

    Capability *c;
    Treap<Capability> tmp;

    while ((c = static_cast<Capability*>(_caps.remove_root())) != nullptr) {
        tmp.insert(c);
        if (c->type == Capability::MSG) {
            MsgCapability *msgc = static_cast<MsgCapability*>(c);
            if (msgc->localepid != -1) {
                LOG(VPES, "Activating EP " << msgc->localepid);
                KVPE &vpe = PEManager::get().vpe(msgc->table()->id() - 1);
                vpe.xchg_ep(msgc->localepid, nullptr, msgc);
            }
        }
    }

    while ((c = static_cast<Capability*>(tmp.remove_root())) != nullptr) {
        _caps.insert(c);
    }
}
#endif

Capability *CapTable::obtain(capsel_t dst, Capability *c) {
    Capability *nc = c;
    if(c) {
        nc = c->clone(this, dst);
        if(nc)
            inherit(c, nc);
    }
    set(dst, nc);
    return nc;
}

void CapTable::inherit(Capability *parent, Capability *child) {
    child->_parent = parent;
    child->_child = nullptr;
    child->_next = parent->_child;
    child->_prev = nullptr;
    if(child->_next)
        child->_next->_prev = child;
    parent->_child = child;
}

Errors::Code CapTable::revoke_rec(Capability *c, bool revnext) {
    Capability *child = c->child();
    Capability *next = c->next();

    Errors::Code res = c->revoke();
    // actually, this is a bit specific for service+session. although it failed to revoke the service
    // we want to revoke all childs, i.e. the sessions to remove them from the service.
    // TODO if there are other failable revokes, we need to reconsider that
    if(res == Errors::NO_ERROR)
        c->table()->unset(c->sel());
    // reset the child-pointer since we're revoking all childs
    // note that we would need to do much more if delegatable capabilities could deny a revoke
    else
        c->_child = nullptr;

    if(child)
        revoke_rec(child, true);
    // on the first level, we don't want to revoke siblings
    if(revnext && next)
        revoke_rec(next, true);
    return res;
}

Errors::Code CapTable::revoke(Capability *c) {
    if(c) {
        if(c->_next)
            c->_next->_prev = c->_prev;
        if(c->_prev)
            c->_prev->_next = c->_next;
        if(c->_parent && c->_parent->_child == c)
            c->_parent->_child = c->_next;
        return revoke_rec(c, false);
    }
    return Errors::NO_ERROR;
}

Errors::Code CapTable::revoke(const m3::CapRngDesc &crd) {
    for(capsel_t i = 0; i < crd.count(); ++i) {
        Errors::Code res = revoke(get(i + crd.start()));
        if(res != Errors::NO_ERROR)
            return res;
    }
    return Errors::NO_ERROR;
}

OStream &operator<<(OStream &os, const CapTable &ct) {
    os << "CapTable[" << ct.id() << "]:\n";
    ct._caps.print(os, false);
    return os;
}

}
