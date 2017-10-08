use com::{RecvGate, SendGate};
use collections::String;
use core::intrinsics;
use core::slice;
use dtu;
use errors::Error;
use libc;
use util;

pub const MAX_MSG_SIZE: usize = 512;

pub struct GateOStream {
    arr: [u64; MAX_MSG_SIZE / 8],
    pos: usize,
}

impl GateOStream {
    pub fn new() -> Self {
        GateOStream {
            arr: unsafe { intrinsics::uninit() },
            pos: 0,
        }
    }

    pub fn size(&self) -> usize {
        self.pos * util::size_of::<u64>()
    }

    pub fn push<T : Marshallable>(&mut self, item: &T) {
        item.marshall(self);
    }

    pub fn send(&self, gate: &mut SendGate) -> Result<(), Error> {
        gate.send(&self.arr[0..self.pos])
    }
}

pub struct GateIStream {
    arr: &'static [u64],
    pos: usize,
}

impl GateIStream {
    pub fn new(msg: &'static dtu::Message) -> Self {
        let slice = unsafe {
            let ptr = msg.data.as_ptr() as *const u64;
            let sl: &[u64] = slice::from_raw_parts(ptr, (msg.header.length / 8) as usize);
            sl
        };
        GateIStream {
            arr: slice,
            pos: 0,
        }
    }

    pub fn size(&self) -> usize {
        self.arr.len() * util::size_of::<u64>()
    }

    pub fn pop<T : Unmarshallable>(&mut self) -> T {
        T::unmarshall(self)
    }
}

pub trait Marshallable {
    fn marshall(&self, os: &mut GateOStream);
}

pub trait Unmarshallable : Sized {
    fn unmarshall(is: &mut GateIStream) -> Self;
}

macro_rules! impl_xfer_prim {
    ( $t:ty ) => (
        impl Marshallable for $t {
            fn marshall(&self, os: &mut GateOStream) {
                os.arr[os.pos] = *self as u64;
                os.pos += 1;
            }
        }
        impl Unmarshallable for $t {
            fn unmarshall(is: &mut GateIStream) -> Self {
                is.pos += 1;
                is.arr[is.pos - 1] as $t
            }
        }
    )
}

impl_xfer_prim!(u8);
impl_xfer_prim!(i8);
impl_xfer_prim!(u16);
impl_xfer_prim!(i16);
impl_xfer_prim!(u32);
impl_xfer_prim!(i32);
impl_xfer_prim!(u64);
impl_xfer_prim!(i64);
impl_xfer_prim!(usize);
impl_xfer_prim!(isize);

fn marshall_str(s: &str, os: &mut GateOStream) {
    os.arr[os.pos] = s.len() as u64;

    unsafe {
        libc::memcpy(
            (&mut os.arr[os.pos + 1..]).as_mut_ptr() as *mut u8,
            s.as_bytes().as_ptr(),
            s.len(),
        );
    }

    os.pos += 1 + ((s.len() + 7) / 8);
}

impl<'a> Marshallable for &'a str {
    fn marshall(&self, os: &mut GateOStream) {
        marshall_str(self, os);
    }
}

impl Marshallable for String {
    fn marshall(&self, os: &mut GateOStream) {
        marshall_str(self.as_str(), os);
    }
}
impl Unmarshallable for String {
    fn unmarshall(is: &mut GateIStream) -> Self {
        let len = is.arr[is.pos] as usize;

        let res = unsafe {
            let bytes: *mut u8 = intrinsics::transmute((&is.arr[is.pos + 1..]).as_ptr());
            let copy = libc::heap_alloc(len + 1);
            libc::memcpy(copy, bytes, len);
            String::from_raw_parts(copy, len, len)
        };

        is.pos += 1 + ((len + 7) / 8);
        res
    }
}

#[macro_export]
macro_rules! send_vmsg {
    ( $sg:expr, $( $args:expr ),* ) => ({
        let mut os = GateOStream::new();
        $( os.push(&$args); )*
        os.send($sg)
    });
}

pub fn recv_msg(rgate: &mut RecvGate) -> Result<GateIStream, Error> {
    match rgate.wait(None) {
        Err(e) => Err(e),
        Ok(msg) => Ok(GateIStream::new(msg)),
    }
}

#[macro_export]
macro_rules! recv_vmsg {
    ( $rg:expr, $($x:ty),* ) => ({
        match recv_msg($rg) {
            Err(e)      => Err(e),
            Ok(mut is)  => Ok(( $( is.pop::<$x>() ),* )),
        }
    });
}
