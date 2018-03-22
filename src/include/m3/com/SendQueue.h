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

#if defined(__host__)

#include <base/col/SList.h>
#include <base/WorkLoop.h>

#include <m3/com/SendGate.h>

namespace m3 {

class SendQueue : public WorkItem {
public:
    using del_func = void (*)(void*);

private:
    struct SendItem : public SListItem {
        explicit SendItem(SendGate &gate, void *data, size_t len, del_func deleter)
            : SListItem(),
              gate(gate),
              data(data),
              len(len),
              deleter(deleter) {
        }
        ~SendItem() {
            deleter(data);
        }

        SendGate &gate;
        void *data;
        size_t len;
        del_func deleter;
    };

    explicit SendQueue() : _queue() {
    }

public:
    static SendQueue &get() {
        return _inst;
    }

    template<class T>
    static void def_deleter(void *data) {
        delete reinterpret_cast<T*>(data);
    }
    template<class T>
    static void array_deleter(void *data) {
        delete[] reinterpret_cast<T*>(data);
    }

    template<class T>
    void send(SendGate &gate, T *data, size_t len, del_func deleter = def_deleter<T>) {
        SendItem *it = new SendItem(gate, data, len, deleter);
        _queue.append(it);
        if(_queue.length() == 1)
            send_async(*it);
    }
    size_t length() const {
        return _queue.length();
    }

    virtual void work() override;

private:
    void send_async(SendItem &it);

    SList<SendItem> _queue;
    static SendQueue _inst;
};

}

#endif
