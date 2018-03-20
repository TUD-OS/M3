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

#include <base/log/Services.h>

#include <m3/Syscalls.h>

#include "Session.h"

#define PRINT(pipe, expr)           SLOG(PIPE, fmt((word_t)(pipe), "#x") << ": " expr)
#define PRINTCHAN(pipe, id, expr)   SLOG(PIPE, fmt((word_t)(pipe), "#x") << "[" << (id) << "]: " expr)

using namespace m3;

template<typename... Args>
static void reply_vmsg_late(RecvGate &rgate, const DTU::Message *msg, const Args &... args) {
    auto reply = create_vmsg(args...);
    size_t idx = DTU::get().get_msgoff(rgate.ep(), msg);
    rgate.reply(reply.bytes(), reply.total(), idx);
}

void PipeData::WorkItem::work() {
    pipe->handle_pending_write();
    pipe->handle_pending_read();
}

PipeData::PipeData(m3::RecvGate *rgate, size_t _memsize)
    : PipeSession(), nextid(), flags(), memory(), rgate(rgate), rbuf(_memsize), workitem(),
      reader(), writer(), last_reader(), last_writer(), pending_reads(), pending_writes() {
    workitem.pipe = this;
    m3::env()->workloop()->add(&workitem, false);
}

PipeData::~PipeData() {
    for(auto it = reader.begin(); it != reader.end(); ) {
        auto old = it++;
        delete &*old;
    }
    for(auto it = writer.begin(); it != writer.end(); ) {
        auto old = it++;
        delete &*old;
    }

    m3::env()->workloop()->remove(&workitem);
}

PipeChannel *PipeData::attach(capsel_t srv, bool read) {
    PipeChannel *handler;
    if(read) {
        handler = &*reader.append(new PipeReadChannel(this, srv));
        PRINT(this, "attach: read-refs=" << reader.length());
    }
    else {
        handler = &*writer.append(new PipeWriteChannel(this, srv));
        PRINT(this, "attach: write-refs=" << writer.length());
    }

    return handler;
}

PipeChannel *PipeChannel::clone(capsel_t srv) const {
    return pipe->attach(srv, type() == RCHAN);
}

PipeChannel::PipeChannel(PipeData *_pipe, capsel_t srv)
    : PipeSession(), m3::SListItem(),
      id(_pipe->nextid++), caps(VPE::self().alloc_sels(2)), epcap(ObjCap::INVALID), lastamount(),
      sgate(m3::SendGate::create(_pipe->rgate, reinterpret_cast<label_t>(this), 64, nullptr, caps + 1)),
      memory(), pipe(_pipe) {
    Syscalls::get().createsessat(caps + 0, srv, reinterpret_cast<word_t>(this));
}

Errors::Code PipeChannel::activate() {
    if(epcap != ObjCap::INVALID) {
        if(pipe->memory == nullptr)
            return Errors::INV_ARGS;

        // derive a new memgate with read / write permission
        if(memory == nullptr) {
            auto perms = type() == RCHAN ? MemGate::R : MemGate::W;
            memory = new MemGate(pipe->memory->derive(0, pipe->rbuf.size(), perms));
        }

        if(Syscalls::get().activate(epcap, memory->sel(), 0) != Errors::NONE)
            return Errors::last;
        epcap = ObjCap::INVALID;
    }
    return Errors::NONE;
}

Errors::Code PipeReadChannel::close() {
    if(pipe->flags & READ_EOF)
        return Errors::INV_ARGS;

    if(pipe->last_reader == this) {
        PRINTCHAN(pipe, id, "read-pull: " << lastamount);
        pipe->rbuf.pull(lastamount);
        pipe->last_reader = nullptr;
    }

    pipe->reader.remove(this);
    if(pipe->reader.length() > 0) {
        PRINTCHAN(pipe, id, "close: read-refs=" << pipe->reader.length());
        return Errors::NONE;
    }

    pipe->flags |= READ_EOF;
    PRINTCHAN(pipe, id, "close: read end");

    return Errors::NONE;
}

void PipeReadChannel::read(GateIStream &is, size_t submit) {
    Errors::Code res = activate();
    if(res != Errors::NONE) {
        reply_error(is, res);
        return;
    }

    if(pipe->last_reader) {
        if(pipe->last_reader != this) {
            append_request(pipe, is);
            return;
        }

        size_t amount = submit == 0 ? lastamount : submit;
        PRINTCHAN(pipe, id, "read-pull: " << amount);
        pipe->rbuf.pull(amount);
        pipe->last_reader = nullptr;
    }

    if(submit > 0) {
        reply_vmsg(is, Errors::NONE, pipe->rbuf.size());
        return;
    }

    if(pipe->pending_reads.length() > 0) {
        if(!(pipe->flags & WRITE_EOF)) {
            append_request(pipe, is);
            return;
        }
    }

    // TODO hand out less, if it is above a certain threshold
    size_t amount = pipe->rbuf.size() / static_cast<size_t>(4 * pipe->reader.length());
    ssize_t pos = pipe->rbuf.get_read_pos(&amount);
    if(pos == -1) {
        if(pipe->flags & WRITE_EOF) {
            PRINTCHAN(pipe, id, "read: EOF");
            reply_vmsg(is, Errors::NONE, (size_t)0, (size_t)0);
        }
        else
            append_request(pipe, is);
    }
    else {
        pipe->last_reader = this;
        lastamount = amount;
        PRINTCHAN(pipe, id, "read: " << amount << " @" << pos);
        reply_vmsg(is, Errors::NONE, pos, amount);
    }
}

