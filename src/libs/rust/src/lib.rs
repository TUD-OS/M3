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

#[cfg(target_os = "linux")]
extern crate libc;
#[cfg(target_os = "none")]
mod libc {
    pub use arch::libc::*;
}

use core::intrinsics;

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

// TODO use pub(crate) for some things

#[cfg(target_os = "linux")]
#[no_mangle]
pub extern "C" fn rust_init(argc: i32, argv: *const *const u8) {
    extern "C" {
        fn dummy_func();
    }

    // ensure that host's init library gets linked in
    unsafe {
        dummy_func();
    }

    arch::env::init(argc, argv);
    heap::init();
    vpe::init();
    io::init();
    com::init();
    arch::dtu::init();
}

#[cfg(target_os = "linux")]
#[no_mangle]
pub extern "C" fn rust_deinit() {
    syscalls::exit(0 /* TODO */);
    arch::dtu::deinit();
}

#[cfg(target_os = "none")]
extern "C" {
    fn main() -> i32;
}

pub fn exit(code: i32) {
    syscalls::exit(code);
    #[cfg(target_os = "none")]
    util::jmp_to(arch::env::data().exit_addr());
}

#[cfg(target_os = "none")]
#[no_mangle]
pub extern fn env_run() {
    let envdata = arch::env::data();
    let res = if envdata.has_lambda() {
        io::reinit();
        vpe::reinit();
        com::reinit();
        arch::env::closure().call()
    }
    else {
        heap::init();
        vpe::init();
        io::init();
        com::init();
        unsafe { main() }
    };
    exit(res)
}

// These functions are used by the compiler, but not
// for a bare-bones hello world. These are normally
// provided by libstd.
#[lang = "eh_personality"]
#[no_mangle]
pub extern fn rust_eh_personality() {
    unsafe { intrinsics::abort() }
}

// This function may be needed based on the compilation target.
#[lang = "eh_unwind_resume"]
#[no_mangle]
pub extern fn rust_eh_unwind_resume() {
    unsafe { intrinsics::abort() }
}

#[allow(non_snake_case)]
#[no_mangle]
pub extern "C" fn _Unwind_Resume() -> ! {
    unsafe { intrinsics::abort() }
}

#[lang = "panic_fmt"]
#[no_mangle]
pub extern fn rust_begin_panic(msg: core::fmt::Arguments,
                               file: &'static str,
                               line: u32,
                               column: u32) -> ! {
    use vfs::Write;
    let l = io::log::Log::get();
    l.write_fmt(format_args!("PANIC at {}, line {}, column {}: ", file, line, column)).unwrap();
    l.write_fmt(msg).unwrap();
    l.write("\n".as_bytes()).unwrap();
    exit(1);
    unsafe { intrinsics::abort() }
}
