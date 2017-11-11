use core::intrinsics;
use core::slice;
use libc;

// TODO move to proper place
pub fn jmp_to(addr: usize) {
    unsafe {
        asm!(
            "jmp *$0"
            : : "r"(addr)
        );
    }
}

// source: https://en.wikipedia.org/wiki/Methods_of_computing_square_roots
pub fn sqrt(n: f32) -> f32 {
    let mut val_int: u32 = unsafe { intrinsics::transmute(n) };

    val_int = val_int.wrapping_sub(1 << 23); /* Subtract 2^m. */
    val_int >>= 1;                           /* Divide by 2. */
    val_int = val_int.wrapping_add(1 << 29); /* Add ((b + 1) / 2) * 2^m. */

    unsafe { intrinsics::transmute(val_int) }
}

pub fn size_of<T>() -> usize {
    unsafe {
        intrinsics::size_of::<T>()
    }
}

pub fn size_of_val<T: ?Sized>(val: &T) -> usize {
    unsafe {
        intrinsics::size_of_val(val)
    }
}

pub unsafe fn cstr_to_str(s: *const i8) -> &'static str {
    let len = libc::strlen(s);
    let sl = slice::from_raw_parts(s, len as usize + 1);
    intrinsics::transmute(&sl[..sl.len() - 1])
}

pub unsafe fn slice_for<T>(start: *const T, size: usize) -> &'static [T] {
    slice::from_raw_parts(start, size)
}

pub unsafe fn slice_for_mut<T>(start: *mut T, size: usize) -> &'static mut [T] {
    slice::from_raw_parts_mut(start, size)
}

pub fn object_to_bytes<T : Sized>(obj: &T) -> &[u8] {
    let p: *const T = obj;
    let p: *const u8 = p as *const u8;
    unsafe {
        slice::from_raw_parts(p, size_of::<T>())
    }
}

pub fn object_to_bytes_mut<T : Sized>(obj: &mut T) -> &mut [u8] {
    let p: *mut T = obj;
    let p: *mut u8 = p as *mut u8;
    unsafe {
        slice::from_raw_parts_mut(p, size_of::<T>())
    }
}

fn _next_log2(size: usize, shift: i32) -> i32 {
    if size > (1 << shift) {
        shift + 1
    }
    else if shift == 0 {
        0
    }
    else {
        _next_log2(size, shift - 1)
    }
}

pub fn next_log2(size: usize) -> i32 {
    _next_log2(size, (size_of::<usize>() * 8 - 2) as i32)
}

// TODO make these generic
pub fn round_up(value: usize, align: usize) -> usize {
    (value + align - 1) & !(align - 1)
}

pub fn round_dn(value: usize, align: usize) -> usize {
    value & !(align - 1)
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
        a
    }
    else {
        b
    }
}

#[macro_export]
macro_rules! __int_enum_impl {
    (
        struct $Name:ident: $T:ty {
            $(
                const $Flag:ident = $value:expr;
            )+
        }
    ) => (
        impl $Name {
            $(
                #[allow(dead_code)]
                pub const $Flag: $Name = $Name { val: $value };
            )+
        }

        impl $crate::serialize::Marshallable for $Name {
            fn marshall(&self, s: &mut $crate::serialize::Sink) {
                s.push_word(self.val as u64);
            }
        }

        impl $crate::serialize::Unmarshallable for $Name {
            fn unmarshall(s: &mut $crate::serialize::Source) -> Self {
                let val = s.pop_word() as $T;
                $Name { val: val }
            }
        }

        impl From<$T> for $Name {
            fn from(val: $T) -> Self {
                $Name { val: val }
            }
        }
    )
}

#[macro_export]
macro_rules! int_enum {
    (
        pub struct $Name:ident: $T:ty {
            $(
                const $Flag:ident = $value:expr;
            )+
        }
    ) => (
        #[derive(Copy, PartialEq, Eq, Clone, PartialOrd, Ord)]
        pub struct $Name {
            pub val: $T,
        }

        __int_enum_impl! {
            struct $Name : $T {
                $(
                    const $Flag = $value;
                )+
            }
        }
    );
    (
        struct $Name:ident: $T:ty {
            $(
                const $Flag:ident = $value:expr;
            )+
        }
    ) => (
        #[derive(Copy, PartialEq, Eq, Clone, PartialOrd, Ord)]
        struct $Name {
            pub val: $T,
        }

        __int_enum_impl! {
            struct $Name : $T {
                $(
                    const $Flag = $value;
                )+
            }
        }
    )
}
