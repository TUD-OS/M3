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
public:
    static constexpr size_t MAX_FILES   = 16;

    explicit M3FSMetaSession(m3::RecvGate &rgate, FSHandle &handle)
        : M3FSSession(), _rgate(rgate), _handle(handle), _files() {
    }
    virtual ~M3FSMetaSession() {
        for(size_t i = 0; i < MAX_FILES; ++i)
            delete _files[i];
    }

    virtual Type type() const override {
        return META;
    }

    virtual void close() override;

    m3::RecvGate &rgate() {
        return _rgate;
    }
    FSHandle &handle() {
        return _handle;
    }

    m3::Errors::Code open_file(capsel_t srv, m3::KIF::Service::ExchangeData &data);
    void remove_file(M3FSFileSession *file);

private:
    ssize_t alloc_file(capsel_t srv, const char *path, int flags, const m3::INode &inode);

    m3::RecvGate &_rgate;
    FSHandle &_handle;
    // TODO change that to a list?
    M3FSFileSession *_files[MAX_FILES];
};
