/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

use m3::cap::Selector;
use m3::cfg::PAGE_SIZE;
use m3::dtu::{EP_COUNT, FIRST_FREE_EP};
use m3::errors::Code;
use m3::kif::{CapRngDesc, CapType, INVALID_SEL, Perm};
use m3::kif::syscalls::{ExchangeArgs, SrvOp, VPEOp};
use m3::com::{MemGate, RecvGate, SendGate};
use m3::session::M3FS;
use m3::syscalls;
use m3::test;
use m3::vpe::VPE;

pub fn run(t: &mut test::Tester) {
    run_test!(t, create_srv);
    run_test!(t, create_sgate);
    run_test!(t, create_rgate);
    run_test!(t, create_mgate);
    run_test!(t, create_sess);
    run_test!(t, create_map);
    run_test!(t, create_vpe_group);
    run_test!(t, create_vpe);

    run_test!(t, activate);
    run_test!(t, derive_mem);
    run_test!(t, srv_ctrl);
    run_test!(t, vpe_ctrl);
    run_test!(t, vpe_wait);
    run_test!(t, open_sess);

    run_test!(t, exchange);
    run_test!(t, delegate);
    run_test!(t, obtain);
    run_test!(t, revoke);
}

fn create_srv() {
    let sel = VPE::cur().alloc_sel();
    let mut rgate = assert_ok!(RecvGate::new(10, 10));

    // invalid dest selector
    assert_err!(syscalls::create_srv(0, VPE::cur().sel(), rgate.sel(), "test"), Code::InvArgs);

    // invalid rgate selector
    assert_err!(syscalls::create_srv(sel, VPE::cur().sel(), 0, "test"), Code::InvArgs);
    // again, with real rgate, but not activated
    assert_err!(syscalls::create_srv(sel, VPE::cur().sel(), rgate.sel(), "test"), Code::InvArgs);
    assert_ok!(rgate.activate());

    // invalid VPE selector
    assert_err!(syscalls::create_srv(sel, 1, rgate.sel(), "test"), Code::InvArgs);

    // invalid name
    assert_err!(syscalls::create_srv(sel, VPE::cur().sel(), rgate.sel(), ""), Code::InvArgs);

    // existing name
    assert_ok!(syscalls::create_srv(sel, VPE::cur().sel(), rgate.sel(), "test"));
    let sel2 = VPE::cur().alloc_sel();
    assert_err!(syscalls::create_srv(sel2, VPE::cur().sel(), rgate.sel(), "test"), Code::Exists);
    assert_ok!(syscalls::revoke(VPE::cur().sel(), CapRngDesc::new(CapType::OBJECT, sel, 1), true));
}

fn create_sgate() {
    let sel = VPE::cur().alloc_sel();
    let rgate = assert_ok!(RecvGate::new(10, 10));

    // invalid dest selector
    assert_err!(syscalls::create_sgate(0, rgate.sel(), 0xDEADBEEF, 123), Code::InvArgs);
    // invalid rgate selector
    assert_err!(syscalls::create_sgate(sel, 0, 0xDEADBEEF, 123), Code::InvArgs);
}

fn create_rgate() {
    let sel = VPE::cur().alloc_sel();

    // invalid dest selector
    assert_err!(syscalls::create_rgate(0, 10, 10), Code::InvArgs);
    // invalid order
    assert_err!(syscalls::create_rgate(sel, 2000, 10), Code::InvArgs);
    assert_err!(syscalls::create_rgate(sel, -1, 10), Code::InvArgs);
    // invalid msg order
    assert_err!(syscalls::create_rgate(sel, 10, 11), Code::InvArgs);
    assert_err!(syscalls::create_rgate(sel, 10, -1), Code::InvArgs);
    // invalid order and msg order
    assert_err!(syscalls::create_rgate(sel, -1, -1), Code::InvArgs);
}

fn create_mgate() {
    let sel = VPE::cur().alloc_sel();

    // invalid dest selector
    assert_err!(syscalls::create_mgate(0, !0, 0x1000, Perm::RW), Code::InvArgs);
    // invalid size
    assert_err!(syscalls::create_mgate(sel, !0, 0, Perm::RW), Code::InvArgs);
    assert_err!(syscalls::create_mgate(sel, !0, Perm::R.bits() as usize, Perm::RW), Code::InvArgs);
    assert_err!(syscalls::create_mgate(sel, !0, 0x40000000, Perm::RW), Code::OutOfMem);
}

