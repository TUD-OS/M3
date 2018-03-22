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
#include <base/DTU.h>

#include <m3/com/MemGate.h>

#include <string.h>

namespace m3 {

#define M3FS_IFMT              0160000
#define M3FS_IFLNK             0120000
#define M3FS_IFPIP             0110000
#define M3FS_IFREG             0100000
#define M3FS_IFBLK             0060000
#define M3FS_IFDIR             0040000
#define M3FS_IFCHR             0020000
#define M3FS_ISUID             0004000
#define M3FS_ISGID             0002000
#define M3FS_ISSTICKY          0001000
#define M3FS_IRWXU             0000700
#define M3FS_IRUSR             0000400
#define M3FS_IWUSR             0000200
#define M3FS_IXUSR             0000100
#define M3FS_IRWXG             0000070
#define M3FS_IRGRP             0000040
#define M3FS_IWGRP             0000020
#define M3FS_IXGRP             0000010
#define M3FS_IRWXO             0000007
#define M3FS_IROTH             0000004
#define M3FS_IWOTH             0000002
#define M3FS_IXOTH             0000001

#define M3FS_ISDIR(mode)       (((mode) & M3FS_IFMT) == M3FS_IFDIR)
#define M3FS_ISREG(mode)       (((mode) & M3FS_IFMT) == M3FS_IFREG)
#define M3FS_ISLNK(mode)       (((mode) & M3FS_IFMT) == M3FS_IFLNK)
#define M3FS_ISCHR(mode)       (((mode) & M3FS_IFMT) == M3FS_IFCHR)
#define M3FS_ISBLK(mode)       (((mode) & M3FS_IFMT) == M3FS_IFBLK)
#define M3FS_ISPIP(mode)       (((mode) & M3FS_IFMT) == M3FS_IFPIP)

#define M3FS_MODE_READ         (M3FS_IRUSR | M3FS_IRGRP | M3FS_IROTH)
#define M3FS_MODE_WRITE        (M3FS_IWUSR | M3FS_IWGRP | M3FS_IWOTH)
#define M3FS_MODE_EXEC         (M3FS_IXUSR | M3FS_IXGRP | M3FS_IXOTH)

using dev_t     = uint8_t;
using mode_t    = uint32_t;
using inodeno_t = uint32_t;
using blockno_t = uint32_t;
using time_t    = uint32_t;

enum {
    INODE_DIR_COUNT     = 3,
    MAX_BLOCK_SIZE      = 4096,
};

constexpr inodeno_t INVALID_INO = static_cast<inodeno_t>(-1);

#define M3FS_SEEK_SET 0
#define M3FS_SEEK_CUR 1
#define M3FS_SEEK_END 2

enum {
    FILE_R      = 1,
    FILE_W      = 2,
    FILE_X      = 4,
    FILE_RW     = FILE_R | FILE_W,
    FILE_RWX    = FILE_R | FILE_W | FILE_X,
    FILE_TRUNC  = 8,
    FILE_APPEND = 16,
    FILE_CREATE = 32
};

static_assert(FILE_R == MemGate::R, "FILE_R is out of sync");
static_assert(FILE_W == MemGate::W, "FILE_W is out of sync");
static_assert(FILE_X == MemGate::X, "FILE_X is out of sync");

struct Extent {
    uint32_t start;
    uint32_t length;
};

struct FileInfo {
    dev_t devno;
    inodeno_t inode;
    mode_t mode;
    unsigned links;
    size_t size;
    time_t lastaccess;
    time_t lastmod;
    // for debugging
    unsigned extents;
    blockno_t firstblock;
};

// should be 64 bytes large
struct alignas(DTU_PKG_SIZE) INode {
    dev_t devno;
    uint16_t links;
    uint8_t : 8;
    inodeno_t inode;
    mode_t mode;
    uint64_t size;
    time_t lastaccess;
    time_t lastmod;
    uint32_t extents;
    Extent direct[INODE_DIR_COUNT];
    blockno_t indirect;
    blockno_t dindirect;
} PACKED;

struct DirEntry {
    inodeno_t nodeno;
    uint32_t namelen;
    uint32_t next;
    char name[];
} PACKED;

struct alignas(DTU_PKG_SIZE) SuperBlock {
    blockno_t first_inodebm_block() const {
        return 1;
    }
    blockno_t inodebm_blocks() const {
        return (((total_inodes + 7) / 8) + blocksize  - 1) / blocksize;
    }
    blockno_t first_blockbm_block() const {
        return first_inodebm_block() + inodebm_blocks();
    }
    blockno_t blockbm_blocks() const {
        return (((total_blocks + 7) / 8) + blocksize  - 1) / blocksize;
    }
    blockno_t first_inode_block() const {
        return first_blockbm_block() + blockbm_blocks();
    }
    blockno_t inode_blocks() const {
        return (total_inodes * sizeof(INode) + blocksize  - 1) / blocksize;
    }
    blockno_t first_data_block() const {
        return first_inode_block() + inode_blocks();
    }
    uint extents_per_block() const {
        return blocksize / sizeof(Extent);
    }
    uint inodes_per_block() const {
        return blocksize / sizeof(INode);
    }
    uint32_t get_checksum() const {
        return 1 + blocksize * 2 + total_inodes * 3 +
            total_blocks * 5 + free_inodes * 7 + free_blocks * 11 +
            first_free_inode * 13 + first_free_block * 17;
    }

