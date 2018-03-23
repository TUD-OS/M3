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

#include <base/Common.h>
#include <base/log/Services.h>

#include <m3/com/GateStream.h>

#define PRINT(sess, expr) SLOG(FS, fmt((word_t)sess, "#x") << ": " << expr)

class M3FSSession {
public:
    static constexpr size_t MSG_SIZE = 128;

    enum Type {
        META,
        FILE,
    };

    virtual ~M3FSSession() {
    }

    virtual Type type() const = 0;

    virtual void read(m3::GateIStream &is) {
        m3::reply_error(is, m3::Errors::NOT_SUP);
    }
    virtual void write(m3::GateIStream &is) {
        m3::reply_error(is, m3::Errors::NOT_SUP);
    }
    virtual void seek(m3::GateIStream &is) {
        m3::reply_error(is, m3::Errors::NOT_SUP);
    }
    virtual void fstat(m3::GateIStream &is) {
        m3::reply_error(is, m3::Errors::NOT_SUP);
    }

    virtual void stat(m3::GateIStream &is) {
        m3::reply_error(is, m3::Errors::NOT_SUP);
    }
    virtual void mkdir(m3::GateIStream &is) {
        m3::reply_error(is, m3::Errors::NOT_SUP);
    }
    virtual void rmdir(m3::GateIStream &is) {
        m3::reply_error(is, m3::Errors::NOT_SUP);
    }
    virtual void link(m3::GateIStream &is) {
        m3::reply_error(is, m3::Errors::NOT_SUP);
    }
    virtual void unlink(m3::GateIStream &is) {
        m3::reply_error(is, m3::Errors::NOT_SUP);
    }
};
