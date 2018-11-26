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
#include <base/util/String.h>
#include <base/util/Util.h>
#include <base/Errors.h>
#include <pthread.h>
#include <ostream>
#include <iomanip>
#include <limits>
#include <assert.h>

// bad place, but prevents circular dependencies of headers
#define HEAP_SIZE           (64 * 1024 * 1024)

// we have no alignment or size requirements here
#define DTU_PKG_SIZE        (static_cast<size_t>(8))
#define EP_COUNT            16

namespace m3 {

class Gate;
class DTUBackend;

class DTU {
    friend class Gate;
    friend class MsgBackend;
    friend class SocketBackend;

    static constexpr size_t MAX_DATA_SIZE   = HEAP_SIZE;
public:
    struct Header {
        size_t length;          // = mtype -> has to be non-zero
        unsigned char opcode;   // should actually be part of length but causes trouble in msgsnd
        label_t label;
        uint8_t has_replycap;
        uint16_t pe;
        uint8_t rpl_ep;
        uint8_t snd_ep;
        label_t replylabel;
        uint8_t credits;
        uint8_t crd_ep;
    } PACKED;

    struct Buffer : public Header {
        char data[MAX_DATA_SIZE];
    };

    struct Message : public Header {
        epid_t send_ep() const {
            return snd_ep;
        }
        epid_t reply_ep() const {
            return rpl_ep;
        }

        unsigned char data[];
    } PACKED;

    static constexpr size_t HEADER_SIZE         = sizeof(Buffer) - MAX_DATA_SIZE;
    static const size_t HEADER_COUNT            = std::numeric_limits<size_t>::max();

    static constexpr size_t MAX_MSGS            = sizeof(word_t) * 8;

    // command registers
    static constexpr size_t CMD_ADDR            = 0;
    static constexpr size_t CMD_SIZE            = 1;
    static constexpr size_t CMD_EPID            = 2;
    static constexpr size_t CMD_CTRL            = 3;
    static constexpr size_t CMD_OFFSET          = 4;
    static constexpr size_t CMD_REPLYLBL        = 5;
    static constexpr size_t CMD_REPLY_EPID      = 6;
    static constexpr size_t CMD_LENGTH          = 7;

    // register starts and counts (cont.)
    static constexpr size_t CMDS_RCNT           = 1 + CMD_LENGTH;

    // receive buffer registers
    static constexpr size_t EP_BUF_ADDR         = 0;
    static constexpr size_t EP_BUF_ORDER        = 1;
    static constexpr size_t EP_BUF_MSGORDER     = 2;
    static constexpr size_t EP_BUF_ROFF         = 3;
    static constexpr size_t EP_BUF_WOFF         = 4;
    static constexpr size_t EP_BUF_MSGCNT       = 5;
    static constexpr size_t EP_BUF_MSGQID       = 6;
    static constexpr size_t EP_BUF_UNREAD       = 7;
    static constexpr size_t EP_BUF_OCCUPIED     = 8;

    // for sending message and accessing memory
    static constexpr size_t EP_PEID             = 9;
    static constexpr size_t EP_EPID             = 10;
    static constexpr size_t EP_LABEL            = 11;
    static constexpr size_t EP_CREDITS          = 12;
    static constexpr size_t EP_MSGORDER         = 13;

    // bits in ctrl register
    static constexpr word_t CTRL_START          = 0x1;
    static constexpr word_t CTRL_DEL_REPLY_CAP  = 0x2;
    static constexpr word_t CTRL_ERROR          = 0x4;

    static constexpr size_t OPCODE_SHIFT        = 3;

    // register counts (cont.)
    static constexpr size_t EPS_RCNT            = 1 + EP_MSGORDER;

    enum CmdFlags {
        NOPF                                    = 1,
    };

    enum Op {
        READ                                    = 1,
        WRITE                                   = 2,
        SEND                                    = 3,
        REPLY                                   = 4,
        RESP                                    = 5,
        FETCHMSG                                = 6,
        ACKMSG                                  = 7,
    };

