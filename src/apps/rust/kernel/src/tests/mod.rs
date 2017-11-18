use base::test::Tester;
use base::heap;

mod tcaps;
mod tmemmap;

struct MyTester {
}

impl Tester for MyTester {
    fn run_suite(&mut self, name: &str, f: &Fn(&mut Tester)) {
        klog!(DEF, "Running test suite {} ...", name);
        f(self);
        klog!(DEF, "Done\n");
    }

    fn run_test(&mut self, name: &str, f: &Fn()) {
        klog!(DEF, "-- Running test {} ...", name);
        let free_mem = heap::free_memory();
        f();
        assert_eq!(heap::free_memory(), free_mem);
        klog!(DEF, "-- Done");
    }
}

pub fn run() {
    let mut tester = MyTester {};
    run_suite!(tester, tcaps::run);
    run_suite!(tester, tmemmap::run);
}
