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

#include <base/Common.h>
#include <base/col/DList.h>
#include <base/col/SList.h>
#include <base/util/Profile.h>

#include <m3/stream/Standard.h>

using namespace m3;

struct MySItem : public SListItem {
    explicit MySItem(uint32_t _val) : val(_val) {
    }

    uint32_t val;
};

struct MyDItem : public DListItem {
    explicit MyDItem(uint32_t _val) : val(_val) {
    }

    uint32_t val;
};

static void bench_slist_append() {
    cycles_t total = 0;
    for(int j = 0; j < 10; ++j) {
        cycles_t start = Profile::start();

        SList<MySItem> list;
        for(uint32_t i = 0; i < 100; ++i) {
            list.append(new MySItem(i));
        }

        cycles_t end = Profile::stop();
        total += end - start;

        for(auto it = list.begin(); it != list.end(); ) {
            auto old = it++;
            delete &*old;
        }
    }

    cout << "[slist] Append 100 elements: " << (total / 10) << "\n";
}

static void bench_slist_clear() {
    cycles_t total = 0;
    for(int j = 0; j < 10; ++j) {
        SList<MySItem> list;
        for(uint32_t i = 0; i < 100; ++i) {
            list.append(new MySItem(i));
        }

        cycles_t start = Profile::start();
        for(auto it = list.begin(); it != list.end(); ) {
            auto old = it++;
            delete &*old;
        }
        cycles_t end = Profile::stop();
        total += end - start;
    }

    cout << "[slist] Clear 100-element list: " << (total / 10) << "\n";
}

static void bench_dlist_append() {
    cycles_t total = 0;
    for(int j = 0; j < 10; ++j) {
        cycles_t start = Profile::start();
        DList<MyDItem> list;
        for(uint32_t i = 0; i < 100; ++i) {
            list.append(new MyDItem(i));
        }
        cycles_t end = Profile::stop();
        total += end - start;

        for(auto it = list.begin(); it != list.end(); ) {
            auto old = it++;
            delete &*old;
        }
    }

    cout << "[dlist] Append 100 elements: " << (total / 10) << "\n";
}

static void bench_dlist_clear() {
    cycles_t total = 0;
    for(int j = 0; j < 10; ++j) {
        DList<MyDItem> list;
        for(uint32_t i = 0; i < 100; ++i) {
            list.append(new MyDItem(i));
        }

        cycles_t start = Profile::start();
        for(auto it = list.begin(); it != list.end(); ) {
            auto old = it++;
            delete &*old;
        }
        cycles_t end = Profile::stop();
        total += end - start;
    }

    cout << "[dlist] Clear 100-element list: " << (total / 10) << "\n";
}

int main() {
    bench_dlist_append();
    bench_dlist_clear();
    bench_slist_append();
    bench_slist_clear();
    return 0;
}
