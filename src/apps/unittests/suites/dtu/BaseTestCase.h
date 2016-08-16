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

#include <base/DTU.h>

#include <test/TestCase.h>

class BaseTestCase : public test::TestCase {
public:
    explicit BaseTestCase(const m3::String &name) : TestCase(name) {
    }

protected:
    void dmacmd(const void *data, size_t len, size_t epid, size_t offset, size_t length, int op) {
        m3::DTU &dtu = m3::DTU::get();
        dtu.set_cmd(m3::DTU::CMD_ADDR, reinterpret_cast<word_t>(data));
        dtu.set_cmd(m3::DTU::CMD_SIZE, len);
        dtu.set_cmd(m3::DTU::CMD_EPID, epid);
        dtu.set_cmd(m3::DTU::CMD_OFFSET, offset);
        dtu.set_cmd(m3::DTU::CMD_LENGTH, length);
        dtu.set_cmd(m3::DTU::CMD_REPLYLBL, 0);
        dtu.set_cmd(m3::DTU::CMD_REPLY_EPID, 0);
        dtu.set_cmd(m3::DTU::CMD_CTRL, (op << 3) | m3::DTU::CTRL_START |
                m3::DTU::CTRL_DEL_REPLY_CAP);
        while(dtu.get_cmd(m3::DTU::CMD_CTRL) & m3::DTU::CTRL_START)
            dtu.wait();
    }

    void dmasend(const void *data, size_t len, size_t epid) {
        m3::DTU &dtu = m3::DTU::get();
        dtu.set_cmd(m3::DTU::CMD_ADDR, reinterpret_cast<word_t>(data));
        dtu.set_cmd(m3::DTU::CMD_SIZE, len);
        dtu.set_cmd(m3::DTU::CMD_EPID, epid);
        dtu.set_cmd(m3::DTU::CMD_REPLYLBL, 0);
        dtu.set_cmd(m3::DTU::CMD_REPLY_EPID, 0);
        dtu.set_cmd(m3::DTU::CMD_CTRL, (m3::DTU::SEND << 3) | m3::DTU::CTRL_START);
        while(dtu.get_cmd(m3::DTU::CMD_CTRL) & m3::DTU::CTRL_START)
            dtu.wait();
    }

    m3::DTU::Message *getmsg(size_t epid, size_t cnt) {
        m3::DTU &dtu = m3::DTU::get();
        while(dtu.get_ep(epid, m3::DTU::EP_BUF_MSGCNT) != cnt)
            dtu.wait();
        return m3::DTU::get().message(epid);
    }

    m3::DTU::Message *getmsgat(size_t epid, size_t cnt, size_t idx) {
        m3::DTU &dtu = m3::DTU::get();
        while(dtu.get_ep(epid, m3::DTU::EP_BUF_MSGCNT) != cnt)
            dtu.wait();
        return m3::DTU::get().message_at(epid, idx);
    }
};
