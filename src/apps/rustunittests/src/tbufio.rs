use m3::collections::String;
use m3::session::M3FS;
use m3::vfs::{BufReader, BufWriter, FileSystem, OpenFlags, Read, Write};

pub fn run(t: &mut ::test::Tester) {
    run_test!(t, read_write);
}

fn read_write() {
    let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");

    {
        let file = assert_ok!(m3fs.borrow_mut().open("/myfile",
            OpenFlags::CREATE | OpenFlags::W));
        let mut bfile = BufWriter::new(file);

        assert_ok!(write!(bfile, "This {:.3} is the {}th test of {:#0X}!\n", "foobar", 42, 0xABCDEF));
    }

    {
        let file = assert_ok!(m3fs.borrow_mut().open("/myfile", OpenFlags::R));
        let mut bfile = BufReader::new(file);

        let mut s = String::new();
        assert_eq!(bfile.read_to_string(&mut s), Ok(39));
        assert_eq!(s, "This foo is the 42th test of 0xABCDEF!\n");
    }
}