use arch;
use core::intrinsics;
use core::fmt;
use io;
use vfs::Write;

#[lang = "panic_fmt"]
#[no_mangle]
pub extern fn rust_begin_panic(msg: fmt::Arguments, file: &'static str, line: u32, column: u32) -> ! {
    let l = io::log::Log::get();
    l.write_fmt(format_args!("PANIC at {}, line {}, column {}: ", file, line, column)).unwrap();
    l.write_fmt(msg).unwrap();
    l.write("\n".as_bytes()).unwrap();

    arch::init::exit(1);
    unsafe { intrinsics::abort() }
}

#[lang = "eh_personality"]
#[no_mangle]
pub extern fn rust_eh_personality() {
    unsafe { intrinsics::abort() }
}
