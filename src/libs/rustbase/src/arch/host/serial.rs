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

use errors::{Code, Error};
use libc;

static mut LOG_FD: i32 = -1;

pub fn read(buf: &mut [u8]) -> Result<usize, Error> {
    match unsafe { libc::read(0, buf.as_mut_ptr() as *mut libc::c_void, buf.len()) } {
        res if res < 0 => Err(Error::new(Code::ReadFailed)),
        res            => Ok(res as usize),
    }
}

pub fn write(buf: &[u8]) -> Result<usize, Error> {
    match unsafe { libc::write(LOG_FD, buf.as_ptr() as *const libc::c_void, buf.len()) } {
        res if res < 0 => Err(Error::new(Code::WriteFailed)),
        res            => Ok(res as usize),
    }
}

pub fn init() {
    unsafe {
        LOG_FD = libc::open(
            "run/log.txt\0".as_ptr() as *const libc::c_char,
            if cfg!(feature = "kernel") {
                libc::O_WRONLY | libc::O_APPEND | libc::O_CREAT | libc::O_TRUNC
            }
            else {
                libc::O_WRONLY | libc::O_APPEND
            }
        );
        assert!(LOG_FD != -1);
    }
}
