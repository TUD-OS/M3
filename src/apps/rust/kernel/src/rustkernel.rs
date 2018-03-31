#![feature(core_intrinsics)]
#![feature(const_atomic_usize_new)]
#![feature(ptr_internals)]

#![no_std]

#[macro_use]
extern crate base;
#[macro_use]
extern crate bitflags;
extern crate thread;

#[macro_use]
mod log;

pub mod arch;
mod cap;
mod com;
mod mem;
mod pes;
mod platform;
mod syscalls;
mod tests;
mod workloop;
