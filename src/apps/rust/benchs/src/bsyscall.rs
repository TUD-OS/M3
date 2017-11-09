use m3::com::{MemGate, Perm};
use m3::profile;
use m3::syscalls;
use m3::test;
use m3::vpe::VPE;

pub fn run(t: &mut test::Tester) {
    run_test!(t, noop);
    run_test!(t, activate);
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
