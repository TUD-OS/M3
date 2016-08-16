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

#include <base/Common.h>
#include <base/stream/IOSBase.h>
#include <base/stream/OStringStream.h>
#include <base/util/String.h>
#include <base/util/Chars.h>
#include <assert.h>

namespace m3 {

/**
 * The input-stream is used to read formatted input from various sources. Subclasses have
 * to implement the method to actually read a character. This class provides the higher-level
 * stuff around it.
 */
class IStream : public virtual IOSBase {
public:
    explicit IStream() : IOSBase() {
    }
    virtual ~IStream() {
    }

    /**
     * No cloning
     */
    IStream(const IStream&) = delete;
    IStream &operator=(const IStream&) = delete;

    /**
     * Reads a value out of the stream and stores it in <val>.
     *
     * @param val the value
     * @return *this
     */
    IStream & operator>>(char &val) {
        val = read();
        return *this;
    }
    IStream & operator>>(uchar &val) {
        return read_unsigned(val);
    }
    IStream & operator>>(ushort &val) {
        return read_unsigned(val);
    }
    IStream & operator>>(short &val) {
        return read_signed(val);
    }
    IStream & operator>>(uint &val) {
        return read_unsigned(val);
    }
    IStream & operator>>(int &val) {
        return read_signed(val);
    }
    IStream & operator>>(ulong &val) {
        return read_unsigned(val);
    }
    IStream & operator>>(long &val) {
        return read_signed(val);
    }
    IStream & operator>>(ullong &val) {
        return readu<ullong>(val);
    }
    IStream & operator>>(llong &val) {
        return readn<llong>(val);
    }
    IStream & operator>>(float &val) {
        return read_float(val);
    }

    /**
     * Reads a string until the next newline is found.
     *
     * @param str will be set to the read string
     * @return *this
     */
    IStream & operator>>(String &str);

    /**
     * Reads a string into <buffer> until <delim> is found or <max> characters have been stored
     * to <buffer>, including '\0'.
     *
     * @param buffer the destination
     * @param max the maximum number of bytes to store to <buffer>
     * @param delim the delimited ('\n' by default)
     * @return the number of written characters (excluding '\0')
     */
    size_t getline(char *buffer, size_t max, char delim = '\n');

    /**
     * Reads one character from the stream.
     *
     * @return the character
     */
    virtual char read() = 0;

    /**
     * Puts <c> back into the stream. This is guaranteed to work at least once after one character
     * has been read, if the same character is put back. In other cases, it might fail, depending
     * on the backend.
     *
     * @param c the character to put back
     * @return true on success
     */
    virtual bool putback(char c) = 0;

private:
    template<typename T>
    IStream &read_unsigned(T &u) {
        ulong tmp;
        readu<ulong>(tmp);
        u = tmp;
        return *this;
    }

    template<typename T>
    IStream &read_signed(T &n) {
        long tmp;
        readn<long>(tmp);
        n = tmp;
        return *this;
    }

    void skip_whitespace() {
        char c = read();
        if(good()) {
            while(good() && Chars::isspace(c))
                c = read();
            putback(c);
        }
    }

    template<typename T>
    IStream &readu(T &u) {
        uint base = 10;
        skip_whitespace();
        u = 0;
        char c = read();
        if(c == '0' && ((c = read()) == 'x' || c == 'X')) {
            base = 16;
            c = read();
        }
        // ensure that we consume at least one character
        if(Chars::isdigit(c) || (base == 16 && Chars::isxdigit(c))) {
            do {
                if(c >= 'a' && c <= 'f')
                    u = u * base + c + 10 - 'a';
                else if(c >= 'A' && c <= 'F')
                    u = u * base + c + 10 - 'A';
                else
                    u = u * base + c - '0';
                c = read();
            }
            while(Chars::isdigit(c) || (base == 16 && Chars::isxdigit(c)));
            putback(c);
        }
        return *this;
    }

    template<typename T>
    IStream &readn(T &n) {
        bool neg = false;
        skip_whitespace();

        char c = read();
        if(c == '-' || c == '+') {
            neg = c == '-';
            c = read();
        }

        n = 0;
        if(Chars::isdigit(c)) {
            do {
                n = n * 10 + c - '0';
                c = read();
            }
            while(Chars::isdigit(c));
            putback(c);
        }

        // switch sign?
        if(neg)
            n = -n;
        return *this;
    }

    IStream &read_float(float &f);
};

}
