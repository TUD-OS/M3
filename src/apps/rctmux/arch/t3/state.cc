#include <m3/arch/t3/RCTMux.h>
#include <m3/DTU.h>
#include <m3/util/Math.h>
#include <m3/util/Sync.h>
#include <m3/util/Profile.h>

#include "rctmux.h"

#define RCTMUX_MAGIC (0x42C0FFEE)

EXTERN_C void _start();

using namespace m3;

/**
 * Processor and register state is put into state.cpu_regs by
 * exception-handler.S just before the _interrupt_handler() gets called.
 *
 * Its also used to restore everything when _interrupt_handler()
 * returns.
 */
volatile static struct alignas(DTU_PKG_SIZE) {
    word_t magic;
    word_t *cpu_regs[22];
    uint64_t local_ep_config[EP_COUNT];
    word_t : 8 * sizeof(word_t);    // padding
} _state;

volatile word_t *_regstate = (word_t*)&(_state.cpu_regs);

void mem_write(size_t channel, void *data, size_t size, unsigned int *offset)
{
    DTU::get().wait_until_ready(channel);
    DTU::get().write(channel, data, size, *offset);
    *offset += size;
}

void mem_read(size_t channel, void *data, size_t size, unsigned int *offset)
{
    DTU::get().wait_until_ready(channel);
    DTU::get().read(channel, data, size, *offset);
    *offset += size;
}

void arch_setup() {
    _state.magic = RCTMUX_MAGIC;
}

bool arch_init() {

    // save local endpoint config (workaround)
    for(int i = 0; i < EP_COUNT; ++i) {
        _state.local_ep_config[i] = DTU::get().get_ep_config(i);
    }

    // prevent irq from triggering again
    *(volatile unsigned *)IRQ_ADDR_INTERN = 0;

    return true;
}

void arch_finalize() {

    // restore local endpoint config
    for(int i = 0; i < EP_COUNT; ++i) {
        DTU::get().set_ep_config(i, _state.local_ep_config[i]);
    }
}

bool arch_save_state() {

    alignas(DTU_PKG_SIZE) uint32_t addr;
    size_t offset = 0;

    // state
    mem_write(RCTMUX_STORE_EP, (void*)&_state, sizeof(_state), &offset);

    // app layout
    AppLayout *l = applayout();
    mem_write(RCTMUX_STORE_EP, (void*)l, sizeof(*l), &offset);

    // reset vector
    mem_write(RCTMUX_STORE_EP, (void*)l->reset_start, l->reset_size, &offset);

    // text
    mem_write(RCTMUX_STORE_EP, (void*)l->text_start, l->text_size, &offset);

    // data and heap
    mem_write(RCTMUX_STORE_EP, (void*)l->data_start, l->data_size, &offset);

    // copy end-area of heap and runtime
    addr = Math::round_dn((uintptr_t)(RT_SPACE_END - DTU_PKG_SIZE), DTU_PKG_SIZE);
    mem_write(RCTMUX_STORE_EP, (void*)addr, DMEM_VEND - addr, &offset);

    // copy stack
    addr = (uint32_t)_state.cpu_regs[1] - REGSPILL_AREA_SIZE;
    mem_write(RCTMUX_STORE_EP, (void*)addr,
        Math::round_dn((uintptr_t)l->stack_top - addr, DTU_PKG_SIZE),
        &offset);

	return true;
}

bool arch_restore_state() {

    alignas(DTU_PKG_SIZE) uint32_t addr;
    size_t offset = 0;

    // read state
    mem_read(RCTMUX_RESTORE_EP, (void*)&_state, sizeof(_state), &offset);

    if (_state.magic != RCTMUX_MAGIC) {
        return false;
    }

    // restore app layout
    AppLayout *l = applayout();
    mem_read(RCTMUX_RESTORE_EP, (void*)l, sizeof(*l), &offset);

    // restore reset vector
    mem_read(RCTMUX_RESTORE_EP, (void*)l->reset_start, l->reset_size, &offset);

    // restore text
    mem_read(RCTMUX_RESTORE_EP, (void*)l->text_start, l->text_size, &offset);

    // restore data and heap
    mem_read(RCTMUX_RESTORE_EP, (void*)l->data_start, l->data_size, &offset);

    // restore end-area of heap and runtime
    addr = Math::round_dn((uintptr_t)(RT_SPACE_END - DTU_PKG_SIZE), DTU_PKG_SIZE);
    mem_read(RCTMUX_RESTORE_EP, (void*)addr, DMEM_VEND - addr, &offset);

    // restore stack
    addr = ((uint32_t)_state.cpu_regs[1]) - REGSPILL_AREA_SIZE;
    mem_read(RCTMUX_RESTORE_EP, (void*)addr,
        Math::round_up((uintptr_t)l->stack_top - addr, DTU_PKG_SIZE),
        &offset);

	return true;
}

void arch_wipe_mem() {

    // TODO
    /*AppLayout *l = applayout();
    memset((void*)l->data_start, 0,
        l->data_size + (RT_SPACE_END - DMEM_VEND));
    memset((void*)_state.cpu_regs[1], 0,
        l->stack_top - (uint32_t)_state.cpu_regs[1]);*/
}

void arch_idle_mode() {
    // set epc (exception program counter) to jump into idle mode
    // when returning from exception
    _state.cpu_regs[EPC_REG] = (word_t*)&_start;
}
