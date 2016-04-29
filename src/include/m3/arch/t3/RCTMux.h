/**
 * Copyright (C) 2015, René Küttner <rene.kuettner@.tu-dresden.de>
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

#include <m3/Config.h>

#define RCTMUX_STORE_EP     (EP_COUNT - 1)
#define RCTMUX_RESTORE_EP   (EP_COUNT - 2)

#if defined(__cplusplus)
namespace m3 {

/**
 * The SPMemLayout structure holds information on the scratchpad memory
 * layout for the currently running app. This information is used for time
 * multiplexing and VPE management. It is initialized by m3lib.
 *
 * Note: We do not store the stack pointer here, since it can easily
 * be collected whenever necessary. The data size will be maintained
 * by the heap manager.
 */
struct SPMemLayout {
    word_t reset_start;
    word_t reset_size;
    word_t text_start;
    word_t text_size;
    word_t data_start;
    word_t data_size;
    word_t stack_top;
    word_t _unused;
} PACKED;

static inline SPMemLayout *spmemlayout() {
    return reinterpret_cast<SPMemLayout*>(SPMEM_LAYOUT_LOCAL);
}

}
#endif /* defined(__cplusplus) */
