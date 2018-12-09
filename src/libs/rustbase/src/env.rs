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

//! Provides access to the program environment

use arch;
use boxed::{Box, FnBox};
use core::iter;
use core::mem;
use util;

/// The closure used by `VPE::run`
pub struct Closure {
    func: Option<Box<FnBox() -> i32 + Send>>,
}

impl Closure {
    /// Creates a new object for given closure
    pub fn new<F>(func: Box<F>) -> Self
                  where F: FnBox() -> i32, F: Send + 'static {
        Closure {
            func: Some(func),
        }
    }

    /// Calls the closure (can only be done once) and returns its exit code
    pub fn call(&mut self) -> i32 {
        match mem::replace(&mut self.func, None) {
            Some(c) => c.call_box(()),
            None    => 1
        }
    }
}

/// The command line argument iterator
///
/// # Examples
///
/// ```
/// for arg in env::args() {
///     println!("{}", arg);
/// }
/// ```
#[derive(Copy, Clone)]
pub struct Args {
    pos: isize,
}

impl Args {
    fn arg(&self, idx: isize) -> &'static str {
        unsafe {
            let args = arch::envdata::get().argv as *const u64;
            let arg = *args.offset(idx);
            util::cstr_to_str(arg as *const i8)
        }
    }

    /// Returns the number of arguments
    pub fn len(&self) -> usize {
        arch::envdata::get().argc as usize
    }
}

impl iter::Iterator for Args {
    type Item = &'static str;

    fn next(&mut self) -> Option<Self::Item> {
        if self.pos < arch::envdata::get().argc as isize {
            let arg = self.arg(self.pos);
            self.pos += 1;
            Some(arg)
        }
        else {
            None
        }
    }
}

/// Returns the argument iterator
pub fn args() -> Args {
    Args {
        pos: 0,
    }
}
