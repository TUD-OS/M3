use m3::col::DList;
use m3::profile;
use m3::test;

pub fn run(t: &mut test::Tester) {
    run_test!(t, push_back);
    run_test!(t, push_front);
    run_test!(t, clear);
}

fn push_back() {
    let mut prof = profile::Profiler::new().repeats(10).warmup(0);

    #[derive(Default)]
    struct ListTester(DList<u32>);

    impl profile::Runner for ListTester {
        fn pre(&mut self) {
            self.0.clear();
        }
        fn run(&mut self) {
            for i in 0..100 {
                self.0.push_back(i);
            }
        }
    }

    println!("Appending 100 elements: {}", prof.runner_with_id(&mut ListTester::default(), 0x50));
}

fn push_front() {
    let mut prof = profile::Profiler::new().repeats(10).warmup(0);

    #[derive(Default)]
    struct ListTester(DList<u32>);

    impl profile::Runner for ListTester {
        fn pre(&mut self) {
            self.0.clear();
        }
        fn run(&mut self) {
            for i in 0..100 {
                self.0.push_front(i);
            }
        }
    }

    println!("Prepending 100 elements: {}", prof.runner_with_id(&mut ListTester::default(), 0x51));
}

fn clear() {
    let mut prof = profile::Profiler::new().repeats(10).warmup(0);

    #[derive(Default)]
    struct ListTester(DList<u32>);

    impl profile::Runner for ListTester {
        fn pre(&mut self) {
            for i in 0..100 {
                self.0.push_back(i);
            }
        }
        fn run(&mut self) {
            self.0.clear();
        }
    }

    println!("Clearing 100-element list: {}", prof.runner_with_id(&mut ListTester::default(), 0x52));
}
