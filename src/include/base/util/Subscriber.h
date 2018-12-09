/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/col/SList.h>
#include <functional>

namespace m3 {

template<typename T>
struct Subscriber : public SListItem {
    using callback_type = std::function<void(T,Subscriber<T>*)>;

    callback_type callback;

    explicit Subscriber(const callback_type &cb) : callback(cb) {
    }
};

template<typename T>
class Subscriptions {
public:
    using callback_type = typename Subscriber<T>::callback_type;
    using iterator      = typename SList<Subscriber<T>>::iterator;

    explicit Subscriptions() : _list() {
    }

    size_t subscribers() const {
        return _list.length();
    }
    iterator begin() {
        return _list.begin();
    }
    iterator end() {
        return _list.end();
    }

    Subscriber<T> *subscribe(const callback_type &callback) {
        Subscriber<T> *s = new Subscriber<T>(callback);
        _list.append(s);
        return s;
    }
    void unsubscribe(Subscriber<T> *s) {
        _list.remove(s);
        delete s;
    }

protected:
    SList<Subscriber<T>> _list;
};

}
