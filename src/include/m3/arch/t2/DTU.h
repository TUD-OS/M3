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

#include <m3/Common.h>
#include <m3/Config.h>
#include <m3/stream/OStream.h>
#include <m3/tracing/Tracing.h>
#include <m3/util/Util.h>
#include <m3/Errors.h>
#include <assert.h>

#define DTU_PKG_SIZE        (static_cast<size_t>(8))

namespace m3 {

class KDTU;
class RecvBuf;

class DTU {
    friend class KDTU;
    friend class RecvBuf;

    static const uintptr_t DRAM_START       = 0x8000;

    explicit DTU() {
        reset();
    }

public:
    struct Header {
        label_t label;
        struct {
            word_t has_replycap : 1,
                   core : 15,
                   epid : 16;
        } PACKED;
        label_t replylabel;
        word_t length;        // has to be non-zero
    } PACKED;

    struct Message : public Header {
        int send_epid() const {
            return 0;
        }
        int reply_epid() const {
            return epid;
        }

        unsigned char data[];
    } PACKED;

    static const size_t HEADER_SIZE         = sizeof(Header);
    static const size_t PACKET_SIZE         = 8;

    // TODO not yet supported
    static const int FLAG_NO_RINGBUF        = 0;
    static const int FLAG_NO_HEADER         = 1;

    static const int MEM_EP                 = 0;
    static const int SYSC_EP                = 1;
    static const int DEF_RECVEP             = 2;

    enum Register {
        REG_TARGET,
        REG_REM_ADDR,
        REG_LOC_ADDR,
        REG_TYPE,
        REG_SIZE,
        REG_STATUS,
    };

    enum Operation {
        WRITE   = 0x0,      // write from local to remote
        READ    = 0x1,      // read from remote to local
        CMPXCHG = 0x2,
        SEND    = 0x3,
        REPLY   = 0x4,
    };

    static DTU &get() {
        return inst;
    }

    void reset();

    Errors::Code send(int ep, const void *msg, size_t size, label_t replylbl, int reply_ep);
    Errors::Code reply(int ep, const void *msg, size_t size, size_t msgidx);
    Errors::Code read(int ep, void *msg, size_t size, size_t off);
    Errors::Code write(int ep, const void *msg, size_t size, size_t off);
    Errors::Code cmpxchg(int, const void *, size_t, size_t, size_t) {
        return Errors::NO_ERROR;
    }
    void sendcrd(UNUSED int ep, UNUSED int crdep, UNUSED size_t size) {
    }

    bool fetch_msg(int ep);

    DTU::Message *message(int ep) const {
        assert(_last[ep]);
        return _last[ep];
    }
    Message *message_at(int ep, size_t msgidx) const {
        return reinterpret_cast<Message*>(DTU::get().recvbuf_offset(coreid(), ep) + msgidx);
    }

    size_t get_msgoff(int ep, RecvGate *rcvgate) const {
        return get_msgoff(ep, rcvgate, message(ep));
    }
    size_t get_msgoff(int ep, RecvGate *, const Message *msg) const {
        // currently we just return the offset here, because we're implementing reply() in SW.
        return reinterpret_cast<uintptr_t>(msg) - DTU::get().recvbuf_offset(coreid(), ep);
    }

    void ack_message(int ep) {
        // we might have already acked it by doing a reply
        if(_last[ep]) {
            volatile Message *msg = message(ep);
            msg->length = 0;
            _last[ep] = nullptr;
        }
    }

    bool wait() {
        return true;
    }

    void wait_until_ready(int) {
        int core = coreid();
        volatile uint *ptr = get_base_addr(core);
        size_t *regs = get_regs(core);

        volatile uint *status = ptr + regs[REG_STATUS];
        while(status[0] != 0)
            ;
        EVENT_TRACE_MEM_FINISH();
    }

    bool wait_for_mem_cmd() {
        // we've already waited
        return true;
    }

    void set_target(int, uchar dst, uintptr_t addr) {
        int core = coreid();
        volatile uint *ptr = get_base_addr(core);
        size_t *regs = get_regs(core);

        if(dst == CM_CORE)
            addr -= 0x60000000;
        ptr[regs[REG_TARGET]]      = dst;
        ptr[regs[REG_REM_ADDR]]    = addr;
    }

    void fire(int, Operation op, const void *msg, size_t size) {
        int core = coreid();
        volatile uint *ptr = get_base_addr(core);
        size_t *regs = get_regs(core);

        // currently we have to substract the DRAM start
        UNUSED uintptr_t addr = reinterpret_cast<uintptr_t>(msg);
        // both have to be packet-size aligned
        assert((addr & (PACKET_SIZE - 1)) == 0);
        assert((size & (PACKET_SIZE - 1)) == 0);

        if(op == READ)
            ptr[regs[REG_TYPE]]    = 0;
        else if(core == CM_CORE)
            ptr[regs[REG_TYPE]]    = 1;
        else
            ptr[regs[REG_TYPE]]    = 2;
        ptr[regs[REG_LOC_ADDR]]    = addr;
        ptr[regs[REG_SIZE]]        = size;

        if(core == CM_CORE) {
            /* send transfer slot to CM_TU (always 0 for us) */
            uint *tuFifo = (uint*)CM_TU_SET_FIFO;
            *tuFifo = 0;
        }
    }

    size_t get_remaining(int) {
        int core = coreid();
        volatile uint *ptr = get_base_addr(core);
        size_t *regs = get_regs(core);

        return ptr[regs[REG_SIZE]];
    }

private:
    static volatile uint *get_base_addr(int core) {
        return reinterpret_cast<volatile uint*>(core == CM_CORE ? 0x6001FA00 : 0x6001E078);
    }
    static size_t *get_regs(int core) {
        return regs[core == CM_CORE ? 0 : 1];
    }

    size_t recvbuf_offset(int core, int ep) {
        return core * RECV_BUF_MSGSIZE * EP_COUNT + ep * RECV_BUF_MSGSIZE;
    }

    void check_rw_access(uintptr_t base, size_t len, size_t off, size_t size, int perms, int type);

    EPConf *conf(int ep) {
        return coreconf()->eps + ep;
    }

    size_t _pos[EP_COUNT];
    Message *_last[EP_COUNT];
    static size_t regs[2][6];
    static DTU inst;
};

static_assert(sizeof(DTU::Message) == DTU::HEADER_SIZE, "Header do not match");

}
