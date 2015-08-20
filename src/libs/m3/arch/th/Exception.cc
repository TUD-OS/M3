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

#include <xtensa/config/core-isa.h>
#include <m3/Common.h>
#include <m3/stream/Serial.h>
#include <stdlib.h>

using namespace m3;

#define VERBOSE_EXCEPTIONS          0

#define CALL_INSTR_SIZE             3
#define MAKE_PC_FROM_RA(ra, sp)     (((ra) & 0x3fffffff) | ((sp) & 0xc0000000))

struct State {
    uint32_t ar[16];
    uint32_t lbeg;
    uint32_t lend;
    uint32_t lcount;
    uint32_t windowbase;
    uint32_t windowstart;
    uint32_t sar;
    uint32_t ps;
    uint32_t pc;
} PACKED;

static const char *excauses[] = {
    /*  0 */ "Illegal Instruction",
    /*  1 */ "System Call",
    /*  2 */ "Instruction Fetch Error",
    /*  3 */ "Load Store Error",
    /*  4 */ "Level 1 Interrupt",
    /*  5 */ "Stack Extension Assist",
    /*  6 */ "Integer Divide by Zero",
    /*  7 */ "Next PC Value Illegal",
    /*  8 */ "Privileged Instruction",
    /*  9 */ "Unaligned Load Store",
    /* 10 */ "",
    /* 11 */ "",
    /* 12 */ "Instruction PIF Data Error",
    /* 13 */ "Load Store PIF Data Error",
    /* 14 */ "Instruction PIF Addr Error",
    /* 15 */ "Load Store PIF Addr Error",
    /* 16 */ "ITlb Miss Exception",
    /* 17 */ "ITlb Multihit Exception",
    /* 18 */ "ITlb Privilege Exception",
    /* 19 */ "ITlb Size Restriction Exception",
    /* 20 */ "Fetch Cache Attribute Exception",
    /* 21 */ "",
    /* 22 */ "",
    /* 23 */ "",
    /* 24 */ "DTlb Miss Exception",
    /* 25 */ "DTlb Multihit Exception",
    /* 26 */ "DTlb Privilege Exception",
    /* 27 */ "",
    /* 28 */ "Load Prohibited Exception",
    /* 29 */ "Store Prohibited Exception",
    /* 30 */ "",
    /* 31 */ "",
    /* 32 */ "Coprocessor 0 disabled",
    /* 33 */ "Coprocessor 1 disabled",
    /* 34 */ "Coprocessor 2 disabled",
    /* 35 */ "Coprocessor 3 disabled",
    /* 36 */ "Coprocessor 4 disabled",
    /* 37 */ "Coprocessor 5 disabled",
    /* 38 */ "Coprocessor 6 disabled",
    /* 39 */ "Coprocessor 7 disabled",
};

static inline uint32_t get_excvaddr() {
    uint32_t val;
    asm volatile (
          "rsr    %0, EXCVADDR;"
          : "=a" (val)
    );
    return val;
}

static uintptr_t get_caller(const State *state) {
    return MAKE_PC_FROM_RA(state->ar[0], state->pc) - CALL_INSTR_SIZE;
}

EXTERN_C void ExceptionHandler(uint cause, const State *state) {
    Serial &ser = Serial::get();
    ser << "PANIC: " << excauses[cause] << "\n";
    ser << "    @ " << fmt(state->pc, "p")
        << ", for addr " << fmt(get_excvaddr(), "p")
        << ", called from " << fmt(get_caller(state), "p") << "\n";

#if VERBOSE_EXCEPTIONS
    ser << "State @ " << state << "\n";
    for(size_t i = 0; i < ARRAY_SIZE(state->ar); ++i)
        ser << "    a" << fmt(i, "0", 2) << "  = " << fmt(state->ar[i], "#0x", 8) << "\n";
    ser << "    lbeg = " << fmt(state->lbeg, "#0x", 8) << "\n";
    ser << "    lend = " << fmt(state->lend, "#0x", 8) << "\n";
    ser << "    lcnt = " << fmt(state->lcount, "#0x", 8) << "\n";
    ser << "    winb = " << fmt(state->windowbase, "#0x", 8) << "\n";
    ser << "    wins = " << fmt(state->windowstart, "#0x", 8) << "\n";
    ser << "    sar  = " << fmt(state->sar, "#0x", 8) << "\n";
    ser << "    ps   = " << fmt(state->ps, "#0x", 8) << "\n";
    ser << "    pc   = " << fmt(state->pc, "#0x", 8) << "\n";
#endif

    exit(1);
}
