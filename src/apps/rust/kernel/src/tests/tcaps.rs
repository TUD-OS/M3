use base::kif::{CapRngDesc, CapType, Perm};
use base::test;

use cap::{Capability, CapTable, KObject, MGateObject};
use pes::INVALID_VPE;

pub fn run(t: &mut test::Tester) {
    run_test!(t, basics);
    run_test!(t, exchange);
}

fn basics() {
    let mut tbl = CapTable::new();

    tbl.insert(Capability::new(23, KObject::MGate(MGateObject::new(
        0, INVALID_VPE, 0xDEAD, 0xBEEF, Perm::RW
    ))));

    assert_eq!(tbl.unused(23), false);
    {
        let mgate = tbl.get(23).and_then(|cap| cap.get().as_mgate());
        assert_eq!(mgate.unwrap().addr, 0xDEAD);
        assert_eq!(mgate.unwrap().size, 0xBEEF);
    }

    tbl.revoke(CapRngDesc::new(CapType::OBJECT, 23, 1), true);
    assert!(tbl.get(23).is_none());
}

fn exchange() {
    let mut tbl1 = CapTable::new();
    let mut tbl2 = CapTable::new();

    tbl1.insert(Capability::new(11, KObject::MGate(MGateObject::new(
        0, INVALID_VPE, 0xDEAD, 0xBEEF, Perm::RW
    ))));

    {
        let cap = tbl1.get_mut(11).unwrap();
        tbl2.obtain(12, cap);
        tbl2.obtain(13, cap);
    }

    {
        let mgate = tbl2.get(12).and_then(|cap| cap.get().as_mgate());
        assert_eq!(mgate.unwrap().addr, 0xDEAD);
        assert_eq!(mgate.unwrap().size, 0xBEEF);
    }

    {
        let mgate = tbl2.get(13).and_then(|cap| cap.get().as_mgate());
        assert_eq!(mgate.unwrap().addr, 0xDEAD);
        assert_eq!(mgate.unwrap().size, 0xBEEF);
    }

    klog!(DEF, "tbl1 = {:?}", tbl1);
    klog!(DEF, "tbl2 = {:?}", tbl2);

    tbl1.revoke(CapRngDesc::new(CapType::OBJECT, 11, 1), true);
    assert!(tbl1.get(11).is_none());
    assert!(tbl2.get(12).is_none());
    assert!(tbl2.get(13).is_none());
}
