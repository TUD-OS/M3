/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/util/Util.h>
#include <base/Errors.h>

#include <m3/com/MemGate.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

#include <thread/ThreadManager.h>

#include <assert.h>

namespace m3 {

MemGate MemGate::create_global_for(uintptr_t addr, size_t size, int perms, capsel_t sel) {
    uint flags = 0;
    if(sel == INVALID)
        sel = VPE::self().alloc_cap();
    else
        flags |= KEEP_SEL;
    Syscalls::get().reqmemat(sel, addr, size, perms);
    return MemGate(flags, sel);
}

MemGate MemGate::derive(size_t offset, size_t size, int perms) const {
    capsel_t cap = VPE::self().alloc_cap();
    Syscalls::get().derivemem(sel(), cap, offset, size, perms);
    return MemGate(0, cap);
}

MemGate MemGate::derive(capsel_t cap, size_t offset, size_t size, int perms) const {
    Syscalls::get().derivemem(sel(), cap, offset, size, perms);
    return MemGate(ObjCap::KEEP_SEL, cap);
}

Errors::Code MemGate::forward(void *&data, size_t &len, size_t &offset, uint flags) {
    void *event = ThreadManager::get().get_wait_event();
    size_t amount = Math::min(KIF::MAX_MSG_SIZE, len);
    Errors::Code res = Syscalls::get().forwardmem(sel(), data, amount, offset, flags, event);

    // if this has been done, go to sleep and wait until the kernel sends us the upcall
    if(res == Errors::UPCALL_REPLY) {
        ThreadManager::get().wait_for(event);
        auto *msg = reinterpret_cast<const KIF::Upcall::Notify*>(
            ThreadManager::get().get_current_msg());
        res = static_cast<Errors::Code>(msg->error);
    }

    if(res == Errors::NO_ERROR) {
        len -= amount;
        offset += amount;
        data = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(data) + amount);
    }
    return res;
}

Errors::Code MemGate::read_sync(void *data, size_t len, size_t offset) {
    EVENT_TRACER_read_sync();
    ensure_activated();

retry:
    Errors::Code res = DTU::get().read(epid(), data, len, offset, _cmdflags);
    if(EXPECT_FALSE(res == Errors::VPE_GONE)) {
        res = forward(data, len, offset, _cmdflags);
        if(len > 0 || res != m3::Errors::NO_ERROR)
            goto retry;
    }

    return res;
}

Errors::Code MemGate::write_sync(const void *data, size_t len, size_t offset) {
    EVENT_TRACER_write_sync();
    ensure_activated();

retry:
    Errors::Code res = DTU::get().write(epid(), data, len, offset, _cmdflags);
    if(EXPECT_FALSE(res == Errors::VPE_GONE)) {
        res = forward(const_cast<void*&>(data), len, offset,
            _cmdflags | KIF::Syscall::ForwardMem::WRITE);
        if(len > 0 || res != m3::Errors::NO_ERROR)
            goto retry;
    }

    return res;
}

#if defined(__host__)
Errors::Code MemGate::cmpxchg_sync(void *data, size_t len, size_t offset) {
    EVENT_TRACER_cmpxchg_sync();
    ensure_activated();

    return DTU::get().cmpxchg(epid(), data, len, offset, len / 2);
}
#endif

}
