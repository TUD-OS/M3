/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/log/Lib.h>
#include <base/Panic.h>

#include <m3/com/Marshalling.h>
#include <m3/pipe/AccelPipeReader.h>
#include <m3/pipe/AccelPipeWriter.h>
#include <m3/pipe/DirectPipeReader.h>
#include <m3/pipe/DirectPipeWriter.h>
#include <m3/vfs/FileTable.h>
#include <m3/vfs/File.h>
#include <m3/vfs/GenericFile.h>
#include <m3/vfs/SerialFile.h>

namespace m3 {

fd_t FileTable::alloc(File *file) {
    for(fd_t i = 0; i < MAX_FDS; ++i) {
        if(_fds[i] == nullptr) {
            file->set_fd(i);
            _fds[i] = file;
            return i;
        }
    }
    return MAX_FDS;
}

File *FileTable::free(fd_t fd) {
    File *file = _fds[fd];

    // remove from multiplexing table
    if(file) {
        _fds[fd] = nullptr;
        for(size_t i = 0; i < MAX_EPS; ++i) {
            if(_file_eps[i].file == file) {
                LLOG(FILES, "FileEPs[" << i << "] = --");
                _file_eps[i].file = nullptr;
                _file_ep_count--;
                break;
            }
        }
    }

    return file;
}

epid_t FileTable::request_ep(GenericFile *file) {
    if(_file_ep_count < MAX_EPS) {
        epid_t ep = VPE::self().alloc_ep();
        if(ep != 0) {
            for(size_t i = 0; i < MAX_EPS; ++i) {
                if(_file_eps[i].file == nullptr) {
                    LLOG(FILES, "FileEPs[" << i << "] = EP:" << ep << ",FD:" << file->fd());
                    _file_eps[i].file = file;
                    _file_eps[i].epid = ep;
                    _file_ep_count++;
                    return ep;
                }
            }
            UNREACHED;
        }
    }

    // TODO be smarter here
    size_t count = 0;
    for(size_t i = _file_ep_victim; count < MAX_EPS; i = (i + 1) % MAX_EPS, ++count) {
        if(_file_eps[i].file != nullptr) {
            LLOG(FILES, "FileEPs[" << i << "] = EP:" << _file_eps[i].epid << ", FD: switching from "
                << _file_eps[i].file->fd() << " to " << file->fd());
            _file_eps[i].file->evict();
            _file_eps[i].file = file;
            _file_ep_victim = (i + 1) % MAX_EPS;
            return _file_eps[i].epid;
        }
    }
    PANIC("Unable to find victim");
}

Errors::Code FileTable::delegate(VPE &vpe) const {
    Errors::Code res = Errors::NONE;
    for(fd_t i = 0; i < MAX_FDS; ++i) {
        if(_fds[i]) {
            res = _fds[i]->delegate(vpe);
            if(res != Errors::NONE)
                return res;
        }
    }
    return res;
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
            case 'F':
                obj->_fds[fd] = GenericFile::unserialize(um);
                break;
            case 'S':
                obj->_fds[fd] = SerialFile::unserialize(um);
                break;
// TODO currently, m3fs gets too large when enabling that
#if !defined(__t2__)
            case 'P':
                obj->_fds[fd] = DirectPipeWriter::unserialize(um);
                break;
            case 'Q':
                obj->_fds[fd] = DirectPipeReader::unserialize(um);
                break;
#endif

            // TODO
            // case 'A':
            //     obj->_fds[fd] = AccelPipeReader::unserialize(um);
            //     break;
            // case 'B':
            //     obj->_fds[fd] = AccelPipeWriter::unserialize(um);
            //     break;
        }
    }
    return obj;
}

}
