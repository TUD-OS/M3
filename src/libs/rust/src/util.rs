use core::intrinsics;

// TODO move to proper place
pub fn jmpto(addr: u64) {
    unsafe {
        asm!(
            "jmp *$0"
            : : "r"(addr)
        );
    }
}

pub fn size_of<T: ?Sized>(val: &T) -> usize {
    unsafe { intrinsics::size_of_val(val) }
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

