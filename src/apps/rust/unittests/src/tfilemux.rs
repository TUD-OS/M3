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
use m3::io::Read;
use m3::kif;
use m3::test;
use m3::util;
use m3::vfs::{BufReader, FileHandle, IndirectPipe, OpenFlags, VFS};
use m3::vpe::VPE;

pub fn run(t: &mut test::Tester) {
    run_test!(t, genfile_mux);
    run_test!(t, pipe_mux);
}

fn genfile_mux() {
    const NUM: usize        = 6;
    const STEP_SIZE: usize  = 400;
    const FILE_SIZE: usize  = 12 * 1024;

    let mut files = vec![];
    for _ in 0..NUM {
        let file = assert_ok!(VFS::open("/pat.bin", OpenFlags::R));
        files.push(BufReader::new(file));
    }

    let mut pos = 0;
    while pos < FILE_SIZE {
        for f in &mut files {
            let end = util::min(FILE_SIZE, pos + STEP_SIZE);
            for tpos in pos..end {
                let mut buf = [0u8];
                assert_eq!(f.read(&mut buf), Ok(1));
                assert_eq!(buf[0], (tpos & 0xFF) as u8);
            }
        }

        pos += STEP_SIZE;
    }
}

fn pipe_mux() {
    const NUM: usize        = 6;
    const STEP_SIZE: usize  = 16;
    const DATA_SIZE: usize  = 1024;
    const PIPE_SIZE: usize  = 256;

    struct Pipe {
        _mgate: MemGate,
        _pipe: IndirectPipe,
        reader: FileHandle,
        writer: FileHandle,
    };

    let mut pipes = vec![];
    for _ in 0..NUM {
        let mgate = assert_ok!(MemGate::new(PIPE_SIZE, kif::Perm::RW));
        let pipe = assert_ok!(IndirectPipe::new(&mgate, PIPE_SIZE));
        pipes.push(Pipe {
            reader: VPE::cur().files().get(pipe.reader_fd()).unwrap(),
            writer: VPE::cur().files().get(pipe.writer_fd()).unwrap(),
            _pipe: pipe,
            _mgate: mgate,
        });
    }

    let mut src_buf = [0u8; STEP_SIZE];
    for i in 0..STEP_SIZE {
        src_buf[i] = i as u8;
    }

    let mut pos = 0;
    while pos < DATA_SIZE {
        for p in &mut pipes {
            assert_ok!(p.writer.borrow_mut().write(&src_buf));
            assert_ok!(p.writer.borrow_mut().flush());
        }

        for p in &mut pipes {
            let mut dst_buf = [0u8; STEP_SIZE];

            assert_ok!(p.reader.borrow_mut().read(&mut dst_buf));
            assert_eq!(dst_buf, src_buf);
        }

        pos += STEP_SIZE;
    }
}
