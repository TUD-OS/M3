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

#include "CapTable.h"
#include "PEManager.h"

namespace m3 {

CapTable CapTable::_kcaps(0);

void CapTable::revoke_all() {
    for(capsel_t i = 0; i < MAX_ENTRIES; ++i) {
        if(_caps[i])
            revoke(CapRngDesc(i));
    }
}

Capability *CapTable::obtain(capsel_t dst, Capability *c) {
    Capability *nc = c;
    if(c) {
        nc = c->clone();
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

Errors::Code CapTable::revoke(const m3::CapRngDesc &crd) {
    for(capsel_t i = 0; i < crd.count(); ++i) {
        Capability *c = get(i + crd.start());
        if(c) {
            if(c->_next)
                c->_next->_prev = c->_prev;
            if(c->_prev)
                c->_prev->_next = c->_next;
            if(c->_parent && c->_parent->_child == c)
                c->_parent->_child = c->_next;
            Errors::Code res = revoke_rec(c, false);
            if(res != Errors::NO_ERROR)
                return res;
        }
    }
    return Errors::NO_ERROR;
}

void CapTable::print_rec(OStream &os, int level, const Capability *c) {
    os << fmt("", level * 2) << *c << "\n";
    if(c->child())
        print_rec(os, level + 1, c->child());
    if(c->next())
        print_rec(os, level, c->next());
}

OStream &operator<<(OStream &os, const CapTable &ct) {
    os << "CapTable[" << ct.id() << "]:\n";
    for(size_t i = 0; i < CapTable::MAX_ENTRIES; ++i) {
        if(ct._caps[i]) {
            os << "  " << *ct._caps[i] << "\n";
            if(ct._caps[i]->child())
                CapTable::print_rec(os, 2, ct._caps[i]->child());
        }
    }
    return os;
}

}