    static const epid_t SYSC_SEP                = 0;
    static const epid_t SYSC_REP                = 1;
    static const epid_t UPCALL_REP              = 2;
    static const epid_t DEF_REP                 = 3;
    static const epid_t FIRST_FREE_EP           = 4;

    static DTU &get() {
        return inst;
    }

    static peid_t gaddr_to_pe(gaddr_t) {
        return 0;
    }
    static uintptr_t gaddr_to_virt(gaddr_t) {
        return 0;
    }
    static gaddr_t build_gaddr(int, uintptr_t) {
        return 0;
    }

    explicit DTU();

    void reset();

    word_t get_cmd(size_t reg) const {
        return _cmdregs[reg];
    }
    void set_cmd(size_t reg, word_t val) {
        _cmdregs[reg] = val;
    }

    word_t *ep_regs() {
        return const_cast<word_t*>(_epregs);
    }

    word_t get_ep(epid_t ep, size_t reg) const {
        return _epregs[ep * EPS_RCNT + reg];
    }
    void set_ep(epid_t ep, size_t reg, word_t val) {
        _epregs[ep * EPS_RCNT + reg] = val;
    }

    void configure(epid_t ep, label_t label, peid_t pe, epid_t dstep, word_t credits, uint msgorder) {
        configure(const_cast<word_t*>(_epregs), ep, label, pe, dstep, credits, msgorder);
    }
    static void configure(word_t *eps, epid_t ep, label_t label, peid_t pe, epid_t dstep, word_t credits, uint msgorder) {
        eps[ep * EPS_RCNT + EP_LABEL] = label;
        eps[ep * EPS_RCNT + EP_PEID] = pe;
        eps[ep * EPS_RCNT + EP_EPID] = dstep;
        eps[ep * EPS_RCNT + EP_CREDITS] = credits;
        eps[ep * EPS_RCNT + EP_MSGORDER] = msgorder;
    }

    void configure_recv(epid_t ep, uintptr_t buf, uint order, uint msgorder);

    Errors::Code send(epid_t ep, const void *msg, size_t size, label_t replylbl, epid_t replyep) {
        setup_command(ep, SEND, msg, size, 0, 0, replylbl, replyep);
        return exec_command();
    }
    Errors::Code reply(epid_t ep, const void *msg, size_t size, size_t msgidx) {
        setup_command(ep, REPLY, msg, size, msgidx, 0, label_t(), 0);
        Errors::Code res = exec_command();
        mark_read(ep, msgidx);
        return res;
    }
    Errors::Code read(epid_t ep, void *msg, size_t size, size_t off, uint) {
        setup_command(ep, READ, msg, size, off, size, label_t(), 0);
        return exec_command();
    }
    Errors::Code write(epid_t ep, const void *msg, size_t size, size_t off, uint) {
        setup_command(ep, WRITE, msg, size, off, size, label_t(), 0);
        return exec_command();
    }

    bool is_valid(epid_t) const {
        // TODO not supported
        return true;
    }
    bool has_missing_credits(epid_t) const {
        // TODO not supported
        return false;
    }

    Message *fetch_msg(epid_t ep) {
        if(get_ep(ep, EP_BUF_MSGCNT) == 0)
            return nullptr;

        set_cmd(CMD_EPID, ep);
        set_cmd(CMD_CTRL, (FETCHMSG << OPCODE_SHIFT) | CTRL_START);
        exec_command();
        return reinterpret_cast<Message*>(get_cmd(CMD_OFFSET));
    }

    size_t get_msgoff(epid_t, const Message *msg) const {
        return reinterpret_cast<size_t>(msg);
    }

    void mark_read(epid_t ep, size_t addr) {
        set_cmd(CMD_EPID, ep);
        set_cmd(CMD_OFFSET, addr);
        set_cmd(CMD_CTRL, (ACKMSG << OPCODE_SHIFT) | CTRL_START);
        exec_command();
    }

    bool is_ready() const {
        return (get_cmd(CMD_CTRL) >> OPCODE_SHIFT) == 0;
    }

