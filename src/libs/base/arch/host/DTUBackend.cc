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
#include <poll.h>
#include <signal.h>
#include <unistd.h>

namespace m3 {

static const char *ev_names[] = {
    "REQ", "RESP", "MSG"
};

DTUBackend::DTUBackend()
    : _sock(socket(AF_UNIX, SOCK_DGRAM, 0)),
      _pending(),
      _localsocks(),
      _endpoints() {
    if(_sock == -1)
        PANIC("Unable to open socket: " << strerror(errno));

    // build socket names for all endpoints on all PEs
    for(peid_t pe = 0; pe < PE_COUNT; ++pe) {
        for(epid_t ep = 0; ep < EP_COUNT + 3; ++ep) {
            sockaddr_un *addr = _endpoints + pe * (EP_COUNT + 3) + ep;
            addr->sun_family = AF_UNIX;
            // we can't put that in the format string
            addr->sun_path[0] = '\0';
            snprintf(addr->sun_path + 1, sizeof(addr->sun_path) - 1, "m3_ep_%d.%d", (int)pe, (int)ep);
        }
    }

    // create sockets and bind them for our own endpoints
    for(epid_t ep = 0; ep < ARRAY_SIZE(_localsocks); ++ep) {
        _localsocks[ep] = socket(AF_UNIX, SOCK_DGRAM, 0);
        if(_localsocks[ep] == -1)
            PANIC("Unable to create socket for ep " << ep << ": " << strerror(errno));

        // if we do fork+exec in kernel/lib we want to close all sockets. they are recreated anyway
        if(fcntl(_localsocks[ep], F_SETFD, FD_CLOEXEC) == -1)
            PANIC("Setting FD_CLOEXEC failed: " << strerror(errno));

        sockaddr_un *addr = _endpoints + env()->pe * (EP_COUNT + 3) + ep;
        if(bind(_localsocks[ep], (struct sockaddr*)addr, sizeof(*addr)) == -1)
            PANIC("Binding socket for ep " << ep << " failed: " << strerror(errno));
    }

    for(size_t i = 0; i < ARRAY_SIZE(_fds); ++i) {
        _fds[i].fd = _localsocks[i];
        _fds[i].events = POLLIN | POLLERR;
        _fds[i].revents = 0;
    }
}

DTUBackend::~DTUBackend() {
    for(epid_t ep = 0; ep < ARRAY_SIZE(_localsocks); ++ep)
        close(_localsocks[ep]);
}

void DTUBackend::poll() {
    _pending = ::ppoll(_fds, ARRAY_SIZE(_fds), nullptr, nullptr);
    if(_pending < 0 && errno != EINTR)
        LLOG(DTUERR, "Polling for notifications failed: " << strerror(errno));
}

bool DTUBackend::has_command() {
    if(_pending <= 0)
        poll();

    size_t fdidx = EP_COUNT + static_cast<size_t>(Event::REQ);
    if(_fds[fdidx].revents != 0) {
        uint8_t dummy = 0;
        if(recvfrom(_fds[fdidx].fd, &dummy, sizeof(dummy), 0, nullptr, nullptr) <= 0) {
            LLOG(DTUERR, "Receiving notification from " << ev_names[static_cast<size_t>(Event::REQ)]
                                                        << " failed: " << strerror(errno));
        }

        _fds[fdidx].revents = 0;
        _pending--;
        return true;
    }
    return false;
}

epid_t DTUBackend::has_msg() {
    if(_pending <= 0)
        poll();

    for(epid_t i = 0; i < EP_COUNT; ++i) {
        if(_fds[i].revents != 0) {
            _fds[i].revents = 0;
            _pending--;
            return i;
        }
    }
    return EP_COUNT;
}

void DTUBackend::notify(Event ev) {
    uint8_t dummy = 0;
    sockaddr_un *dstsock = _endpoints + env()->pe * (EP_COUNT + 3) + EP_COUNT + static_cast<size_t>(ev);
    int res = sendto(_sock, &dummy, sizeof(dummy), 0, (struct sockaddr*)dstsock, sizeof(sockaddr_un));
    if(res == -1) {
        LLOG(DTUERR, "Sending notification to " << ev_names[static_cast<size_t>(ev)]
                                                << " failed: " << strerror(errno));
    }
}

bool DTUBackend::wait(Event ev) {
    struct pollfd fds;
    fds.fd = _localsocks[EP_COUNT + static_cast<size_t>(ev)];
    fds.events = POLLIN;
    if(::ppoll(&fds, 1, nullptr, nullptr) == -1) {
        if(errno != EINTR) {
            LLOG(DTUERR, "Polling for notification from " << ev_names[static_cast<size_t>(ev)]
                                                          << " failed: " << strerror(errno));
        }
        return false;
    }

    uint8_t dummy = 0;
    if(recvfrom(fds.fd, &dummy, sizeof(dummy), 0, nullptr, nullptr) <= 0) {
        LLOG(DTUERR, "Receiving notification from " << ev_names[static_cast<size_t>(ev)]
                                                    << " failed: " << strerror(errno));
        return false;
    }
    return true;
}

void DTUBackend::send(peid_t pe, epid_t ep, const DTU::Buffer *buf) {
    int res = sendto(_sock, buf, buf->length + DTU::HEADER_SIZE, 0,
                     (struct sockaddr*)(_endpoints + pe * (EP_COUNT + 3) + ep), sizeof(sockaddr_un));
    if(res == -1)
        LLOG(DTUERR, "Sending message to EP " << pe << ":" << ep << " failed: " << strerror(errno));
}

ssize_t DTUBackend::recv(epid_t ep, DTU::Buffer *buf) {
    ssize_t res = recvfrom(_localsocks[ep], buf, sizeof(*buf), 0, nullptr, nullptr);
    if(res <= 0)
        return -1;
    return res;
}

}
