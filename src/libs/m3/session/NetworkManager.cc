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

#include <m3/com/GateStream.h>
#include <m3/session/NetworkManager.h>
#include <base/Heap.h>
#include <m3/stream/Standard.h>

namespace m3 {

NetDirectPipe::NetDirectPipe(capsel_t caps, size_t size, bool create)
    : _keep_caps(!create), _caps(caps), _size(size),
      _rgate(create ? RecvGate::create(caps, nextlog2<MSG_BUF_SIZE>::val, nextlog2<MSG_SIZE>::val)
                    : RecvGate::bind(caps, nextlog2<MSG_BUF_SIZE>::val)),
      _mem(create ? MemGate::create_global(size, MemGate::RW, caps + 1)
                  : MemGate::bind(caps + 1, MemGate::RW)),
      _sgate(create ? SendGate::create(&_rgate, 0, CREDITS, nullptr, caps + 2)
                    : SendGate::bind(caps + 2, &_rgate)),
      _reader(nullptr), _writer(nullptr) {
    assert(Math::is_aligned(size, DTU_PKG_SIZE));
}

NetDirectPipe::~NetDirectPipe() {
    delete _reader;
    _reader = nullptr;
    delete _writer;
    _writer = nullptr;
    if(!_keep_caps)
        VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _caps, 3));
}

DirectPipeReader *NetDirectPipe::reader() {
    if(!_reader) {
        DirectPipeReader::State *rstate = new DirectPipeReader::State(_caps);
        _reader = new DirectPipeReader(_caps, rstate);
    }
    return _reader;
}

DirectPipeWriter *NetDirectPipe::writer() {
    if(!_writer) {
        DirectPipeWriter::State *wstate = new DirectPipeWriter::State(_caps + 1, _size);
        _writer = new DirectPipeWriter(_caps + 1, _size, wstate);
    }
    return _writer;
}

InetSocket::InetSocket(int sd, NetworkManager &nm)
    : _sd(sd), _nm(nm), _recv_pipe(nullptr), _send_pipe(nullptr) {
}

InetSocket::~InetSocket() {
    delete _recv_pipe;
    _recv_pipe = nullptr;
    delete _send_pipe;
    _send_pipe = nullptr;
}

Errors::Code InetSocket::bind(IpAddr addr, uint16_t port) {
    return _nm.bind(sd(), addr, port);
}

Errors::Code InetSocket::listen() {
    return _nm.listen(sd());
}

Errors::Code InetSocket::connect(IpAddr addr, uint16_t port) {
    return _nm.connect(sd(), addr, port);
}

ssize_t InetSocket::send(const void *buffer, size_t count, bool blocking) {
    // TODO: Verify socket is in an appropriate state
    // return _send_pipe->writer()->write(buffer, count, blocking);
    return sendto(buffer, count, IpAddr(), 0, blocking);
}

// TODO: When end of pipe buffer is reached, unblocking mode fails, cause it sends only a part of the message, and then returns -1.
ssize_t InetSocket::sendto(const void *buffer, size_t count, IpAddr addr, uint16_t port, bool blocking) {
    // The write of header and data needs to be an "atomic" action
    size_t size = MessageHeader::serialize_length() + count;
    uint8_t *buf = static_cast<uint8_t*>(Heap::alloc(size));
    Marshaller m(buf, MessageHeader::serialize_length());
    MessageHeader hdr(addr, port, count);
    hdr.serialize(m);
    // cout << "InetSocket::sendto: total " << m.total() << " " << MessageHeader::serialize_length();
    assert(m.total() == MessageHeader::serialize_length());
    memcpy(buf + MessageHeader::serialize_length(), buffer, count);
    ssize_t write_size = _send_pipe->writer()->write(buf, size, blocking);
    Heap::free(buf);
    assert((!blocking && write_size == -1) || write_size == static_cast<ssize_t>(size));
    return write_size != -1 ? write_size - static_cast<ssize_t>(MessageHeader::serialize_length()) : -1;
}

ssize_t InetSocket::recv(void* buffer, size_t count, bool blocking) {
    // TODO: Verify socket is in an appropriate state
    // return _recv_pipe->reader()->read(buffer, count, blocking);
    return recvmsg(buffer, count, nullptr, nullptr, blocking);
}

ssize_t InetSocket::recvmsg(void *buffer, size_t count, IpAddr *addr, uint16_t *port, bool blocking) {
    uint8_t buf[MessageHeader::serialize_length()];
    MessageHeader hdr;
    if(_recv_pipe->reader()->read(buf, sizeof(buf), blocking) == -1)
        return -1;
    // cout << "recvmsg: Read header...\n";
    Unmarshaller um(buf, sizeof(buf));
    hdr.unserialize(um);
    if(addr)
        *addr = hdr.addr;
    if(port)
        *port = hdr.port;
    size_t msg_size = Math::min(hdr.size, count);
    // cout << "recvmsg: hdr.size=" << hdr.size << ", msg_size=" << msg_size << "\n";
    ssize_t read_size = _recv_pipe->reader()->read(buffer, msg_size, blocking);
    // cout << "recvmsg: read_size=" << read_size << "\n";
    assert(static_cast<ssize_t>(msg_size) == read_size);
    if(read_size != -1) {
        // discard excess bytes that do not fit into the supplied buffer
        if(hdr.size > static_cast<size_t>(read_size))
            _recv_pipe->reader()->read(nullptr, hdr.size - static_cast<size_t>(read_size), blocking);
    }
    return read_size;
}

Errors::Code InetSocket::close() {
    return _nm.close(sd());
}

InetSocket* NetworkManager::create(SocketType type, uint8_t protocol) {
    GateIStream reply = send_receive_vmsg(_metagate, CREATE, type, protocol);
    reply >> Errors::last;
    if(Errors::last == Errors::NONE) {
        int sd;
        reply >> sd;

        KIF::ExchangeArgs args;
        args.count = 1;
        args.vals[0] = static_cast<xfer_t>(sd);

        KIF::CapRngDesc caps = obtain(6, &args);
        InetSocket *socket = new InetSocket(sd, *this);
        socket->_recv_pipe = new NetDirectPipe(caps.start(), NetDirectPipe::BUFFER_SIZE, false);
        socket->_send_pipe = new NetDirectPipe(caps.start() + 3, NetDirectPipe::BUFFER_SIZE, false);
        return socket;
    }
    return nullptr;
}

Errors::Code NetworkManager::bind(int sd, IpAddr addr, uint16_t port) {
    GateIStream reply = send_receive_vmsg(_metagate, BIND, sd, addr.addr(), port);
    reply >> Errors::last;
    return Errors::last;
}

Errors::Code NetworkManager::listen(int sd) {
    GateIStream reply = send_receive_vmsg(_metagate, LISTEN, sd);
    reply >> Errors::last;
    return Errors::last;
}

Errors::Code NetworkManager::connect(int sd, IpAddr addr, uint16_t port) {
    GateIStream reply = send_receive_vmsg(_metagate, CONNECT, sd, addr.addr(), port);
    reply >> Errors::last;
    return Errors::last;
}

Errors::Code NetworkManager::close(int sd) {
    GateIStream reply = send_receive_vmsg(_metagate, CLOSE, sd);
    reply >> Errors::last;
    return Errors::last;
}

}
