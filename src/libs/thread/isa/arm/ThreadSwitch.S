/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/Asm.h>

#include <thread/isa/arm/Thread.h>

# bool thread_save(m3::Thread::Regs *regs);
BEGIN_FUNC(thread_save)
    mov     r1, #1 // thread_resume's return value
    stm     r0, {r1,r4-r11,r13,r14}
    mrs     r1, CPSR
    str     r1, [r0, #11 * 4]

    mov     r0, #0
    mov     pc, lr
END_FUNC(thread_save)

# bool thread_resume(m3::Thread::Regs *regs);
BEGIN_FUNC(thread_resume)
    ldr     r1, [r0, #11 * 4]
    msr     CPSR_f, r1
    ldm     r0, {r0,r4-r11,r13,r14}

    mov     pc, lr
END_FUNC(thread_resume)
