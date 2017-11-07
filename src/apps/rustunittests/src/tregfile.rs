use m3::col::{String, Vec};
use m3::errors::Code;
use m3::test;
use m3::vfs::{FileRef, OpenFlags, Seek, SeekMode, Read, Write, VFS};

pub fn run(t: &mut test::Tester) {
    run_test!(t, permissions);
    run_test!(t, read_string);
    run_test!(t, read_exact);
    run_test!(t, read_file_at_once);
    run_test!(t, read_file_in_small_steps);
    run_test!(t, read_file_in_large_steps);
    run_test!(t, write_and_read_file);
    run_test!(t, write_fmt);
    run_test!(t, extend_small_file);
    run_test!(t, overwrite_beginning);
    run_test!(t, truncate);
    run_test!(t, append);
    run_test!(t, append_read);
}

fn permissions() {
    let filename = "/subdir/subsubdir/testfile.txt";
    let mut buf = [0u8; 16];

    {
        let mut file = assert_ok!(VFS::open(filename, OpenFlags::R));
        assert_err!(file.write(&buf), Code::NoPerm);
    }

    {
        let mut file = assert_ok!(VFS::open(filename, OpenFlags::W));
        assert_err!(file.read(&mut buf), Code::NoPerm);
    }
}

fn read_string() {
    let filename = "/subdir/subsubdir/testfile.txt";
    let content = "This is a test!\n";

    let mut file = assert_ok!(VFS::open(filename, OpenFlags::R));

    for i in 0..content.len() {
        assert_eq!(file.seek(0, SeekMode::SET), Ok(0));
        let s = assert_ok!(file.read_string(i));
        assert_eq!(&s, &content[0..i]);
    }
}

fn read_exact() {
    let filename = "/subdir/subsubdir/testfile.txt";
    let content = b"This is a test!\n";

    let mut file = assert_ok!(VFS::open(filename, OpenFlags::R));

    let mut buf = [0u8; 32];
    assert_ok!(file.read_exact(&mut buf[0..8]));
    assert_eq!(&buf[0..8], &content[0..8]);

    assert_eq!(file.seek(0, SeekMode::SET), Ok(0));
    assert_ok!(file.read_exact(&mut buf[0..16]));
    assert_eq!(&buf[0..16], &content[0..16]);

    assert_eq!(file.seek(0, SeekMode::SET), Ok(0));
    assert_err!(file.read_exact(&mut buf), Code::EndOfFile);
}

fn read_file_at_once() {
    let filename = "/subdir/subsubdir/testfile.txt";

    let mut file = assert_ok!(VFS::open(filename, OpenFlags::R));
    let mut s = String::new();
    assert_eq!(file.read_to_string(&mut s), Ok(16));
    assert_eq!(s, "This is a test!\n");
}

fn read_file_in_small_steps() {
    let filename = "/pat.bin";

    let mut file = assert_ok!(VFS::open(filename, OpenFlags::R));
    let mut buf = [0u8; 64];

    assert_eq!(_validate_pattern_content(&mut file, &mut buf), 64 * 1024);
}

fn read_file_in_large_steps() {
    let filename = "/pat.bin";

    let mut file = assert_ok!(VFS::open(filename, OpenFlags::R));
    let mut buf = vec![0u8; 8 * 1024];

    assert_eq!(_validate_pattern_content(&mut file, &mut buf), 64 * 1024);
}

fn write_and_read_file() {
    let content = "Foobar, a test and more and more and more!";
    let filename = "/mat.txt";

    let mut file = assert_ok!(VFS::open(filename, OpenFlags::RW));

    assert_ok!(write!(file, "{}", content));

    assert_eq!(file.seek(0, SeekMode::CUR), Ok(content.len()));
    assert_eq!(file.seek(0, SeekMode::SET), Ok(0));

    let res = assert_ok!(file.read_string(content.len()));
    assert_eq!(&content, &res);

    // undo the write
    let mut old = vec![0u8; content.len()];
    assert_eq!(file.seek(0, SeekMode::SET), Ok(0));
    for i in 0..content.len() {
        old[i] = i as u8;
    }
    assert_eq!(file.write(&old), Ok(content.len()));
}

