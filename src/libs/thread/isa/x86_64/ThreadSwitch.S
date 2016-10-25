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

#include <thread/isa/x86_64/Thread.h>

.global thread_save
.global thread_resume

# bool thread_save(m3::Thread::Regs *regs);
.type thread_save, function
thread_save:
    push    %rbp
    mov     %rsp,%rbp

    # save registers
    mov     %rax,0(%rdi)
    mov     %rbx,8(%rdi)
    mov     %rcx,16(%rdi)
    mov     %rdx,24(%rdi)
    mov     %rsi,32(%rdi)
    mov     %rsp,48(%rdi)
    mov     %rbp,56(%rdi)
    mov     %r8,64(%rdi)
    mov     %r9,72(%rdi)
    mov     %r10,80(%rdi)
    mov     %r11,88(%rdi)
    mov     %r12,96(%rdi)
    mov     %r13,104(%rdi)
    mov     %r14,112(%rdi)
    mov     %r15,120(%rdi)
    pushfq                              # load eflags
    popq    128(%rdi)                   # store

    mov     $0,%rax                   	# return 0
    mov     %rbp,%rsp
    pop     %rbp
    ret

# bool thread_resume(m3::Thread::Regs *regs);
.type thread_resume, function
thread_resume:
    push    %rbp
    mov     %rsp,%rbp

    # restore registers
    mov     120(%rdi),%r15
    mov     112(%rdi),%r14
    mov     104(%rdi),%r13
    mov     96(%rdi),%r12
    mov     88(%rdi),%r11
    mov     80(%rdi),%r10
    mov     72(%rdi),%r9
    mov     64(%rdi),%r8
    mov     56(%rdi),%rbp
    mov     48(%rdi),%rsp
    mov     32(%rdi),%rsi
    mov     24(%rdi),%rdx
    mov     16(%rdi),%rcx
    mov     8(%rdi),%rbx
    mov     0(%rdi),%rax
    pushq   128(%rdi)
    mfence                              # TODO unnecessary, but fixes a bug in gem5
    # it looks like a similar problem I already had with pagefaults. if a store is
    # followed by a load and the store takes a bit longer, gem5 does not always
    # fetch the value from the store buffer for the load, but from the cache.
    # I have no idea why this occurs that seldom, but this seems to fix it for us
    popfq                            	# load eflags
    mov     40(%rdi),%rdi               # load rdi (necessary for startup)

    mov     $1,%rax                    	# return 1
    mov     %rbp,%rsp
    pop     %rbp
    ret
