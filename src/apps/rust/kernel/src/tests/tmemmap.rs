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

use base::col::Vec;
use base::errors::Code;
use base::goff;
use base::profile;
use base::test;

use mem::MemMap;

pub fn run(t: &mut test::Tester) {
    run_test!(t, basics);
    run_test!(t, perf_alloc);
    run_test!(t, perf_free);
}

fn basics() {
    let mut m = MemMap::new(0, 0x1000);

    assert_eq!(m.allocate(0x100, 0x10), Ok(0x0));
    assert_eq!(m.allocate(0x100, 0x10), Ok(0x100));
    assert_eq!(m.allocate(0x100, 0x10), Ok(0x200));

    m.free(0x100, 0x100);
    m.free(0x0, 0x100);

    assert_err!(m.allocate(0x1000, 0x10), Code::NoSpace);
    assert_eq!(m.allocate(0x200, 0x10), Ok(0x0));

    m.free(0x200, 0x100);
    m.free(0x0, 0x200);

    assert_eq!(m.size(), (0x1000, 1));
}

fn perf_alloc() {
    let mut prof = profile::Profiler::new().repeats(10);

    struct MemMapTester {
        map: MemMap,
    }

    impl profile::Runner for MemMapTester {
        fn pre(&mut self) {
            self.map = MemMap::new(0, 0x1000000);
        }
        fn run(&mut self) {
            for _ in 0..100 {
                assert_ok!(self.map.allocate(0x1000, 0x1000));
            }
        }
    }

    let mut tester = MemMapTester {
        map: MemMap::new(0, 0x100000),
    };

    klog!(DEF, "Allocating 100 areas: {}", prof.runner_with_id(&mut tester, 0x10));
}

fn perf_free() {
    let mut prof = profile::Profiler::new().repeats(10);

    struct MemMapTester {
        map: MemMap,
        addrs: Vec<goff>,
        forward: bool,
    }

    impl profile::Runner for MemMapTester {
        fn pre(&mut self) {
            self.map = MemMap::new(0, 0x100000);
            self.addrs.clear();
            for _ in 0..100 {
                self.addrs.push(assert_ok!(self.map.allocate(0x1000, 0x1000)));
            }
        }
        fn run(&mut self) {
            for i in 0..100 {
                let idx = if self.forward { i } else { 100 - i - 1 };
                self.map.free(self.addrs[idx], 0x1000);
            }
            assert_eq!(self.map.size(), (0x100000, 1));
        }
    }

    let mut tester = MemMapTester {
        map: MemMap::new(0, 0x100000),
        addrs: Vec::new(),
        forward: true,
    };
    klog!(DEF, "Freeing 100 areas forward  : {}", prof.runner_with_id(&mut tester, 0x11));

    tester.forward = false;
    klog!(DEF, "Freeing 100 areas backwards: {}", prof.runner_with_id(&mut tester, 0x12));
}
