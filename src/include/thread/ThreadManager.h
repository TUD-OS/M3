/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Copyright (C) 2016, Matthias Hille <matthias.hille@tu-dresden.de>
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

#include <thread/Thread.h>

#include <base/log/Lib.h>

namespace m3 {

class ThreadManager {
    friend class Thread;

public:
    static ThreadManager &get() {
        return inst;
    }

    Thread *current() {
        return _current;
    }
    size_t thread_count() const {
        return _ready.length() + _blocked.length() + _sleep.length();
    }
    size_t ready_count() const {
        return _ready.length();
    }
    size_t sleeping_count() const {
        return _sleep.length();
    }
    const unsigned char *get_current_msg() const {
        return _current->get_msg();
    }

    void wait_for(void *event) {
        assert(_sleep.length() > 0);
        _current->subscribe(event);
        _blocked.append(_current);
        LLOG(THREAD, "Thread " << _current->id() << " waits for " << event);
        if(_ready.length())
            switch_to(_ready.remove_first());
        else
            switch_to(_sleep.remove_first());
    }

    void yield() {
        if(_ready.length()) {
            _sleep.append(_current);
            switch_to(_ready.remove_first());
        }
    }

    void notify(void *event, const void *msg = nullptr, size_t size = 0) {
        assert(size <= Thread::MAX_MSG_SIZE);
        for(auto it = _blocked.begin(); it != _blocked.end(); ) {
            auto old = it++;
            if(old->trigger_event(event)) {
                Thread* t = &(*old);
                t->set_msg(msg, size);
                LLOG(THREAD, "Waking up thread " << t->id() << " for event " << event);
                _blocked.remove(t);
                _ready.append(t);
            }
        }
    }

    void stop() {
        assert(_sleep.length() > 0 || _ready.length() > 0);
        LLOG(THREAD, "Stopping thread " << _current->id());
        if(_ready.length())
            switch_to(_ready.remove_first());
        else
            switch_to(_sleep.remove_first());
    }

private:
    explicit ThreadManager() : _current(), _ready(), _blocked(), _sleep() {
        _current = new Thread();
    }

    void add(Thread *t) {
        _sleep.append(t);
    }
    void remove(Thread *t) {
        _ready.remove(t);
        _blocked.remove(t);
        _sleep.remove(t);
    }

    void switch_to(Thread *t) {
        LLOG(THREAD, "Switching from " << _current->id() << " to " << t->id());
        if(!_current->save()) {
            _current = t;
            _current->resume();
        }
    }

    Thread *_current;
    m3::SList<Thread> _ready;
    m3::SList<Thread> _blocked;
    m3::SList<Thread> _sleep;
    static ThreadManager inst;
};

}