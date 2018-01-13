#![no_std]

#[macro_use]
extern crate m3;

use m3::test::Tester;

mod tbufio;
mod tdir;
mod tdlist;
mod tboxlist;
mod tm3fs;
mod tmgate;
mod tpipe;
mod tregfile;
mod trgate;
mod tsgate;
mod ttreap;
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
        let free_mem = m3::heap::free_memory();
        f();
        assert_eq!(m3::heap::free_memory(), free_mem);
        println!("-- Done");
    }
}

#[no_mangle]
pub fn main() -> i32 {
    let mut tester = MyTester {};
    run_suite!(tester, tbufio::run);
    run_suite!(tester, tdir::run);
    run_suite!(tester, tdlist::run);
    run_suite!(tester, tboxlist::run);
    run_suite!(tester, tm3fs::run);
    run_suite!(tester, tmgate::run);
    run_suite!(tester, tpipe::run);
    run_suite!(tester, tregfile::run);
    run_suite!(tester, trgate::run);
    run_suite!(tester, tsgate::run);
    run_suite!(tester, ttreap::run);
    run_suite!(tester, tvpe::run);

    0
}
