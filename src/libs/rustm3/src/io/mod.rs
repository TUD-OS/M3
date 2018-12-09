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

mod serial;
mod std;

pub use base::io::*;
pub use self::serial::*;
pub use self::std::{stdin, stdout, stderr};
pub use self::std::{STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};

#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => ({
        #[allow(unused_imports)]
        use $crate::io::Write;
        $crate::io::stdout().write_fmt(format_args!($($arg)*)).unwrap();
    });
}

#[macro_export]
macro_rules! println {
    ($fmt:expr)              => (print!(concat!($fmt, "\n")));
    ($fmt:expr, $($arg:tt)*) => (print!(concat!($fmt, "\n"), $($arg)*));
}

pub fn init() {
    ::base::io::init();
    std::init();
}

pub fn reinit() {
    ::base::io::reinit();
    std::reinit();
}

pub fn deinit() {
    std::deinit();
}
