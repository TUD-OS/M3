/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Copyright (C) 2016, Matthias Hille <matthias.hille@tu-dresden.de>
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

#include <thread/Thread.h>
#include <thread/ThreadManager.h>
#include <base/Heap.h>
#include <base/Panic.h>

namespace m3 {

int Thread::_next_id = 1;

Thread::Thread(thread_func func, void *arg)
    : _id(_next_id++),
      _regs(),
      _stack(),
      _event(0) {
    // TODO
    // better leave one page before and behind each stack free to detect stack-under-/overflows
    void* addr = m3::Heap::alloc(T_STACK_SZ);

    _stack = reinterpret_cast<word_t*>(reinterpret_cast<uintptr_t>(addr));
    thread_init(func, arg, &_regs, _stack);
    ThreadManager::get().add(this);
}

Thread::~Thread() {
    ThreadManager::get().remove(this);
    if(_stack)
        m3::Heap::free(_stack);
}

}
