use core::intrinsics;
use core::slice;
use libc;

// TODO move to proper place
pub fn jmp_to(addr: u64) {
    unsafe {
        asm!(
            "jmp *$0"
            : : "r"(addr)
        );
    }
}

pub fn size_of<T: ?Sized>(val: &T) -> usize {
    unsafe {
        intrinsics::size_of_val(val)
    }
}

pub fn cstr_to_str(s: *const u8) -> &'static str {
    unsafe {
        let len = libc::strlen(s);
        let sl = slice::from_raw_parts(s, len as usize + 1);
        intrinsics::transmute(&sl[..sl.len() - 1])
    }
}

pub fn min<T: Ord>(a: T, b: T) -> T {
    if a > b {
        b
    }
    else {
        a
    }
}

pub fn max<T: Ord>(a: T, b: T) -> T {
    if a > b {
        b
    }
    else {
        a
    }
}

