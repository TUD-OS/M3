/*
 * Copyright (C) 2017, Georg Kotheimer <georg.kotheimer@mailbox.tu-dresden.de>
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

#include <m3/session/ClientSession.h>
#include <m3/com/MemGate.h>
#include <m3/com/SendGate.h>
#include <m3/pipe/DirectPipeReader.h>
#include <m3/pipe/DirectPipeWriter.h>

namespace m3 {

class IpAddr {
public:
    IpAddr()
        : _addr(0) {
    }

    explicit IpAddr(uint32_t addr)
        : _addr(addr) {
    }
    IpAddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
        : _addr(static_cast<uint32_t>(a << 24 | b << 16 | c << 8 | d)) {
    }

    uint32_t addr() {
        return _addr;
    }
    void addr(uint32_t addr) {
        _addr = addr;
    }
private:
    uint32_t _addr;
};

class NetDirectPipe {
public:
    static const size_t BUFFER_SIZE         = 2 * 1024 * 1024;

    static const size_t MSG_SIZE            = 64;
    static const size_t MSG_BUF_SIZE        = MSG_SIZE * 16;
    static const size_t CREDITS             = MSG_BUF_SIZE;

    NetDirectPipe(capsel_t caps, size_t size, bool create);
    ~NetDirectPipe();

    DirectPipeReader *reader();
    DirectPipeWriter *writer();

private:
    bool _keep_caps;
    capsel_t _caps;
    size_t _size;
    RecvGate _rgate;
    MemGate _mem;
    SendGate _sgate;
    DirectPipeReader * _reader;
    DirectPipeWriter * _writer;
};

struct MessageHeader {
    explicit MessageHeader()
        : addr(), port(0), size(0) {
    }
    explicit MessageHeader(IpAddr _addr, uint16_t _port, size_t _size)
        : addr(_addr), port(_port), size(_size) {
    }

    static size_t serialize_length() {
        return ostreamsize<uint32_t, uint16_t, size_t>();
    }

    void serialize(Marshaller &m) {
        m.vput(addr.addr(), port, size);
    }

    void unserialize(Unmarshaller &um) {
        uint32_t _addr;
        um.vpull(_addr, port, size);
        addr.addr(_addr);
    }

    IpAddr addr;
    uint16_t port;
    size_t size;
};

class NetworkManager;

class InetSocket {
public:
    explicit InetSocket(int sd, NetworkManager &nm);
    ~InetSocket();

    int sd() {
        return _sd;
    }

    Errors::Code bind(IpAddr addr, uint16_t port);
    Errors::Code listen();
    Errors::Code connect(IpAddr addr, uint16_t port);

    // blocks when send buffer of the socket is full
    ssize_t send(const void *buffer, size_t count, bool blocking = true);
    ssize_t sendto(const void *buffer, size_t count, IpAddr addr, uint16_t port, bool blocking = true);

    // blocks when receive buffer of the socket is empty
    ssize_t recv(void *buffer, size_t count, bool blocking = true);
    ssize_t recvmsg(void *buffer, size_t count, IpAddr *addr, uint16_t *port, bool blocking = true);

    Errors::Code close();

private:
    friend NetworkManager;
    int _sd;
    NetworkManager &_nm;
    NetDirectPipe *_recv_pipe;
    NetDirectPipe *_send_pipe;
};

// Maybe RawSocket or something...
class NetworkManager : public ClientSession {
public:
    enum Operation {
        CREATE, BIND, LISTEN, CONNECT, ACCEPT, CLOSE,
        // SEND, // provided by pipes
        // RECV, // provided by pipes
        COUNT
    };

    enum SocketType {
        SOCK_STREAM, // TCP
        SOCK_DGRAM,  // UDP
        SOCK_RAW     // IP
    };

    explicit NetworkManager(const String &service)
        : ClientSession(service), _metagate(SendGate::bind(obtain(1).start())) {
    }
    explicit NetworkManager(capsel_t session, capsel_t metagate)
        : ClientSession(session), _metagate(SendGate::bind(metagate)) {
    }

    const SendGate &meta_gate() const {
        return _metagate;
    }

    InetSocket *create(SocketType type, uint8_t protocol = 0);
    Errors::Code bind(int sd, IpAddr addr, uint16_t port);
    Errors::Code listen(int sd);
    Errors::Code connect(int sd, IpAddr addr, uint16_t port);
    Errors::Code close(int sd);

private:
    SendGate _metagate;
};

}
