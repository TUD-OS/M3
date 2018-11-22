#pragma once

#include "Buffer.h"
#include <fs/internal.h>

#define FILE_BUFFER_SIZE        16384     // at least 128
#define LOAD_LIMIT              128

using namespace m3;

struct InodeExt : public DListItem {
    blockno_t _start;
    size_t _size;
    InodeExt(blockno_t start, size_t size) : DListItem(), _start(start), _size(size) {}
};

class FileBufferHead : public BufferHead {
friend class FileBuffer;

public:
        explicit FileBufferHead(blockno_t bno, size_t size, size_t blocksize);

private:
        MemGate _data;
        DList<InodeExt> _extents;
};


class FileBuffer : public Buffer {

public:
        explicit FileBuffer(size_t blocksize, DiskSession *disk, size_t max_load);

        size_t get_extent(blockno_t bno, size_t size, capsel_t sel, int perms, size_t accessed, bool load = true, bool check = false);
        void flush() override;

private:
        FileBufferHead *get(blockno_t bno) override;
        void flush_chunk(BufferHead *b) override;

        size_t _max_load;
};
