#include "Buffer.h"

#include <fs/internal.h>

using namespace m3;

BufferHead::BufferHead(blockno_t bno, size_t size)
    : TreapNode(bno),
      DListItem(),
      _size(size),
      locked(true),
      dirty(false),
      unlock(ThreadManager::get().get_wait_event()) {
}

Buffer::Buffer(size_t blocksize, Disk *disk)
    : ht(),
      lru(),
      _blocksize(blocksize),
      _disk(disk) {
}

void Buffer::lock(blockno_t bno) {
    BufferHead *b = get(bno);
    if(b)
        b->locked = true;
}

void Buffer::unlock(blockno_t bno) {
    BufferHead *b = get(bno);
    if(b) {
        b->locked = false;
        ThreadManager::get().notify(b->unlock);
    }
}

void Buffer::mark_dirty(blockno_t bno) {
    BufferHead *b = get(bno);
    if(b)
        b->dirty = true;
}
