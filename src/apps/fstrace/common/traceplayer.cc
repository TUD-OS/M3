// vim:ft=cpp
/*
 * (c) 2007-2013 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING-GPL-2 file for details.
 */

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#include "buffer.h"
#include "exceptions.h"
#include "platform_common.h"
#include "traceplayer.h"
#include "exceptions.h"

#ifndef __LINUX__
#   include <base/stream/Serial.h>
#   include <base/util/Time.h>
#endif

__attribute__((unused)) static const char *op_names[] = {
    "INVALID",
    "WAITUNTIL",
    "OPEN",
    "CLOSE",
    "FSYNC",
    "READ",
    "WRITE",
    "PREAD",
    "PWRITE",
    "LSEEK",
    "FTRUNCATE",
    "FSTAT",
    "FSTATAT",
    "STAT",
    "RENAME",
    "UNLINK",
    "RMDIR",
    "MKDIR",
    "SENDFILE",
    "GETDENTS",
    "CREATEFILE",
    "ACCEPT",
    "RECVFROM",
    "WRITEV"
};

/*
 * *************************************************************************
 */

int TracePlayer::play(Trace *trace, bool wait, bool data, bool stdio, bool keep_time, bool) {
    size_t rdBufSize = 0;
    size_t wrBufSize = 0;

    unsigned int numTraceOps = 0;
    trace_op_t *op = trace->trace_ops;
    while (op && op->opcode != INVALID_OP) {
        op++;
        if (op->opcode != WAITUNTIL_OP)
            numTraceOps++;

        // determine max read and write buf size
        switch(op->opcode) {
            case READ_OP:
            case PREAD_OP:
                rdBufSize = rdBufSize < op->args.read.size ? op->args.read.size : rdBufSize;
                break;
            case RECVFROM_OP:
                rdBufSize = rdBufSize < op->args.recvfrom.size ? op->args.recvfrom.size : rdBufSize;
                break;
            case WRITE_OP:
            case PWRITE_OP:
                wrBufSize = wrBufSize < op->args.write.size ? op->args.write.size : wrBufSize;
                break;
            case WRITEV_OP:
                wrBufSize = wrBufSize < op->args.writev.size ? op->args.writev.size : wrBufSize;
                break;
            case SENDFILE_OP:
                rdBufSize = rdBufSize < Buffer::MaxBufferSize ? Buffer::MaxBufferSize : rdBufSize;
                break;
        }
    }

    Platform::logf("Replaying %u operations ...\n", numTraceOps);

    Buffer buf(rdBufSize, wrBufSize);
    int lineNo = 1;
    unsigned int numReplayed = 0;
    FSAPI *fs = Platform::fsapi(wait, data, stdio, pathPrefix);

    fs->start();
#ifndef __LINUX__
    m3::Time::start(0xBBBB);
#endif

    // let's play
    op = trace->trace_ops;
    while (op && op->opcode != INVALID_OP) {
#ifndef __LINUX__
        m3::Time::start(static_cast<uint>(lineNo));

        if(op->opcode != WAITUNTIL_OP)
            m3::Time::stop(0xBBBB);

        // m3::Serial::get() << "line " << lineNo << ": opcode=" << op_names[op->opcode] << "\n";
#endif

        switch (op->opcode) {
            case WAITUNTIL_OP:
            {
                if (!keep_time)
                    break;

                fs->waituntil(&op->args.waituntil, lineNo);
                break;
            }
            case OPEN_OP:
            {
                fs->open(&op->args.open, lineNo);
                break;
            }
            case CLOSE_OP:
            {
                fs->close(&op->args.close, lineNo);
                break;
            }
            case FSYNC_OP:
            {
                fs->fsync(&op->args.fsync, lineNo);
                break;
            }
            case READ_OP:
            {
                read_args_t *args = &op->args.read;
                size_t amount = (stdio && args->fd == 0) ? static_cast<size_t>(args->err) : args->size;
                for (unsigned int i = 0; i < args->count; i++) {
                    ssize_t err = fs->read(args->fd, buf.readBuffer(amount), amount);
                    if (err != (ssize_t)args->err)
                        THROW1(ReturnValueException, err, args->err, lineNo);
                }
                break;
            }
            case WRITE_OP:
            {
                write_args_t *args = &op->args.write;
                size_t amount = (stdio && args->fd == 1) ? static_cast<size_t>(args->err) : args->size;
                for (unsigned int i = 0; i < args->count; i++) {
                    ssize_t err = fs->write(args->fd, buf.writeBuffer(amount), amount);
                    if (err != (ssize_t)args->err)
                        THROW1(ReturnValueException, err, args->err, lineNo);
                }
                break;
            }
            case PREAD_OP:
            {
                pread_args_t *args = &op->args.pread;
                ssize_t err = fs->pread(args->fd, buf.readBuffer(args->size), args->size, args->offset);
                if (err != (ssize_t)args->err)
                    THROW1(ReturnValueException, err, args->err, lineNo);
                break;
            }
            case PWRITE_OP:
            {
                pwrite_args_t *args = &op->args.pwrite;
                ssize_t err = fs->pwrite(args->fd, buf.writeBuffer(args->size), args->size, args->offset);
                if (err != (ssize_t)args->err)
                    THROW1(ReturnValueException, err, args->err, lineNo);
                break;
            }
            case LSEEK_OP:
            {
                fs->lseek(&op->args.lseek, lineNo);
                break;
            }
            case FTRUNCATE_OP:
            {
                fs->ftruncate(&op->args.ftruncate, lineNo);
                break;
            }
            case FSTAT_OP:
            {
                fs->fstat(&op->args.fstat, lineNo);
                break;
            }
            case FSTATAT_OP:
            {
                fs->fstatat(&op->args.fstatat, lineNo);
                break;
            }
            case STAT_OP:
            {
                fs->stat(&op->args.stat, lineNo);
                break;
            }
            case RENAME_OP:
            {
                fs->rename(&op->args.rename, lineNo);
                break;
            }
            case UNLINK_OP:
            {
                fs->unlink(&op->args.unlink, lineNo);
                break;
            }
            case RMDIR_OP:
            {
                fs->rmdir(&op->args.rmdir, lineNo);
                break;
            }
            case MKDIR_OP:
            {
                fs->mkdir(&op->args.mkdir, lineNo);
                break;
            }
            case SENDFILE_OP:
            {
                fs->sendfile(buf, &op->args.sendfile, lineNo);
                break;
            }
            case GETDENTS_OP:
            {
                fs->getdents(&op->args.getdents, lineNo);
                break;
            }
            case CREATEFILE_OP:
            {
                fs->createfile(&op->args.createfile, lineNo);
                break;
            }
            case ACCEPT_OP:
            {
                fs->accept(&op->args.accept, lineNo);
                break;
            }
            case RECVFROM_OP:
            {
                fs->recvfrom(buf, &op->args.recvfrom, lineNo);
                break;
            }
            case WRITEV_OP:
            {
                fs->writev(buf, &op->args.writev, lineNo);
                break;
            }
            default:
                Platform::logf("unsupported trace operation: %d\n", op->opcode);
                return -ENOSYS;
        }

#ifndef __LINUX__
        if(op->opcode != WAITUNTIL_OP)
            m3::Time::start(0xBBBB);
#endif

        if (op->opcode != WAITUNTIL_OP)
            numReplayed++;
        op++;

#ifndef __LINUX__
        m3::Time::stop(static_cast<uint>(lineNo));
#endif
        lineNo++;
    }

#ifndef __LINUX__
    m3::Time::stop(0xBBBB);
#endif
    fs->stop();
    return 0;
}
