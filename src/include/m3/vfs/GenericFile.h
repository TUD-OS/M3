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

#include <m3/com/SendGate.h>
#include <m3/com/MemGate.h>
#include <m3/session/ClientSession.h>
#include <m3/vfs/File.h>
#include <m3/VPE.h>

namespace m3 {

class M3FS;

class GenericFile : public File {
    friend class FileTable;

public:
    enum Operation {
        STAT,
        SEEK,
        NEXT_IN,
        NEXT_OUT,
        COMMIT,
        COUNT,
    };

    explicit GenericFile(int flags, capsel_t caps, size_t id = 0, epid_t mep = EP_COUNT,
                         M3FS *sess_obj = nullptr, size_t memoff = 0);
    virtual ~GenericFile();

    SendGate &sgate() {
        return *_sg;
    }
    ClientSession &sess() {
        return _sess;
    }
    const ClientSession &sess() const {
        return _sess;
    }

    /**
     * @return true if there is still data to read or write without contacting the server
     */
    bool has_data() const {
        return _pos < _len;
    }

    /**
     * Sends the next/output input request using the given reply label
     *
     * @param reply_label the reply label to set
     * @return true on success
     */
    bool send_next_input(label_t reply_label);
    bool send_next_output(label_t reply_label);

    /**
     * Passes the reply for the previous input/output request to this object.
     *
     * @param is the reply
     * @return the number of bytes that can be read/write afterwards
     */
    size_t received_next_resp(GateIStream &is);

    virtual Errors::Code stat(FileInfo &info) const override;

    virtual ssize_t seek(size_t offset, int whence) override;

    virtual ssize_t read(void *buffer, size_t count) override;
    virtual ssize_t write(const void *buffer, size_t count) override;

    virtual Errors::Code flush() override {
        return _writing ? submit() : Errors::NONE;
    }

    virtual char type() const override {
        return 'F';
    }

    virtual File *clone() const override {
        if(flags() & FILE_NOSESS)
            return nullptr;
        KIF::CapRngDesc crd = _sess.obtain(2);
        return new GenericFile(flags(), crd.start());
    }

    virtual Errors::Code delegate(VPE &vpe) override {
        if(flags() & FILE_NOSESS)
            return Errors::NOT_SUP;
        KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, _sess.sel(), 2);
        return _sess.obtain_for(vpe, crd);
    }

    virtual void serialize(Marshaller &m) override {
        m << flags() << _sess.sel() << _id;
    }

    static File *unserialize(Unmarshaller &um) {
        int fl;
        capsel_t caps;
        size_t id;
        um >> fl >> caps >> id;
        return new GenericFile(fl, caps, id);
    }

private:
    bool have_sess() const {
        return !(flags() & FILE_NOSESS);
    }
    void evict();
    Errors::Code submit();
    Errors::Code delegate_ep();

    size_t _id;
    M3FS *_sess_obj;
    mutable ClientSession _sess;
    mutable SendGate *_sg;
    MemGate _mg;
    size_t _memoff;
    size_t _goff;
    size_t _off;
    size_t _pos;
    size_t _len;
    bool _writing;
};

}
