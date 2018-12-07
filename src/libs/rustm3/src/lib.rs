#![feature(asm)]
#![feature(const_fn)]
#![feature(core_intrinsics)]
#![feature(fnbox)]
#![feature(rustc_private)]
#![feature(trace_macros)]

#![no_std]

#[macro_use]
extern crate base;
#[macro_use]
extern crate bitflags;

// init stuff
#[cfg(target_os = "none")]
pub use arch::init::{env_run, exit};
#[cfg(target_os = "linux")]
pub use arch::init::{rust_init, rust_deinit};

#[macro_use]
pub mod io;
#[macro_use]
pub mod com;

pub use base::*;

pub mod cap;
pub mod server;
pub mod session;
pub mod syscalls;
pub mod vfs;
pub mod vpe;

mod arch;
