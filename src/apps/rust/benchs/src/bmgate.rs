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

use m3::com::MemGate;
use m3::kif;
use m3::profile;
use m3::test;

const SIZE: usize = 2 * 1024 * 1024;

pub fn run(t: &mut test::Tester) {
    run_test!(t, read);
    run_test!(t, write);
}

fn read() {
    let mut buf = vec![0u8; 8192];
    let mgate = MemGate::new(8192, kif::Perm::R).expect("Unable to create mgate");

    let mut prof = profile::Profiler::new().repeats(2).warmup(1);

    println!("2 MiB with 8K buf: {}", prof.run_with_id(|| {
        let mut total = 0;
        while total < SIZE {
            mgate.read(&mut buf, 0).expect("Reading failed");
            total += buf.len();
        }
    }, 0x30));
}

fn write() {
    let buf = vec![0u8; 8192];
    let mgate = MemGate::new(8192, kif::Perm::W).expect("Unable to create mgate");

    let mut prof = profile::Profiler::new().repeats(2).warmup(1);

    println!("2 MiB with 8K buf: {}", prof.run_with_id(|| {
        let mut total = 0;
        while total < SIZE {
            mgate.write(&buf, 0).expect("Writing failed");
            total += buf.len();
        }
    }, 0x31));
}