    uint32_t blocksize;
    uint32_t total_inodes;
    uint32_t total_blocks;
    uint32_t free_inodes;
    uint32_t free_blocks;
    uint32_t first_free_inode;
    uint32_t first_free_block;
    uint32_t checksum;
} PACKED;

class Bitmap {
public:
    typedef uint32_t word_t;

    static const int WORD_BITS = sizeof(word_t) * 8;

    explicit Bitmap(size_t bits)
        : _allocated(true),
          _words(new word_t[(bits + WORD_BITS - 1) / WORD_BITS]()) {
    }
    explicit Bitmap(word_t *bytes)
        : _allocated(false),
          _words(bytes) {
    }
    ~Bitmap() {
        if(_allocated)
            delete[] _words;
    }

    word_t *bytes() {
        return _words;
    }

    bool is_word_set(uint bit) const {
        return _words[idx(bit)] == static_cast<word_t>(-1);
    }
    bool is_word_free(uint bit) const {
        return _words[idx(bit)] == 0;
    }
    void set_word(uint bit) {
        _words[idx(bit)] = static_cast<word_t>(-1);
    }
    void clear_word(uint bit) {
        _words[idx(bit)] = 0;
    }

    bool is_set(uint bit) const {
        return (_words[idx(bit)] & bitpos(bit)) != 0;
    }
    void set(uint bit) {
        _words[idx(bit)] |= bitpos(bit);
    }
    void unset(uint bit) {
        _words[idx(bit)] &= ~bitpos(bit);
    }

private:
    static size_t idx(uint bit) {
        return bit / WORD_BITS;
    }
    static word_t bitpos(uint bit) {
        return 1UL << (bit % WORD_BITS);
    }

    bool _allocated;
    word_t *_words;
};

}

#if defined(__tools__)

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <err.h>

extern m3::SuperBlock sb;
extern FILE *file;

static UNUSED inline void read_from_block(void *buffer, size_t len, m3::blockno_t bno, size_t off = 0) {
    off_t offset = static_cast<off_t>(static_cast<size_t>(bno * sb.blocksize) + off);
    if(fseek(file, offset, SEEK_SET) != 0)
        err(1, "Unable to seek to block %u+%zu: %s\n", bno, off, strerror(errno));
    if(fread(buffer, 1, len, file) != len)
        err(1, "Unable to read from block %u: %s\n", bno, strerror(errno));
}

static UNUSED inline void write_to_block(const void *buffer, size_t len, m3::blockno_t bno, size_t off = 0) {
    off_t offset = static_cast<off_t>(static_cast<size_t>(bno * sb.blocksize) + off);
    if(fseek(file, offset, SEEK_SET) != 0)
        err(1, "Unable to seek to block %u+%zu: %s\n", bno, off, strerror(errno));
    if(fwrite(buffer, 1, len, file) != len)
        err(1, "Unable to write to block %u: %s\n", bno, strerror(errno));
}

static UNUSED m3::INode read_inode(m3::inodeno_t ino) {
    m3::INode inode;
    read_from_block(&inode, sizeof(inode), sb.first_inode_block(), ino * sizeof(m3::INode));
    return inode;
}

static UNUSED m3::blockno_t get_block_no_rec(const m3::INode &ino, m3::blockno_t indirect, size_t &no, int layer) {
    m3::blockno_t res = 0;
    m3::Extent *extents = new m3::Extent[sb.extents_per_block()];
    read_from_block(extents, sb.blocksize, indirect);
    for(size_t i = 0; i < sb.extents_per_block(); ++i) {
        if(layer > 0) {
            res = get_block_no_rec(ino, extents[i].start, no, layer - 1);
            if(res)
                break;
        }
        else {
            if(extents[i].length > no) {
                res = extents[i].start + no;
                break;
            }
            no -= extents[i].length;
        }
    }
    delete[] extents;
    return res;
}

static UNUSED m3::blockno_t get_block_no(const m3::INode &ino, size_t no) {
    for(size_t i = 0; i < m3::INODE_DIR_COUNT; ++i) {
        if(ino.direct[i].length > no)
            return ino.direct[i].start + no;
        no -= ino.direct[i].length;
    }

    m3::blockno_t res = get_block_no_rec(ino, ino.indirect, no, 0);
    if(res)
        return res;
    return get_block_no_rec(ino, ino.dindirect, no, 1);
}

static UNUSED uint first_free(m3::Bitmap &bm, uint total) {
    uint i;
    for(i = 0; bm.is_set(i) && i < total; ++i)
        ;
    return i;
}

#endif
