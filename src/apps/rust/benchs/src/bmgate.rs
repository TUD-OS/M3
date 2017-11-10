use m3::com::MemGate;
use m3::kif;
use m3::profile;
use m3::test;

pub fn run(t: &mut test::Tester) {
    run_test!(t, read);
    run_test!(t, write);
}

fn read() {
    let mut buf = vec![0u8; 8192];
    let mgate = MemGate::new(8192, kif::Perm::R).expect("Unable to create mgate");

    let mut prof = profile::Profiler::new();

    println!("8K: {}", prof.run_with_id(|| {
        mgate.read(&mut buf, 0).expect("Reading failed");
    }, 0x30));
}

fn write() {
    let buf = vec![0u8; 8192];
    let mgate = MemGate::new(8192, kif::Perm::W).expect("Unable to create mgate");

    let mut prof = profile::Profiler::new();

    println!("8K: {}", prof.run_with_id(|| {
        mgate.write(&buf, 0).expect("Writing failed");
    }, 0x31));
}
