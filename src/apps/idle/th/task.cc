#include <base/Common.h>
#include <base/Config.h>

#include <assert.h>

#include <xtensa/xtruntime.h>

#define INT_NUMBER_0    0

using task_call_1_t = void (*)(int *);
using task_call_2_t = void (*)(int *, int *);
using task_call_3_t = void (*)(int *, int *, int *);
using task_call_4_t = void (*)(int *, int *, int *, int *);
using task_call_5_t = void (*)(int *, int *, int *, int *, int *);
using task_call_6_t = void (*)(int *, int *, int *, int *, int *, int *);
using task_call_7_t = void (*)(int *, int *, int *, int *, int *, int *, int *);
using task_call_8_t = void (*)(int *, int *, int *, int *, int *, int *, int *, int *);

void interrupt_handler(int) {
    /* clear level-triggered interrupt */
    volatile unsigned *tmp = (volatile unsigned *)IRQ_ADDR_INTERN;
    *tmp = 0;
}

extern "C" void setup_irq() {
    _xtos_set_interrupt_handler_arg(INT_NUMBER_0, (_xtos_handler)interrupt_handler, (void *)INT_NUMBER_0);
    _xtos_ints_on(1 << INT_NUMBER_0);
}

extern "C" int run_task() {
    static unsigned actual_pe_inst_slot = 0;
    unsigned *inst_ptr_ptr = (unsigned*)CM_SO_PE_INST_POINTER;
    if(actual_pe_inst_slot == 1)
        inst_ptr_ptr += (CM_SO_PE_TASK_INCREMENT / 4);

    /* no task to run */
    if(*inst_ptr_ptr == 0)
        return 1;

    unsigned args_count = *(inst_ptr_ptr + 2);
    int *arg = (int *)(inst_ptr_ptr + 4);

    if(args_count == 1) {
        task_call_1_t task_call_1 = (task_call_1_t)(*inst_ptr_ptr);
        task_call_1((int *)arg[0]);
    }
    else if(args_count == 2) {
        task_call_2_t task_call_2 = (task_call_2_t)(*inst_ptr_ptr);
        task_call_2((int *)arg[0], (int *)arg[2]);
    }
    else if(args_count == 3) {
        task_call_3_t task_call_3 = (task_call_3_t)(*inst_ptr_ptr);
        task_call_3((int *)arg[0], (int *)arg[2], (int *)arg[4]);
    }
    else if(args_count == 4) {
        task_call_4_t task_call_4 = (task_call_4_t)(*inst_ptr_ptr);
        task_call_4((int *)arg[0], (int *)arg[2], (int *)arg[4], (int *)arg[6]);
    }
    else if(args_count == 5) {
        task_call_5_t task_call_5 = (task_call_5_t)(*inst_ptr_ptr);
        task_call_5((int *)arg[0], (int *)arg[2], (int *)arg[4], (int *)arg[6], (int *)arg[8]);
    }
    else if(args_count == 6) {
        task_call_6_t task_call_6 = (task_call_6_t)(*inst_ptr_ptr);
        task_call_6((int *)arg[0], (int *)arg[2], (int *)arg[4], (int *)arg[6], (int *)arg[8],
            (int *)arg[10]);
    }
    else if(args_count == 7) {
        task_call_7_t task_call_7 = (task_call_7_t)(*inst_ptr_ptr);
        task_call_7((int *)arg[0], (int *)arg[2], (int *)arg[4], (int *)arg[6], (int *)arg[8],
            (int *)arg[10], (int *)arg[12]);
    }
    else if(args_count == 8) {
        task_call_8_t task_call_8 = (task_call_8_t)(*inst_ptr_ptr);
        task_call_8((int *)arg[0], (int *)arg[2], (int *)arg[4], (int *)arg[6], (int *)arg[8],
            (int *)arg[10], (int *)arg[12], (int *)arg[14]);
    }

    actual_pe_inst_slot = (actual_pe_inst_slot + 1) & 0x1;

    /* task finished flit */
    *inst_ptr_ptr = 1;

    return 0;
}
