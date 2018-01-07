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

#pragma once

#include <base/Common.h>
#include <base/KIF.h>

#include <m3/session/M3FS.h>
#include <m3/vfs/LocList.h>
#include <m3/vfs/FileSystem.h>
#include <m3/vfs/File.h>

namespace m3 {

class M3FS;
class FStream;
class RegFileBuffer;

/**
 * The File implementation for regular files. Note that this is the low-level API for working with
 * files. Every read/write operation will actually issue a read/write operation using the DTU.
 * FStream is provided as a buffered version on top of this that will delay read/write operations
 * due to buffering. On the other hand, it has a bit of overhead, obviously. You can't instantiate
 * this class, because VFS::open should be used.
 */
class RegularFile : public File {
    friend class FStream;
    friend class M3FS;
    friend class RegFileBuffer;

    struct ExtentCache {
    public:
        explicit ExtentCache() : locs(), first(), offset(), length() {
        }

        bool valid() const {
            return locs.count() > 0;
        }
        void invalidate() {
            locs.clear();
        }

        bool contains_pos(size_t off) const {
            return valid() && off >= offset && off < offset + length;
        }
        bool contains_ext(uint16_t ext) const {
            return ext >= first && ext < first + locs.count();
        }

        size_t ext_len(uint16_t ext) const {
            return locs.get_len(static_cast<size_t>(ext - first));
        }
        capsel_t sel(uint16_t ext) const {
            return locs.get_sel(static_cast<size_t>(ext - first));
        }

        bool find(size_t off, uint16_t &ext, size_t &extoff) const;
        Errors::Code request_next(Reference<M3FS> &sess, int fd, bool writing, bool &extended);

        loclist_type locs;
        uint16_t first;
        size_t offset;
        size_t length;
    };

    struct Position {
        explicit Position() : ext(0), extoff(0), abs(0) {
        }
        explicit Position(uint16_t ext, size_t extoff, size_t abs) : ext(ext), extoff(extoff), abs(abs) {
        }

        size_t advance(size_t extlen, size_t count) {
            if(count >= extlen - extoff) {
                size_t res = extlen - extoff;
                abs += res;
                ext += 1;
                extoff = 0;
                return res;
            }
            else {
                extoff += count;
                abs += count;
                return count;
            }
        }

        friend OStream &operator<<(OStream &os, const Position &p) {
            os << "Pos[abs=" << fmt(p.abs, "#0x")
                 << ", ext=" << p.ext
                 << ", extoff=" << fmt(p.extoff, "#0x") << "]";
            return os;
        }

        uint16_t ext;
        size_t extoff;
        size_t abs;
    };

    friend inline Unmarshaller &operator>>(Unmarshaller &u, RegularFile::Position &pos) {
        u >> pos.ext >> pos.extoff >> pos.abs;
        return u;
    }

    friend inline GateIStream &operator>>(GateIStream &is, RegularFile::Position &pos) {
        is >> pos.ext >> pos.extoff >> pos.abs;
        return is;
    }

    friend inline Marshaller &operator<<(Marshaller &m, const RegularFile::Position &pos) {
        m << pos.ext << pos.extoff << pos.abs;
        return m;
    }

    explicit RegularFile(int fd, Reference<M3FS> fs, int perms);
public:
    virtual ~RegularFile();

    /**
     * @return the file descriptor
     */
    int fd() const {
        return _fd;
    }
    /**
     * @return the M3FS instance
     */
    const Reference<M3FS> &fs() const {
        return _fs;
    }

    virtual Errors::Code stat(FileInfo &info) const override;
    virtual ssize_t seek(size_t offset, int whence) override;
    virtual ssize_t read(void *buffer, size_t count) override;
    virtual ssize_t write(const void *buffer, size_t count) override;

    virtual Errors::Code read_next(capsel_t *memgate, size_t *offset, size_t *length) override;
    virtual Errors::Code begin_write(capsel_t *memgate, size_t *offset, size_t *length) override;
    virtual void commit_write(size_t length) override;

    virtual char type() const override {
        return 'M';
    }
    virtual size_t serialize_length() override;
    virtual void delegate(VPE &vpe) override;
    virtual void serialize(Marshaller &m) override;
    static RegularFile *unserialize(Unmarshaller &um);

private:
    void set_pos(const Position &pos);
    ssize_t get_ext_len(bool writing, bool rebind);
    ssize_t advance(size_t count, bool writing);

    int _fd;
    Reference<M3FS> _fs;
    Position _pos;
    ExtentCache _cache;
    MemGate _mem;
    bool _extended;
    Position _max_write;
};

}
