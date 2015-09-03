/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/arch/host/DTUBackend.h>
#include <m3/DTU.h>
#include <m3/Log.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>

namespace m3 {

void MsgBackend::create() {
    for(size_t c = 0; c < MAX_CORES; ++c) {
        for(size_t i = 0; i < CHAN_COUNT; ++i) {
            size_t off = c * CHAN_COUNT + i;
            _ids[off] = msgget(get_msgkey(c, i), IPC_CREAT | IPC_EXCL | 0777);
            if(_ids[off] == -1)
                PANIC("Creation of message queue failed: " << strerror(errno));
        }
    }
}

void MsgBackend::destroy() {
    for(size_t i = 0; i < MAX_CORES * CHAN_COUNT; ++i)
        msgctl(_ids[i], IPC_RMID, nullptr);
}


void MsgBackend::reset() {
    // reset all msgqids because might have changed due to a different core we're running on
    for(int i = 0; i < CHAN_COUNT; ++i)
        DTU::get().set_ep(i, DTU::EP_BUF_MSGQID, 0);
}

void MsgBackend::send(int core, int chan, const DTU::Buffer *buf) {
    int msgqid = msgget(get_msgkey(core, chan), 0);
    // send it
    int res;
    do
        res = msgsnd(msgqid, buf, buf->length + DTU::HEADER_SIZE - sizeof(buf->length), 0);
    while(res == -1 && errno == EINTR);
    if(res != 0)
        PANIC("msgsnd to " << msgqid << " (SEP " << chan << ") failed: " << strerror(errno));
}

ssize_t MsgBackend::recv(int chan, DTU::Buffer *buf) {
    int msgqid = DTU::get().get_ep(chan, DTU::EP_BUF_MSGQID);
    if(msgqid == 0) {
        msgqid = msgget(get_msgkey(coreid(), chan), 0);
        DTU::get().set_ep(chan, DTU::EP_BUF_MSGQID, msgqid);
    }

    ssize_t res = msgrcv(msgqid, buf, sizeof(*buf) - sizeof(buf->length), 0, IPC_NOWAIT);
    if(res == -1)
        return -1;
    res += sizeof(buf->length);
    return res;
}

SocketBackend::SocketBackend() : _sock(socket(AF_UNIX, SOCK_DGRAM, 0)), _localsocks(), _endpoints() {
    if(_sock == -1)
        PANIC("Unable to open socket: " << strerror(errno));

    // build socket names for all channels on all cores
    for(int core = 0; core < MAX_CORES; ++core) {
        for(int chan = 0; chan < CHAN_COUNT; ++chan) {
            sockaddr_un *ep = _endpoints + core * CHAN_COUNT + chan;
            ep->sun_family = AF_UNIX;
            // we can't put that in the format string
            ep->sun_path[0] = '\0';
            snprintf(ep->sun_path + 1, sizeof(ep->sun_path) - 1, "m3_ep_%d.%d", core, chan);
        }
    }

    // create sockets and bind them for our own channels
    for(int chan = 0; chan < CHAN_COUNT; ++chan) {
        _localsocks[chan] = socket(AF_UNIX, SOCK_DGRAM, 0);
        if(_localsocks[chan] == -1)
            PANIC("Unable to create socket for chan " << chan << ": " << strerror(errno));

        // if we do fork+exec in kernel/lib we want to close all sockets. they are recreated anyway
        if(fcntl(_localsocks[chan], F_SETFD, FD_CLOEXEC) == -1)
            PANIC("Setting FD_CLOEXEC failed: " << strerror(errno));
        // all calls should be non-blocking
        if(fcntl(_localsocks[chan], F_SETFL, O_NONBLOCK) == -1)
            PANIC("Setting O_NONBLOCK failed: " << strerror(errno));

        sockaddr_un *ep = _endpoints + coreid() * CHAN_COUNT + chan;
        if(bind(_localsocks[chan], (struct sockaddr*)ep, sizeof(*ep)) == -1)
            PANIC("Binding socket for chan " << chan << " failed: " << strerror(errno));
    }
}

void SocketBackend::send(int core, int chan, const DTU::Buffer *buf) {
    if(sendto(_sock, buf, buf->length + DTU::HEADER_SIZE, 0,
        (struct sockaddr*)(_endpoints + core * CHAN_COUNT + chan), sizeof(sockaddr_un)) == -1)
        LOG(DTUERR, "Sending message to EP " << core << ":" << chan << " failed: " << strerror(errno));
}

ssize_t SocketBackend::recv(int chan, DTU::Buffer *buf) {
    ssize_t res = recvfrom(_localsocks[chan], buf, sizeof(*buf), 0, nullptr, nullptr);
    if(res <= 0)
        return -1;
    return res;
}

}
