/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/Common.h>
#include <m3/Config.h>
#include <m3/util/Util.h>
#include <assert.h>

#define DTU_PKG_SIZE        (static_cast<size_t>(8))

namespace m3 {

class DTU {
    explicit DTU() {
    }

public:
    typedef uint64_t reg_t;

    struct Endpoint {
        reg_t mode;
        reg_t maxMessageSize;
        reg_t bufferMessageCount;
        reg_t bufferAddr;
        reg_t bufferSize;
        reg_t bufferReadPtr;
        reg_t bufferWritePtr;

        reg_t targetCoreId;
        reg_t targetEpId;
        reg_t messageAddr;
        reg_t messageSize;
        reg_t label;
        reg_t replyEpId;
        reg_t replyLabel;

        reg_t requestLocalAddr;
        reg_t requestRemoteAddr;
        reg_t requestSize;

        reg_t credits;
    } PACKED;

    struct Header {
        uint8_t flags; // if bit 0 is set its a reply, if bit 1 is set we grant credits
        uint8_t senderCoreId;
        uint8_t senderEpId;
        uint8_t replyEpId; // for a normal message this is the reply epId
                           // for a reply this is the enpoint that receives credits
        uint16_t length;

        uint64_t label;
        uint64_t replylabel;
    } PACKED;

    enum EpMode : reg_t {
        RECEIVE_MESSAGE,
        TRANSMIT_MESSAGE,
        READ_MEMORY,
        WRITE_MEMORY,
    };

    static const uintptr_t BASE_ADDR        = 0x1000000;
    static const size_t HEADER_SIZE         = sizeof(Header);

    // TODO not yet supported
    static const int FLAG_NO_RINGBUF        = 0;
    static const int FLAG_NO_HEADER         = 0;

    enum class Command {
        IDLE                = 0,
        START_OPERATION     = 1,
        INC_READ_PTR        = 2,
    };

    static DTU &get() {
        return inst;
    }

    void configure(int ep, label_t label, int coreid, int epid, word_t credits) {
        Endpoint *e = get_ep(ep);
        e->mode = TRANSMIT_MESSAGE;
        e->label = label;
        e->targetCoreId = coreid;
        e->targetEpId = epid;
        e->credits = credits;
        // TODO that's not correct
        e->maxMessageSize = credits;
    }

    void configure_mem(int ep, int coreid, uintptr_t addr, size_t) {
        Endpoint *e = get_ep(ep);
        e->mode = READ_MEMORY;
        e->targetCoreId = coreid;
        e->requestRemoteAddr = addr;
    }

    void send(int ep, const void *msg, size_t size, label_t replylbl, int reply_ep);
    void reply(int ep, const void *msg, size_t size, size_t msgidx);
    void read(int ep, void *msg, size_t size, size_t off);
    void write(int ep, const void *msg, size_t size, size_t off);
    void set_receiving(int ep, uintptr_t buf, uint order, uint msgorder, int flags);

    void cmpxchg(UNUSED int ep, UNUSED const void *msg, UNUSED size_t msgsize, UNUSED size_t off, UNUSED size_t size) {
        // TODO unsupported
    }
    void sendcrd(UNUSED int ep, UNUSED int crdep, UNUSED size_t size) {
        // TODO unsupported
    }

    bool wait() {
        return true;
    }
    void wait_until_ready(int) {
        volatile reg_t *status = get_status_reg();
        while(*status != 0)
            ;
    }
    bool wait_for_mem_cmd() {
        // we've already waited
        return true;
    }

    Endpoint *get_ep(int ep) const {
        return reinterpret_cast<Endpoint*>(BASE_ADDR + sizeof(reg_t) * 2 + ep * sizeof(Endpoint));
    }

    void execCommand(int ep, Command c) {
        reg_t *cmd = get_cmd_reg();
        *cmd = static_cast<uint>(c) | (ep << 2);
    }

private:
    reg_t *get_cmd_reg() const {
        return reinterpret_cast<reg_t*>(BASE_ADDR) + 0;
    }
    reg_t *get_status_reg() const {
        return reinterpret_cast<reg_t*>(BASE_ADDR) + 1;
    }

    static DTU inst;
};

}
