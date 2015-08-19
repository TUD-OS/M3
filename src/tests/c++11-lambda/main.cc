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

#include <iostream>
#include <functional>

using namespace std;

using myfunc = void (*)(int);

myfunc funcs[1];
function<void(int)> funcs2[1];

static void test1() {
    int x;
    cin >> x;
    funcs[0] = [] (int i) { cout << "Hello world: " << i << endl; };
    funcs[0](x);
}

static void test2() {
    int x;
    cin >> x;
    cout << "before: " << &x << endl;
    funcs2[0] = [&] (int) { cout << "Hello world: " << &x << endl; };
    funcs2[0](0);
}

static void test3() {
    int x;
    cin >> x;
    cout << "before: " << &x << endl;
    funcs2[0] = [=] (int) { cout << "Hello world: " << &x << endl; };
    funcs2[0](0);
}

struct A {
    A() {
    }
    A(const A&) {
        cout << "Copied A" << endl;
    }
    A(A&&) = delete;
    A &operator=(A&&) = delete;
    A &operator=(const A&) = delete;
};

static void test4() {
    int i = 4;
    A *s = new A();
    cout << "Initial: &i=" << &i << ", &s=" << &s << endl;
    auto fn1 = [=] () {
        cout << "Closure: s=" << s << ", &s=" << &s << ", &i=" << &i << endl;
    };
    fn1();
    auto fn2 = [&] () {
        cout << "Closure: s=" << s << ", &s=" << &s << ", &i=" << &i << endl;
    };
    fn2();
    A &a = *s;
    cout << "Initial: &a=" << &a << endl;
    auto fn3 = [=] () {
        cout << "Closure: &a=" << &a << endl;
    };
    fn3();
    auto fn4 = [&] () {
        cout << "Closure: &a=" << &a << endl;
    };
    fn4();
}

int main() {
    test1();
    test2();
    test3();
    test4();
    return 0;
}
