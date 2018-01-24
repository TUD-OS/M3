use m3::col::Treap;
use m3::profile;
use m3::test;

pub fn run(t: &mut test::Tester) {
    run_test!(t, insert);
    run_test!(t, find);
    run_test!(t, clear);
}

fn insert() {
    let mut prof = profile::Profiler::new().repeats(30);

    #[derive(Default)]
    struct BTreeTester(Treap<u32, u32>);

    impl profile::Runner for BTreeTester {
        fn pre(&mut self) {
            self.0.clear();
        }
        fn run(&mut self) {
            for i in 0..100 {
                self.0.insert(i, i);
            }
        }
    }

    println!("Inserting 100 elements: {}", prof.runner_with_id(&mut BTreeTester::default(), 0x71));
}

fn find() {
    let mut prof = profile::Profiler::new().repeats(30);

    #[derive(Default)]
    struct BTreeTester(Treap<u32, u32>);

    impl profile::Runner for BTreeTester {
        fn pre(&mut self) {
            for i in 0..100 {
                self.0.insert(i, i);
            }
        }
        fn run(&mut self) {
            for i in 0..100 {
                let val = self.0.get(&i);
                assert_eq!(val, Some(&i));
            }
        }
        fn post(&mut self) {
            self.0.clear();
        }
    }

    println!("Searching for 100 elements: {}", prof.runner_with_id(&mut BTreeTester::default(), 0x72));
}

fn clear() {
    let mut prof = profile::Profiler::new().repeats(30);

    #[derive(Default)]
    struct BTreeTester(Treap<u32, u32>);

    impl profile::Runner for BTreeTester {
        fn pre(&mut self) {
            for i in 0..100 {
                self.0.insert(i, i);
            }
        }
        fn run(&mut self) {
            self.0.clear();
        }
    }

    println!("Removing 100-element list: {}", prof.runner_with_id(&mut BTreeTester::default(), 0x73));
}
