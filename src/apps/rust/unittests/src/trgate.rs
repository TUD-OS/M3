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

use m3::com::{RecvGate, RGateArgs};
use m3::errors::Code;
use m3::test;

pub fn run(t: &mut test::Tester) {
    run_test!(t, create);
}

fn create() {
    assert_err!(RecvGate::new(8, 9), Code::InvArgs);
    assert_err!(RecvGate::new_with(RGateArgs::new().sel(1)), Code::InvArgs);
}
