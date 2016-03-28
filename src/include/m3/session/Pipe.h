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

#include <m3/session/Session.h>
#include <m3/com/MemGate.h>
#include <m3/com/SendGate.h>

namespace m3 {

class Pipe : public Session {
public:
    enum MetaOp {
        ATTACH,
        CLOSE,
        COUNT
    };

    explicit Pipe(capsel_t sess, capsel_t metagate, capsel_t rdgate, capsel_t wrgate)
        : Session(sess),
          _metagate(SendGate::bind(metagate)),
          _rdgate(SendGate::bind(rdgate)),
          _wrgate(SendGate::bind(wrgate)) {
    }
    explicit Pipe(const String &service, size_t memsize)
        : Session(service, create_vmsg(memsize)),
          _metagate(SendGate::bind(obtain(1).start())),
          _rdgate(SendGate::bind(obtain(1).start())),
          _wrgate(SendGate::bind(obtain(1).start())) {
    }

    const SendGate &meta_gate() const {
        return _metagate;
    }
    const SendGate &read_gate() const {
        return _rdgate;
    }
    const SendGate &write_gate() const {
        return _wrgate;
    }

    void attach(bool reading);
    Errors::Code read(size_t *pos, size_t *amount, int *lastid);
    Errors::Code write(size_t *pos, size_t amount, int *lastid);
    void close(bool reading, int lastid);

private:
    SendGate _metagate;
    SendGate _rdgate;
    SendGate _wrgate;
};

}
