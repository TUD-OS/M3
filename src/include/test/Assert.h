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

#include <m3/Common.h>
#include <m3/util/String.h>
#include <m3/stream/OStringStream.h>

namespace test {

#define assert_int(actual,expected) \
    this->do_assert(test::AssertEqual<int>((expected),(actual),__FILE__,__LINE__))
#define assert_long(actual,expected) \
    this->do_assert(test::AssertEqual<int>((expected),(actual),__FILE__,__LINE__))
#define assert_uint(actual,expected) \
    this->do_assert(test::AssertEqual<unsigned int>((expected),(actual),__FILE__,__LINE__))
#define assert_ulong(actual,expected) \
    this->do_assert(test::AssertEqual<unsigned long>((expected),(actual),__FILE__,__LINE__))
#define assert_size(actual,expected) \
    this->do_assert(test::AssertEqual<size_t>((expected),(actual),__FILE__,__LINE__))
#define assert_word(actual,expected) \
    this->do_assert(test::AssertEqual<word_t>((expected),(actual),__FILE__,__LINE__))
#define assert_str(actual,expected) \
    this->do_assert(test::AssertEqual<m3::String>((expected),(actual),__FILE__,__LINE__))
#define assert_true(expected) \
    this->do_assert(test::AssertTrue((expected),__FILE__,__LINE__))
#define assert_false(expected) \
    this->do_assert(test::AssertTrue(!(expected),__FILE__,__LINE__))
#define assert_float(actual,expected) \
    this->do_assert(test::AssertEqual<float>((expected),(actual),__FILE__,__LINE__))

class Assert {
public:
    explicit Assert(const m3::String& file,int line)
        : _file(file), _line(line) {
    }
    virtual ~Assert() {
    }

    const m3::String& get_file() const {
        return _file;
    }
    int get_line() const {
        return _line;
    }

    virtual m3::String get_desc() const = 0;
    virtual operator bool() const = 0;

private:
    m3::String _file;
    int _line;
};

class AssertTrue : public Assert {
public:
    explicit AssertTrue(bool val,const m3::String& file,int line)
        : Assert(file,line), _val(val) {
    }

    virtual m3::String get_desc() const override {
        m3::OStringStream os;
        os << "Expected true, got " << _val;
        return os.str();
    }
    virtual operator bool() const override {
        return _val;
    }

private:
    bool _val;
};

template<class T>
class AssertEqual : public Assert {
public:
    explicit AssertEqual(T expected,T actual,const m3::String& file,int line)
        : Assert(file,line), _expected(expected), _actual(actual) {
    }

    virtual m3::String get_desc() const override {
        m3::OStringStream os;
        os << "Expected '" << _expected << "', got '" << _actual << "'";
        return os.str();
    }
    virtual operator bool() const override {
        return _expected == _actual;
    }

private:
    const T _expected;
    const T _actual;
};

}
