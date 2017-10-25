use m3::session::M3FS;
use m3::profile;
use m3::test;
use m3::vfs::{OpenFlags, FileSystem, Read, Write};

pub fn run(t: &mut test::Tester) {
    run_test!(t, read);
    run_test!(t, write);
}

fn read() {
    let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");
    let mut buf = vec![0u8; 8192];

    let mut prof = profile::Profiler::new().repeats(2).warmup(1);

    println!("{}", prof.run_with_id(|| {
        let mut file = assert_ok!(m3fs.borrow_mut().open("/large.bin", OpenFlags::R));
        loop {
            let amount = assert_ok!(file.read(&mut buf));
            if amount == 0 {
                break;
            }
        }
    }, 0x20));
}

fn write() {
    const SIZE: usize = 2 * 1024 * 1024;
    let m3fs = M3FS::new("m3fs").expect("connect to m3fs failed");
    let buf = vec![0u8; 8192];

    let mut prof = profile::Profiler::new().repeats(2).warmup(1);

    println!("{}", prof.run_with_id(|| {
        let mut file = assert_ok!(m3fs.borrow_mut().open("/newfile",
            OpenFlags::W | OpenFlags::CREATE | OpenFlags::TRUNC));

        let mut total = 0;
        while total < SIZE {
            let amount = assert_ok!(file.write(&buf));
            if amount == 0 {
                break;
            }
            total += amount;
        }
    }, 0x21));
}
