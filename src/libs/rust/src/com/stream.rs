use com::{RecvGate, SendGate};
use collections::String;
use core::intrinsics;
use core::ops;
use core::slice;
use dtu;
use errors::Error;
use libc;
use util;

pub const MAX_MSG_SIZE: usize = 512;

pub struct GateOStream {
    pub arr: [u64; MAX_MSG_SIZE / 8],
    pub pos: usize,
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

    pub fn send(&self, gate: &SendGate, reply_gate: &RecvGate) -> Result<(), Error> {
        gate.send(&self.arr[0..self.pos], reply_gate)
    }
}

pub struct GateIStream<'r> {
    pub msg: &'static dtu::Message,
    pub pos: usize,
    rgate: &'r RecvGate<'r>,
    ack: bool,
}

impl<'r> GateIStream<'r> {
    pub fn new(msg: &'static dtu::Message, rgate: &'r RecvGate<'r>) -> Self {
        GateIStream {
            msg: msg,
            pos: 0,
            rgate: rgate,
            ack: true,
        }
    }

    pub fn label(&self) -> u64 {
        self.msg.header.label
    }

    pub fn data(&self) -> &'static [u64] {
        unsafe {
            let ptr = self.msg.data.as_ptr() as *const u64;
            slice::from_raw_parts(ptr, (self.msg.header.length / 8) as usize)
        }
    }

    pub fn size(&self) -> usize {
        self.data().len() * util::size_of::<u64>()
    }

    pub fn pop<T : Unmarshallable>(&mut self) -> T {
        T::unmarshall(self)
    }

    pub fn reply<T>(&mut self, reply: &[T]) -> Result<(), Error> {
        match self.rgate.reply(reply, self.msg) {
            Ok(_)   => {
                self.ack = false;
                Ok(())
            },
            Err(e)  => Err(e),
        }
    }

    pub fn reply_os(&mut self, os: GateOStream) -> Result<(), Error> {
        self.reply(&os.arr[0..os.pos])
    }
}

impl<'r> ops::Drop for GateIStream<'r> {
    fn drop(&mut self) {
        if self.ack {
            dtu::DTU::mark_read(self.rgate.ep().unwrap(), self.msg);
        }
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
                is.data()[is.pos - 1] as $t
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
        let len = is.data()[is.pos] as usize;

        let res = unsafe {
            let bytes: *mut u8 = intrinsics::transmute((&is.data()[is.pos + 1..]).as_ptr());
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
    ( $sg:expr, $rg:expr, $( $args:expr ),* ) => ({
        let mut os = $crate::com::GateOStream::new();
        $( os.push(&$args); )*
        os.send($sg, $rg)
    });
}

#[macro_export]
macro_rules! reply_vmsg {
    ( $is:expr, $( $args:expr ),* ) => ({
        let mut os = $crate::com::GateOStream::new();
        $( os.push(&$args); )*
        $is.reply_os(os)
    });
}

pub fn recv_msg<'r>(rgate: &'r RecvGate) -> Result<GateIStream<'r>, Error> {
    recv_msg_from(rgate, None)
}

pub fn recv_msg_from<'r>(rgate: &'r RecvGate, sgate: Option<&SendGate>) -> Result<GateIStream<'r>, Error> {
    match rgate.wait(sgate) {
        Err(e) => Err(e),
        Ok(msg) => Ok(GateIStream::new(msg, rgate)),
    }
}

#[macro_export]
macro_rules! recv_vmsg {
    ( $rg:expr, $x:ty ) => ({
        match $crate::com::recv_msg($rg) {
            Err(e)      => Err(e),
            Ok(mut is)  => Ok(( is.pop::<$x>(), )),
        }
    });

    ( $rg:expr, $x1:ty, $($xs:ty),+ ) => ({
        match $crate::com::recv_msg($rg) {
            Err(e)      => Err(e),
            Ok(mut is)  => Ok(( is.pop::<$x1>(), $( is.pop::<$xs>() ),+ )),
        }
    });
}

pub fn recv_res<'r>(rgate: &'r RecvGate) -> Result<GateIStream<'r>, Error> {
    recv_res_from(rgate, None)
}

pub fn recv_res_from<'r>(rgate: &'r RecvGate, sgate: Option<&SendGate>) -> Result<GateIStream<'r>, Error> {
    let mut reply = try!(recv_msg_from(rgate, sgate));
    let res: u32 = reply.pop();
    match res {
        0 => Ok(reply),
        e => Err(Error::from(e)),
    }
}

#[macro_export]
macro_rules! send_recv {
    ( $sg:expr, $rg:expr, $( $args:expr ),* ) => ({
        match send_vmsg!($sg, $rg, $( $args ),* ) {
            Ok(_)   => $crate::com::recv_msg_from($rg, Some($sg)),
            Err(e)  => Err(e),
        }
    });
}

#[macro_export]
macro_rules! send_recv_res {
    ( $sg:expr, $rg:expr, $( $args:expr ),* ) => ({
        match send_vmsg!($sg, $rg, $( $args ),* ) {
            Ok(_)   => $crate::com::recv_res_from($rg, Some($sg)),
            Err(e)  => Err(e),
        }
    });
}
