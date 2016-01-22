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

#include <m3/GateStream.h>
#include <m3/Errors.h>
#include <sys/stat.h>

namespace m3 {

class FSProxy {
public:
    enum {
        BUFFER_SIZE = 7 * 1024
    };

    enum Operation {
        OPEN,
        READ,
        WRITE,
        LSEEK,
        DUP,
        DUP2,
        CLOSE,
        COUNT
    };

    struct ReadResponse {
        ssize_t res;
        char buffer[BUFFER_SIZE];
    };
    struct WriteRequest {
        Operation op;
        int fd;
        size_t size;
        char buffer[BUFFER_SIZE];
    };

    static int open(SendGate &gate, const char *pathname, int flags, mode_t mode, bool use64) {
        int res;
        GateIStream reply = send_receive_vmsg(gate, OPEN, String(pathname), flags, mode, use64);
        reply >> res;
        return res;
    }

    static ssize_t read(SendGate &gate, int fd, void *buffer, size_t size) {
        ssize_t res = 0;
        char *buf = static_cast<char*>(buffer);
        while(size > 0) {
            size_t amount = std::min<size_t>(BUFFER_SIZE, size);
            GateIStream reply = send_receive_vmsg(gate, READ, fd, amount);
            const ReadResponse *resp = reinterpret_cast<const ReadResponse*>(reply.buffer());
            if(resp->res < 0)
                return resp->res;
            if(resp->res == 0)
                break;
            res += resp->res;
            memcpy(buf, resp->buffer, resp->res);
            buf += resp->res;
            size -= resp->res;
        }
        return res;
    }

    static ssize_t write(SendGate &gate, int fd, const void *buffer, size_t size) {
        ssize_t res = 0;
        const char *buf = static_cast<const char*>(buffer);
        WriteRequest *req = new WriteRequest;
        req->op = WRITE;
        while(size > 0) {
            size_t amount = std::min<size_t>(BUFFER_SIZE, size);
            memcpy(req->buffer, buf, amount);
            req->fd = fd;
            req->size = amount;
            GateIStream reply = send_receive_msg(gate, req, sizeof(*req));
            ssize_t rres;
            reply >> rres;
            if(rres < 0) {
                delete req;
                return rres;
            }
            res += rres;
            buf += amount;
            size -= amount;
        }
        delete req;
        return res;
    }

    static off_t lseek(SendGate &gate, int fd, off_t offset, int whence) {
        off_t res;
        GateIStream reply = send_receive_vmsg(gate, LSEEK, fd, offset, whence);
        reply >> res;
        return res;
    }

    static int dup(SendGate &gate, int fd) {
        int res;
        GateIStream reply = send_receive_vmsg(gate, DUP, fd);
        reply >> res;
        return res;
    }

    static int dup2(SendGate &gate, int oldfd, int newfd) {
        int res;
        GateIStream reply = send_receive_vmsg(gate, DUP2, oldfd, newfd);
        reply >> res;
        return res;
    }

    static int close(SendGate &gate, int fd) {
        int res;
        GateIStream reply = send_receive_vmsg(gate, CLOSE, fd);
        reply >> res;
        return res;
    }
};

}
