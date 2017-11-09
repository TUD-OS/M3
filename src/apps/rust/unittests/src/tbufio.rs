use m3::col::String;
use m3::test;
use m3::vfs::{BufReader, BufWriter, OpenFlags, Read, Write, VFS};

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