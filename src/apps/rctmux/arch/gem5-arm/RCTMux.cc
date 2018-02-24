/**
 * Copyright (C) 2016, René Küttner <rene.kuettner@.tu-dresden.de>
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

#include <base/DTU.h>
#include <base/Env.h>
#include <base/RCTMux.h>
#include <base/Exceptions.h>

#include "../../RCTMux.h"

EXTERN_C void _save(void *state);
EXTERN_C void *_restore();
EXTERN_C void _signal();

namespace RCTMux {

static m3::Exceptions::isr_func isrs[8];

struct PFHandler {

static void *isr_irq(m3::Exceptions::State *state) {
    m3::DTU &dtu = m3::DTU::get();

    m3::DTU::reg_t ext_req = dtu.get_ext_req();
    assert(ext_req != 0);

    // ack
    dtu.set_ext_req(0);

    uint cmd = ext_req & 0x3;
    switch(cmd) {
        case m3::DTU::ExtReqOpCode::SET_ROOTPT:
            assert(false);
            break;

        case m3::DTU::ExtReqOpCode::INV_PAGE:
            assert(false);
            break;

        case m3::DTU::ExtReqOpCode::RCTMUX: {
            // context switch request from kernel?
            uint64_t flags = flags_get();

            if(flags & m3::RESTORE) {
                PRINTSTR("restore from interrupt\n");
                m3::DTU::get().clear_irq();
                return _restore();
            }

            if(flags & m3::STORE) {
                _save(state);

                m3::DTU::get().clear_irq();

                // stay here until reset
                asm volatile (
                    "mrs     r0, CPSR;\n"
                    "bic     r0, #1 << 7;\n"
                    "msr     CPSR, r0;\n"
                );
                while(1)
                    sleep();
                UNREACHED;
            }

            if(flags & m3::WAITING)
                _signal();
            break;
        }
    }

    m3::DTU::get().clear_irq();
    return state;
}

static void *isr_null(m3::Exceptions::State *state) {
    return state;
}

};

EXTERN_C void exc_handler(m3::Exceptions::State *state) {
    // repeat last instruction, except for SWIs
    if(state->vector != 2)
        state->pc -= 4;
    isrs[state->vector](state);
}

void init() {
    for(size_t i = 0; i < ARRAY_SIZE(isrs); ++i)
        isrs[i] = PFHandler::isr_null;
    isrs[6] = PFHandler::isr_irq;
}

void *init_state() {
    m3::Env *senv = m3::env();
    senv->isrs = reinterpret_cast<uintptr_t>(isrs);

    auto state = reinterpret_cast<m3::Exceptions::State*>(senv->sp) - 1;
    for(size_t i = 0; i < ARRAY_SIZE(state->r); ++i)
        state->r[i] = 0;
    // don't set the stackpointer in crt0
    state->r[1]     = 0xDEADBEEF;
    state->pc       = senv->entry;
    state->cpsr     = 0x13;  // supervisor mode
    state->lr       = 0;

    return state;
}

} /* namespace RCTMux */
