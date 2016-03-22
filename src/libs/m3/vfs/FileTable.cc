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

#include <base/com/Marshalling.h>

#include <m3/vfs/FileTable.h>
#include <m3/vfs/File.h>
#include <m3/vfs/RegularFile.h>
#include <m3/vfs/SerialFile.h>

namespace m3 {

fd_t FileTable::alloc(File *file) {
    for(fd_t i = 0; i < MAX_FDS; ++i) {
        if(_fds[i] == nullptr) {
            _fds[i] = file;
            return i;
        }
    }
    return MAX_FDS;
}

void FileTable::delegate(VPE &vpe) const {
    for(fd_t i = 0; i < MAX_FDS; ++i) {
        if(_fds[i])
            _fds[i]->delegate(vpe);
    }
}

size_t FileTable::serialize(void *buffer, size_t size) const {
    Marshaller m(static_cast<unsigned char*>(buffer), size);

    size_t count = 0;
    for(fd_t i = 0; i < MAX_FDS; ++i) {
        if(_fds[i])
            count++;
    }

    m << count;
    for(fd_t i = 0; i < MAX_FDS; ++i) {
        if(_fds[i]) {
            m << i << _fds[i]->type();
            _fds[i]->serialize(m);
        }
    }
    return m.total();
}

FileTable *FileTable::unserialize(const void *buffer, size_t size) {
    FileTable *obj = new FileTable();
    Unmarshaller um(static_cast<const unsigned char*>(buffer), size);
    size_t count;
    um >> count;
    while(count-- > 0) {
        fd_t fd;
        char type;
        um >> fd >> type;
        switch(type) {
            case 'M':
                obj->_fds[fd] = RegularFile::unserialize(um);
                break;
            case 'S':
                obj->_fds[fd] = SerialFile::unserialize(um);
                break;
        }
    }
    return obj;
}

}
