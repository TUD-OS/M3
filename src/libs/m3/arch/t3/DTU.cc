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

#include <m3/util/Sync.h>
#include <m3/cap/MemGate.h>
#include <m3/DTU.h>
#include <m3/ChanMng.h>
#include <m3/Log.h>

namespace m3 {

DTU DTU::inst INIT_PRIORITY(106);

void DTU::set_receiving(int slot, uintptr_t buf, uint order, UNUSED uint msgorder, UNUSED int flags) {
    size_t size = 1 << order;
    size_t msgsize = 1 << msgorder;
    config_local(slot, buf, size, msgsize);
    config_remote(slot, coreid(), slot, 0, 0);
    fire(slot, READ, 0);

    // TODO not possible because of bootstrap problems
    //LOG(IPC, "Activated receive-buffer @ " << (void*)buf << " on " << coreid() << ":" << slot);
}

void DTU::send(int slot, const void *msg, size_t size, label_t reply_lbl, int reply_slot) {
    LOG(DTU, "-> " << fmt(size, 4) << "b from " << fmt(msg, "p") << " over " << slot);

    word_t *ptr = get_cmd_addr(slot, HEADER_CFG_REPLY_LABEL_SLOT_ENABLE_ADDR);
    store_to(ptr + 0, reply_lbl);
    config_header(slot, true, IDMA_HEADER_REPLY_CREDITS_MASK, reply_slot);
    config_transfer_size(slot, size);
    config_local(slot, reinterpret_cast<uintptr_t>(msg), 0, 0);
    fire(slot, WRITE, 1);
}

void DTU::reply(int slot, const void *msg, size_t size, size_t msgidx) {
    assert(((uintptr_t)msg & (PACKET_SIZE - 1)) == 0);
    assert((size & (PACKET_SIZE - 1)) == 0);

    LOG(DTU, ">> " << fmt(size, 4) << "b from " << fmt(msg, "p") << " to msg idx " << msgidx);

    word_t *ptr = get_cmd_addr(slot, REPLY_CAP_RESP_CMD);
    store_to(ptr + 0, ((size / DTU_PKG_SIZE) << 16) | msgidx);
    store_to(ptr + 1, (uintptr_t)msg);

    // TODO what to wait for??
    for(volatile int i = 0; i < 2; ++i)
        ;

    // TODO this assumes that we reply to the messages in order. but we do that currently
    // word_t addr = element_ptr(slot);
    // LOG(DTU, "Got " << fmt(addr, "p") << " for " << slot);
    // ChanMng::Message *m = reinterpret_cast<ChanMng::Message*>(addr);
    // LOG(DTU, "Sending " << m->length << " credits back to " << m->modid << ":" << m->slot);
    // send_credits(slot, m->modid, m->slot, 0x80000000 | m->length);
}

void DTU::send_credits(int slot, uchar dst, int dst_slot, uint credits) {
    word_t *ptr = (word_t*)(PE_IDMA_CONFIG_ADDRESS + (EXTERN_CFG_ADDRESS_MODULE_CHIP_CTA_INC_CMD << PE_IDMA_CMD_POS)
        + (slot << PE_IDMA_SLOT_POS) + (0 << PE_IDMA_SLOT_TRG_ID_POS));
    uintptr_t addr = get_slot_addr(dst_slot);
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
        + (slot << PE_IDMA_SLOT_POS) + (0 << PE_IDMA_SLOT_TRG_ID_POS));
    store_to(ptr + 0, credits);
}

void DTU::read(int slot, void *msg, size_t size, size_t off) {
    LOG(DTU, "Reading " << size << "b @ " << off << " to " << msg <<  " over " << slot);

    // temporary hack: read current external address, add offset, store it and restore it later
    // set address + offset
    word_t *ptr = get_cmd_addr(slot, EXTERN_CFG_ADDRESS_MODULE_CHIP_CTA_INC_CMD);
    uintptr_t base = read_from(ptr + 0);
    store_to(ptr + 0, base + off);

    config_transfer_size(slot, size);
    config_local(slot, reinterpret_cast<uintptr_t>(msg), size, DTU_PKG_SIZE);
    fire(slot, READ, 1);

    Sync::memory_barrier();
    wait_until_ready(slot);
    Sync::memory_barrier();

    // restore old value
    store_to(ptr + 0, base);
}

void DTU::write(int slot, const void *msg, size_t size, size_t off) {
    LOG(DTU, "Writing " << size << "b @ " << off << " from " << msg << " over " << slot);

    // set address + offset
    word_t *ptr = get_cmd_addr(slot, EXTERN_CFG_ADDRESS_MODULE_CHIP_CTA_INC_CMD);
    uintptr_t base = read_from(ptr + 0);
    store_to(ptr + 0, base + off);

    config_transfer_size(slot, size);
    config_local(slot, reinterpret_cast<uintptr_t>(msg), 0, 0);
    fire(slot, WRITE, 1);

    Sync::memory_barrier();
    wait_until_ready(slot);
    Sync::memory_barrier();

    // restore old value
    store_to(ptr + 0, base);
}

}