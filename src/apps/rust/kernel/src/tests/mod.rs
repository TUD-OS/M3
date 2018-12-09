/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

use base::test::Tester;
use base::heap;

mod tmemmap;

struct MyTester {
}

impl Tester for MyTester {
    fn run_suite(&mut self, name: &str, f: &Fn(&mut Tester)) {
        klog!(DEF, "Running test suite {} ...", name);
        f(self);
        klog!(DEF, "Done\n");
    }

    fn run_test(&mut self, name: &str, f: &Fn()) {
        klog!(DEF, "-- Running test {} ...", name);
        let free_mem = heap::free_memory();
        f();
        assert_eq!(heap::free_memory(), free_mem);
        klog!(DEF, "-- Done");
    }
}

pub fn run() {
    let mut tester = MyTester {};
    run_suite!(tester, tmemmap::run);
}
