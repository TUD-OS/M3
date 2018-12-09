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

use m3::boxed::Box;
use m3::com::MemGate;
use m3::col::String;
use m3::io::{self, Read};
use m3::kif;
use m3::test;
use m3::vfs::IndirectPipe;
use m3::vpe::{Activity, VPE, VPEArgs};

pub fn run(t: &mut test::Tester) {
    run_test!(t, child_to_parent);
    run_test!(t, parent_to_child);
    run_test!(t, child_to_child);
    run_test!(t, exec_child_to_child);
}

fn child_to_parent() {
    let pipe_mem = assert_ok!(MemGate::new(0x10000, kif::Perm::RW));
    let pipe = assert_ok!(IndirectPipe::new(&pipe_mem, 0x10000));

    let mut vpe = assert_ok!(VPE::new_with(VPEArgs::new("writer")));
    vpe.files().set(io::STDOUT_FILENO, VPE::cur().files().get(pipe.writer_fd()).unwrap());
    assert_ok!(vpe.obtain_fds());

    let act = assert_ok!(vpe.run(Box::new(|| {
        println!("This is a test!");
        0
    })));

    pipe.close_writer();

    let input = VPE::cur().files().get(pipe.reader_fd()).unwrap();
    let mut s = String::new();
    assert_eq!(input.borrow_mut().read_to_string(&mut s), Ok(16));
    assert_eq!(s, "This is a test!\n");

    assert_eq!(act.wait(), Ok(0));
}

fn parent_to_child() {
    let pipe_mem = assert_ok!(MemGate::new(0x10000, kif::Perm::RW));
    let pipe = assert_ok!(IndirectPipe::new(&pipe_mem, 0x10000));

    let mut vpe = assert_ok!(VPE::new_with(VPEArgs::new("reader")));
    vpe.files().set(io::STDIN_FILENO, VPE::cur().files().get(pipe.reader_fd()).unwrap());
    assert_ok!(vpe.obtain_fds());

    let act = assert_ok!(vpe.run(Box::new(|| {
        let mut s = String::new();
        assert_eq!(io::stdin().read_to_string(&mut s), Ok(16));
        assert_eq!(s, "This is a test!\n");
        0
    })));

    pipe.close_reader();

    let output = VPE::cur().files().get(pipe.writer_fd()).unwrap();
    assert_eq!(output.borrow_mut().write(b"This is a test!\n"), Ok(16));

    pipe.close_writer();

    assert_eq!(act.wait(), Ok(0));
}

fn child_to_child() {
    let pipe_mem = assert_ok!(MemGate::new(0x10000, kif::Perm::RW));
    let pipe = assert_ok!(IndirectPipe::new(&pipe_mem, 0x10000));

    let mut writer = assert_ok!(VPE::new_with(VPEArgs::new("writer")));
    let mut reader = assert_ok!(VPE::new_with(VPEArgs::new("reader")));
    writer.files().set(io::STDOUT_FILENO, VPE::cur().files().get(pipe.writer_fd()).unwrap());
    reader.files().set(io::STDIN_FILENO, VPE::cur().files().get(pipe.reader_fd()).unwrap());
    assert_ok!(writer.obtain_fds());
    assert_ok!(reader.obtain_fds());

    let wr_act = assert_ok!(writer.run(Box::new(|| {
        println!("This is a test!");
        0
    })));

    let rd_act = assert_ok!(reader.run(Box::new(|| {
        let mut s = String::new();
        assert_eq!(io::stdin().read_to_string(&mut s), Ok(16));
        assert_eq!(s, "This is a test!\n");
        0
    })));

    pipe.close_reader();
    pipe.close_writer();

    assert_eq!(wr_act.wait(), Ok(0));
    assert_eq!(rd_act.wait(), Ok(0));
}

fn exec_child_to_child() {
    let pipe_mem = assert_ok!(MemGate::new(0x10000, kif::Perm::RW));
    let pipe = assert_ok!(IndirectPipe::new(&pipe_mem, 0x10000));

    let mut writer = assert_ok!(VPE::new_with(VPEArgs::new("writer")));
    let mut reader = assert_ok!(VPE::new_with(VPEArgs::new("reader")));
    writer.files().set(io::STDOUT_FILENO, VPE::cur().files().get(pipe.writer_fd()).unwrap());
    reader.files().set(io::STDIN_FILENO, VPE::cur().files().get(pipe.reader_fd()).unwrap());
    assert_ok!(writer.obtain_fds());
    assert_ok!(reader.obtain_fds());

    let wr_act = assert_ok!(writer.exec(&["/bin/hello"]));

    let rd_act = assert_ok!(reader.run(Box::new(|| {
        let mut s = String::new();
        assert_eq!(io::stdin().read_to_string(&mut s), Ok(12));
        assert_eq!(s, "Hello World\n");
        0
    })));

    pipe.close_reader();
    pipe.close_writer();

    assert_eq!(wr_act.wait(), Ok(0));
    assert_eq!(rd_act.wait(), Ok(0));
}
