#![feature(lang_items, core_intrinsics)]
#![feature(i128_type)]
#![feature(asm)]
#![no_std]

use core::intrinsics;

pub mod dtu;
pub mod env;
#[macro_use]
pub mod io;
pub mod kif;
pub mod errors;
pub mod syscalls;
pub mod util;
pub mod time;
mod libc;

extern "C" {
    pub fn main(argc: i32, argv: *const *const u8) -> i32;
}

pub fn exit(code: i32) {
    syscalls::exit(code);
    util::jmpto(env::get().exit_addr);
}

#[no_mangle]
pub extern fn env_run() {
    let res = unsafe {
        main(0, 0 as *const *const u8)
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

#[no_mangle]
pub extern "C" fn __muloti4() {
    unsafe { intrinsics::abort() }
}
