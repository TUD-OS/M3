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
#include <base/col/Treap.h>
#include <base/util/Profile.h>
#include <base/Panic.h>

#include <m3/stream/Standard.h>

#include "../cppbench.h"

using namespace m3;

struct MyTItem : public TreapNode<MyTItem, uint32_t> {
    explicit MyTItem(uint32_t _val) : TreapNode(_val), val(_val) {
    }

    uint32_t val;
};

NOINLINE static void insert() {
    struct TreapInsertRunner : public Runner {
        void run() override {
            for(uint32_t i = 0; i < 100; ++i) {
                treap.insert(new MyTItem(i));
            }
        }
        void post() override {
            MyTItem *item;
            while((item = treap.remove_root()) != nullptr) {
                delete item;
            }
        }

        Treap<MyTItem> treap;
    };

    Profile pr(30);
    TreapInsertRunner runner;
    cout << "100-elements: " << pr.runner_with_id(runner, 0x03) << "\n";
}

NOINLINE static void find() {
    struct TreapSearchRunner : public Runner {
        void pre() override {
            for(uint32_t i = 0; i < 100; ++i) {
                treap.insert(new MyTItem(i));
            }
        }
        void run() override {
            for(uint32_t i = 0; i < 100; ++i) {
                MyTItem *item = treap.find(i);
                if(!item || item->val != i)
                    PANIC("Test failed: " << (item ? item->val : 0) << " != " << i);
            }
        }
        void post() override {
            MyTItem *item;
            while((item = treap.remove_root()) != nullptr) {
                delete item;
            }
        }

        Treap<MyTItem> treap;
    };

    Profile pr(30);
    TreapSearchRunner runner;
    cout << "100-elements: " << pr.runner_with_id(runner, 0x02) << "\n";
}

NOINLINE static void clear() {
    struct TreapClearRunner : public Runner {
        void pre() override {
            for(uint32_t i = 0; i < 100; ++i) {
                treap.insert(new MyTItem(i));
            }
        }
        void run() override {
            MyTItem *item;
            while((item = treap.remove_root()) != nullptr) {
                delete item;
            }
        }

        Treap<MyTItem> treap;
    };

    Profile pr(30);
    TreapClearRunner runner;
    cout << "100-elements: " << pr.runner_with_id(runner, 0x01) << "\n";
}

void btreap() {
    RUN_BENCH(insert);
    RUN_BENCH(find);
    RUN_BENCH(clear);
}
