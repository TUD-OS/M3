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
#include <sys/uio.h>
#include <dirent.h>
#include <assert.h>

#include "buffer.h"
#include "exceptions.h"
#include "platform_common.h"
#include "traceplayer.h"
#include "exceptions.h"

#ifndef __LINUX__
#   include <base/util/Profile.h>
#   include <base/Log.h>
#endif

/*
 * *************************************************************************
 */

int TracePlayer::play(bool keep_time, bool make_chkpt) {
    trace_op_t *op = trace_ops;

    // touch all operations to make sure we don't get pagefaults in trace_ops arrary
    unsigned int numTraceOps = 0;
    while (op && op->opcode != INVALID_OP) {
        op++;
        if (op->opcode != WAITUNTIL_OP)
            numTraceOps++;
    }

    Platform::logf("Replaying %u operations ...\n", numTraceOps);

    Buffer buf;
    unsigned int lineNo = 1;
    unsigned int numReplayed = 0;
    FSAPI *fs = Platform::fsapi(pathPrefix);

    fs->start();
#ifndef __LINUX__
    m3::Profile::start(0xAAAA);
#endif

    // let's play
    op = trace_ops;
    while (op && op->opcode != INVALID_OP) {
#ifndef __LINUX__
        m3::Profile::start(lineNo);
#endif

        if (lineNo % 100 == 0xAAAA)
            fs->checkpoint(numReplayed, numTraceOps, make_chkpt);

#ifndef __LINUX__
        if(op->opcode != WAITUNTIL_OP)
            m3::Profile::stop(0xAAAA);
#endif

        //Platform::logf("line #%u: opcode=%u,op=%p\n", lineNo, op->opcode, op);
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
                for (unsigned int i = 0; i < args->count; i++) {
                    ssize_t err = fs->read(args->fd, buf.readBuffer(args->size), args->size);
                    if (err != (ssize_t)args->err)
                        THROW1(ReturnValueException, err, args->err, lineNo);
                }
                break;
            }
            case WRITE_OP:
            {
                write_args_t *args = &op->args.write;
                for (unsigned int i = 0; i < args->count; i++) {
                    ssize_t err = fs->write(args->fd, buf.writeBuffer(args->size), args->size);
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
            default:
                Platform::logf("unsupported trace operation: %d\n", op->opcode);
                return -ENOSYS;
        }

#ifndef __LINUX__
        if(op->opcode != WAITUNTIL_OP)
            m3::Profile::start(0xAAAA);
#endif

        if (op->opcode != WAITUNTIL_OP)
            numReplayed++;
        op++;

#ifndef __LINUX__
        m3::Profile::stop(lineNo);
#endif
        lineNo++;
    }

#ifndef __LINUX__
    m3::Profile::stop(0xAAAA);
#endif
    fs->stop();
    return 0;
}
