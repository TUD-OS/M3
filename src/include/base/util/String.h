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
#include <cstring>

namespace m3 {

class OStream;

class String {
public:
    static const size_t DEFAULT_MAX_LEN = 64;

    /**
     * Constructor. Creates an empty string (without allocation on the heap)
     */
    explicit String() : _str(0), _len() {
    }
    /**
     * Constructor. Copies the given string onto the heap.
     *
     * @param str the string
     * @param len the length of the string (-1 by default, which means: use strlen())
     */
    String(const char *str, size_t len = static_cast<size_t>(-1))
        : _str(), _len() {
        if(str)
            init(str, len);
    }

    explicit String(const String& s)
        : _str(), _len() {
        init(s._str, s._len);
    }
    String(String &&s) : _str(s._str), _len(s._len) {
        s._str = nullptr;
    }
    String & operator=(const String& s) {
        if(&s != this)
            reset(s._str, s._len);
        return *this;
    }

    ~String() {
        delete[] _str;
    }

    /**
     * @param i the index
     * @return the <i>th character of the string.
     */
    char operator[](size_t i) const {
        return _str[i];
    }
    /**
     * Gives you access to a character. But keep in mind: there are no checks and the string
     * length can NOT be changed in this way!
     *
     * @param i the index
     * @return a reference to the <i>th character of the string.
     */
    char &operator[](size_t i) {
        return _str[i];
    }

    /**
     * @return the string (always null-terminated)
     */
    const char *c_str() const {
        return _str ? _str : "";
    }
    /**
     * @return the length of the string
     */
    size_t length() const {
        return _len;
    }
    /**
     * @return true if <this> contains <str>
     */
    bool contains(const String &str) const {
        if(!_str || !str._str)
            return false;
        return strstr(_str, str._str) != nullptr;
    }

    /**
     * Resets the string to the given one. That is, it free's the current string and copies
     * the given one into a new place on the heap
     *
     * @param str the string
     * @param len the length of the string (-1 by default, which means: use strlen())
     */
    void reset(const char *str, size_t len = static_cast<size_t>(-1)) {
        delete[] _str;
        init(str, len);
    }

private:
    void init(const char *str, size_t len) {
        _len = len == static_cast<size_t>(-1) ? strlen(str) : len;
        if(_len > 0) {
            _str = new char[_len + 1];
            memcpy(_str, str, _len);
            _str[_len] = '\0';
        }
        else
            _str = nullptr;
    }

    char *_str;
    size_t _len;
};

/**
 * @return true if s1 and s2 are equal
 */
static inline bool operator==(const String &s1, const String &s2) {
    return s1.length() == s2.length() && strcmp(s1.c_str(), s2.c_str()) == 0;
}
/**
 * @return true if s1 and s2 are not equal
 */
static inline bool operator!=(const String &s1, const String &s2) {
    return !operator==(s1, s2);
}

/**
 * Writes the string into the given output-stream
 *
 * @param os the stream
 * @param str the string
 * @return the stream
 */
OStream &operator<<(OStream &os, const String &str);

}
