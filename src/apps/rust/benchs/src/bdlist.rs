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

use m3::col::DList;
use m3::profile;
use m3::test;

pub fn run(t: &mut test::Tester) {
    run_test!(t, push_back);
    run_test!(t, push_front);
    run_test!(t, clear);
}

fn push_back() {
    let mut prof = profile::Profiler::new().repeats(30);

    #[derive(Default)]
    struct ListTester(DList<u32>);

    impl profile::Runner for ListTester {
        fn pre(&mut self) {
            self.0.clear();
        }
        fn run(&mut self) {
            for i in 0..100 {
                self.0.push_back(i);
            }
        }
    }

    println!("Appending 100 elements: {}", prof.runner_with_id(&mut ListTester::default(), 0x50));
}

fn push_front() {
    let mut prof = profile::Profiler::new().repeats(30);

    #[derive(Default)]
    struct ListTester(DList<u32>);

    impl profile::Runner for ListTester {
        fn pre(&mut self) {
            self.0.clear();
        }
        fn run(&mut self) {
            for i in 0..100 {
                self.0.push_front(i);
            }
        }
    }

    println!("Prepending 100 elements: {}", prof.runner_with_id(&mut ListTester::default(), 0x51));
}

fn clear() {
    let mut prof = profile::Profiler::new().repeats(30);

    #[derive(Default)]
    struct ListTester(DList<u32>);

    impl profile::Runner for ListTester {
        fn pre(&mut self) {
            for i in 0..100 {
                self.0.push_back(i);
            }
        }
        fn run(&mut self) {
            self.0.clear();
        }
    }

    println!("Clearing 100-element list: {}", prof.runner_with_id(&mut ListTester::default(), 0x52));
}
