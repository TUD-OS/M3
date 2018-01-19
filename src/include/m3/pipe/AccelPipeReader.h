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
#include <base/log/Lib.h>

#include <m3/com/GateStream.h>
#include <m3/pipe/AccelPipeState.h>
#include <m3/vfs/File.h>

#include <accel/stream/Stream.h>
#include <accel/stream/StreamAccel.h>

namespace m3 {

class AccelPipeReader : public File {
public:
    explicit AccelPipeReader()
        : _rgate(RecvGate::bind(accel::StreamAccelVPE::RGATE_SEL,
                                getnextlog2(accel::StreamAccelVPE::RB_SIZE))),
          _msg(), _buf() {
        // gate is already activated
        _rgate.ep(accel::StreamAccelVPE::EP_RECV);
    }
    ~AccelPipeReader() {
        send_reply();
    }

    virtual Errors::Code stat(FileInfo &) const override {
        // not supported
        return Errors::NOT_SUP;
    }
    virtual ssize_t seek(size_t, int) override {
        // not supported
        return Errors::NOT_SUP;
    }

    virtual ssize_t read(void *buffer, size_t size) override {
        using namespace accel;

        auto state = AccelPipeState::get();

        if(_buf == nullptr) {
            GateIStream is = receive_msg(_rgate);
            uint64_t cmd;
            is >> cmd;

            assert(static_cast<StreamAccelVPE::Command>(cmd) == StreamAccelVPE::Command::INIT);
            auto *init = reinterpret_cast<const StreamAccelVPE::InitCommand*>(is.message().data);
            state->report_size = init->report_size;
            state->out_size = init->out_size;
            LLOG(ACCEL, "AccelPipeReader: got init("
                << "report_size=" << init->report_size
                << ", out_size=" << init->out_size
                << ", buf_size=" << init->buf_size
                << ")");
            _buf = new char[init->buf_size];
            reply_vmsg(is, (uintptr_t)_buf);
        }

        if(state->pos == state->len) {
            GateIStream is = receive_msg(_rgate);
            uint64_t cmd;
            is >> cmd;

            assert(static_cast<StreamAccelVPE::Command>(cmd) == StreamAccelVPE::Command::UPDATE);
            auto *upd = reinterpret_cast<const StreamAccelVPE::UpdateCommand*>(is.message().data);
            state->off = upd->off;
            state->len = upd->len;
            state->eof = upd->eof;
            state->pos = 0;
            LLOG(ACCEL, "AccelPipeReader: got update("
                << "off=" << upd->off
                << ", len=" << upd->len
                << ", eof=" << upd->eof
                << ")");
            is.claim();
            _msg = &is.message();
        }

        size_t amount = 0;
        if(state->pos < state->len) {
            amount = Math::min(state->len - state->pos, size);
            memcpy(buffer, _buf + state->off + state->pos, amount);
            state->pos += amount;

            if(state->pos == state->len)
                send_reply();
        }

        LLOG(ACCEL, "AccelPipeReader: read(" << size << ") = " << amount);
        return static_cast<ssize_t>(amount);
    }
    virtual ssize_t write(const void *, size_t) override {
        // not supported
        return 0;
    }

    virtual char type() const override {
        return 'A';
    }
    virtual size_t serialize_length() override {
        return 0;
    }
    virtual void delegate(VPE &) override {
    }
    virtual void serialize(Marshaller &) override {
    }
    static File *unserialize(Unmarshaller &) {
        return new AccelPipeReader();
    }

private:
    void send_reply() {
        if(_msg) {
            char dummy[1];
            size_t idx = DTU::get().get_msgoff(_rgate.ep(), _msg);
            LLOG(ACCEL, "AccelPipeReader: reply()");
            _rgate.reply(dummy, sizeof(dummy), idx);
            _msg = nullptr;
        }
    }

    RecvGate _rgate;
    const DTU::Message *_msg;
    char *_buf;
};

}
