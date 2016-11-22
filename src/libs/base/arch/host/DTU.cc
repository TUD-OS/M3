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

#include <base/arch/host/HWInterrupts.h>
#include <base/arch/host/DTUBackend.h>
#include <base/log/Lib.h>
#include <base/util/Math.h>
#include <base/DTU.h>
#include <base/Env.h>
#include <base/Init.h>
#include <base/KIF.h>
#include <base/Panic.h>

#include <cstdio>
#include <cstring>
#include <sstream>
#include <unistd.h>

namespace m3 {

static void dumpBytes(uint8_t *bytes, size_t length) {
    std::ostringstream tmp;
    tmp << std::hex << std::setfill('0');
    for(size_t i = 0; i < length; ++i) {
        if(i > 0 && i % 8 == 0) {
            LLOG(DTUERR, "  " << tmp.str().c_str());
            tmp.str(std::string());
            tmp << std::hex << std::setfill('0');
        }
        tmp << "0x" << std::setw(2) << (unsigned)bytes[i] << " ";
    }
    if(!tmp.str().empty())
        LLOG(DTUERR, "  " << tmp.str().c_str());
}

INIT_PRIO_DTU DTU DTU::inst;
INIT_PRIO_DTU DTU::Buffer DTU::_buf;

DTU::DTU() : _run(true), _cmdregs(), _epregs(), _tid() {
}

void DTU::start() {
#if USE_MSGBACKEND
    _backend = new MsgBackend();
#else
    _backend = new SocketBackend();
#endif
    if(env()->is_kernel())
        _backend->create();

    int res = pthread_create(&_tid, nullptr, thread, this);
    if(res != 0)
        PANIC("pthread_create");
}

void DTU::reset() {
    // TODO this is a hack; we cannot leave the recv EPs here in all cases. sometimes the REPs are
    // not inherited so that the child might want to reuse the EP for something else, which does
    // not work, because the cmpxchg fails.
    for(epid_t i = 0; i < EP_COUNT; ++i) {
        if(get_ep(i, EP_BUF_ADDR) == 0)
            memset(ep_regs() + i * EPS_RCNT, 0, EPS_RCNT * sizeof(word_t));
    }

    delete _backend;
}

void DTU::try_sleep(bool, uint64_t) const {
    usleep(1);
}

void DTU::configure_recv(epid_t ep, uintptr_t buf, uint order, uint msgorder) {
    set_ep(ep, EP_BUF_ADDR, buf);
    set_ep(ep, EP_BUF_ORDER, order);
    set_ep(ep, EP_BUF_MSGORDER, msgorder);
    set_ep(ep, EP_BUF_ROFF, 0);
    set_ep(ep, EP_BUF_WOFF, 0);
    set_ep(ep, EP_BUF_MSGCNT, 0);
    set_ep(ep, EP_BUF_UNREAD, 0);
    set_ep(ep, EP_BUF_OCCUPIED, 0);
    assert((1UL << (order - msgorder)) <= sizeof(word_t) * 8);
}

word_t DTU::check_cmd(epid_t ep, int op, word_t label, word_t credits, size_t offset, size_t length) {
    if(op == READ || op == WRITE || op == CMPXCHG) {
        uint perms = label & KIF::Perm::RWX;
        if(!(perms & (1U << op))) {
            LLOG(DTUERR, "DMA-error: operation not permitted on ep " << ep << " (perms="
                    << perms << ", op=" << op << ")");
            return CTRL_ERROR;
        }
        if(offset >= credits || offset + length < offset || offset + length > credits) {
            LLOG(DTUERR, "DMA-error: invalid parameters (credits=" << credits
                    << ", offset=" << offset << ", datalen=" << length << ")");
            return CTRL_ERROR;
        }
    }
    return 0;
}

word_t DTU::prepare_reply(epid_t ep, peid_t &dstpe, epid_t &dstep) {
    const void *src = reinterpret_cast<const void*>(get_cmd(CMD_ADDR));
    const size_t size = get_cmd(CMD_SIZE);
    const size_t reply = get_cmd(CMD_OFFSET);
    const word_t bufaddr = get_ep(ep, EP_BUF_ADDR);
    const word_t ord = get_ep(ep, EP_BUF_ORDER);
    const word_t msgord = get_ep(ep, EP_BUF_MSGORDER);

    size_t idx = (reply - bufaddr) >> msgord;
    if(idx >= (1UL << (ord - msgord))) {
        LLOG(DTUERR, "DMA-error: EP" << ep << ": invalid message addr " << (void*)reply);
        return CTRL_ERROR;
    }

    Buffer *buf = reinterpret_cast<Buffer*>(reply);
    assert(buf->has_replycap);

    if(!buf->has_replycap) {
        LLOG(DTUERR, "DMA-error: EP" << ep << ": double-reply for msg " << (void*)reply);
        return CTRL_ERROR;
    }

    dstpe = buf->pe;
    dstep = buf->rpl_ep;
    _buf.label = buf->replylabel;
    _buf.credits = buf->length + HEADER_SIZE;
    _buf.crd_ep = buf->snd_ep;
    _buf.length = size;
    memcpy(_buf.data, src, size);
    // invalidate message for replying
    buf->has_replycap = false;
    return 0;
}

word_t DTU::prepare_send(epid_t ep, peid_t &dstpe, epid_t &dstep) {
    const void *src = reinterpret_cast<const void*>(get_cmd(CMD_ADDR));
    const word_t credits = get_ep(ep, EP_CREDITS);
    const size_t size = get_cmd(CMD_SIZE);
    // check if we have enough credits
    if(credits != static_cast<word_t>(-1)) {
        if(size + HEADER_SIZE > credits) {
            LLOG(DTUERR, "DMA-error: insufficient credits on ep " << ep
                    << " (have #" << fmt(credits, "x") << ", need #" << fmt(size + HEADER_SIZE, "x")
                    << ")." << " Ignoring send-command");
            return CTRL_ERROR;
        }
        set_ep(ep, EP_CREDITS, credits - (size + HEADER_SIZE));
    }

    dstpe = get_ep(ep, EP_PEID);
    dstep = get_ep(ep, EP_EPID);
    _buf.credits = 0;
    _buf.label = get_ep(ep, EP_LABEL);

    _buf.length = size;
    memcpy(_buf.data, src, size);
    return 0;
}

word_t DTU::prepare_read(epid_t ep, peid_t &dstpe, epid_t &dstep) {
    dstpe = get_ep(ep, EP_PEID);
    dstep = get_ep(ep, EP_EPID);

    _buf.credits = 0;
    _buf.label = get_ep(ep, EP_LABEL);
    _buf.length = sizeof(word_t) * 3;
    reinterpret_cast<word_t*>(_buf.data)[0] = get_cmd(CMD_OFFSET);
    reinterpret_cast<word_t*>(_buf.data)[1] = get_cmd(CMD_LENGTH);
    reinterpret_cast<word_t*>(_buf.data)[2] = get_cmd(CMD_ADDR);
    return 0;
}

word_t DTU::prepare_write(epid_t ep, peid_t &dstpe, epid_t &dstep) {
    const void *src = reinterpret_cast<const void*>(get_cmd(CMD_ADDR));
    const size_t size = get_cmd(CMD_SIZE);
    dstpe = get_ep(ep, EP_PEID);
    dstep = get_ep(ep, EP_EPID);

    _buf.credits = 0;
    _buf.label = get_ep(ep, EP_LABEL);
    _buf.length = sizeof(word_t) * 2;
    reinterpret_cast<word_t*>(_buf.data)[0] = get_cmd(CMD_OFFSET);
    reinterpret_cast<word_t*>(_buf.data)[1] = get_cmd(CMD_LENGTH);
    memcpy(_buf.data + _buf.length, src, size);
    _buf.length += size;
    return 0;
}

word_t DTU::prepare_cmpxchg(epid_t ep, peid_t &dstpe, epid_t &dstep) {
    const void *src = reinterpret_cast<const void*>(get_cmd(CMD_ADDR));
    const size_t size = get_cmd(CMD_SIZE);
    dstpe = get_ep(ep, EP_PEID);
    dstep = get_ep(ep, EP_EPID);

    if(size != get_cmd(CMD_LENGTH) * 2) {
        LLOG(DTUERR, "DMA-error: cmpxchg: CMD_SIZE != CMD_LENGTH * 2. Ignoring send-command");
        return CTRL_ERROR;
    }

    _buf.credits = 0;
    _buf.label = get_ep(ep, EP_LABEL);
    _buf.length = sizeof(word_t) * 3;
    reinterpret_cast<word_t*>(_buf.data)[0] = get_cmd(CMD_OFFSET);
    reinterpret_cast<word_t*>(_buf.data)[1] = get_cmd(CMD_LENGTH);
    reinterpret_cast<word_t*>(_buf.data)[2] = get_cmd(CMD_ADDR);
    memcpy(_buf.data + _buf.length, src, size);
    _buf.length += size;
    return 0;
}

word_t DTU::prepare_sendcrd(epid_t ep, peid_t &dstpe, epid_t &dstep) {
    const size_t size = get_cmd(CMD_SIZE);
    const epid_t crdep = get_cmd(CMD_OFFSET);

    dstpe = get_ep(ep, EP_PEID);
    dstep = get_ep(ep, EP_EPID);
    _buf.credits = size + HEADER_SIZE;
    _buf.length = 1;    // can't be 0
    _buf.crd_ep = crdep;
    return 0;
}

word_t DTU::prepare_ackmsg(epid_t ep) {
    const word_t addr = get_cmd(CMD_OFFSET);
    size_t bufaddr = get_ep(ep, EP_BUF_ADDR);
    size_t msgord = get_ep(ep, EP_BUF_MSGORDER);
    size_t ord = get_ep(ep, EP_BUF_ORDER);

    size_t idx = (addr - bufaddr) >> msgord;
    if(idx >= (1UL << (ord - msgord))) {
        LLOG(DTUERR, "DMA-error: EP" << ep << ": invalid message addr " << (void*)addr);
        return CTRL_ERROR;
    }

    word_t occupied = get_ep(ep, EP_BUF_OCCUPIED);
    assert(is_occupied(occupied, idx));
    set_occupied(occupied, idx, false);
    set_ep(ep, EP_BUF_OCCUPIED, occupied);

    LLOG(DTU, "EP" << ep << ": acked message at index " << idx);
    return 0;
}

word_t DTU::prepare_fetchmsg(epid_t ep) {
    word_t msgs = get_ep(ep, EP_BUF_MSGCNT);
    if(msgs == 0)
        return CTRL_ERROR;

    size_t roff = get_ep(ep, EP_BUF_ROFF);
    word_t unread = get_ep(ep, EP_BUF_UNREAD);
    size_t ord = get_ep(ep, EP_BUF_ORDER);
    size_t msgord = get_ep(ep, EP_BUF_MSGORDER);
    size_t size = 1UL << (ord - msgord);

    size_t i;
    for(i = roff; i < size; ++i) {
        if(is_unread(unread, i))
            goto found;
    }
    for(i = 0; i < roff; ++i) {
        if(is_unread(unread, i))
            goto found;
    }

    // should not get here
    assert(false);

found:
    assert(is_occupied(get_ep(ep, EP_BUF_OCCUPIED), i));

    set_unread(unread, i, false);
    msgs--;
    roff = i + 1;
    assert(Math::bits_set(unread) == msgs);

    LLOG(DTU, "EP" << ep << ": fetched message at index " << i << " (count=" << msgs << ")");

    set_ep(ep, EP_BUF_UNREAD, unread);
    set_ep(ep, EP_BUF_ROFF, roff);
    set_ep(ep, EP_BUF_MSGCNT, msgs);

    size_t addr = get_ep(ep, EP_BUF_ADDR);
    set_cmd(CMD_OFFSET, addr + i * (1UL << msgord));

    return 0;
}

void DTU::handle_command(peid_t pe) {
    word_t newctrl = 0;
    peid_t dstpe;
    epid_t dstep;

    // clear error
    set_cmd(CMD_CTRL, get_cmd(CMD_CTRL) & ~CTRL_ERROR);

    // get regs
    const epid_t ep = get_cmd(CMD_EPID);
    const epid_t reply_ep = get_cmd(CMD_REPLY_EPID);
    const word_t ctrl = get_cmd(CMD_CTRL);
    int op = (ctrl >> OPCODE_SHIFT) & 0xF;
    if(ep >= EP_COUNT) {
        LLOG(DTUERR, "DMA-error: invalid ep-id (" << ep << ")");
        newctrl |= CTRL_ERROR;
        goto error;
    }

    newctrl |= check_cmd(ep, op, get_ep(ep, EP_LABEL), get_ep(ep, EP_CREDITS),
        get_cmd(CMD_OFFSET), get_cmd(CMD_LENGTH));
    switch(op) {
        case REPLY:
            newctrl |= prepare_reply(ep, dstpe, dstep);
            break;
        case SEND:
            newctrl |= prepare_send(ep, dstpe, dstep);
            break;
        case READ:
            newctrl |= prepare_read(ep, dstpe, dstep);
            break;
        case WRITE:
            newctrl |= prepare_write(ep, dstpe, dstep);
            break;
        case CMPXCHG:
            newctrl |= prepare_cmpxchg(ep, dstpe, dstep);
            break;
        case SENDCRD:
            newctrl |= prepare_sendcrd(ep, dstpe, dstep);
            break;
        case FETCHMSG:
            newctrl |= prepare_fetchmsg(ep);
            set_cmd(CMD_CTRL, newctrl);
            return;
        case ACKMSG:
            newctrl |= prepare_ackmsg(ep);
            set_cmd(CMD_CTRL, newctrl);
            return;
    }
    if(newctrl & CTRL_ERROR)
        goto error;

    // prepare message (add length and label)
    _buf.opcode = op;
    if(ctrl & CTRL_DEL_REPLY_CAP) {
        _buf.has_replycap = 1;
        _buf.pe = pe;
        _buf.snd_ep = ep;
        _buf.rpl_ep = reply_ep;
        _buf.replylabel = get_cmd(CMD_REPLYLBL);
    }
    else
        _buf.has_replycap = 0;

    send_msg(ep, dstpe, dstep, op == REPLY);

error:
    set_cmd(CMD_CTRL, newctrl);
}

void DTU::send_msg(epid_t ep, peid_t dstpe, epid_t dstep, bool isreply) {
    LLOG(DTU, (isreply ? ">> " : "-> ") << fmt(_buf.length, 3) << "b"
            << " lbl=" << fmt(_buf.label, "#0x", sizeof(label_t) * 2)
            << " over " << ep << " to pe:ep=" << dstpe << ":" << dstep
            << " (crd=#" << fmt((long)get_ep(dstep, EP_CREDITS), "x")
            << " rep=" << _buf.rpl_ep << ")");

    _backend->send(dstpe, dstep, &_buf);
}

void DTU::handle_read_cmd(epid_t ep) {
    word_t base = _buf.label & ~static_cast<word_t>(KIF::Perm::RWX);
    word_t offset = base + reinterpret_cast<word_t*>(_buf.data)[0];
    word_t length = reinterpret_cast<word_t*>(_buf.data)[1];
    word_t dest = reinterpret_cast<word_t*>(_buf.data)[2];
    LLOG(DTU, "(read) " << length << " bytes from #" << fmt(base, "x")
            << "+#" << fmt(offset - base, "x") << " -> " << fmt(dest, "p"));
    peid_t dstpe = _buf.pe;
    epid_t dstep = _buf.rpl_ep;
    assert(length <= sizeof(_buf.data));

    _buf.opcode = RESP;
    _buf.credits = 0;
    _buf.label = 0;
    _buf.length = sizeof(word_t) * 3;
    reinterpret_cast<word_t*>(_buf.data)[0] = dest;
    reinterpret_cast<word_t*>(_buf.data)[1] = length;
    reinterpret_cast<word_t*>(_buf.data)[2] = 0;
    memcpy(_buf.data + _buf.length, reinterpret_cast<void*>(offset), length);
    _buf.length += length;
    send_msg(ep, dstpe, dstep, true);
}

void DTU::handle_write_cmd(epid_t) {
    word_t base = _buf.label & ~static_cast<word_t>(KIF::Perm::RWX);
    word_t offset = base + reinterpret_cast<word_t*>(_buf.data)[0];
    word_t length = reinterpret_cast<word_t*>(_buf.data)[1];
    LLOG(DTU, "(write) " << length << " bytes to #" << fmt(base, "x")
            << "+#" << fmt(offset - base, "x"));
    assert(length <= sizeof(_buf.data));
    memcpy(reinterpret_cast<void*>(offset), _buf.data + sizeof(word_t) * 2, length);
}

void DTU::handle_resp_cmd() {
    word_t base = _buf.label & ~static_cast<word_t>(KIF::Perm::RWX);
    word_t offset = base + reinterpret_cast<word_t*>(_buf.data)[0];
    word_t length = reinterpret_cast<word_t*>(_buf.data)[1];
    word_t resp = reinterpret_cast<word_t*>(_buf.data)[2];
    LLOG(DTU, "(resp) " << length << " bytes to #" << fmt(base, "x")
            << "+#" << fmt(offset - base, "x") << " -> " << resp);
    assert(length <= sizeof(_buf.data));
    memcpy(reinterpret_cast<void*>(offset), _buf.data + sizeof(word_t) * 3, length);
    /* provide feedback to SW */
    set_cmd(CMD_CTRL, get_cmd(CMD_CTRL) | resp);
    set_cmd(CMD_SIZE, 0);
}

void DTU::handle_cmpxchg_cmd(epid_t ep) {
    word_t base = _buf.label & ~static_cast<word_t>(KIF::Perm::RWX);
    word_t offset = base + reinterpret_cast<word_t*>(_buf.data)[0];
    word_t length = reinterpret_cast<word_t*>(_buf.data)[1];
    LLOG(DTU, "(cmpxchg) " << length << " bytes @ #" << fmt(base, "x")
            << "+#" << fmt(offset - base, "x"));
    peid_t dstpe = _buf.pe;
    epid_t dstep = _buf.rpl_ep;

    // do the compare exepge; no need to lock anything or so because our DTU is single-threaded
    word_t res;
    if(memcmp(reinterpret_cast<void*>(offset), _buf.data + sizeof(word_t) * 3, length) == 0) {
        memcpy(reinterpret_cast<void*>(offset), _buf.data + sizeof(word_t) * 3 + length, length);
        res = 0;
    }
    else {
        uint8_t *expected = reinterpret_cast<uint8_t*>(_buf.data) + sizeof(word_t) * 3;
        uint8_t *actual = reinterpret_cast<uint8_t*>(offset);
        LLOG(DTUERR, "(cmpxchg) failed; expected:");
        dumpBytes(expected, length);
        LLOG(DTUERR, "actual:");
        dumpBytes(actual, length);
        res = CTRL_ERROR;
    }

    // use the write command to send the data back to the desired location
    _buf.opcode = RESP;
    _buf.credits = 0;
    _buf.label = 0;
    _buf.length = sizeof(word_t) * 3;
    reinterpret_cast<word_t*>(_buf.data)[0] = 0;
    reinterpret_cast<word_t*>(_buf.data)[1] = 0;
    reinterpret_cast<word_t*>(_buf.data)[2] = res;
    send_msg(ep, dstpe, dstep, true);
}

void DTU::handle_msg(size_t len, epid_t ep) {
    const size_t msgord = get_ep(ep, EP_BUF_MSGORDER);
    const size_t msgsize = 1UL << msgord;
    if(len > msgsize) {
        LLOG(DTUERR, "DMA-error: dropping message because space is not sufficient"
                << " (required: " << len << ", available: " << msgsize << ")");
        return;
    }

    word_t occupied = get_ep(ep, EP_BUF_OCCUPIED);
    word_t unread = get_ep(ep, EP_BUF_UNREAD);
    word_t msgs = get_ep(ep, EP_BUF_MSGCNT);
    size_t woff = get_ep(ep, EP_BUF_WOFF);
    size_t ord = get_ep(ep, EP_BUF_ORDER);
    size_t size = 1UL << (ord - msgord);

    size_t i;
    for (i = woff; i < size; ++i)
    {
        if (!is_occupied(occupied, i))
            goto found;
    }
    for (i = 0; i < woff; ++i)
    {
        if (!is_occupied(occupied, i))
            goto found;
    }

    LLOG(DTUERR, "EP" << ep << ": dropping message because no slot is free");
    return;

found:
    set_occupied(occupied, i, true);
    set_unread(unread, i, true);
    msgs++;
    woff = i + 1;
    assert(Math::bits_set(unread) == msgs);

    LLOG(DTU, "EP" << ep << ": put message at index " << i << " (count=" << msgs << ")");

    set_ep(ep, EP_BUF_OCCUPIED, occupied);
    set_ep(ep, EP_BUF_UNREAD, unread);
    set_ep(ep, EP_BUF_MSGCNT, msgs);
    set_ep(ep, EP_BUF_WOFF, woff);

    size_t addr = get_ep(ep, EP_BUF_ADDR);
    memcpy(reinterpret_cast<void*>(addr + i * (1UL << msgord)), &_buf, len);
}

void DTU::handle_receive(epid_t ep) {
    ssize_t res = _backend->recv(ep, &_buf);
    if(res < 0)
        return;

    const int op = _buf.opcode;
    switch(op) {
        case READ:
            handle_read_cmd(ep);
            break;
        case RESP:
            handle_resp_cmd();
            break;
        case WRITE:
            handle_write_cmd(ep);
            break;
        case CMPXCHG:
            handle_cmpxchg_cmd(ep);
            break;
        case SEND:
        case REPLY:
            handle_msg(static_cast<size_t>(res), ep);
            break;
    }

    // refill credits
    if(_buf.crd_ep >= EP_COUNT)
        LLOG(DTUERR, "DMA-error: should give credits to endpoint " << _buf.crd_ep);
    else {
        word_t credits = get_ep(_buf.crd_ep, EP_CREDITS);
        if(_buf.credits && credits != static_cast<word_t>(-1)) {
            LLOG(DTU, "Refilling credits of ep " << _buf.crd_ep
                << " from #" << fmt(credits, "x") << " to #" << fmt(credits + _buf.credits, "x"));
            set_ep(_buf.crd_ep, EP_CREDITS, credits + _buf.credits);
        }
    }

    if(op != SENDCRD) {
        LLOG(DTU, "<- " << fmt(static_cast<size_t>(res) - HEADER_SIZE, 3)
               << "b lbl=" << fmt(_buf.label, "#0x", sizeof(label_t) * 2)
               << " ep=" << ep
               << " (cnt=#" << fmt(get_ep(ep, EP_BUF_MSGCNT), "x") << ","
               << "crd=#" << fmt(get_ep(ep, EP_CREDITS), "x") << ")");
    }
}

void *DTU::thread(void *arg) {
    DTU *dma = static_cast<DTU*>(arg);
    peid_t pe = env()->pe;

    // don't allow any interrupts here
    HWInterrupts::Guard noints;
    while(dma->_run) {
        // should we send something?
        if(dma->get_cmd(CMD_CTRL) & CTRL_START)
            dma->handle_command(pe);

        // have we received a message?
        for(epid_t i = 0; i < EP_COUNT; ++i)
            dma->handle_receive(i);

        dma->try_sleep();
    }

    if(env()->is_kernel())
        dma->_backend->destroy();
    delete dma->_backend;
    return 0;
}

}
