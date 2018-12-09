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

use m3::col::String;
use m3::test;
use m3::io::{Read, Write};
use m3::vfs::{BufReader, BufWriter, OpenFlags, VFS};

pub fn run(t: &mut test::Tester) {
    run_test!(t, read_write);
}

fn read_write() {
    {
        let file = assert_ok!(VFS::open("/myfile", OpenFlags::CREATE | OpenFlags::W));
        let mut bfile = BufWriter::new(file);

        assert_ok!(write!(bfile, "This {:.3} is the {}th test of {:#0X}!\n", "foobar", 42, 0xABCDEF));
    }

    {
        let file = assert_ok!(VFS::open("/myfile", OpenFlags::R));
        let mut bfile = BufReader::new(file);

        let mut s = String::new();
        assert_eq!(bfile.read_to_string(&mut s), Ok(39));
        assert_eq!(s, "This foo is the 42th test of 0xABCDEF!\n");
    }
}