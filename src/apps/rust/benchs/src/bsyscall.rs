use m3::com::{MemGate, RecvGate, Perm};
use m3::kif;
use m3::profile;
use m3::syscalls;
use m3::test;
use m3::vpe::{VPE, VPEArgs};

pub fn run(t: &mut test::Tester) {
    run_test!(t, noop);
    run_test!(t, activate);
    run_test!(t, create_rgate);
    run_test!(t, create_sgate);
    run_test!(t, create_mgate);
    run_test!(t, create_srv);
    run_test!(t, create_sess);
    run_test!(t, derive_mem);
    run_test!(t, exchange);
    run_test!(t, revoke);
}

fn noop() {
    let mut prof = profile::Profiler::new();

    println!("{}", prof.run_with_id(|| {
        assert_ok!(syscalls::noop());
    }, 0x10));
}

fn activate() {
    let mgate = assert_ok!(MemGate::new(0x1000, Perm::RW));
    let mut buf = [0u8; 8];
    assert_ok!(mgate.read(&mut buf, 0));
    let ep = mgate.ep().unwrap();

    let mut prof = profile::Profiler::new();

    println!("{}", prof.run_with_id(|| {
        assert_ok!(syscalls::activate(VPE::cur().sel(), mgate.sel(), ep, 0));
    }, 0x11));
}

fn create_rgate() {
    let mut prof = profile::Profiler::new().repeats(100).warmup(4);

    #[derive(Default)]
    struct Tester();

    impl profile::Runner for Tester {
        fn run(&mut self) {
            assert_ok!(syscalls::create_rgate(100, 10, 10));
        }
        fn post(&mut self) {
            assert_ok!(syscalls::revoke(0, kif::CapRngDesc::new(kif::CapType::OBJECT, 100, 1), true));
        }
    }

    println!("{}", prof.runner_with_id(&mut Tester::default(), 0x12));
}

fn create_sgate() {
    let mut prof = profile::Profiler::new().repeats(100).warmup(4);

    #[derive(Default)]
    struct Tester(Option<RecvGate>);

    impl profile::Runner for Tester {
        fn pre(&mut self) {
            if self.0.is_none() {
                self.0 = Some(assert_ok!(RecvGate::new(10, 10)));
            }
        }
        fn run(&mut self) {
            assert_ok!(syscalls::create_sgate(100, self.0.as_ref().unwrap().sel(), 0x1234, 1024));
        }
        fn post(&mut self) {
            assert_ok!(syscalls::revoke(0, kif::CapRngDesc::new(kif::CapType::OBJECT, 100, 1), true));
        }
    }

    println!("{}", prof.runner_with_id(&mut Tester::default(), 0x13));
}

fn create_mgate() {
    let mut prof = profile::Profiler::new().repeats(100).warmup(4);

    #[derive(Default)]
    struct Tester();

    impl profile::Runner for Tester {
        fn run(&mut self) {
            assert_ok!(syscalls::create_mgate(100, !0, 0x1000, Perm::RW));
        }
        fn post(&mut self) {
            assert_ok!(syscalls::revoke(0, kif::CapRngDesc::new(kif::CapType::OBJECT, 100, 1), true));
        }
    }

    println!("{}", prof.runner_with_id(&mut Tester::default(), 0x14));
}

fn create_srv() {
    let mut prof = profile::Profiler::new().repeats(100).warmup(4);

    #[derive(Default)]
    struct Tester(Option<RecvGate>);

    impl profile::Runner for Tester {
        fn pre(&mut self) {
            if self.0.is_none() {
                self.0 = Some(assert_ok!(RecvGate::new(10, 10)));
                self.0.as_mut().unwrap().activate().unwrap();
            }
        }
        fn run(&mut self) {
            assert_ok!(syscalls::create_srv(100, self.0.as_ref().unwrap().sel(), "test"));
        }
        fn post(&mut self) {
            assert_ok!(syscalls::revoke(0, kif::CapRngDesc::new(kif::CapType::OBJECT, 100, 1), true));
        }
    }

    println!("{}", prof.runner_with_id(&mut Tester::default(), 0x15));
}

fn create_sess() {
    let mut prof = profile::Profiler::new().repeats(100).warmup(4);

    #[derive(Default)]
    struct Tester();

    impl profile::Runner for Tester {
        fn run(&mut self) {
            assert_ok!(syscalls::create_sess(100, "m3fs", 0));
        }
        fn post(&mut self) {
            assert_ok!(syscalls::revoke(0, kif::CapRngDesc::new(kif::CapType::OBJECT, 100, 1), true));
        }
    }

    println!("{}", prof.runner_with_id(&mut Tester::default(), 0x16));
}

fn derive_mem() {
    let mut prof = profile::Profiler::new().repeats(100).warmup(4);

    #[derive(Default)]
    struct Tester(Option<MemGate>);

    impl profile::Runner for Tester {
        fn pre(&mut self) {
            if self.0.is_none() {
                self.0 = Some(assert_ok!(MemGate::new(0x1000, Perm::RW)));
            }
        }
        fn run(&mut self) {
            assert_ok!(syscalls::derive_mem(100, self.0.as_ref().unwrap().sel(), 0, 0x1000, Perm::RW));
        }
        fn post(&mut self) {
            assert_ok!(syscalls::revoke(0, kif::CapRngDesc::new(kif::CapType::OBJECT, 100, 1), true));
        }
    }

    println!("{}", prof.runner_with_id(&mut Tester::default(), 0x17));
}

fn exchange() {
    let mut prof = profile::Profiler::new().repeats(100).warmup(4);

    #[derive(Default)]
    struct Tester(Option<VPE>);

    impl profile::Runner for Tester {
        fn pre(&mut self) {
            if self.0.is_none() {
                self.0 = Some(assert_ok!(VPE::new_with(VPEArgs::new("test"))));
            }
        }
        fn run(&mut self) {
            assert_ok!(syscalls::exchange(
                self.0.as_ref().unwrap().sel(),
                kif::CapRngDesc::new(kif::CapType::OBJECT, 1, 1),
                100,
                false,
            ));
        }
        fn post(&mut self) {
            assert_ok!(syscalls::revoke(
                self.0.as_ref().unwrap().sel(),
                kif::CapRngDesc::new(kif::CapType::OBJECT, 100, 1),
                true
            ));
        }
    }

    println!("{}", prof.runner_with_id(&mut Tester::default(), 0x18));
}

fn revoke() {
    let mut prof = profile::Profiler::new().repeats(100).warmup(4);

    #[derive(Default)]
    struct Tester(Option<MemGate>);

    impl profile::Runner for Tester {
        fn pre(&mut self) {
            self.0 = Some(assert_ok!(MemGate::new(0x1000, Perm::RW)));
        }
        fn run(&mut self) {
            self.0 = None;
        }
    }

    println!("{}", prof.runner_with_id(&mut Tester::default(), 0x19));
}
