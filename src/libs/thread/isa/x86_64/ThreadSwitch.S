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

#include <thread/isa/x86_64/Thread.h>

# bool thread_save(m3::Thread::Regs *regs);
BEGIN_FUNC(thread_save)
    # save registers
    mov     %rbx,  0(%rdi)
    mov     %rsp,  8(%rdi)
    mov     %rbp, 16(%rdi)
    mov     %r12, 24(%rdi)
    mov     %r13, 32(%rdi)
    mov     %r14, 40(%rdi)
    mov     %r15, 48(%rdi)
    pushfq                              # load eflags
    popq    56(%rdi)                    # store

    mov     $0, %rax                    # return 0
    ret
END_FUNC(thread_save)

# bool thread_resume(m3::Thread::Regs *regs);
BEGIN_FUNC(thread_resume)
    # restore registers
    mov     48(%rdi), %r15
    mov     40(%rdi), %r14
    mov     32(%rdi), %r13
    mov     24(%rdi), %r12
    mov     16(%rdi), %rbp
    mov      8(%rdi), %rsp
    mov      0(%rdi), %rbx
    pushq   56(%rdi)
    mfence                              # TODO unnecessary, but fixes a bug in gem5
    # it looks like a similar problem I already had with pagefaults. if a store is
    # followed by a load and the store takes a bit longer, gem5 does not always
    # fetch the value from the store buffer for the load, but from the cache.
    # I have no idea why this occurs that seldom, but this seems to fix it for us
    popfq                               # load eflags
    mov     64(%rdi), %rdi              # load rdi (necessary for startup)

    mov     $1, %rax                    # return 1
    ret
END_FUNC(thread_resume)
