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

#include <thread/isa/arm/Thread.h>

.global thread_save
.global thread_resume

# bool thread_save(m3::Thread::Regs *regs);
.type thread_save, function
thread_save:
    stm     r0, {r0-r14}
    mrs     r1, CPSR
    str     r1, [r0, #60]

    mov     r0, #0
    mov     pc, lr

# bool thread_resume(m3::Thread::Regs *regs);
.type thread_resume, function
thread_resume:
    ldr     r1, [r0, #60]
    msr     CPSR_f, r1
    ldm     r0, {r0-r14}

    mov     r0, #1
    mov     pc, lr
