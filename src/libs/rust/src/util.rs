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

pub fn cstr_to_str(s: *const u8) -> &'static str {
    unsafe {
        let len = libc::strlen(s);
        let sl = slice::from_raw_parts(s, len as usize + 1);
        intrinsics::transmute(&sl[..sl.len() - 1])
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

        impl $crate::com::Marshallable for $Name {
            fn marshall(&self, os: &mut $crate::com::GateOStream) {
                os.arr[os.pos] = self.val as u64;
                os.pos += 1;
            }
        }

        impl $crate::com::Unmarshallable for $Name {
            fn unmarshall(is: &mut $crate::com::GateIStream) -> Self {
                is.pos += 1;
                let val = is.data()[is.pos - 1] as $T;
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
            val: $T,
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
            val: $T,
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
