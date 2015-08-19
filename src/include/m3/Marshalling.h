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

#pragma once

#include <m3/util/String.h>
#include <m3/util/Math.h>
#include <m3/DTU.h>
#include <assert.h>

namespace m3 {

class Unmarshaller;

/**
 * The marshaller puts values into a buffer, which is for example used by GateOStream.
 */
class Marshaller {
public:
    explicit Marshaller(unsigned char *bytes, size_t total)
        : _bytecount(0), _bytes(bytes), _total(total) {
    }

    Marshaller(const Marshaller &) = default;
    Marshaller &operator=(const Marshaller &) = default;

    /**
     * @return the total number of bytes of the data
     */
    size_t total() const {
        return Math::round_up(_bytecount, DTU_PKG_SIZE);
    }
    /**
     * @return the bytes of the data
     */
    const unsigned char *bytes() const {
        return _bytes;
    }

    /**
     * Puts the given values into this Marshaller.
     *
     * @param val the first value
     * @param args the other values
     */
    template<typename T, typename... Args>
    void vput(const T &val, const Args &... args) {
        *this << val;
        vput(args...);
    }

    /**
     * Puts the given value into this Marshaller.
     *
     * @param value the value
     * @return *this
     */
    template<typename T>
    Marshaller & operator<<(const T& value) {
        assert(fits(_bytecount, sizeof(T)));
        *reinterpret_cast<T*>(_bytes + _bytecount) = value;
        _bytecount += Math::round_up(sizeof(T), sizeof(ulong));
        return *this;
    }
    Marshaller & operator<<(const char *value) {
        return put_str(value, strlen(value));
    }
    Marshaller & operator<<(const String& value) {
        return put_str(value.c_str(), value.length());
    }

    /**
     * Puts all remaining items (the ones that haven't been read yet) of <is> into this Marshaller.
     *
     * @param is the GateIStream
     * @return *this
     */
    void put(const Unmarshaller &is);
    /**
     * Puts all items of <os> into this Marshaller.
     *
     * @param os the Marshaller
     * @return *this
     */
    void put(const Marshaller &os);

protected:
    Marshaller & put_str(const char *value, size_t len) {
        assert(fits(_bytecount, len + sizeof(size_t)));
        unsigned char *start = const_cast<unsigned char*>(bytes());
        *reinterpret_cast<size_t*>(start + _bytecount) = len;
        memcpy(start + _bytecount + sizeof(size_t), value, len);
        _bytecount += Math::round_up(len + sizeof(size_t), sizeof(ulong));
        return *this;
    }

    // needed as recursion-end
    void vput() {
    }
    bool fits(size_t current, size_t bytes) {
        return current + bytes <= _total;
    }

    size_t _bytecount;
    unsigned char *_bytes;
    size_t _total;
};

/**
 * The unmarshaller reads values from a buffer, used e.g. in GateIStream.
 */
class Unmarshaller {
protected:
    explicit Unmarshaller() {
    }

public:
    /**
     * Creates an object to read values from the given marshalled data.
     *
     * @param data the data to unmarshall
     * @param length the length of the data
     */
    explicit Unmarshaller(const unsigned char *data, size_t length)
        : _pos(0), _length(length), _data(data) {
    }

    Unmarshaller(const Unmarshaller &) = default;
    Unmarshaller &operator=(const Unmarshaller &) = default;

    /**
     * @return the current position, i.e. the offset of the unread data
     */
    size_t pos() const {
        return _pos;
    }
    /**
     * @return the length of the data in bytes
     */
    size_t length() const {
        return _length;
    }
    /**
     * @return the remaining bytes to read
     */
    size_t remaining() const {
        return length() - _pos;
    }
    /**
     * @return the data
     */
    const unsigned char *buffer() const {
        return _data;
    }

    /**
     * Pulls the given values out of this stream
     *
     * @param val the value to write to
     * @param args the other values to write to
     */
    template<typename T, typename... Args>
    void vpull(T &val, Args &... args) {
        *this >> val;
        vpull(args...);
    }

    /**
     * Pulls a value into <value>.
     *
     * @param value the value to write to
     * @return *this
     */
    template<typename T>
    Unmarshaller & operator>>(T &value) {
        assert(_pos + sizeof(T) <= length());
        value = *reinterpret_cast<const T*>(_data + _pos);
        _pos += Math::round_up(sizeof(T), sizeof(ulong));
        return *this;
    }
    Unmarshaller & operator>>(String &value) {
        assert(_pos + sizeof(size_t) <= length());
        size_t len = *reinterpret_cast<const size_t*>(_data + _pos);
        _pos += sizeof(size_t);
        assert(_pos + len <= length());
        value.reset(reinterpret_cast<const char*>(_data + _pos), len);
        _pos += Math::round_up(len, sizeof(ulong));
        return *this;
    }

protected:
    // needed as recursion-end
    void vpull() {
    }

    size_t _pos;
    size_t _length;
    const unsigned char *_data;
};

inline void Marshaller::put(const Unmarshaller &is) {
    assert(fits(_bytecount, is.remaining()));
    memcpy(const_cast<unsigned char*>(bytes()) + _bytecount, is.buffer() + is.pos(), is.remaining());
    _bytecount += is.remaining();
}
inline void Marshaller::put(const Marshaller &os) {
    assert(fits(_bytecount, os.total()));
    memcpy(const_cast<unsigned char*>(bytes()) + _bytecount, os.bytes(), os.total());
    _bytecount += os.total();
}

}
