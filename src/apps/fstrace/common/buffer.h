// vim:ft=cpp
/*
 * (c) 2007-2013 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING-GPL-2 file for details.
 */

#ifndef __TRACE_BENCH_BUFFER_H
#define __TRACE_BENCH_BUFFER_H

#include <string.h>
#include <new>

#include "exceptions.h"

/*
 * *************************************************************************
 */

class Buffer {
  public:
#if defined(__t2__)
    enum { MaxBufferSize = 4*1024 };
#else
    enum { MaxBufferSize = 32*1024 };
#endif

    Buffer(size_t maxReadSize = MaxBufferSize,
           size_t maxWriteSize = MaxBufferSize);
    virtual ~Buffer();

    char *readBuffer(size_t size);
    char *writeBuffer(size_t size, bool nonNull = false);

  protected:
    size_t maxReadSize;
    size_t maxWriteSize;
    char   state;
    char  *readBuf;
    char  *writeBuf;
};

#endif // __TRACE_BENCH_BUFFER_H
