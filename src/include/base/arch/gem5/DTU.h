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
#include <base/util/Util.h>
#include <base/CPU.h>
#include <base/Env.h>
#include <base/Errors.h>
#include <assert.h>

#define DTU_PKG_SIZE        (static_cast<size_t>(8))

namespace kernel {
class DTU;
class DTURegs;
class DTUState;
class VPE;
}

namespace m3 {

class DTU {
    friend class kernel::DTU;
    friend class kernel::DTURegs;
    friend class kernel::DTUState;
    friend class kernel::VPE;

    explicit DTU() {
    }

public:
    typedef uint64_t reg_t;

private:
    static const uintptr_t BASE_ADDR        = 0xF0000000;
    static const size_t DTU_REGS            = 9;
    static const size_t CMD_REGS            = 7;
    static const size_t EP_REGS             = 3;

    static const size_t CREDITS_UNLIM       = 0xFFFF;
    static const size_t MAX_PKT_SIZE        = 1024;

    enum class DtuRegs {
        FEATURES            = 0,
        ROOT_PT             = 1,
        PF_EP               = 2,
        RW_BARRIER          = 3,
        VPE_ID              = 4,
        CUR_TIME            = 5,
        IDLE_TIME           = 6,
        MSG_CNT             = 7,
        EXT_CMD             = 8,
    };

    enum class CmdRegs {
        COMMAND             = DTU_REGS + 0,
        ABORT               = DTU_REGS + 1,
        DATA_ADDR           = DTU_REGS + 2,
        DATA_SIZE           = DTU_REGS + 3,
        OFFSET              = DTU_REGS + 4,
        REPLY_EP            = DTU_REGS + 5,
        REPLY_LABEL         = DTU_REGS + 6,
    };

    enum MemFlags : reg_t {
        R                   = 1 << 0,
        W                   = 1 << 1,
    };

    enum StatusFlags : reg_t {
        PRIV                = 1 << 0,
        PAGEFAULTS          = 1 << 1,
        COM_DISABLED        = 1 << 2,
        IRQ_WAKEUP          = 1 << 3,
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
        FETCH_MSG           = 5,
        ACK_MSG             = 6,
        SLEEP               = 7,
        DEBUG_MSG           = 8,
        PRINT               = 9,
    };

    enum class ExtCmdOpCode {
        IDLE                = 0,
        WAKEUP_CORE         = 1,
        INV_EP              = 2,
        INV_PAGE            = 3,
        INV_TLB             = 4,
        INJECT_IRQ          = 5,
        RESET               = 6,
    };

public:
    typedef uint64_t pte_t;

    enum CmdFlags {
        NOPF                = 1,
    };

    enum {
        PTE_BITS            = 3,
        PTE_SIZE            = 1 << PTE_BITS,
        LEVEL_CNT           = 2,
        LEVEL_BITS          = PAGE_BITS - PTE_BITS,
        LEVEL_MASK          = (1 << LEVEL_BITS) - 1,
        PTE_REC_IDX         = LEVEL_MASK,
    };

    enum {
        PTE_R               = 1,
        PTE_W               = 2,
        PTE_X               = 4,
        PTE_I               = 8,
        PTE_GONE            = 16,
        PTE_RW              = PTE_R | PTE_W,
        PTE_RWX             = PTE_RW | PTE_X,
        PTE_IRWX            = PTE_RWX | PTE_I,
    };

    enum {
        ABORT_VPE           = 1,
        ABORT_CMD           = 2,
    };

    struct Header {
        enum {
            FL_REPLY            = 1 << 0,
            FL_GRANT_CREDITS    = 1 << 1,
            FL_REPLY_ENABLED    = 1 << 2,
            FL_PAGEFAULT        = 1 << 3,
            FL_REPLY_FAILED     = 1 << 4,
        };

        uint8_t flags; // if bit 0 is set its a reply, if bit 1 is set we grant credits
        uint8_t senderPe;
        uint8_t senderEp;
        uint8_t replyEp;   // for a normal message this is the reply epId
                           // for a reply this is the enpoint that receives credits
        uint16_t length;
        uint16_t senderVpeId;

        uint64_t label;
        uint64_t replylabel;
    } PACKED;

    struct Message : Header {
        epid_t send_ep() const {
            return senderEp;
        }
        epid_t reply_ep() const {
            return replyEp;
        }

        unsigned char data[];
    } PACKED;

    static const size_t HEADER_SIZE         = sizeof(Header);

    static const epid_t SYSC_SEP            = 0;
    static const epid_t SYSC_REP            = 1;
    static const epid_t UPCALL_REP          = 2;
    static const epid_t DEF_REP             = 3;
    static const epid_t FIRST_FREE_EP       = 4;

    static DTU &get() {
        return inst;
    }

    static peid_t gaddr_to_pe(gaddr_t noc) {
        return (noc >> 52) - 0x80;
    }
    static uintptr_t gaddr_to_virt(gaddr_t noc) {
        return noc & ((static_cast<gaddr_t>(1) << 52) - 1);
    }
    static gaddr_t build_gaddr(peid_t pe, uintptr_t virt) {
        return (static_cast<gaddr_t>(0x80 + pe) << 52) | virt;
    }

    Errors::Code send(epid_t ep, const void *msg, size_t size, label_t replylbl, epid_t reply_ep);
    Errors::Code reply(epid_t ep, const void *msg, size_t size, size_t off);
    Errors::Code read(epid_t ep, void *msg, size_t size, size_t off, uint flags);
    Errors::Code write(epid_t ep, const void *msg, size_t size, size_t off, uint flags);
    Errors::Code cmpxchg(epid_t, const void *, size_t, size_t, size_t) {
        // TODO unsupported
        return Errors::NONE;
    }

