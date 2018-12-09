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

use m3::errors::Code;
use m3::test;
use m3::io::Write;
use m3::vfs::{OpenFlags, VFS};

pub fn run(t: &mut test::Tester) {
    run_test!(t, meta_ops);
}

pub fn meta_ops() {
    assert_ok!(VFS::mkdir("/example", 0o755));
    assert_err!(VFS::mkdir("/example", 0o755), Code::Exists);
    assert_err!(VFS::mkdir("/example/foo/bar", 0o755), Code::NoSuchFile);

    {
        let mut file = assert_ok!(VFS::open("/example/myfile",
            OpenFlags::W | OpenFlags::CREATE));
        assert_ok!(write!(file, "text\n"));
    }

    {
        assert_ok!(VFS::mount("/fs/", "m3fs"));
        assert_err!(VFS::link("/example/myfile", "/fs/foo"), Code::XfsLink);
        assert_ok!(VFS::unmount("/fs/"));
    }

    assert_err!(VFS::rmdir("/example/foo/bar"), Code::NoSuchFile);
    assert_err!(VFS::rmdir("/example/myfile"), Code::IsNoDir);
    assert_err!(VFS::rmdir("/example"), Code::DirNotEmpty);

    assert_err!(VFS::link("/example", "/newpath"), Code::IsDir);
    assert_ok!(VFS::link("/example/myfile", "/newpath"));

    assert_err!(VFS::unlink("/example"), Code::IsDir);
    assert_err!(VFS::unlink("/example/foo"), Code::NoSuchFile);
    assert_ok!(VFS::unlink("/example/myfile"));

    assert_ok!(VFS::rmdir("/example"));

    assert_ok!(VFS::unlink("/newpath"));
}
