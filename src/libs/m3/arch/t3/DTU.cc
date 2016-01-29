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

#include <m3/util/Sync.h>
#include <m3/cap/MemGate.h>
#include <m3/cap/RecvGate.h>
#include <m3/DTU.h>
#include <m3/Log.h>

namespace m3 {

DTU DTU::inst INIT_PRIORITY(106);

size_t DTU::get_msgoff(int ep, RecvGate *rcvgate) const {
    return get_msgoff(ep, rcvgate, message(ep));
}

size_t DTU::get_msgoff(int, RecvGate *rcvgate, const Message *msg) const {
    size_t off = (reinterpret_cast<uintptr_t>(msg) - reinterpret_cast<uintptr_t>(rcvgate->buffer()->addr()));
    return off >> rcvgate->buffer()->msgorder();
}

void DTU::configure_recv(int ep, uintptr_t buf, uint order, UNUSED uint msgorder, UNUSED int flags) {
    size_t size = 1 << order;
    size_t msgsize = 1 << msgorder;
    config_local(ep, buf, size, msgsize);
    config_remote(ep, coreid(), ep, 0, 0);
    fire(ep, READ, 0);

    // TODO not possible because of bootstrap problems
    //LOG(IPC, "Activated receive-buffer @ " << (void*)buf << " on " << coreid() << ":" << ep);
}

Errors::Code DTU::send(int ep, const void *msg, size_t size, label_t reply_lbl, int reply_ep) {
    LOG(DTU, "-> " << fmt(size, 4) << "b from " << fmt(msg, "p") << " over " << ep);

    word_t *ptr = get_cmd_addr(ep, HEADER_CFG_REPLY_LABEL_SLOT_ENABLE_ADDR);
    store_to(ptr + 0, reply_lbl);
    config_header(ep, true, IDMA_HEADER_REPLY_CREDITS_MASK, reply_ep);
    config_transfer_size(ep, size);
    config_local(ep, reinterpret_cast<uintptr_t>(msg), 0, 0);
    fire(ep, WRITE, 1);

    return Errors::NO_ERROR;
}

Errors::Code DTU::reply(int ep, const void *msg, size_t size, size_t msgidx) {
    assert(((uintptr_t)msg & (PACKET_SIZE - 1)) == 0);
    assert((size & (PACKET_SIZE - 1)) == 0);

    LOG(DTU, ">> " << fmt(size, 4) << "b from " << fmt(msg, "p") << " to msg idx " << msgidx);

    word_t *ptr = get_cmd_addr(ep, REPLY_CAP_RESP_CMD);
    store_to(ptr + 0, ((size / DTU_PKG_SIZE) << 16) | msgidx);
    store_to(ptr + 1, (uintptr_t)msg);

    // TODO what to wait for??
    for(volatile int i = 0; i < 2; ++i)
        ;

    // TODO this assumes that we reply to the messages in order. but we do that currently
    // word_t addr = element_ptr(ep);
    // LOG(DTU, "Got " << fmt(addr, "p") << " for " << ep);
    // DTU::Message *m = reinterpret_cast<DTU::Message*>(addr);
    // LOG(DTU, "Sending " << m->length << " credits back to " << m->modid << ":" << m->slot);
    // send_credits(ep, m->modid, m->slot, 0x80000000 | m->length);

    return Errors::NO_ERROR;
}

void DTU::send_credits(int ep, uchar dst, int dst_ep, uint credits) {
    word_t *ptr = (word_t*)(PE_IDMA_CONFIG_ADDRESS + (EXTERN_CFG_ADDRESS_MODULE_CHIP_CTA_INC_CMD << PE_IDMA_CMD_POS)
        + (ep << PE_IDMA_SLOT_POS) + (0 << PE_IDMA_SLOT_TRG_ID_POS));
    uintptr_t addr = get_slot_addr(dst_ep);
    store_to(ptr + 0, addr);

    const int activate = 1;
    const int dstchipid = 0;
    const int addrinc = 0;
    word_t data = 0;
    data = (activate & 0xFF) << 8;
    data = (data | (0xFF & addrinc)) << 16;
    data = (data | ((dstchipid << 8) | dst));
    store_to(ptr + 1, data);
    store_to(ptr + 3, credits & ~0x80000000);

    ptr = (word_t*)(PE_IDMA_CONFIG_ADDRESS + (IDMA_CREDIT_RESPONSE_CMD << PE_IDMA_CMD_POS)
        + (ep << PE_IDMA_SLOT_POS) + (0 << PE_IDMA_SLOT_TRG_ID_POS));
    store_to(ptr + 0, credits);
}

Errors::Code DTU::read(int ep, void *msg, size_t size, size_t off) {
    LOG(DTU, "Reading " << size << "b @ " << off << " to " << msg <<  " over " << ep);

    // temporary hack: read current external address, add offset, store it and restore it later
    // set address + offset
    word_t *ptr = get_cmd_addr(ep, EXTERN_CFG_ADDRESS_MODULE_CHIP_CTA_INC_CMD);
    uintptr_t base = read_from(ptr + 0);
    store_to(ptr + 0, base + off);

    config_transfer_size(ep, size);
    config_local(ep, reinterpret_cast<uintptr_t>(msg), size, DTU_PKG_SIZE);
    fire(ep, READ, 1);

    Sync::memory_barrier();
    wait_until_ready(ep);
    Sync::memory_barrier();

    // restore old value
    store_to(ptr + 0, base);

    return Errors::NO_ERROR;
}

Errors::Code DTU::write(int ep, const void *msg, size_t size, size_t off) {
    LOG(DTU, "Writing " << size << "b @ " << off << " from " << msg << " over " << ep);

    // set address + offset
    word_t *ptr = get_cmd_addr(ep, EXTERN_CFG_ADDRESS_MODULE_CHIP_CTA_INC_CMD);
    uintptr_t base = read_from(ptr + 0);
    store_to(ptr + 0, base + off);

    config_transfer_size(ep, size);
    config_local(ep, reinterpret_cast<uintptr_t>(msg), 0, 0);
    fire(ep, WRITE, 1);

    Sync::memory_barrier();
    wait_until_ready(ep);
    Sync::memory_barrier();

    // restore old value
    store_to(ptr + 0, base);

    return Errors::NO_ERROR;
}

}
