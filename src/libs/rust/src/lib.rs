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
extern crate alloc;
#[macro_use]
extern crate bitflags;
extern crate compiler_builtins;

use core::intrinsics;

pub mod collections {
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

pub mod cap;
pub mod env;
pub mod elf;
pub mod errors;
pub mod dtu;
pub mod heap;
pub mod kif;
pub mod server;
pub mod session;
pub mod syscalls;
pub mod time;
pub mod vfs;
pub mod vpe;

mod libc;

extern "C" {
    fn main() -> i32;
}

pub fn exit(code: i32) {
    syscalls::exit(code);
    util::jmp_to(env::data().exit_addr);
}

#[no_mangle]
pub extern fn env_run() {
    io::init();
    let envdata = env::data();
    let res = unsafe {
        if envdata.lambda != 0 {
            com::reinit();
            env::closure().call()
        }
        else {
            heap::init();
            vpe::init();
            com::init();
            main()
        }
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
    print!("PANIC at {}, line {}, column {}: ", file, line, column);
    io::print_fmt(msg);
    println!("");
    exit(1);
    unsafe { intrinsics::abort() }
}
