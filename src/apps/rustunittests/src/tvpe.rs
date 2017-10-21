use m3::com::{SendGate, SGateArgs, RecvGate};
use m3::boxed::Box;
use m3::env;
use m3::errors::Error;
use m3::session::M3FS;
use m3::util;
use m3::vpe::{Activity, VPE, VPEArgs};

pub fn run(t: &mut ::test::Tester) {
    run_test!(t, run_arguments);
    run_test!(t, run_send_receive);
    run_test!(t, exec_fail);
    run_test!(t, exec_hello);
    run_test!(t, exec_rust_hello);
}

fn run_arguments() {
    let mut vpe = assert_ok!(VPE::new_with(VPEArgs::new("test")));

    let act = assert_ok!(vpe.run(Box::new(|| {
        assert_eq!(env::args().count(), 1);
        assert_eq!(env::args().nth(0), Some("rustunittests"));
        0
    })));

    assert_eq!(act.wait(), Ok(0));
}

fn run_send_receive() {
    let mut vpe = assert_ok!(VPE::new_with(VPEArgs::new("test")));

    let mut rgate = assert_ok!(RecvGate::new(util::next_log2(256), util::next_log2(256)));
    let sgate = assert_ok!(SendGate::new_with(SGateArgs::new(&rgate).credits(256)));

    assert_ok!(vpe.delegate_obj(rgate.sel()));

    let act = assert_ok!(vpe.run(Box::new(move || {
        assert_ok!(rgate.activate());
        let (i1, i2) = assert_ok!(recv_vmsg!(&rgate, u32, u32));
        assert_eq!((i1, i2), (42, 23));
        (i1 + i2) as i32
    })));

    assert_ok!(send_vmsg!(&sgate, RecvGate::def(), 42, 23));

    assert_eq!(act.wait(), Ok(42 + 23));
}

fn exec_fail() {
    let m3fs = assert_ok!(M3FS::new("m3fs"));
    let mut vpe = assert_ok!(VPE::new_with(VPEArgs::new("test")));

    // file too small
    {
        let act = vpe.exec(m3fs.clone(), &["/testfile.txt"]);
        assert!(act.is_err() && act.err() == Some(Error::EndOfFile));
    }

    // not an ELF file
    {
        let act = vpe.exec(m3fs.clone(), &["/pat.bin"]);
        assert!(act.is_err() && act.err() == Some(Error::InvalidElf));
    }
}

fn exec_hello() {
    let m3fs = assert_ok!(M3FS::new("m3fs"));
    let mut vpe = assert_ok!(VPE::new_with(VPEArgs::new("test")));

    let act = assert_ok!(vpe.exec(m3fs.clone(), &["/bin/hello"]));
    assert_eq!(act.wait(), Ok(0));
}

fn exec_rust_hello() {
    let m3fs = assert_ok!(M3FS::new("m3fs"));
    let mut vpe = assert_ok!(VPE::new_with(VPEArgs::new("test")));

    let act = assert_ok!(vpe.exec(m3fs.clone(), &["bin/rusthello"]));
    assert_eq!(act.wait(), Ok(0));
}
