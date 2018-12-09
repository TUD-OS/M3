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

#![no_std]

#[macro_use]
extern crate m3;

use m3::col::String;
use m3::com::*;
use m3::session::ClientSession;

#[no_mangle]
pub fn main() -> i32 {
    for _ in 0..3 {
        let sess = ClientSession::new("test", 0).expect("Unable to connect to 'test'");
        let rgate = RecvGate::def();
        let mut sgate = SendGate::new_bind(sess.obtain_obj().expect("Unable to obtain SGate cap"));

        for _ in 0..5 {
            let req = "123456";

            let mut reply = send_recv!(&mut sgate, rgate, 0, req).expect("Communication failed");
            let resp: String = reply.pop();

            println!("Got '{}'", resp);
        }
    }

    0
}
