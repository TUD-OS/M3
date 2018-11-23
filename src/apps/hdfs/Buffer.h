
#pragma once

#include <base/Errors.h>
#include <base/col/DList.h>
#include <base/col/Treap.h>

#include <m3/session/Disk.h>
#include <m3/session/M3FS.h>
#include <m3/stream/Standard.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

#include <fs/internal.h>
#include <thread/ThreadManager.h>

class BufferHead : public m3::TreapNode<BufferHead, m3::blockno_t>, public m3::DListItem {
    friend class Buffer;
    friend class FileBuffer;
    friend class MetaBuffer;

public:
    BufferHead(m3::blockno_t bno, size_t size);

    bool matches(m3::blockno_t bno) {
        return (key() <= bno) && (bno < key() + _size);
    }

protected:
    // number of blocks
    size_t _size;
    bool locked;
    bool dirty;
    event_t unlock;
};

class Buffer {
    uint64_t event_count = 0;

public:
    Buffer(size_t blocksize, m3::Disk *disk);
    virtual ~Buffer(){};
    void lock(m3::blockno_t bno);
    void unlock(m3::blockno_t bno);
    void mark_dirty(m3::blockno_t bno);
    virtual void flush() = 0;

protected:
    m3::Treap<BufferHead> ht;
    m3::DList<BufferHead> lru;

    size_t _blocksize;
    size_t _size;

    virtual BufferHead *get(m3::blockno_t bno)  = 0;
    virtual void flush_chunk(BufferHead *b) = 0;
    m3::Disk *_disk;
};
