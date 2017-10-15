#![no_std]

#[macro_use]
extern crate m3;

use m3::test::Tester;

struct MyTester {
}

impl m3::test::Tester for MyTester {
    fn run_suite(&mut self, name: &str, f: &Fn(&mut m3::test::Tester)) {
        println!("Running test suite {} ...", name);
        f(self);
        println!("Done");
    }

    fn run_test(&mut self, name: &str, f: &Fn()) {
        println!("-- Running test {} ...", name);
        f();
        println!("-- Done");
    }
}

#[no_mangle]
pub fn main() -> i32 {
    let mut tester = MyTester {};
    run_suite!(tester, m3::com::tests::mgate::run);
    run_suite!(tester, m3::com::tests::rgate::run);
    run_suite!(tester, m3::com::tests::sgate::run);
    run_suite!(tester, m3::vfs::tests::regfile::run);
    run_suite!(tester, m3::vfs::tests::bufio::run);
    run_suite!(tester, m3::session::tests::m3fs::run);

    0
}
