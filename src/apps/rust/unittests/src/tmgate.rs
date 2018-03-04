use m3::com::{MemGate, MGateArgs, Perm};
use m3::errors::Code;
use m3::test;

pub fn run(t: &mut test::Tester) {
    run_test!(t, create);
    run_test!(t, create_readonly);
    run_test!(t, create_writeonly);
    run_test!(t, derive);
    run_test!(t, read_write);
    run_test!(t, read_write_object);
    #[cfg(target_os = "none")]
    run_test!(t, read_write_forward_small);
    #[cfg(target_os = "none")]
    run_test!(t, read_write_forward_big);
}

fn create() {
    assert_err!(MemGate::new_with(MGateArgs::new(0x1000, Perm::R).sel(1)), Code::InvArgs);
}

fn create_readonly() {
    let mgate = assert_ok!(MemGate::new(0x1000, Perm::R));
    let mut data = [0u8; 8];
    assert_err!(mgate.write(&data, 0), Code::InvEP);
    assert_ok!(mgate.read(&mut data, 0));
}

fn create_writeonly() {
    let mgate = assert_ok!(MemGate::new(0x1000, Perm::W));
    let mut data = [0u8; 8];
    assert_err!(mgate.read(&mut data, 0), Code::InvEP);
    assert_ok!(mgate.write(&data, 0));
}

fn derive() {
    let mgate = assert_ok!(MemGate::new(0x1000, Perm::RW));
    assert_err!(mgate.derive(0x0, 0x2000, Perm::RW), Code::InvArgs);
    assert_err!(mgate.derive(0x1000, 0x10, Perm::RW), Code::InvArgs);
    assert_err!(mgate.derive(0x800, 0x1000, Perm::RW), Code::InvArgs);
    let dgate = assert_ok!(mgate.derive(0x800, 0x800, Perm::R));
    let mut data = [0u8; 8];
    assert_err!(dgate.write(&data, 0), Code::InvEP);
    assert_ok!(dgate.read(&mut data, 0));
}

fn read_write() {
    let mgate = assert_ok!(MemGate::new(0x1000, Perm::RW));
    let refdata = [0u8, 1, 2, 3, 4, 5, 6, 7];
    let mut data = [0u8; 8];
    assert_ok!(mgate.write(&refdata, 0));
    assert_ok!(mgate.read(&mut data, 0));
    assert_eq!(data, refdata);

    assert_ok!(mgate.read(&mut data[0..4], 4));
    assert_eq!(&data[0..4], &refdata[4..8]);
    assert_eq!(&data[4..8], &refdata[4..8]);
}

fn read_write_object() {
    #[derive(Clone, Copy, Debug, Eq, PartialEq)]
    struct Test {
        a: u32,
        b: u64,
        c: bool,
    }

    let mgate = assert_ok!(MemGate::new(0x1000, Perm::RW));
    let refobj = Test { a: 0x1234, b: 0xF000_F000_AAAA_BBBB, c: true };

    assert_ok!(mgate.write_obj(&refobj, 0));
    let obj: Test = assert_ok!(mgate.read_obj(0));

    assert_eq!(refobj, obj);
}

#[cfg(target_os = "none")]
fn read_write_forward_small() {
    use m3::goff;
    use m3::kif;
    use m3::vpe;

    let vpe1 = assert_ok!(vpe::VPE::new_with(vpe::VPEArgs::new("v1").muxable(true)));
    let vpe2 = assert_ok!(vpe::VPE::new_with(vpe::VPEArgs::new("v2").muxable(true)));

    const TEST_ADDR: goff = 0x20000;

    if let Some(ref mut pg) = vpe1.pager() {
        assert_ok!(pg.map_anon(TEST_ADDR, 0x1000, kif::Perm::RW));
    }
    if let Some(ref mut pg) = vpe2.pager() {
        assert_ok!(pg.map_anon(TEST_ADDR, 0x1000, kif::Perm::RW));
    }

    let refdata = [0u8, 1, 2, 3, 4, 5, 6, 7];
    let mut data = [0u8; 8];

    assert_ok!(vpe1.mem().write(&refdata, TEST_ADDR));
    assert_ok!(vpe2.mem().write(&refdata, TEST_ADDR));

    assert_ok!(vpe1.mem().read(&mut data, TEST_ADDR));
    assert_eq!(refdata, data);

    assert_ok!(vpe2.mem().read(&mut data, TEST_ADDR));
    assert_eq!(refdata, data);
}

#[cfg(target_os = "none")]
fn read_write_forward_big() {
    use m3::goff;
    use m3::kif;
    use m3::vpe;

    let vpe1 = assert_ok!(vpe::VPE::new_with(vpe::VPEArgs::new("v1").muxable(true)));
    let vpe2 = assert_ok!(vpe::VPE::new_with(vpe::VPEArgs::new("v2").muxable(true)));

    const TEST_ADDR: goff = 0x20000;

    if let Some(ref mut pg) = vpe1.pager() {
        assert_ok!(pg.map_anon(TEST_ADDR, 0x1000, kif::Perm::RW));
    }
    if let Some(ref mut pg) = vpe2.pager() {
        assert_ok!(pg.map_anon(TEST_ADDR, 0x1000, kif::Perm::RW));
    }

    let mut refdata = vec![0x00u8; 1024];
    let mut data = vec![0x00u8; 1024];
    for i in 0..refdata.len() {
        refdata[i] = i as u8;
    }

    assert_ok!(vpe1.mem().write(&refdata, TEST_ADDR));
    assert_ok!(vpe2.mem().write(&refdata, TEST_ADDR));

    assert_ok!(vpe1.mem().read(&mut data, TEST_ADDR));
    assert_eq!(refdata, data);

    assert_ok!(vpe2.mem().read(&mut data, TEST_ADDR));
    assert_eq!(refdata, data);
}
