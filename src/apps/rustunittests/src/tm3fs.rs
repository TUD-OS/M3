use m3::errors::Error;
use m3::session::M3FS;
use m3::vfs::{FileSystem, OpenFlags, Write};

pub fn run(t: &mut ::test::Tester) {
    run_test!(t, meta_ops);
}

pub fn meta_ops() {
    let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");

    assert_ok!(m3fs.borrow_mut().mkdir("/example", 0o755));
    assert_err!(m3fs.borrow_mut().mkdir("/example", 0o755), Error::Exists);
    assert_err!(m3fs.borrow_mut().mkdir("/example/foo/bar", 0o755), Error::NoSuchFile);

    {
        let mut file = assert_ok!(m3fs.borrow_mut().open("/example/myfile",
            OpenFlags::W | OpenFlags::CREATE));
        assert_ok!(write!(file, "text\n"));
    }

    // TODO add that as soon as mounting is supported
    // {
    //     assert_int(m3fs.borrow_mut().mount("/fs/", "m3fs"), Errors::NONE);
    //     assert_int(m3fs.borrow_mut().link("/example/myfile", "/fs/foo"), Errors::XFS_LINK);
    //     m3fs.borrow_mut().unmount("/fs");
    // }

    assert_err!(m3fs.borrow_mut().rmdir("/example/foo/bar"), Error::NoSuchFile);
    assert_err!(m3fs.borrow_mut().rmdir("/example/myfile"), Error::IsNoDir);
    assert_err!(m3fs.borrow_mut().rmdir("/example"), Error::DirNotEmpty);

    assert_err!(m3fs.borrow_mut().link("/example", "/newpath"), Error::IsDir);
    assert_ok!(m3fs.borrow_mut().link("/example/myfile", "/newpath"));

    assert_err!(m3fs.borrow_mut().unlink("/example"), Error::IsDir);
    assert_err!(m3fs.borrow_mut().unlink("/example/foo"), Error::NoSuchFile);
    assert_ok!(m3fs.borrow_mut().unlink("/example/myfile"));

    assert_ok!(m3fs.borrow_mut().rmdir("/example"));

    assert_ok!(m3fs.borrow_mut().unlink("/newpath"));
}
