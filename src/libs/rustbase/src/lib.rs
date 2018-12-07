#![feature(alloc, alloc_error_handler, allocator_internals)]
#![feature(asm)]
#![feature(box_into_raw_non_null)]
#![feature(compiler_builtins_lib)]
#![feature(const_fn)]
#![feature(core_intrinsics)]
#![feature(fnbox)]
#![feature(lang_items)]
#![feature(panic_info_message)]
#![feature(ptr_offset_from)]

#![default_lib_allocator]
#![no_std]

#[macro_use]
extern crate alloc;
#[macro_use]
extern crate bitflags;
// for int_enum!
pub extern crate core as _core;
pub extern crate static_assertions;

// Macros
pub use static_assertions::const_assert;
pub use alloc::{vec, format};

// lang stuff
mod lang;
pub use lang::*;

#[cfg(target_os = "linux")]
pub extern crate libc;
#[cfg(target_os = "none")]
/// The C library
pub mod libc {
    pub use arch::libc::*;
}

/// Pointer types for heap allocation
pub mod boxed {
    pub use alloc::boxed::{Box, FnBox};
}

/// Single-threaded reference-counting pointers
pub mod rc {
    pub use alloc::rc::{Rc, Weak};
}

/// Thread-safe reference-counting pointers
pub mod sync {
    pub use alloc::sync::{Arc, Weak};
}

#[macro_use]
pub mod io;
#[macro_use]
pub mod util;
#[macro_use]
pub mod test;

pub mod backtrace;
pub mod col;
pub mod cell;
pub mod elf;
pub mod env;
pub mod errors;
pub mod heap;
pub mod kif;
pub mod profile;
pub mod serialize;
pub mod time;

mod arch;
mod globaddr;

pub use globaddr::GlobAddr;
#[allow(non_camel_case_types)]
pub type goff = u64;

/// The target-dependent configuration
pub mod cfg {
    pub use arch::cfg::*;
}
/// CPU-specific functions
pub mod cpu {
    pub use arch::cpu::*;
}
/// The Data Transfer Unit interface
pub mod dtu {
    pub use arch::dtu::*;
}
/// The environment data
pub mod envdata {
    pub use arch::envdata::*;
}
