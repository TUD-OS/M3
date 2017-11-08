#![feature(lang_items, core_intrinsics)]
#![feature(i128_type)]
#![feature(offset_to)]
#![feature(alloc, allocator_internals)]
#![feature(compiler_builtins_lib)]
#![feature(macro_reexport)]
#![feature(asm)]
#![feature(const_fn, const_cell_new)]
#![feature(rustc_private)]
#![feature(trace_macros)]
#![feature(fnbox)]

#![default_lib_allocator]
#![no_std]

#[macro_reexport(vec, format)]
#[macro_use]
extern crate alloc;
#[macro_use]
extern crate bitflags;
extern crate compiler_builtins;

// libc
#[cfg(target_os = "linux")]
extern crate libc;
#[cfg(target_os = "none")]
mod libc {
    pub use arch::libc::*;
}

// init stuff
#[cfg(target_os = "none")]
pub use arch::init::env_run;
#[cfg(target_os = "linux")]
pub use arch::init::{rust_init, rust_deinit};

// lang stuff
mod lang;
pub use lang::rust_begin_panic;

pub mod col {
    pub use alloc::binary_heap::BinaryHeap;
    pub use alloc::btree_map::BTreeMap;
    pub use alloc::btree_set::BTreeSet;
    pub use alloc::linked_list::LinkedList;
    pub use alloc::string::{String, ToString};
    pub use alloc::vec_deque::VecDeque;
    pub use alloc::vec::Vec;
}

pub mod boxed {
    pub use alloc::boxed::{Box, FnBox};
}

pub mod rc {
    pub use alloc::rc::{Rc, Weak};
}

pub mod arc {
    pub use alloc::arc::{Arc, Weak};
}

pub mod cell {
    pub use core::cell::{Cell, Ref, RefCell, RefMut};
}

#[macro_use]
pub mod io;
#[macro_use]
pub mod util;
#[macro_use]
pub mod com;
#[macro_use]
pub mod test;

pub mod cap;
pub mod env;
pub mod elf;
pub mod errors;
pub mod heap;
pub mod kif;
pub mod profile;
pub mod server;
pub mod session;
pub mod syscalls;
pub mod time;
pub mod vfs;
pub mod vpe;

mod arch;
mod backtrace;
mod cfg;