fn create_sess() {
    let srv = VPE::cur().alloc_sel();
    let mut rgate = assert_ok!(RecvGate::new(10, 10));
    assert_ok!(rgate.activate());
    assert_ok!(syscalls::create_srv(srv, VPE::cur().sel(), rgate.sel(), "test"));

    let sel = VPE::cur().alloc_sel();

    // invalid dest selector
    assert_err!(syscalls::create_sess(0, srv, 0), Code::InvArgs);
    // invalid service selector
    assert_err!(syscalls::create_sess(sel, 1, 0), Code::InvArgs);

    assert_ok!(syscalls::revoke(VPE::cur().sel(), CapRngDesc::new(CapType::OBJECT, srv, 1), true));
}

fn create_map() {
    if !VPE::cur().pe().has_virtmem() {
        return;
    }

    let meminv = assert_ok!(MemGate::new(64, Perm::RW));    // not page-granular
    let mem    = assert_ok!(MemGate::new(PAGE_SIZE * 4, Perm::RW));

    // invalid VPE selector
    assert_err!(syscalls::create_map(0, 1, mem.sel(), 0, 4, Perm::RW), Code::InvArgs);
    // invalid memgate selector
    assert_err!(syscalls::create_map(0, VPE::cur().sel(), 0, 0, 4, Perm::RW), Code::InvArgs);
    assert_err!(syscalls::create_map(0, VPE::cur().sel(), meminv.sel(), 0, 4, Perm::RW), Code::InvArgs);
    // invalid first page
    assert_err!(syscalls::create_map(0, VPE::cur().sel(), mem.sel(), 4, 4, Perm::RW), Code::InvArgs);
    assert_err!(syscalls::create_map(0, VPE::cur().sel(), mem.sel(), !0, 4, Perm::RW), Code::InvArgs);
    // invalid page count
    assert_err!(syscalls::create_map(0, VPE::cur().sel(), mem.sel(), 0, 5, Perm::RW), Code::InvArgs);
    assert_err!(syscalls::create_map(0, VPE::cur().sel(), mem.sel(), 3, 2, Perm::RW), Code::InvArgs);
    assert_err!(syscalls::create_map(0, VPE::cur().sel(), mem.sel(), 4, 0, Perm::RW), Code::InvArgs);
    assert_err!(syscalls::create_map(0, VPE::cur().sel(), mem.sel(), !0, !0, Perm::RW), Code::InvArgs);
    // invalid permissions
    assert_err!(syscalls::create_map(0, VPE::cur().sel(), mem.sel(), 0, 4, Perm::X), Code::InvArgs);
    assert_err!(syscalls::create_map(0, VPE::cur().sel(), mem.sel(), 0, 4, Perm::RWX), Code::InvArgs);
}

fn create_vpe_group() {
    assert_err!(syscalls::create_vpe_group(0), Code::InvArgs);
}

