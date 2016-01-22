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

#include <m3/arch/host/SharedMemory.h>
#include <m3/util/Util.h>
#include <m3/Log.h>
#include <m3/Config.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace m3 {

SharedMemory::SharedMemory(const String &name, size_t size, Op op)
        : _fd(-1), _name(name), _addr(), _size(size) {
    OStringStream os;
    os << Config::get().shm_prefix() << name;
    _fd = shm_open(os.str(), O_RDWR | (op == CREATE ? O_CREAT : 0) | O_EXCL, S_IRUSR | S_IWUSR);
    if(_fd == -1)
        PANIC("shm_open: Unable to open '" << os.str() << "': " << strerror(errno));

    if(op == CREATE && ftruncate(_fd, size) == -1)
        PANIC("ftruncate");

    _addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0);
    if(_addr == MAP_FAILED)
        PANIC("mmap");

    LOG(SHM, "Shm " << os.str() << " @ " << _addr);
}

SharedMemory::~SharedMemory() {
    if(_addr) {
        munmap(_addr, _size);
        // shm_open seems to do no reference-counting. thus, shm_unlink will remove it regardless
        // of how many users there are left. so, let only the kernel unlink a shared memory.
        if(Config::get().is_kernel()) {
            OStringStream os;
            os << Config::get().shm_prefix() << _name;
            LOG(SHM, "Del " << os.str());
            shm_unlink(os.str());
        }
    }
}

}
