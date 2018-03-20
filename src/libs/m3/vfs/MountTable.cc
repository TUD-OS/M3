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

#include <m3/com/Marshalling.h>
#include <m3/com/GateStream.h>
#include <m3/session/M3FS.h>
#include <m3/vfs/MountTable.h>

namespace m3 {

static size_t charcount(const char *str, char c) {
    size_t cnt = 0;
    while(*str) {
        if(*str == c)
            cnt++;
        str++;
    }
    // if the path does not end with a slash, we have essentially one '/' more
    if(str[-1] != '/')
        cnt++;
    return cnt;
}

// TODO this is a very simple solution that expects "perfect" paths, i.e. with no "." or ".." and
// no duplicate slashes (at least not just one path):
static size_t is_in_mount(const String &mount, const char *in) {
    const char *p1 = mount.c_str();
    const char *p2 = in;
    while(*p2 && *p1 == *p2) {
        p1++;
        p2++;
    }
    while(*p1 == '/')
        p1++;
    if(*p1)
        return 0;
    while(*p2 == '/')
        p2++;
    return static_cast<size_t>(p2 - in);
}

MountTable::MountTable(const MountTable &ms) : _count(ms._count) {
    memcpy(_mounts, ms._mounts, sizeof(_mounts));
}

MountTable &MountTable::operator=(const MountTable &ms) {
    if(&ms != this) {
        _count = ms._count;
        memcpy(_mounts, ms._mounts, sizeof(_mounts));
    }
    return *this;
}

Errors::Code MountTable::add(const char *path, FileSystem *fs) {
    if(_count == MAX_MOUNTS)
        return Errors::NO_SPACE;

    size_t compcount = charcount(path, '/');
    size_t i = 0;
    for(; i < _count; ++i) {
        // mounts are always tightly packed
        assert(_mounts[i]);

        if(strcmp(_mounts[i]->path().c_str(), path) == 0)
            return Errors::last = Errors::EXISTS;

        // sort them by the number of slashes
        size_t cnt = charcount(_mounts[i]->path().c_str(), '/');
        if(compcount > cnt)
            break;
    }
    assert(i < MAX_MOUNTS);

    // move following items forward
    if(_count > 0) {
        for(size_t j = _count; j > i; --j)
            _mounts[j] = _mounts[j - 1];
    }
    _mounts[i] = new MountPoint(path, fs);
    _count++;
    return Errors::NONE;
}

Reference<FileSystem> MountTable::resolve(const char *path, size_t *pos) {
    for(size_t i = 0; i < _count; ++i) {
        *pos = is_in_mount(_mounts[i]->path(), path);
        if(*pos != 0)
            return _mounts[i]->fs();
    }
    return Reference<FileSystem>();
}

void MountTable::remove(const char *path) {
    for(size_t i = 0; i < _count; ++i) {
        if(strcmp(_mounts[i]->path().c_str(), path) == 0) {
            MountPoint *mp = _mounts[i];
            remove(i);
            delete mp;
            break;
        }
    }
}

void MountTable::remove(size_t i) {
    assert(_mounts[i]);
    assert(_count > 0);
    _mounts[i] = nullptr;
    // move following items backwards
    for(; i < _count - 1; ++i)
        _mounts[i] = _mounts[i + 1];
    _count--;
}

size_t MountTable::get_mount_id(FileSystem *fs) const {
    for(size_t i = 0; i < _count; ++i) {
        if(&*_mounts[i]->fs() == fs)
            return i;
    }
    return MAX_MOUNTS;
}

FileSystem *MountTable::get_mount(size_t id) const {
    for(size_t i = 0; i < _count; ++i) {
        if(id-- == 0)
            return const_cast<FileSystem*>(&*_mounts[i]->fs());
    }
    return nullptr;
}

size_t MountTable::serialize(void *buffer, size_t size) const {
    Marshaller m(static_cast<unsigned char*>(buffer), size);

    m << _count;
    for(size_t i = 0; i < _count; ++i) {
        char type = _mounts[i]->fs()->type();
        m << _mounts[i]->path() << type;
        switch(type) {
            case 'M': {
                const M3FS *m3fs = static_cast<const M3FS*>(&*_mounts[i]->fs());
                m << m3fs->sel() << m3fs->gate().sel();
            }
            break;
        }
    }
    return m.total();
}

void MountTable::delegate(VPE &vpe) const {
    for(size_t i = 0; i < _count; ++i) {
        char type = _mounts[i]->fs()->type();
        switch(type) {
            case 'M': {
                const M3FS *m3fs = static_cast<const M3FS*>(&*_mounts[i]->fs());
                // might fail if we've already done that
                vpe.delegate_obj(m3fs->sel());
                vpe.delegate_obj(m3fs->gate().sel());
            }
            break;
        }
    }
}

MountTable *MountTable::unserialize(const void *buffer, size_t size) {
    MountTable *ms = new MountTable();
    Unmarshaller um(static_cast<const unsigned char*>(buffer), size);
    size_t count;
    um >> count;
    while(count-- > 0) {
        char type;
        String path;
        um >> path >> type;
        switch(type) {
            case 'M':
                capsel_t sess, gate;
                um >> sess >> gate;
                ms->add(path.c_str(), new M3FS(sess, gate));
                break;
        }
    }
    return ms;
}

void MountTable::print(OStream &os) const {
    os << "Mounts:\n";
    for(size_t i = 0; i < _count; ++i)
        os << "  " << _mounts[i]->path() << ": " << _mounts[i]->fs()->type() << "\n";
}

}
