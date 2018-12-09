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

pub static DEF: bool    = true;
pub static ERR: bool    = true;
pub static EPS: bool    = false;
pub static SYSC: bool   = false;
pub static KENV: bool   = false;
pub static MEM: bool    = false;
pub static SERV: bool   = false;
pub static SQUEUE: bool = false;
pub static PTES: bool   = false;
pub static VPES: bool   = false;

#[macro_export]
macro_rules! klog {
    ($type:tt, $fmt:expr)              => (log_impl!($crate::log::$type, concat!($fmt, "\n")));
    ($type:tt, $fmt:expr, $($arg:tt)*) => (log_impl!($crate::log::$type, concat!($fmt, "\n"), $($arg)*));
}
