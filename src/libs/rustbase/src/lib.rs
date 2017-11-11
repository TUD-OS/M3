#![feature(alloc, allocator_internals)]
#![feature(asm)]
#![feature(compiler_builtins_lib)]
#![feature(const_fn, const_cell_new)]
#![feature(core_intrinsics)]
#![feature(fnbox)]
#![feature(i128_type)]
#![feature(lang_items)]
#![feature(macro_reexport)]
#![feature(offset_to)]

#![default_lib_allocator]
#![no_std]

#[macro_reexport(vec, format)]
#[macro_use]
extern crate alloc;
#[macro_use]
extern crate bitflags;
extern crate compiler_builtins;

// lang stuff
mod lang;
pub use lang::{rust_begin_panic, rust_eh_personality, _Unwind_Resume};

// libc
#[cfg(target_os = "linux")]
pub extern crate libc;
#[cfg(target_os = "none")]
pub mod libc {
    pub use arch::libc::*;
}

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
pub mod test;

pub mod backtrace;
pub mod elf;
pub mod env;
pub mod errors;
pub mod heap;
pub mod kif;
pub mod profile;
pub mod serialize;
pub mod time;

mod arch;

pub mod cfg {
    pub use arch::cfg::*;
}
pub mod dtu {
    pub use arch::dtu::*;
}
pub mod envdata {
    pub use arch::envdata::*;
}
