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

#include <m3/arch/t3/RCTMux.h>
#include <m3/DTU.h>
#include <m3/Syscalls.h>
#include <m3/util/Math.h>
#include <m3/util/Sync.h>
#include <m3/util/Profile.h>
#include <string.h>
#include <xtensa/xtruntime.h>

#include "RCTMux.h"
#include "../../Debug.h"

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

volatile word_t *_regstate = (word_t*)&(_state.cpu_regs);
volatile word_t *_irqaddr = (word_t*)IRQ_ADDR_INTERN;

namespace RCTMux {

static struct alignas(DTU_PKG_SIZE) syscall_tmuxctl {
    Syscalls::Operation syscall_op;
} _sc_tmuxctl = { Syscalls::TMUXRESUME };

static void notify_kernel() {
    DTU::get().wait_until_ready(DTU::SYSC_EP);
    DTU::get().send(DTU::SYSC_EP, &_sc_tmuxctl, sizeof(_sc_tmuxctl),
        label_t(), 0);

    while (flag_is_set(RCTMUX_FLAG_SIGNAL))
        ;
}

static void mem_write(size_t ep, void *data, size_t size, size_t *offset) {
    DTU::get().wait_until_ready(ep);
    DTU::get().write(ep, data, size, *offset);
    *offset += size;
}

static void mem_read(size_t ep, void *data, size_t size, size_t *offset) {
    DTU::get().wait_until_ready(ep);
    DTU::get().read(ep, data, size, *offset);
    *offset += size;
}

static void wipe_mem() {
    /*AppLayout *l = applayout();

    // wipe text to heap
    memset((void*)l->text_start, 0, l->data_size);

    // wipe stack
    memset((void*)_state.cpu_regs[1], 0,
        l->stack_top - (uint32_t)_state.cpu_regs[1]);

    // FIXME: wiping the runtime does make problems - why?
    //memset((void*)RT_SPACE_END, 0, DMEM_VEND - RT_SPACE_END);*/
}

EXTERN_C void _setup() {
    _state.magic = RCTMUX_MAGIC;
    flags_reset();
}

EXTERN_C void _loop() {
    volatile m3::Env *senv = m3::env();
    while(1) {
        asm volatile ("waiti   0");

        // is there something to run?
        uintptr_t ptr = senv->entry;
        if(ptr) {
            // remember exit location
            senv->exit = reinterpret_cast<uintptr_t>(&_start);

            // tell crt0 to set this stackpointer
            reinterpret_cast<word_t*>(STACK_TOP)[-1] = 0xDEADBEEF;
            reinterpret_cast<word_t*>(STACK_TOP)[-2] = senv->sp;
            register word_t a2 __asm__ ("a2") = ptr;
            asm volatile (
                "jx    %0;" : : "a"(a2)
            );
        }
    }
}

EXTERN_C void _reset() {
    // simulate reset since resetting the PE from kernel side is not
    // currently supported for t3
    env()->entry = 0;
    asm volatile("jx %0" : : "r"((word_t*)&_start));
}

EXTERN_C void _store_context() {

    // this is necessary for further interrupts
    *(volatile uintptr_t*)IRQ_ADDR_INTERN = 0;

    alignas(DTU_PKG_SIZE) uint32_t addr;
    size_t offset = 0;

    // workaround: store local EP configuration
    for(int i = 0; i < EP_COUNT; ++i) {
        _state.local_ep_config[i] = DTU::get().get_ep_config(i);
    }

    // tell the kernel that we are ready
    flag_unset(RCTMUX_FLAG_SIGNAL);

    // wait for kernel
    while (!flag_is_set(RCTMUX_FLAG_SIGNAL) && !flag_is_set(RCTMUX_FLAG_ERROR))
        ;

    if (flag_is_set(RCTMUX_FLAG_ERROR))
        return;

    // state
    mem_write(RCTMUX_STORE_EP, (void*)&_state, sizeof(_state), &offset);

    // copy end-area of heap and runtime and keep flags
    addr = Math::round_dn((uintptr_t)(RT_SPACE_END - DTU_PKG_SIZE), DTU_PKG_SIZE);
    mem_write(RCTMUX_STORE_EP, (void*)addr, DMEM_VEND - addr, &offset);

    // app layout
    AppLayout *l = applayout();
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

    // notify kernel if there is no restore phase
    if (!flag_is_set(RCTMUX_FLAG_RESTORE))
        notify_kernel();

    // since the remote reset is not supported on this platform we
    // do not need to involve the kernel here and just proceed to the
    // next phases automatically

    _reset();
}

EXTERN_C void _restore_context() {

    alignas(DTU_PKG_SIZE) uint32_t addr;
    size_t offset = 0;

    // tell the kernel that we are ready
    //flag_unset(RCTMUX_FLAG_SIGNAL);

    // wait for kernel
    /*while (!flag_is_set(RCTMUX_FLAG_SIGNAL) && !flag_is_set(RCTMUX_FLAG_ERROR))
        ;*/

    if (flag_is_set(RCTMUX_FLAG_ERROR))
        return;

    // read state
    mem_read(RCTMUX_RESTORE_EP, (void*)&_state, sizeof(_state), &offset);

    if (_state.magic != RCTMUX_MAGIC) {
        flag_set(RCTMUX_FLAG_ERROR);
        return;
    }

    // restore end-area of heap and runtime before accessing applayout
    addr = Math::round_dn((uintptr_t)(RT_SPACE_END - DTU_PKG_SIZE), DTU_PKG_SIZE);
    mem_read(RCTMUX_RESTORE_EP, (void*)addr, DMEM_VEND - addr, &offset);

    // restore app layout
    AppLayout *l = applayout();
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

	notify_kernel();

    // restore local endpoint config (workaround)
    for(int i = 0; i < EP_COUNT; ++i) {
        DTU::get().set_ep_config(i, _state.local_ep_config[i]);
    }

    flags_reset();
}

} /* namespace RCTMux */
