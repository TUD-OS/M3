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

#include <base/Common.h>
#include <base/col/SList.h>

#include <m3/com/GateStream.h>
#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>
#include <m3/session/Pipe.h>
#include <m3/vfs/GenericFile.h>

#include "VarRingBuf.h"

class PipeData;

class PipeSession : public m3::RequestSessionData {
public:
    enum Type {
        META,
        RCHAN,
        WCHAN,
    };

    virtual ~PipeSession() {
    }

    virtual Type type() const {
        // TODO only necessary because every session needs to be (default)-constructable
        return META;
    }

    virtual void read(m3::GateIStream &is) {
        reply_error(is, m3::Errors::NOT_SUP);
    }
    virtual void write(m3::GateIStream &is, size_t) {
        reply_error(is, m3::Errors::NOT_SUP);
    }
    virtual m3::Errors::Code close() {
        return m3::Errors::NOT_SUP;
    }
};

class PipeChannel : public PipeSession {
public:
    enum {
        READ_EOF    = 1,
        WRITE_EOF   = 2,
    };

    explicit PipeChannel(PipeData *pipe, capsel_t srv);
    virtual ~PipeChannel() {
    }

    PipeChannel *clone(capsel_t srv) const;

    void set_ep(capsel_t ep) {
        epcap = ep;
    }
    m3::KIF::CapRngDesc crd() const {
        return m3::KIF::CapRngDesc(m3::KIF::CapRngDesc::OBJ, caps, 2);
    }

    m3::Errors::Code activate();

    int id;
    capsel_t caps;
    capsel_t epcap;
    size_t lastamount;
    m3::SendGate sgate;
    PipeData *pipe;
};

class PipeReadChannel : public PipeChannel {
public:
    explicit PipeReadChannel(PipeData *pipe, capsel_t srv) : PipeChannel(pipe, srv) {
    }

    virtual Type type() const override {
        return RCHAN;
    }

    virtual void read(m3::GateIStream &is) override;
    virtual m3::Errors::Code close() override;

private:
    void append_request(PipeData *pipe, m3::GateIStream &is);
};

class PipeWriteChannel : public PipeChannel {
public:
    explicit PipeWriteChannel(PipeData *pipe, capsel_t srv) : PipeChannel(pipe, srv) {
    }

    virtual Type type() const override {
        return WCHAN;
    }

    virtual void write(m3::GateIStream &is, size_t submit) override;
    virtual m3::Errors::Code close() override;

private:
    void append_request(PipeData *pipe, m3::GateIStream &is);
};

class PipeData : public PipeSession {
    struct WorkItem : public m3::WorkItem {
        virtual void work() override;

        PipeData *pipe;
    };

public:
    template<class T>
    struct RdWrRequest : public m3::SListItem {
        explicit RdWrRequest(T *_chan, const m3::DTU::Message* _lastmsg)
            : m3::SListItem(), chan(_chan), lastmsg(_lastmsg) {
        }

        T *chan;
        const m3::DTU::Message *lastmsg;
    };

    // unused
    explicit PipeData() : memory(), rbuf(0) {
    }

    explicit PipeData(m3::RecvGate *rgate, size_t _memsize);
    virtual ~PipeData();

    virtual Type type() const override {
        return META;
    }

    PipeChannel *attach(capsel_t srv, bool read);
    void handle_pending_read();
    void handle_pending_write();

    int nextid;
    uint flags;
    m3::MemGate *memory;
    m3::RecvGate *rgate;
    VarRingBuf rbuf;
    WorkItem workitem;
    // XXX we are currently using the SListItem here and in Handler
    m3::SList<PipeReadChannel> reader;
    m3::SList<PipeWriteChannel> writer;
    PipeReadChannel *last_reader;
    PipeWriteChannel *last_writer;
    m3::SList<RdWrRequest<PipeReadChannel>> pending_reads;
    m3::SList<RdWrRequest<PipeWriteChannel>> pending_writes;
};
