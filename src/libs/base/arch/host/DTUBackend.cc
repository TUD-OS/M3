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

#include <base/arch/host/DTUBackend.h>
#include <base/log/Lib.h>
#include <base/DTU.h>
#include <base/Panic.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>

namespace m3 {

void MsgBackend::create() {
    for(peid_t pe = 0; pe < PE_COUNT; ++pe) {
        for(epid_t ep = 0; ep < EP_COUNT; ++ep) {
            size_t off = pe * EP_COUNT + ep;
            _ids[off] = msgget(get_msgkey(pe, ep), IPC_CREAT | IPC_EXCL | 0777);
            if(_ids[off] == -1)
                PANIC("Creation of message queue failed: " << strerror(errno));
        }
    }
}

void MsgBackend::destroy() {
    for(size_t i = 0; i < PE_COUNT * EP_COUNT; ++i)
        msgctl(_ids[i], IPC_RMID, nullptr);
}

void MsgBackend::send(peid_t pe, epid_t ep, const DTU::Buffer *buf) {
    int msgqid = msgget(get_msgkey(pe, ep), 0);
    // send it
    int res;
    do
        res = msgsnd(msgqid, buf, buf->length + DTU::HEADER_SIZE - sizeof(buf->length), 0);
    while(res == -1 && errno == EINTR);
    if(res != 0)
        PANIC("msgsnd to " << msgqid << " (SEP " << ep << ") failed: " << strerror(errno));
}

ssize_t MsgBackend::recv(epid_t ep, DTU::Buffer *buf) {
    int msgqid = DTU::get().get_ep(ep, DTU::EP_BUF_MSGQID);
    if(msgqid == 0) {
        msgqid = msgget(get_msgkey(env()->pe, ep), 0);
        DTU::get().set_ep(ep, DTU::EP_BUF_MSGQID, static_cast<word_t>(msgqid));
    }

    ssize_t res = msgrcv(msgqid, buf, sizeof(*buf) - sizeof(buf->length), 0, IPC_NOWAIT);
    if(res == -1)
        return -1;
    res += static_cast<ssize_t>(sizeof(buf->length));
    return res;
}

SocketBackend::SocketBackend() : _sock(socket(AF_UNIX, SOCK_DGRAM, 0)), _localsocks(), _endpoints() {
    if(_sock == -1)
        PANIC("Unable to open socket: " << strerror(errno));

    // build socket names for all endpoints on all PEs
    for(peid_t pe = 0; pe < PE_COUNT; ++pe) {
        for(epid_t ep = 0; ep < EP_COUNT; ++ep) {
            sockaddr_un *addr = _endpoints + pe * EP_COUNT + ep;
            addr->sun_family = AF_UNIX;
            // we can't put that in the format string
            addr->sun_path[0] = '\0';
            snprintf(addr->sun_path + 1, sizeof(addr->sun_path) - 1, "m3_ep_%d.%d", (int)pe, (int)ep);
        }
    }

    // create sockets and bind them for our own endpoints
    for(epid_t ep = 0; ep < EP_COUNT; ++ep) {
        _localsocks[ep] = socket(AF_UNIX, SOCK_DGRAM, 0);
        if(_localsocks[ep] == -1)
            PANIC("Unable to create socket for ep " << ep << ": " << strerror(errno));

        // if we do fork+exec in kernel/lib we want to close all sockets. they are recreated anyway
        if(fcntl(_localsocks[ep], F_SETFD, FD_CLOEXEC) == -1)
            PANIC("Setting FD_CLOEXEC failed: " << strerror(errno));
        // all calls should be non-blocking
        if(fcntl(_localsocks[ep], F_SETFL, O_NONBLOCK) == -1)
            PANIC("Setting O_NONBLOCK failed: " << strerror(errno));

        sockaddr_un *addr = _endpoints + env()->pe * EP_COUNT + ep;
        if(bind(_localsocks[ep], (struct sockaddr*)addr, sizeof(*addr)) == -1)
            PANIC("Binding socket for ep " << ep << " failed: " << strerror(errno));
    }
}

SocketBackend::~SocketBackend() {
    for(epid_t ep = 0; ep < EP_COUNT; ++ep)
        close(_localsocks[ep]);
}

void SocketBackend::send(peid_t pe, epid_t ep, const DTU::Buffer *buf) {
    if(sendto(_sock, buf, buf->length + DTU::HEADER_SIZE, 0,
        (struct sockaddr*)(_endpoints + pe * EP_COUNT + ep), sizeof(sockaddr_un)) == -1)
        LLOG(DTUERR, "Sending message to EP " << pe << ":" << ep << " failed: " << strerror(errno));
}

ssize_t SocketBackend::recv(epid_t ep, DTU::Buffer *buf) {
    ssize_t res = recvfrom(_localsocks[ep], buf, sizeof(*buf), 0, nullptr, nullptr);
    if(res <= 0)
        return -1;
    return res;
}

}
