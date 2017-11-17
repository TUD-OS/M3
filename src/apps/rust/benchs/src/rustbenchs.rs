#![no_std]

#[macro_use]
extern crate m3;

mod bdlist;
mod btreap;
mod btreemap;
mod bmgate;
mod bregfile;
mod bstream;
mod bsyscall;

use m3::test::Tester;

struct MyTester {
}

impl Tester for MyTester {
    fn run_suite(&mut self, name: &str, f: &Fn(&mut Tester)) {
        println!("Running benchmark suite {} ...", name);
        f(self);
        println!("Done\n");
    }

    fn run_test(&mut self, name: &str, f: &Fn()) {
        println!("-- Running benchmark {} ...", name);
        let free_mem = m3::heap::free_memory();
        f();
        assert_eq!(m3::heap::free_memory(), free_mem);
        println!("-- Done");
    }
}

#[no_mangle]
pub fn main() -> i32 {
    let mut tester = MyTester {};
    run_suite!(tester, btreap::run);
    run_suite!(tester, btreemap::run);
    run_suite!(tester, bmgate::run);
    run_suite!(tester, bregfile::run);
    run_suite!(tester, bdlist::run);
    run_suite!(tester, bstream::run);
    run_suite!(tester, bsyscall::run);

    0
}
