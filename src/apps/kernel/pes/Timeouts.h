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

#include <base/col/SList.h>

#include <functional>

namespace kernel {

struct Timeout : public m3::SListItem {
    explicit Timeout(cycles_t when, std::function<void()> &&callback)
        : m3::SListItem(),
          when(when),
          callback(callback) {
    }

    cycles_t when;
    std::function<void ()> callback;
};

class Timeouts {
    explicit Timeouts() : _timeouts() {
    }

public:
    static Timeouts &get() {
        return _inst;
    }

    cycles_t sleep_time() const;

    void trigger();

    Timeout *wait_for(cycles_t cycles, std::function<void()> &&callback);

    void cancel(Timeout *to);

private:
    m3::SList<Timeout> _timeouts;
    static Timeouts _inst;
};

}
