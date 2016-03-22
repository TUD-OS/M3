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

#include <m3/com/GateStream.h>
#include <m3/vfs/File.h>

namespace m3 {

class Pipe;

/**
 * Reads from a previously constructed pipe.
 */
class PipeReader : public File {
    friend class Pipe;

    struct State {
        explicit State(capsel_t caps, size_t rep);

        MemGate _mgate;
        RecvBuf _rbuf;
        RecvGate _rgate;
        size_t _pos;
        size_t _rem;
        size_t _pkglen;
        int _eof;
        GateIStream _is;
    };

    explicit PipeReader(capsel_t caps, size_t rep, State *state);

public:
    /**
     * Sends EOF
     */
    ~PipeReader();

    virtual Buffer *create_buf(size_t size) override {
        return new File::Buffer(size);
    }

    virtual int stat(FileInfo &) const override {
        // not supported
        return -1;
    }
    virtual off_t seek(off_t, int) override {
        // not supported
        return 0;
    }

    virtual ssize_t read(void *, size_t) override;
    virtual ssize_t write(const void *, size_t) override {
        // not supported
        return 0;
    }

    virtual char type() const override {
        return 'Q';
    }
    virtual size_t serialize_length() override;
    virtual void delegate(VPE &vpe) override;
    virtual void serialize(Marshaller &m) override;
    static File *unserialize(Unmarshaller &um);

private:
    virtual bool seek_to(off_t) override {
        return false;
    }
    void send_eof();

    bool _noeof;
    capsel_t _caps;
    size_t _rep;
    State *_state;
};

}
