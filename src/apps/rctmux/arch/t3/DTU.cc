/**
 * Copyright (C) 2015-2016, René Küttner <rene.kuettner@.tu-dresden.de>
 * Economic rights: Technische Universität Dresden (Germany)
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

#include <base/util/Sync.h>
#include <base/DTU.h>

// this is mostly taken from libm3 (arch/t3/DTU.cc)

namespace m3 {

DTU m3::DTU::inst;

/* Re-implement the necessary DTU methods we need. */

Errors::Code DTU::send(epid_t ep, const void *msg, size_t size, label_t reply_lbl,
                       epid_t reply_ep)
{
    word_t *ptr = get_cmd_addr(ep, HEADER_CFG_REPLY_LABEL_SLOT_ENABLE_ADDR);
    store_to(ptr + 0, reply_lbl);
    config_header(ep, true, IDMA_HEADER_REPLY_CREDITS_MASK, reply_ep);
    config_transfer_size(ep, size);
    config_local(ep, reinterpret_cast<uintptr_t>(msg), 0, 0);
    fire(ep, WRITE, 1);

    return Errors::NO_ERROR;
}

Errors::Code DTU::read(epid_t ep, void *msg, size_t size, size_t off)
{
    // temporary hack: read current external address, add offset, store it and
    // restore it later, set address + offset
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

Errors::Code DTU::write(epid_t ep, const void *msg, size_t size, size_t off)
{
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

} /* namespace m3 */