    void setup_command(epid_t ep, int op, const void *msg, size_t size, size_t offset,
                               size_t len, label_t replylbl, epid_t replyep) {
        set_cmd(CMD_ADDR, reinterpret_cast<word_t>(msg));
        set_cmd(CMD_SIZE, size);
        set_cmd(CMD_EPID, ep);
        set_cmd(CMD_OFFSET, offset);
        set_cmd(CMD_LENGTH, len);
        set_cmd(CMD_REPLYLBL, replylbl);
        set_cmd(CMD_REPLY_EPID, replyep);
        if(op == REPLY)
            set_cmd(CMD_CTRL, (op << OPCODE_SHIFT) | CTRL_START);
        else
            set_cmd(CMD_CTRL, (op << OPCODE_SHIFT) | CTRL_START | CTRL_DEL_REPLY_CAP);
    }

    Errors::Code exec_command();

    void start();
    void stop();
    pthread_t tid() const {
        return _tid;
    }
    void try_sleep(bool report = true, uint64_t cycles = 0) const;

    void drop_msgs(epid_t ep, label_t label) {
        // we assume that the one that used the label can no longer send messages. thus, if there are
        // no messages yet, we are done.
        if(get_ep(ep, m3::DTU::EP_BUF_MSGCNT) == 0)
            return;

        goff_t base = get_ep(ep, m3::DTU::EP_BUF_ADDR);
        int order = get_ep(ep, m3::DTU::EP_BUF_ORDER);
        int msgorder = get_ep(ep, m3::DTU::EP_BUF_MSGORDER);
        word_t unread = get_ep(ep, m3::DTU::EP_BUF_UNREAD);
        int max = 1UL << (order - msgorder);
        for(int i = 0; i < max; ++i) {
            if(unread & (1UL << i)) {
                Message *msg = reinterpret_cast<Message*>(base + (static_cast<goff_t>(i) << msgorder));
                if(msg->label == label)
                    mark_read(ep, reinterpret_cast<goff_t>(msg));
            }
        }
    }

private:
    bool is_unread(word_t unread, int idx) const {
        return unread & (static_cast<word_t>(1) << idx);
    }
    void set_unread(word_t &unread, int idx, bool unr) {
        if(unr)
            unread |= static_cast<word_t>(1) << idx;
        else
            unread &= ~(static_cast<word_t>(1) << idx);
    }

    bool is_occupied(word_t occupied, int idx) const {
        return occupied & (static_cast<word_t>(1) << idx);
    }
    void set_occupied(word_t &occupied, int idx, bool occ) {
        if(occ)
            occupied |= static_cast<word_t>(1) << idx;
        else
            occupied &= ~(static_cast<word_t>(1) << idx);
    }

    word_t prepare_reply(epid_t ep, peid_t &dstpe, epid_t &dstep);
    word_t prepare_send(epid_t ep, peid_t &dstpe, epid_t &dstep);
    word_t prepare_read(epid_t ep, peid_t &dstpe, epid_t &dstep);
    word_t prepare_write(epid_t ep, peid_t &dstpe, epid_t &dstep);
    word_t prepare_fetchmsg(epid_t ep);
    word_t prepare_ackmsg(epid_t ep);

    void send_msg(epid_t ep, peid_t dstpe, epid_t dstep, bool isreply);
    void handle_read_cmd(epid_t ep);
    void handle_write_cmd(epid_t ep);
    void handle_resp_cmd();
    void handle_command(peid_t pe);
    void handle_msg(size_t len, epid_t ep);
    void handle_receive(epid_t ep);

    static word_t check_cmd(epid_t ep, int op, word_t addr, word_t credits, size_t offset, size_t length);
    static void *thread(void *arg);

    volatile bool _run;
    volatile word_t _cmdregs[CMDS_RCNT];
    // have to be aligned by 8 because it shouldn't collide with MemGate::RWX bits
    alignas(8) volatile word_t _epregs[EPS_RCNT * EP_COUNT];
    DTUBackend *_backend;
    pthread_t _tid;
    static Buffer _buf;
    static DTU inst;
};

}
