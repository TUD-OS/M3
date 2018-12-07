#![feature(core_intrinsics)]
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
