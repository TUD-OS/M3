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

#include <accel/stream/StreamAccel.h>

namespace m3 {

/**
 * The reading end of an accelerator pipe, i.e., we read from an accelerator.
 */
class AccelPipeReader : public File {
public:
    explicit AccelPipeReader()
        : _rgate(RecvGate::bind(accel::StreamAccel::RGATE_SEL,
                                getnextlog2(accel::StreamAccel::RB_SIZE))),
          _mgate(MemGate::bind(ObjCap::INVALID)),
          _mgate_valid(true),
          _msg(), _buf() {
        // gates are already activated
        _rgate.ep(accel::StreamAccel::EP_RECV);
        _mgate.ep(accel::StreamAccel::EP_INPUT);
    }
    ~AccelPipeReader() {
        send_reply();
    }

    /**
     * @return the internal buffer which already contains the data after calling read()
     */
    char *get_buffer() {
        using namespace accel;

        if(_buf == nullptr) {
            GateIStream is = receive_msg(_rgate);
            uint64_t cmd;
            is >> cmd;

            auto state = AccelPipeState::get();

            assert(static_cast<StreamAccel::Command>(cmd) == StreamAccel::Command::INIT);
            auto *init = reinterpret_cast<const StreamAccel::InitCommand*>(is.message().data);
            state->report_size = init->report_size;
            state->out_size = init->out_size;
            LLOG(ACCEL, "AccelPipeReader: got init("
                << "report_size=" << init->report_size
                << ", out_size=" << init->out_size
                << ")");
            _buf = new char[StreamAccel::BUF_SIZE];
            reply_vmsg(is, (uintptr_t)_buf);
        }

        return _buf;
    }

    virtual Errors::Code stat(FileInfo &) override {
        // not supported
        return Errors::NOT_SUP;
    }
    virtual ssize_t seek(size_t, int) override {
        // not supported
        return Errors::NOT_SUP;
    }

    virtual ssize_t read(void *buffer, size_t size) override {
        auto state = AccelPipeState::get();

        char *ibuf = get_buffer();

        if(state->pos == state->len)
            get_next();

        size_t amount = 0;
        if(state->pos < state->len) {
            amount = Math::min(state->len - state->pos, size);
            if(buffer)
                memcpy(buffer, ibuf + state->off + state->pos, amount);
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
    void get_next() {
        using namespace accel;

        auto state = AccelPipeState::get();

        GateIStream is = receive_msg(_rgate);
        uint64_t cmd;
        is >> cmd;

        assert(static_cast<StreamAccel::Command>(cmd) == StreamAccel::Command::UPDATE);
        auto *upd = reinterpret_cast<const StreamAccel::UpdateCommand*>(is.message().data);
        state->off = upd->off;
        state->len = upd->len;
        state->eof = upd->eof;
        state->pos = 0;
        LLOG(ACCEL, "AccelPipeReader: got update("
            << "off=" << upd->off
            << ", len=" << upd->len
            << ", eof=" << upd->eof
            << ")");

        if(_mgate_valid) {
            if(_mgate.read(_buf + state->off, upd->len, upd->off) != Errors::NONE)
                _mgate_valid = false;
        }

        // we want to reply later
        is.claim();
        _msg = &is.message();
    }

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
    MemGate _mgate;
    bool _mgate_valid;
    const DTU::Message *_msg;
    char *_buf;
};

}
