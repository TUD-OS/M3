/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/cap/VPE.h>
#include <m3/service/M3FS.h>
#include <m3/pipe/PipeFS.h>
#include <m3/vfs/VFS.h>
#include <m3/Marshalling.h>
#include <m3/stream/Serial.h>

namespace m3 {

SList<VFS::MountPoint> VFS::_mounts INIT_PRIORITY(109);
VFS::Init VFS::Init::obj INIT_PRIORITY(109);

VFS::Init::Init() {
    // we can't do that earlier because it has some dependencies
    if(VPE::self()._mounts)
        unserialize(VPE::self()._mounts, VPE::self()._mountlen);
}

static size_t charcount(const char *str, char c) {
    size_t cnt = 0;
    while(*str) {
        if(*str == c)
            cnt++;
        str++;
    }
    return cnt;
}

Errors::Code VFS::mount(const char *path, FileSystem *fs) {
    size_t compcount = charcount(path, '/');
    MountPoint *prev = NULL;
    for(auto &m : _mounts) {
        if(strcmp(m.path().c_str(), path) == 0)
            return Errors::last = Errors::EXISTS;

        size_t cnt = charcount(m.path().c_str(), '/');
        if(compcount > cnt) {
            _mounts.insert(prev, new MountPoint(path, fs));
            return Errors::NO_ERROR;
        }
        prev = &m;
    }
    _mounts.append(new MountPoint(path, fs));
    return Errors::NO_ERROR;
}

void VFS::unmount(const char *path) {
    for(auto &m : _mounts) {
        if(strcmp(m.path().c_str(), path) == 0) {
            _mounts.remove(&m);
            delete &m;
            break;
        }
    }
}

File *VFS::open(const char *path, int perms) {
    size_t pos;
    Reference<FileSystem> fs = resolve(path, &pos);
    if(!fs.valid()) {
        Errors::last = Errors::NO_SUCH_FILE;
        return nullptr;
    }
    return fs->open(path + pos, perms);
}

Errors::Code VFS::stat(const char *path, FileInfo &info) {
    size_t pos;
    Reference<FileSystem> fs = resolve(path, &pos);
    if(!fs.valid())
        return Errors::NO_SUCH_FILE;
    return fs->stat(path + pos, info);
}

Errors::Code VFS::mkdir(const char *path, mode_t mode) {
    size_t pos;
    Reference<FileSystem> fs = resolve(path, &pos);
    if(!fs.valid())
        return Errors::NO_SUCH_FILE;
    return fs->mkdir(path + pos, mode);
}

Errors::Code VFS::rmdir(const char *path) {
    size_t pos;
    Reference<FileSystem> fs = resolve(path, &pos);
    if(!fs.valid())
        return Errors::NO_SUCH_FILE;
    return fs->rmdir(path + pos);
}

Errors::Code VFS::link(const char *oldpath, const char *newpath) {
    size_t pos1, pos2;
    Reference<FileSystem> fs1 = resolve(oldpath, &pos1);
    if(!fs1.valid())
        return Errors::NO_SUCH_FILE;
    Reference<FileSystem> fs2 = resolve(newpath, &pos2);
    if(!fs2.valid())
        return Errors::NO_SUCH_FILE;
    if(fs1.get() != fs2.get())
        return Errors::XFS_LINK;
    return fs1->link(oldpath + pos1, newpath + pos2);
}

Errors::Code VFS::unlink(const char *path) {
    size_t pos;
    Reference<FileSystem> fs = resolve(path, &pos);
    if(!fs.valid())
        return Errors::NO_SUCH_FILE;
    return fs->unlink(path + pos);
}

size_t VFS::serialize_length() {
    size_t len = ostreamsize<size_t>();
    for(auto &mount : _mounts) {
        char type = mount.fs()->type();
        len += vostreamsize(mount.path().length(), sizeof(char), sizeof(size_t));
        switch(type) {
            case 'M':
                len += ostreamsize<capsel_t, capsel_t>();
                break;

            case 'P':
                // nothing to do
                break;
        }
    }
    return Math::round_up(len, DTU_PKG_SIZE);
}

size_t VFS::serialize(void *buffer, size_t size) {
    Marshaller m(static_cast<unsigned char*>(buffer), size);
    m << _mounts.length();
    for(auto &mount : _mounts) {
        char type = mount.fs()->type();
        m << mount.path() << type;
        switch(type) {
            case 'M': {
                const M3FS *m3fs = static_cast<const M3FS*>(&*mount.fs());
                m << m3fs->sel() << m3fs->gate().sel();
            }
            break;

            case 'P':
                // nothing to do
                break;
        }
    }
    return m.total();
}

void VFS::delegate(VPE &vpe, const void *buffer, size_t size) {
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
                if(vpe.is_cap_free(sess))
                    vpe.delegate(CapRngDesc(sess, 1));
                if(vpe.is_cap_free(gate))
                    vpe.delegate(CapRngDesc(gate, 1));
                break;

            case 'P':
                break;
        }
    }
}

void VFS::unserialize(const void *buffer, size_t size) {
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
                mount(path.c_str(), new M3FS(sess, gate));
                break;

            case 'P':
                mount(path.c_str(), new PipeFS());
                break;
        }
    }
}

// TODO this is a very simple solution that expects "perfect" paths, i.e. with no "." or ".." and
// no duplicate slashes (at least not just one path):
size_t VFS::is_in_mount(const String &mount, const char *in) {
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
    return p2 - in;
}

Reference<FileSystem> VFS::resolve(const char *in, size_t *pos) {
    for(auto &m : _mounts) {
        *pos = is_in_mount(m.path(), in);
        if(*pos != 0)
            return m.fs();
    }
    return Reference<FileSystem>();
}

void VFS::print(OStream &os) {
    os << "Mounts:\n";
    for(auto &m : _mounts)
        os << "  " << m.path() << ": " << m.fs()->type() << "\n";
}

}