void PipeReadChannel::append_request(PipeData *pipe, GateIStream &is) {
    PRINTCHAN(pipe, id, "read: waiting");
    pipe->pending_reads.append(new PipeData::RdWrRequest<PipeReadChannel>(this, &is.message()));
    is.claim();
}

void PipeData::handle_pending_read() {
    if(last_reader)
        return;

    while(pending_reads.length() > 0) {
        PipeData::RdWrRequest<PipeReadChannel> *req = &*pending_reads.begin();
        size_t ramount = rbuf.size() / static_cast<size_t>(4 * reader.length());
        ssize_t rpos = rbuf.get_read_pos(&ramount);
        if(rpos != -1) {
            pending_reads.remove_first();
            last_reader = req->chan;
            req->chan->lastamount = ramount;
            PRINTCHAN(this, req->chan->id, "late-read: " << ramount << " @" << rpos);
            reply_vmsg_late(*rgate, req->lastmsg, Errors::NONE, rpos, ramount);
            delete req;
            break;
        }
        else if(flags & PipeChannel::WRITE_EOF) {
            pending_reads.remove_first();
            PRINTCHAN(this, req->chan->id, "late-read: EOF");
            reply_vmsg_late(*rgate, req->lastmsg, Errors::NONE, (size_t)0, (size_t)0);
            delete req;
        }
        else
            break;
    }
}

Errors::Code PipeWriteChannel::close() {
    if(pipe->flags & WRITE_EOF)
        return Errors::INV_ARGS;

    if(pipe->last_writer == this && lastamount != static_cast<size_t>(-1)) {
        PRINTCHAN(pipe, id, "write-push: " << lastamount);
        pipe->rbuf.push(lastamount, lastamount);
        pipe->last_writer = nullptr;
    }

    pipe->writer.remove(this);
    if(pipe->writer.length() > 0) {
        PRINTCHAN(pipe, id, "close: write-refs=" << pipe->writer.length());
        return Errors::NONE;
    }

    pipe->flags |= WRITE_EOF;
    PRINTCHAN(pipe, id, "close: write end");

    return Errors::NONE;
}

void PipeWriteChannel::write(GateIStream &is, size_t submit) {
    Errors::Code res = activate();
    if(res != Errors::NONE) {
        reply_error(is, res);
        return;
    }

    if(pipe->flags & READ_EOF) {
        PRINTCHAN(pipe, id, "write: EOF");
        reply_error(is, Errors::END_OF_FILE);
        return;
    }

    if(pipe->last_writer) {
        if(pipe->last_writer != this) {
            append_request(pipe, is);
            return;
        }

        size_t amount = submit == 0 ? lastamount : submit;
        PRINTCHAN(pipe, id, "write-push: " << amount);
        pipe->rbuf.push(lastamount, amount);
        pipe->last_writer = nullptr;
    }

    if(submit > 0) {
        reply_vmsg(is, Errors::NONE, pipe->rbuf.size());
        return;
    }

    if(pipe->pending_writes.length() > 0) {
        append_request(pipe, is);
        return;
    }

    // TODO hand out less, if it is above a certain threshold
    size_t amount = pipe->rbuf.size() / static_cast<size_t>(4 * pipe->writer.length());
    ssize_t pos = pipe->rbuf.get_write_pos(amount);
    if(pos == -1)
        append_request(pipe, is);
    else {
        pipe->last_writer = this;
        lastamount = amount;
        PRINTCHAN(pipe, id, "write: " << amount << " @" << pos);
        reply_vmsg(is, Errors::NONE, pos, amount);
    }
}

void PipeWriteChannel::append_request(PipeData *pipe, GateIStream &is) {
    PRINTCHAN(pipe, id, "write: waiting");
    pipe->pending_writes.append(new PipeData::RdWrRequest<PipeWriteChannel>(this, &is.message()));
    is.claim();
}

void PipeData::handle_pending_write() {
    if(last_writer)
        return;

    if(flags & PipeChannel::READ_EOF) {
        while(pending_writes.length() > 0) {
            PipeData::RdWrRequest<PipeWriteChannel> *req = pending_writes.remove_first();
            PRINTCHAN(this, req->chan->id, "late-write: EOF");
            reply_vmsg_late(*rgate, req->lastmsg, Errors::END_OF_FILE);
            delete req;
        }
    }
    else if(pending_writes.length() > 0) {
        PipeData::RdWrRequest<PipeWriteChannel> *req = &*pending_writes.begin();
        size_t amount = rbuf.size() / static_cast<size_t>(4 * writer.length());
        ssize_t wpos = rbuf.get_write_pos(amount);
        if(wpos != -1) {
            pending_writes.remove_first();

            last_writer = req->chan;
            req->chan->lastamount = amount;
            PRINTCHAN(this, req->chan->id, "late-write: " << amount << " @" << wpos);
            reply_vmsg_late(*rgate, req->lastmsg, Errors::NONE, wpos, amount);
            delete req;
        }
    }
}
