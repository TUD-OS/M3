use m3::errors::Error;
use m3::test;
use m3::vfs::{OpenFlags, Write, VFS};

pub fn run(t: &mut test::Tester) {
    run_test!(t, meta_ops);
}

pub fn meta_ops() {
    assert_ok!(VFS::mkdir("/example", 0o755));
    assert_err!(VFS::mkdir("/example", 0o755), Error::Exists);
    assert_err!(VFS::mkdir("/example/foo/bar", 0o755), Error::NoSuchFile);

    {
        let mut file = assert_ok!(VFS::open("/example/myfile",
            OpenFlags::W | OpenFlags::CREATE));
        assert_ok!(write!(file, "text\n"));
    }

    {
        assert_ok!(VFS::mount("/fs/", "m3fs"));
        assert_err!(VFS::link("/example/myfile", "/fs/foo"), Error::XfsLink);
        assert_ok!(VFS::unmount("/fs/"));
    }

    assert_err!(VFS::rmdir("/example/foo/bar"), Error::NoSuchFile);
    assert_err!(VFS::rmdir("/example/myfile"), Error::IsNoDir);
    assert_err!(VFS::rmdir("/example"), Error::DirNotEmpty);

    assert_err!(VFS::link("/example", "/newpath"), Error::IsDir);
    assert_ok!(VFS::link("/example/myfile", "/newpath"));

    assert_err!(VFS::unlink("/example"), Error::IsDir);
    assert_err!(VFS::unlink("/example/foo"), Error::NoSuchFile);
    assert_ok!(VFS::unlink("/example/myfile"));

    assert_ok!(VFS::rmdir("/example"));

    assert_ok!(VFS::unlink("/newpath"));
}