fn create_vpe() {
    let cap_count = 2 + (EP_COUNT - FIRST_FREE_EP) as Selector;
    let sels      = VPE::cur().alloc_sels(cap_count);
    let crd       = CapRngDesc::new(CapType::OBJECT, sels, cap_count);
    let rgate     = assert_ok!(RecvGate::new(10, 10));
    let sgate     = assert_ok!(SendGate::new(&rgate));
    let pedesc    = VPE::cur().pe();

    // invalid dest caps
    assert_err!(syscalls::create_vpe(CapRngDesc::new(CapType::OBJECT, 0, cap_count),
                                     INVALID_SEL, "test", pedesc,
                                     0, 0, false, INVALID_SEL), Code::InvArgs);
    assert_err!(syscalls::create_vpe(CapRngDesc::new(CapType::OBJECT, sels, 0),
                                     INVALID_SEL, "test", pedesc,
                                     0, 0, false, INVALID_SEL), Code::InvArgs);
    assert_err!(syscalls::create_vpe(CapRngDesc::new(CapType::OBJECT, sels, cap_count - 1),
                                     INVALID_SEL, "test", pedesc,
                                     0, 0, false, INVALID_SEL), Code::InvArgs);
    assert_err!(syscalls::create_vpe(CapRngDesc::new(CapType::OBJECT, sels, !0),
                                     INVALID_SEL, "test", pedesc,
                                     0, 0, false, INVALID_SEL), Code::InvArgs);

    // invalid sgate
    assert_err!(syscalls::create_vpe(crd, 0, "test", pedesc,
                                     0, 0, false, INVALID_SEL), Code::InvArgs);

    // invalid name
    assert_err!(syscalls::create_vpe(crd, INVALID_SEL, "", pedesc,
                                     0, 0, false, INVALID_SEL), Code::InvArgs);

    // invalid SEP
    assert_err!(syscalls::create_vpe(crd, sgate.sel(), "test", pedesc,
                                     0, 0, false, INVALID_SEL), Code::InvArgs);
    assert_err!(syscalls::create_vpe(crd, sgate.sel(), "test", pedesc,
                                     EP_COUNT, 0, false, INVALID_SEL), Code::InvArgs);
    assert_err!(syscalls::create_vpe(crd, sgate.sel(), "test", pedesc,
                                     !0, 0, false, INVALID_SEL), Code::InvArgs);
    // invalid REP
    assert_err!(syscalls::create_vpe(crd, INVALID_SEL, "test", pedesc,
                                     0, EP_COUNT, false, INVALID_SEL), Code::InvArgs);
    assert_err!(syscalls::create_vpe(crd, INVALID_SEL, "test", pedesc,
                                     0, !0, false, INVALID_SEL), Code::InvArgs);

    // invalid vpe group
    assert_err!(syscalls::create_vpe(crd, INVALID_SEL, "test", pedesc,
                                     0, 0, false, 0), Code::InvArgs);
}

fn activate() {
    let ep1     = VPE::cur().ep_sel(assert_ok!(VPE::cur().alloc_ep()));
    // let ep2     = VPE::cur().ep_sel(assert_ok!(VPE::cur().alloc_ep()));
    let sel     = VPE::cur().alloc_sel();
    let mgate   = assert_ok!(MemGate::new(0x1000, Perm::RW));

    // invalid EP sel
    assert_err!(syscalls::activate(0, mgate.sel(), 0), Code::InvArgs);
    assert_err!(syscalls::activate(sel, mgate.sel(), 0), Code::InvArgs);
    // invalid mgate sel
    assert_err!(syscalls::activate(ep1, 0, 0), Code::InvArgs);
    // invalid address
    assert_err!(syscalls::activate(ep1, mgate.sel(), 0x1000), Code::InvArgs);
    assert_err!(syscalls::activate(ep1, mgate.sel(), !0), Code::InvArgs);
    // already activated; TODO does not work with rustkernel
    // assert_ok!(syscalls::activate(ep1, mgate.sel(), 0));
    // assert_err!(syscalls::activate(ep2, mgate.sel(), 0), Code::Exists);
}

fn derive_mem() {
    let sel = VPE::cur().alloc_sel();
    let mem = assert_ok!(MemGate::new(0x4000, Perm::RW));

    // invalid dest selector
    assert_err!(syscalls::derive_mem(0, mem.sel(), 0, 0x1000, Perm::RW), Code::InvArgs);
    // invalid mem
    assert_err!(syscalls::derive_mem(sel, 0, 0, 0x1000, Perm::RW), Code::InvArgs);
    // invalid offset
    assert_err!(syscalls::derive_mem(sel, mem.sel(), 0x4000, 0x1000, Perm::RW), Code::InvArgs);
    assert_err!(syscalls::derive_mem(sel, mem.sel(), !0, 0x1000, Perm::RW), Code::InvArgs);
    // invalid size
    assert_err!(syscalls::derive_mem(sel, mem.sel(), 0, 0x4001, Perm::RW), Code::InvArgs);
    assert_err!(syscalls::derive_mem(sel, mem.sel(), 0x2000, 0x2001, Perm::RW), Code::InvArgs);
    assert_err!(syscalls::derive_mem(sel, mem.sel(), 0x2000, 0, Perm::RW), Code::InvArgs);
    assert_err!(syscalls::derive_mem(sel, mem.sel(), 0x4000, 0, Perm::RW), Code::InvArgs);
    assert_err!(syscalls::derive_mem(sel, mem.sel(), !0, !0, Perm::RW), Code::InvArgs);
    // perms are arbitrary; will be ANDed
}

