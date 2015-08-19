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

#include <m3/util/SList.h>
#include <m3/DTU.h>

namespace m3 {

class WorkLoop;

class WorkItem : public SListItem {
    friend class WorkLoop;
public:
    virtual ~WorkItem() {
    }

    virtual void work() = 0;
};

class WorkLoop {
    static const size_t MAX_ITEMS   = 8;

    explicit WorkLoop() : _changed(false), _permanents(0), _count(), _items() {
    }

public:
    static WorkLoop &get() {
        return _inst;
    }

    bool has_items() const {
        return _count > _permanents;
    }

    void add(WorkItem *item, bool permanent) {
        assert(_count < MAX_ITEMS);
        _items[_count++] = item;
        if(permanent)
            _permanents++;
    }
    void remove(WorkItem *item) {
        for(size_t i = 0; i < MAX_ITEMS; ++i) {
            if(_items[i] == item) {
                _items[i] = nullptr;
                for(++i; i < MAX_ITEMS; ++i)
                    _items[i - 1] = _items[i];
                _count--;
                break;
            }
        }
    }

    void run() {
        while(_count > _permanents) {
            for(size_t i = 0; i < _count; ++i)
                _items[i]->work();
            DTU::get().wait();
        }
    }
    void stop() {
        _permanents = _count;
    }

private:
    bool _changed;
    uint _permanents;
    size_t _count;
    WorkItem *_items[MAX_ITEMS];
    static WorkLoop _inst;
};

}
