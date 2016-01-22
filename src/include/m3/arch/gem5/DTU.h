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
#include <m3/util/Util.h>
#include <m3/util/Sync.h>
#include <assert.h>

#define DTU_PKG_SIZE        (static_cast<size_t>(8))

namespace m3 {

class KDTU;

class DTU {
    friend class KDTU;

    explicit DTU() {
    }

public:
    typedef uint64_t reg_t;

private:
    static const uintptr_t BASE_ADDR        = 0xF0000000;
    static const size_t DTU_REGS            = 2;
    static const size_t CMD_REGS            = 6;
    static const size_t EP_REGS             = 3;

    enum class DtuRegs {
        STATUS              = 0,
        MSGCNT              = 1,
    };

    enum class CmdRegs {
        COMMAND             = 2,
        DATA_ADDR           = 3,
        DATA_SIZE           = 4,
        OFFSET              = 5,
        REPLY_EP            = 6,
        REPLY_LABEL         = 7,
    };

    enum MemFlags : reg_t {
        R                   = 1 << 0,
        W                   = 1 << 1,
    };

    enum StatusFlags : reg_t {
        PRIV                = 1 << 0,
    };

    enum class EpType {
        INVALID,
        SEND,
        RECEIVE,
        MEMORY
    };

    enum class CmdOpCode {
        IDLE                = 0,
        SEND                = 1,
        REPLY               = 2,
        READ                = 3,
        WRITE               = 4,
        INC_READ_PTR        = 5,
        WAKEUP_CORE         = 6,
    };

public:
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

    struct Message : Header {
        int send_epid() const {
            return senderEpId;
        }
        int reply_epid() const {
            return replyEpId;
        }

        unsigned char data[];
    } PACKED;

    static const size_t HEADER_SIZE         = sizeof(Header);

    // TODO not yet supported
    static const int FLAG_NO_RINGBUF        = 0;
    static const int FLAG_NO_HEADER         = 1;

    static const int MEM_EP                 = 0;    // unused
    static const int SYSC_EP                = 0;
    static const int DEF_RECVEP             = 1;

    static DTU &get() {
        return inst;
    }

    void send(int ep, const void *msg, size_t size, label_t replylbl, int reply_ep);
    void reply(int ep, const void *msg, size_t size, size_t msgidx);
    void read(int ep, void *msg, size_t size, size_t off);
    void write(int ep, const void *msg, size_t size, size_t off);
    void cmpxchg(UNUSED int ep, UNUSED const void *msg, UNUSED size_t msgsize, UNUSED size_t off, UNUSED size_t size) {
        // TODO unsupported
    }

    bool fetch_msg(int epid) {
        reg_t r0 = read_reg(epid, 0);
        return (r0 & 0xFFFF) > 0;
    }

    DTU::Message *message(int epid) const {
        reg_t r1 = read_reg(epid, 1);
        reg_t r2 = read_reg(epid, 2);
        return reinterpret_cast<Message*>(r1 + ((r2 >> 16) & 0xFFFF));
    }
    Message *message_at(int, size_t) const {
        // TODO unsupported
        return nullptr;
    }

    size_t get_msgoff(int, RecvGate *) const {
        return 0;
    }
    size_t get_msgoff(int, RecvGate *, const Message *) const {
        // TODO unsupported
        return 0;
    }

    void ack_message(int ep) {
        wait_until_ready(ep);
        // ensure that we are really done with the message before acking it
        Sync::memory_barrier();
        write_reg(CmdRegs::COMMAND, buildCommand(ep, CmdOpCode::INC_READ_PTR));
        // ensure that we don't do something else before the ack
        Sync::memory_barrier();
    }

    bool wait() {
        // wait until the DTU wakes us up
        // note that we have a race-condition here. if a message arrives between the check and the
        // hlt, we miss it. this case is handled by a pin at the CPU, which indicates whether
        // unprocessed messages are available. if so, hlt does nothing. in this way, the ISA does
        // not have to be changed.
        if(read_reg(DtuRegs::MSGCNT) == 0)
            asm volatile ("hlt");
        return true;
    }
    void wait_until_ready(int) {
        while(read_reg(CmdRegs::COMMAND) != 0)
            ;
    }
    bool wait_for_mem_cmd() {
        // we've already waited
        return true;
    }

private:
    static reg_t read_reg(DtuRegs reg) {
        return read_reg(static_cast<size_t>(reg));
    }
    static reg_t read_reg(CmdRegs reg) {
        return read_reg(static_cast<size_t>(reg));
    }
    static reg_t read_reg(int ep, size_t idx) {
        return read_reg(DTU_REGS + CMD_REGS + EP_REGS * ep + idx);
    }
    static reg_t read_reg(size_t idx) {
        reg_t res;
        asm volatile (
            "mov (%1), %0"
            : "=r"(res)
            : "r"(BASE_ADDR + idx * sizeof(reg_t))
        );
        return res;
    }

    static void write_reg(DtuRegs reg, reg_t value) {
        write_reg(static_cast<size_t>(reg), value);
    }
    static void write_reg(CmdRegs reg, reg_t value) {
        write_reg(static_cast<size_t>(reg), value);
    }
    static void write_reg(size_t idx, reg_t value) {
        asm volatile (
            "mov %0, (%1)"
            : : "r"(value), "r"(BASE_ADDR + idx * sizeof(reg_t))
        );
    }

    static uintptr_t dtu_reg_addr(DtuRegs reg) {
        return BASE_ADDR + static_cast<size_t>(reg) * sizeof(reg_t);
    }
    static uintptr_t cmd_reg_addr(CmdRegs reg) {
        return BASE_ADDR + static_cast<size_t>(reg) * sizeof(reg_t);
    }
    static uintptr_t ep_regs_addr(int ep) {
        return BASE_ADDR + (DTU_REGS + CMD_REGS + ep * EP_REGS) * sizeof(reg_t);
    }

    static reg_t buildCommand(int ep, CmdOpCode c) {
        return static_cast<uint>(c) | (ep << 3);
    }

    static DTU inst;
};

}
