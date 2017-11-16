use backtrace;
use core::intrinsics;
use core::fmt;
use io::{log, Write};

extern "C" {
    fn exit(code: i32);
}

#[lang = "panic_fmt"]
#[no_mangle]
pub extern fn rust_begin_panic(msg: fmt::Arguments, file: &'static str, line: u32, column: u32) -> ! {
    if let Some(l) = log::Log::get() {
        l.write_fmt(format_args!("PANIC at {}, line {}, column {}: ", file, line, column)).unwrap();
        l.write_fmt(msg).unwrap();
        l.write("\n\n".as_bytes()).unwrap();

        let mut bt = [0usize; 16];
        let bt_len = backtrace::collect(&mut bt);
        l.write("Backtrace:\n".as_bytes()).unwrap();
        for i in 0..bt_len {
            l.write_fmt(format_args!("  {:#x}\n", bt[i])).unwrap();
        }
    }

    unsafe {
        exit(1);
        intrinsics::abort();
    }
}

#[lang = "eh_personality"]
#[no_mangle]
pub extern fn rust_eh_personality() {
    unsafe { intrinsics::abort() }
}

#[allow(non_snake_case)]
#[no_mangle]
pub extern "C" fn _Unwind_Resume() -> ! {
    unsafe { intrinsics::abort() }
}
