/*
 * Copyright (C) 2016-2017, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

namespace m3 {

class RefCounted {
public:
    explicit RefCounted() : _refs(0) {
    }

    ulong refcount() const {
        return _refs;
    }
    void add_ref() const {
        _refs++;
    }
    bool rem_ref() const {
        return --_refs == 0;
    }

private:
    mutable ulong _refs;
};

template<class T>
class Reference {
public:
    explicit Reference() : _obj(nullptr) {
    }
    explicit Reference(T *obj) : _obj(obj) {
        attach();
    }
    Reference(const Reference<T> &r) : _obj(r._obj) {
        attach();
    }
    Reference<T> &operator=(const Reference<T> &r) {
        if(&r != this) {
            detach();
            _obj = r._obj;
            attach();
        }
        return *this;
    }
    Reference(Reference<T> &&r) : _obj(r._obj) {
        r._obj = nullptr;
    }
    ~Reference() {
        detach();
    }

    bool valid() const {
        return _obj != nullptr;
    }
    T *operator->() const {
        return _obj;
    }
    T &operator*() const {
        return *_obj;
    }
    T *get() const {
        return _obj;
    }

    void unref() {
        if(_obj && _obj->rem_ref())
            delete _obj;
        _obj = nullptr;
    }
    void forget() {
        _obj = nullptr;
    }

private:
    void attach() {
        if(_obj)
            _obj->add_ref();
    }
    void detach() {
        unref();
        _obj = nullptr;
    }

    T *_obj;
};

}
