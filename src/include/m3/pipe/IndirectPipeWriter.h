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

#include <m3/pipe/IndirectPipe.h>

namespace m3 {

/**
 * Writes into a previously constructed pipe.
 */
class IndirectPipeWriter : public IndirectPipeFile {
    friend class IndirectPipe;

    explicit IndirectPipeWriter(capsel_t mem, Pipe *pipe)
        : IndirectPipeFile(mem, pipe), _lastwrite() {
    }

public:
    explicit IndirectPipeWriter(capsel_t mem, capsel_t sess,
        capsel_t metagate, capsel_t rdgate, capsel_t wrgate)
        : IndirectPipeFile(mem, sess, metagate, rdgate, wrgate), _lastwrite() {
    }
    ~IndirectPipeWriter() {
        _pipe->close(false, _lastwrite);
    }

    virtual ssize_t read(void *, size_t) override {
        // not supported
        return 0;
    }
    virtual ssize_t write(const void *buffer, size_t count) override;

    virtual Errors::Code begin_write(capsel_t *memgate, size_t *offset, size_t *length) override;
    virtual void commit_write(size_t length) override;

    virtual char type() const override {
        return 'J';
    }

    virtual void delegate(VPE &vpe) override;
    static File *unserialize(Unmarshaller &um);

private:
    size_t _lastwrite;
};

}
