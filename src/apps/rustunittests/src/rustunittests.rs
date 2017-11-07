#![no_std]

#[macro_use]
extern crate m3;

use m3::test::Tester;

mod tbufio;
mod tdir;
mod tm3fs;
mod tmgate;
mod tregfile;
mod trgate;
mod tsgate;
mod tvpe;

struct MyTester {
}

impl Tester for MyTester {
    fn run_suite(&mut self, name: &str, f: &Fn(&mut Tester)) {
        println!("Running test suite {} ...", name);
        f(self);
        println!("Done\n");
    }

    fn run_test(&mut self, name: &str, f: &Fn()) {
        println!("-- Running test {} ...", name);
        f();
        println!("-- Done");
    }
}

#[no_mangle]
pub fn main() -> i32 {
    // TODO
    #[cfg(target_os = "linux")]
    ::m3::vfs::VFS::mount("/", "m3fs").unwrap();

    let mut tester = MyTester {};
    run_suite!(tester, tmgate::run);
    run_suite!(tester, trgate::run);
    run_suite!(tester, tsgate::run);
    run_suite!(tester, tm3fs::run);
    run_suite!(tester, tdir::run);
    run_suite!(tester, tbufio::run);
    run_suite!(tester, tregfile::run);
    run_suite!(tester, tvpe::run);

    0
}
