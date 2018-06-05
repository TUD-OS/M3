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

#include "FileSession.h"
#include "Session.h"
#include "../FSHandle.h"

class M3FSMetaSession : public M3FSSession {
    struct MetaSGate : public m3::SListItem {
        explicit MetaSGate(m3::SendGate &&_sgate) : sgate(m3::Util::move(_sgate)) {
        }
        m3::SendGate sgate;
    };

public:
    static constexpr size_t MAX_FILES   = 64;

    explicit M3FSMetaSession(capsel_t srv_sel, m3::RecvGate &rgate, FSHandle &handle)
        : M3FSSession(srv_sel),
          _sgates(),
          _rgate(rgate),
          _handle(handle),
          _ep_start(),
          _ep_count(),
          _files() {
    }
    virtual ~M3FSMetaSession() {
        for(size_t i = 0; i < MAX_FILES; ++i)
            delete _files[i];
        for(auto it = _sgates.begin(); it != _sgates.end(); ) {
            auto old = it++;
            delete &*old;
        }
    }

    virtual Type type() const override {
        return META;
    }

    void set_eps(capsel_t sel, uint count) {
        _ep_start = sel;
        _ep_count = count;
    }

    virtual void open_private_file(m3::GateIStream &is) override;
    virtual void close_private_file(m3::GateIStream &is) override;

    virtual void read(m3::GateIStream &is) override;
    virtual void write(m3::GateIStream &is) override;
    virtual void seek(m3::GateIStream &is) override;
    virtual void fstat(m3::GateIStream &is) override;

    virtual void stat(m3::GateIStream &is) override;
    virtual void mkdir(m3::GateIStream &is) override;
    virtual void rmdir(m3::GateIStream &is) override;
    virtual void link(m3::GateIStream &is) override;
    virtual void unlink(m3::GateIStream &is) override;

    m3::RecvGate &rgate() {
        return _rgate;
    }
    FSHandle &handle() {
        return _handle;
    }

    m3::Errors::Code get_sgate(m3::KIF::Service::ExchangeData &data);
    m3::Errors::Code open_file(capsel_t srv, m3::KIF::Service::ExchangeData &data);
    void remove_file(M3FSFileSession *file);

private:
    m3::Errors::Code do_open(capsel_t srv, const char *path, int flags, size_t *id);
    ssize_t alloc_file(capsel_t srv, const char *path, int flags, m3::inodeno_t ino);

    m3::SList<MetaSGate> _sgates;
    m3::RecvGate &_rgate;
    FSHandle &_handle;
    capsel_t _ep_start;
    capsel_t _ep_count;
    // TODO change that to a list?
    M3FSFileSession *_files[MAX_FILES];
};
