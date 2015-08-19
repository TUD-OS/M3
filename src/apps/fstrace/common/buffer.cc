// vim:ft=cpp
/*
 * (c) 2007-2013 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING-GPL-2 file for details.
 */

#include "buffer.h"

/*
 * *************************************************************************
 */

Buffer::Buffer(size_t maxReadSize, size_t maxWriteSize) {

    this->maxReadSize  = maxReadSize;
    this->maxWriteSize = maxWriteSize;
    this->state        = 1;

    readBuf  = new char[maxReadSize];
    writeBuf = new char[maxWriteSize];

    if (readBuf == 0 || writeBuf == 0) {
        delete [] readBuf;
        delete [] writeBuf;
        THROW(OutOfMemoryException);
    }

    // memset(readBuf, 0, maxReadSize);
    // memset(writeBuf, 0, maxWriteSize);
}


Buffer::~Buffer() {

    delete [] readBuf;
    delete [] writeBuf;
}


char *Buffer::readBuffer(size_t size) {

    if (size > maxReadSize)
        THROW(OutOfMemoryException);

    return readBuf;
}


char *Buffer::writeBuffer(size_t size, bool) {

    if (size > maxWriteSize)
        THROW(OutOfMemoryException);

    // char byte = (nonNull) ? 0 : state++;
    // memset(writeBuf, byte, size);

    return writeBuf;
}
