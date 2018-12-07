use backtrace;
use core::intrinsics;
use core::panic::PanicInfo;
use io::{log, Write};

extern "C" {
    fn exit(code: i32);
}

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    if let Some(l) = log::Log::get() {
        if let Some(loc) = info.location() {
            l.write_fmt(format_args!("PANIC at {}, line {}, column {}: ",
                                     loc.file(), loc.line(), loc.column())).unwrap();
        }
        else {
            l.write("PANIC at unknown location: ".as_bytes()).unwrap();
        }
        if let Some(msg) = info.message() {
            l.write_fmt(*msg).unwrap();
        }
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

#[alloc_error_handler]
fn alloc_error(_: core::alloc::Layout) -> ! {
    panic!("Alloc error");
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

#[cfg(target_arch = "arm")]
#[no_mangle]
pub extern "C" fn __sync_synchronize() {
    // TODO memory barrier
    // unsafe { asm!("dmb"); }
}

macro_rules! def_cmpswap {
    ($name:ident, $type:ty) => {
        #[cfg(target_arch = "arm")]
        #[no_mangle]
        pub extern "C" fn $name(ptr: *mut $type, oldval: $type, newval: $type) -> $type {
            unsafe {
                let old = *ptr;
                if old == oldval {
                    *ptr = newval
                }
                return old;
            }
        }
    };
}

def_cmpswap!(__sync_val_compare_and_swap_1, u8);
def_cmpswap!(__sync_val_compare_and_swap_2, u16);
def_cmpswap!(__sync_val_compare_and_swap_4, u32);
def_cmpswap!(__sync_val_compare_and_swap_8, u64);
