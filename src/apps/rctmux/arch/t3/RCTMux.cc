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

#include <base/util/Math.h>
#include <base/util/Time.h>
#include <base/DTU.h>
#include <base/Env.h>
#include <base/RCTMux.h>

#include <string.h>

#include <xtensa/xtruntime.h>

#include "RCTMux.h"

using namespace m3;

#define REGSPILL_AREA_SIZE (XCHAL_NUM_AREGS * sizeof(word_t))
#define EPC_REG            21
#define NUM_REGS           22
#define RCTMUX_MAGIC       0x42C0FFEE

EXTERN_C void _start();

/**
 * Processor and register state is put into state.cpu_regs by
 * exception-handler.S just before the _interrupt_handler() gets called.
 *
 * Its also used to restore everything when _interrupt_handler()
 * returns.
 */
volatile static struct alignas(DTU_PKG_SIZE) {
    word_t magic;
    word_t *cpu_regs[NUM_REGS];
    uint64_t local_ep_config[EP_COUNT];
    word_t : 8 * sizeof(word_t);    // padding
} _state;

// define an unmangled symbol that can be accessed from assembler
volatile word_t *_regstate = (word_t*)&(_state.cpu_regs);

namespace RCTMux {

static void mem_write(epid_t ep, void *data, size_t size, size_t *offset) {
    DTU::get().wait_until_ready(ep);
    DTU::get().write(ep, data, size, *offset);
    *offset += size;
}

static void mem_read(epid_t ep, void *data, size_t size, size_t *offset) {
    DTU::get().wait_until_ready(ep);
    DTU::get().read(ep, data, size, *offset);
    *offset += size;
}

static void wipe_mem() {
    SPMemLayout *l = spmemlayout();
    memset((void*)l->text_start, 0, l->data_size);
    memset((void*)_state.cpu_regs[1], 0, l->stack_top - (uint32_t)_state.cpu_regs[1]);
    // FIXME: wiping the runtime does make problems - why?
    //memset((void*)RT_SPACE_END, 0, DMEM_VEND - RT_SPACE_END);
}

void setup() {
    _state.magic = RCTMUX_MAGIC;
    flags_reset();
}

void init() {
    // prevent irq from triggering again
    *(volatile unsigned *)IRQ_ADDR_INTERN = 0;

    // save local endpoint config (workaround)
    for(int i = 0; i < EP_COUNT; ++i) {
        _state.local_ep_config[i] = DTU::get().get_ep_config(i);
    }

    // tell kernel that we are ready
    flag_set(SIGNAL);
}

void finish() {
    // restore local endpoint config (workaround)
    for(int i = 0; i < EP_COUNT; ++i) {
        DTU::get().set_ep_config(i, _state.local_ep_config[i]);
    }

    flags_reset();
}

void store() {
    alignas(DTU_PKG_SIZE) uint32_t addr;
    size_t offset = 0;

    // wait for kernel
    while (flag_is_set(SIGNAL) && !flag_is_set(ERROR))
        ;

    if (flag_is_set(ERROR))
        return;

    // state
    mem_write(RCTMUX_STORE_EP, (void*)&_state, sizeof(_state), &offset);

    // copy end-area of heap and runtime and keep flags
    mem_write(RCTMUX_STORE_EP, (void*)RT_START, RT_SIZE, &offset);

    // app layout
    SPMemLayout *l = spmemlayout();
    mem_write(RCTMUX_STORE_EP, (void*)l, sizeof(*l), &offset);

    // reset vector
    mem_write(RCTMUX_STORE_EP, (void*)l->reset_start, l->reset_size, &offset);

    // text
    mem_write(RCTMUX_STORE_EP, (void*)l->text_start, l->text_size, &offset);

    // data and heap
    mem_write(RCTMUX_STORE_EP, (void*)l->data_start, l->data_size, &offset);

    // copy stack
    addr = (uint32_t)_state.cpu_regs[1] - REGSPILL_AREA_SIZE;
    mem_write(RCTMUX_STORE_EP, (void*)addr,
        Math::round_dn((uintptr_t)l->stack_top - addr, DTU_PKG_SIZE),
        &offset);

    wipe_mem();

    // success
    flag_unset(STORE);
}

void restore() {
    alignas(DTU_PKG_SIZE) uint32_t addr;
    size_t offset = 0;

    while (flag_is_set(SIGNAL) && !flag_is_set(ERROR))
        ;

    if (flag_is_set(ERROR))
        return;

    // read state
    mem_read(RCTMUX_RESTORE_EP, (void*)&_state, sizeof(_state), &offset);

    if (_state.magic != RCTMUX_MAGIC) {
        flag_set(ERROR);
        return;
    }

    // restore end-area of heap and runtime before accessing spmemlayout
    mem_read(RCTMUX_RESTORE_EP, (void*)RT_START, RT_SIZE, &offset);

    // restore app layout
    SPMemLayout *l = spmemlayout();
    mem_read(RCTMUX_RESTORE_EP, (void*)l, sizeof(*l), &offset);

    // restore reset vector
    mem_read(RCTMUX_RESTORE_EP, (void*)l->reset_start, l->reset_size, &offset);

    // restore text
    mem_read(RCTMUX_RESTORE_EP, (void*)l->text_start, l->text_size, &offset);

    // restore data and heap
    mem_read(RCTMUX_RESTORE_EP, (void*)l->data_start, l->data_size, &offset);

    // restore stack
    addr = ((uint32_t)_state.cpu_regs[1]) - REGSPILL_AREA_SIZE;
    mem_read(RCTMUX_RESTORE_EP, (void*)addr,
        Math::round_up((uintptr_t)l->stack_top - addr, DTU_PKG_SIZE),
        &offset);

    // success
    flag_unset(RESTORE);
}

void reset() {
    // simulate reset since resetting the PE from kernel side is not
    // currently supported for t3
    // TODO
}

void set_idle_mode() {
    // reset program entry
    m3::env()->entry = 0;
    // set epc (exception program counter) to jump into idle mode
    // when returning from exception
    _state.cpu_regs[EPC_REG] = (word_t*)&_start;
}

} /* namespace RCTMux */