fn write_fmt() {
    let mut file = assert_ok!(VFS::open("/newfile",
        OpenFlags::CREATE | OpenFlags::RW));

    assert_ok!(write!(file, "This {:.3} is the {}th test of {:#0X}!\n", "foobar", 42, 0xABCDEF));
    assert_ok!(write!(file, "More formatting: {:?}", Some(Some(1))));

    assert_eq!(file.seek(0, SeekMode::SET), Ok(0));

    let mut s = String::new();
    assert_eq!(file.read_to_string(&mut s), Ok(69));
    assert_eq!(s, "This foo is the 42th test of 0xABCDEF!\nMore formatting: Some(Some(1))");
}

fn extend_small_file() {
    {
        let mut file = assert_ok!(VFS::open("/test.txt", OpenFlags::W));

        let buf = _get_pat_vector(1024);
        for _ in 0..33 {
            assert_eq!(file.write(&buf[0..1024]), Ok(1024));
        }
    }

    _validate_pattern_file("/test.txt", 1024 * 33);
}

fn overwrite_beginning() {
    {
        let mut file = assert_ok!(VFS::open("/test.txt", OpenFlags::W));

        let buf = _get_pat_vector(1024);
        for _ in 0..3 {
            assert_eq!(file.write(&buf[0..1024]), Ok(1024));
        }
    }

    _validate_pattern_file("/test.txt", 1024 * 33);
}

fn truncate() {
    {
        let mut file = assert_ok!(VFS::open("/test.txt",
            OpenFlags::W | OpenFlags::TRUNC));

        let buf = _get_pat_vector(1024);
        for _ in 0..2 {
            assert_eq!(file.write(&buf[0..1024]), Ok(1024));
        }
    }

    _validate_pattern_file("/test.txt", 1024 * 2);
}

fn append() {
    {
        let mut file = assert_ok!(VFS::open("/test.txt",
            OpenFlags::W | OpenFlags::APPEND));
        // TODO perform the seek to end here, because we cannot do that during open atm (m3fs
        // already borrowed as mutable). it's the wrong semantic anyway, so ...
        assert_ok!(file.seek(0, SeekMode::END));

        let buf = _get_pat_vector(1024);
        for _ in 0..2 {
            assert_eq!(file.write(&buf[0..1024]), Ok(1024));
        }
    }

    _validate_pattern_file("/test.txt", 1024 * 4);
}

fn append_read() {
    {
        let mut file = assert_ok!(VFS::open("/test.txt",
            OpenFlags::RW | OpenFlags::TRUNC | OpenFlags::CREATE));

        let pat = _get_pat_vector(1024);
        for _ in 0..2 {
            assert_eq!(file.write(&pat[0..1024]), Ok(1024));
        }

        // there is nothing to read now
        let mut buf = [0u8; 1024];
        assert_eq!(file.read(&mut buf), Ok(0));

        // seek beyond the end
        assert_eq!(file.seek(2 * 1024, SeekMode::CUR), Ok(4 * 1024));
        // seek back
        assert_eq!(file.seek(2 * 1024, SeekMode::SET), Ok(2 * 1024));

        // now reading should work
        assert_eq!(file.read(&mut buf), Ok(1024));

        // seek back and overwrite
        assert_eq!(file.seek(2 * 1024, SeekMode::SET), Ok(2 * 1024));

        for _ in 0..2 {
            assert_eq!(file.write(&pat[0..1024]), Ok(1024));
        }
    }

    _validate_pattern_file("/test.txt", 1024 * 4);
}

fn _get_pat_vector(size: usize) -> Vec<u8> {
    let mut buf = Vec::with_capacity(size);
    for i in 0..1024 {
        buf.push(i as u8)
    }
    buf
}

fn _validate_pattern_file(filename: &str, size: usize) {
    let mut file = assert_ok!(VFS::open(filename, OpenFlags::R));

    let info = assert_ok!(file.borrow().stat());
    assert_eq!(info.size, size);

    let mut buf = [0u8; 1024];
    assert_eq!(_validate_pattern_content(&mut file, &mut buf), size);
}

fn _validate_pattern_content(file: &mut FileRef, mut buf: &mut [u8]) -> usize {
    let mut pos: usize = 0;
    loop {
        let count = assert_ok!(file.read(&mut buf));
        if count == 0 { break; }

        for i in 0..count {
            assert_eq!(buf[i], (pos & 0xFF) as u8,
                "content wrong at offset {}: {} vs. {}", pos, buf[i], (pos & 0xFF) as u8);
            pos += 1;
        }
    }
    pos
}
