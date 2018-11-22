
#pragma once

#include <base/col/DList.h>
#include <base/col/Treap.h>
#include <base/Errors.h>

#include <m3/session/M3FS.h>
#include <m3/stream/Standard.h>
#include <fs/internal.h>

#include <m3/Syscalls.h>
#include <m3/VPE.h>

#include <thread/ThreadManager.h>

#include "../ide_driver/Session/DiskSession.h"

using namespace m3;

class BufferHead : public TreapNode<BufferHead, blockno_t>, public DListItem {
friend class Buffer;
friend class FileBuffer;
friend class MetaBuffer;

public:
    BufferHead(blockno_t bno, size_t size);

    bool matches(blockno_t bno) {
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
        Buffer(size_t blocksize, DiskSession *disk);
        virtual ~Buffer() {};
        void lock(blockno_t bno);
        void unlock(blockno_t bno);
        void mark_dirty(blockno_t bno);
        virtual void flush() = 0;

protected:

        Treap<BufferHead> ht;
        DList<BufferHead> lru;

        size_t _blocksize;
        size_t _size;

        virtual BufferHead *get(blockno_t bno) = 0;
        virtual void flush_chunk(BufferHead *b) = 0;
        DiskSession *_disk;


};