    void abort(uint flags, reg_t *cmdreg) {
        // save the old value before aborting
        *cmdreg = read_reg(CmdRegs::COMMAND);
        write_reg(CmdRegs::ABORT, flags);
        // wait until the abort is finished. if a command was running and was aborted, we want to
        // retry it later. if no command was running, we want to keep the error code though.
        if(get_error() != Errors::ABORT && (*cmdreg & 0xF) != static_cast<reg_t>(CmdOpCode::IDLE))
            *cmdreg = static_cast<reg_t>(CmdOpCode::IDLE);
    }
    void retry(reg_t cmd) {
        write_reg(CmdRegs::COMMAND, cmd);
    }

    bool is_valid(epid_t ep) const {
        reg_t r0 = read_reg(ep, 0);
        return static_cast<EpType>(r0 >> 61) != EpType::INVALID;
    }

    Message *fetch_msg(epid_t ep) const {
        write_reg(CmdRegs::COMMAND, buildCommand(ep, CmdOpCode::FETCH_MSG));
        CPU::memory_barrier();
        return reinterpret_cast<Message*>(read_reg(CmdRegs::OFFSET));
    }

    size_t get_msgoff(epid_t, const Message *msg) const {
        return reinterpret_cast<uintptr_t>(msg);
    }

    void mark_read(epid_t ep, size_t off) {
        write_reg(CmdRegs::OFFSET, off);
        // ensure that we are really done with the message before acking it
        CPU::memory_barrier();
        write_reg(CmdRegs::COMMAND, buildCommand(ep, CmdOpCode::ACK_MSG));
        // ensure that we don't do something else before the ack
        CPU::memory_barrier();
    }

    uint msgcnt() {
        return read_reg(DtuRegs::MSG_CNT);
    }

    cycles_t tsc() const {
        return read_reg(DtuRegs::CUR_TIME);
    }

    void try_sleep(bool yield = true, uint64_t cycles = 0);
    void sleep(uint64_t cycles = 0) {
        write_reg(CmdRegs::OFFSET, cycles);
        CPU::memory_barrier();
        write_reg(CmdRegs::COMMAND, buildCommand(0, CmdOpCode::SLEEP));
    }

    void wait_until_ready(epid_t) const {
        // this is superfluous now, but leaving it here improves the syscall time by 40 cycles (!!!)
        // compilers are the worst. let's get rid of them and just write assembly code again ;)
        while((read_reg(CmdRegs::COMMAND) & 0xF) != 0)
            ;
    }
    bool wait_for_mem_cmd() const {
        // we've already waited
        return true;
    }

    void debug_msg(uint64_t msg) {
        write_reg(CmdRegs::OFFSET, msg);
        CPU::memory_barrier();
        write_reg(CmdRegs::COMMAND, buildCommand(0, CmdOpCode::DEBUG_MSG));
    }

    void print(const char *str, size_t len) {
        write_reg(CmdRegs::DATA_ADDR, reinterpret_cast<uintptr_t>(str));
        write_reg(CmdRegs::DATA_SIZE, len);
        CPU::memory_barrier();
        write_reg(CmdRegs::COMMAND, buildCommand(0, CmdOpCode::PRINT));
    }

private:
    Errors::Code transfer(reg_t cmd, uintptr_t data, size_t size, size_t off);

    static Errors::Code get_error() {
        while(true) {
            reg_t cmd = read_reg(CmdRegs::COMMAND);
            if(static_cast<CmdOpCode>(cmd & 0xF) == CmdOpCode::IDLE)
                return static_cast<Errors::Code>(cmd >> 13);
        }
        UNREACHED;
    }

    static reg_t read_reg(DtuRegs reg) {
        return read_reg(static_cast<size_t>(reg));
    }
    static reg_t read_reg(CmdRegs reg) {
        return read_reg(static_cast<size_t>(reg));
    }
    static reg_t read_reg(epid_t ep, size_t idx) {
        return read_reg(DTU_REGS + CMD_REGS + EP_REGS * ep + idx);
    }
    static reg_t read_reg(size_t idx) {
        return CPU::read8b(BASE_ADDR + idx * sizeof(reg_t));
    }

    static void write_reg(DtuRegs reg, reg_t value) {
        write_reg(static_cast<size_t>(reg), value);
    }
    static void write_reg(CmdRegs reg, reg_t value) {
        write_reg(static_cast<size_t>(reg), value);
    }
    static void write_reg(size_t idx, reg_t value) {
        CPU::write8b(BASE_ADDR + idx * sizeof(reg_t), value);
    }

    static uintptr_t dtu_reg_addr(DtuRegs reg) {
        return BASE_ADDR + static_cast<size_t>(reg) * sizeof(reg_t);
    }
    static uintptr_t cmd_reg_addr(CmdRegs reg) {
        return BASE_ADDR + static_cast<size_t>(reg) * sizeof(reg_t);
    }
    static uintptr_t ep_regs_addr(epid_t ep) {
        return BASE_ADDR + (DTU_REGS + CMD_REGS + ep * EP_REGS) * sizeof(reg_t);
    }

    static reg_t buildCommand(epid_t ep, CmdOpCode c, uint flags = 0) {
        return static_cast<reg_t>(c) |
                (static_cast<reg_t>(ep) << 4) |
                (static_cast<reg_t>(flags) << 12);
    }

    static DTU inst;
};

}
