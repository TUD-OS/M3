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

#pragma once

#include <base/col/SList.h>
#include <base/KIF.h>

#include <m3/com/SendGate.h>
#include <m3/VPE.h>

#include <fs/internal.h>

#include "Session.h"

class M3FSMetaSession;

enum class TransactionState {
    NONE,
    OPEN,
    ABORTED
};

struct CapContainer {
    struct Entry : public m3::SListItem {
        explicit Entry(const m3::KIF::CapRngDesc &_crd) : crd(_crd) {
        }
        ~Entry() {
            m3::VPE::self().revoke(crd);
        }

        m3::KIF::CapRngDesc crd;
    };

    explicit CapContainer() : caps() {
    }
    ~CapContainer() {
        for(auto it = caps.begin(); it != caps.end(); ) {
            auto old = it++;
            delete &*old;
        }
    }

    void add(const m3::KIF::CapRngDesc &crd) {
        caps.append(new Entry(crd));
    }

    m3::SList<Entry> caps;
};

class M3FSFileSession : public M3FSSession, public m3::SListItem {
public:
    explicit M3FSFileSession(capsel_t srv, M3FSMetaSession *_meta, const m3::String &_filename,
                             int _flags, const m3::INode &_inode);
    virtual ~M3FSFileSession();

    virtual Type type() const override {
        return FILE;
    }

    virtual void read(m3::GateIStream &is) override;
    virtual void write(m3::GateIStream &is) override;
    virtual void seek(m3::GateIStream &is) override;
    virtual void fstat(m3::GateIStream &is) override;

    m3::inodeno_t ino() const {
        return inode.inode;
    }

    m3::KIF::CapRngDesc caps() const {
        return m3::KIF::CapRngDesc(m3::KIF::CapRngDesc::OBJ, sess, 2);
    }
    void set_ep(capsel_t ep) {
        epcap = ep;
    }

    m3::Errors::Code clone(capsel_t srv, m3::KIF::Service::ExchangeData &data);
    m3::Errors::Code get_locs(m3::KIF::Service::ExchangeData &data);

private:
    void read_write(m3::GateIStream &is, bool write);
    m3::Errors::Code commit(size_t extent, size_t extoff);

    // TODO reference counting
    size_t extent;
    size_t extoff;
    size_t lastoff;
    size_t extlen;
    m3::String filename;
    capsel_t epcap;
    capsel_t sess;
    m3::SendGate sgate;
    int oflags;
    TransactionState xstate;
    m3::INode inode;
    capsel_t last;
    CapContainer capscon;
    M3FSMetaSession *meta;
};