fn srv_ctrl() {
    assert_err!(syscalls::srv_ctrl(0, SrvOp::SHUTDOWN), Code::InvArgs);
    assert_err!(syscalls::srv_ctrl(INVALID_SEL, SrvOp::SHUTDOWN), Code::InvArgs);
}

fn vpe_ctrl() {
    let child = assert_ok!(VPE::new("test"));

    assert_err!(syscalls::vpe_ctrl(1, VPEOp::START, 0), Code::InvArgs);
    assert_err!(syscalls::vpe_ctrl(INVALID_SEL, VPEOp::START, 0), Code::InvArgs);
    // can't start ourself
    assert_err!(syscalls::vpe_ctrl(VPE::cur().sel(), VPEOp::START, 0), Code::InvArgs);
    // can't yield others
    assert_err!(syscalls::vpe_ctrl(child.sel(), VPEOp::YIELD, 0), Code::InvArgs);
}

fn vpe_wait() {
    assert_err!(syscalls::vpe_wait(&[]), Code::InvArgs);
}

fn open_sess() {
    let sel = VPE::cur().alloc_sel();

    assert_err!(syscalls::open_sess(0, "m3fs", 0), Code::InvArgs);
    assert_err!(syscalls::open_sess(sel, "", 0), Code::InvArgs);
}

fn exchange() {
    let mut child   = assert_ok!(VPE::new("test"));
    let csel        = child.alloc_sel();

    let sel         = VPE::cur().alloc_sel();
    let unused      = CapRngDesc::new(CapType::OBJECT, sel, 1);
    let used        = CapRngDesc::new(CapType::OBJECT, 0, 1);

    // invalid VPE sel
    assert_err!(syscalls::exchange(1, used, csel, false), Code::InvArgs);
    // invalid own caps (source caps can be invalid)
    assert_err!(syscalls::exchange(VPE::cur().sel(), used, unused.start(), true), Code::InvArgs);
    assert_err!(syscalls::exchange(child.sel(), used, 0, true), Code::InvArgs);
    // invalid other caps
    assert_err!(syscalls::exchange(VPE::cur().sel(), used, 0, false), Code::InvArgs);
    assert_err!(syscalls::exchange(child.sel(), used, 0, false), Code::InvArgs);
}

fn delegate() {
    let m3fs        = assert_ok!(M3FS::new("m3fs"));
    let m3fs        = m3fs.borrow();
    let sess        = m3fs.as_any().downcast_ref::<M3FS>().unwrap().sess();
    let crd         = CapRngDesc::new(CapType::OBJECT, 0, 1);
    let mut args    = ExchangeArgs::default();

    // invalid VPE selector
    assert_err!(syscalls::delegate(1, sess.sel(), crd, &mut args), Code::InvArgs);
    // invalid sess selector
    assert_err!(syscalls::delegate(VPE::cur().sel(), 0, crd, &mut args), Code::InvArgs);
    // CRD can be anything (depends on server)
}

fn obtain() {
    let m3fs        = assert_ok!(M3FS::new("m3fs"));
    let m3fs        = m3fs.borrow();
    let sess        = m3fs.as_any().downcast_ref::<M3FS>().unwrap().sess();
    let sel         = VPE::cur().alloc_sel();
    let crd         = CapRngDesc::new(CapType::OBJECT, sel, 1);
    let inval       = CapRngDesc::new(CapType::OBJECT, 0, 1);
    let mut args    = ExchangeArgs::default();

    // invalid VPE selector
    assert_err!(syscalls::obtain(1, sess.sel(), crd, &mut args), Code::InvArgs);
    // invalid sess selector
    assert_err!(syscalls::obtain(VPE::cur().sel(), 0, crd, &mut args), Code::InvArgs);
    // invalid CRD
    assert_err!(syscalls::obtain(VPE::cur().sel(), sess.sel(), inval, &mut args), Code::InvArgs);
}

fn revoke() {
    let crd_vpe = CapRngDesc::new(CapType::OBJECT, 0, 1);
    let crd_mem = CapRngDesc::new(CapType::OBJECT, 1, 1);

    // invalid VPE selector
    assert_err!(syscalls::revoke(1, crd_vpe, true), Code::InvArgs);
    // can't revoke VPE or mem cap
    assert_err!(syscalls::revoke(VPE::cur().sel(), crd_vpe, true), Code::InvArgs);
    assert_err!(syscalls::revoke(VPE::cur().sel(), crd_mem, true), Code::InvArgs);
}
