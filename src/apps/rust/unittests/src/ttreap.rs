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

use m3::col::{Treap, Vec};
use m3::test;

pub fn run(t: &mut test::Tester) {
    run_test!(t, test_in_order);
    run_test!(t, test_rev_order);
    run_test!(t, test_rand_order);
}

const TEST_NODE_COUNT: u32 = 10;

fn test_in_order() {
    let vals = (0..TEST_NODE_COUNT).collect::<Vec<u32>>();
    test_add_and_rem(&vals);
}

fn test_rev_order() {
    let vals = (0..TEST_NODE_COUNT).rev().collect::<Vec<u32>>();
    test_add_and_rem(&vals);
}

fn test_rand_order() {
    let vals = [1, 6, 2, 3, 8, 9, 7, 5, 4];
    test_add_and_rem(&vals);
}

fn test_add_and_rem(vals: &[u32]) {
    let mut treap = Treap::new();

    // create
    for v in vals {
        treap.insert(v.clone(), v.clone());
    }

    // find all
    for v in vals {
        let val = treap.get(&v);
        assert_eq!(val, Some(v));
    }

    // remove
    for v in vals {
        let val = treap.remove(&v);
        assert_eq!(val, Some(*v));
        assert_eq!(treap.get(&v), None);
    }
}
