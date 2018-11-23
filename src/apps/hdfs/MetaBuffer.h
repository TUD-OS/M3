#pragma once

#include <base/col/Treap.h>

#include <m3/com/MemGate.h>

#include <fs/internal.h>

#include "sess/FileSession.h"
#include "Buffer.h"

#define META_BUFFER_SIZE    512

using namespace m3;

class MetaBufferHead : public BufferHead {
    friend class MetaBuffer;

public:
    explicit MetaBufferHead(blockno_t bno, size_t size, size_t off, char *data);

private:
    size_t _off;
    void *_data;
    size_t _linkcount;
};

/*
 * stores single blocks
 * lru is the list of free blocks
 * blocks will be freed if no session uses them
 */
class MetaBuffer : public Buffer {
public:
    explicit MetaBuffer(size_t blocksize, DiskSession *disk);

    void *get_block(Request &r, blockno_t bno);
    void quit(MetaBufferHead *b);
    void write_back(blockno_t bno);
    void flush() override;
    bool dirty(blockno_t);

private:
    MetaBufferHead *get(blockno_t bno) override;
    void flush_chunk(BufferHead *b) override;
    char *_blocks;
    MemGate gate;
};
