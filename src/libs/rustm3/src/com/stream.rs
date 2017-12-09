use col::{String, Vec};
use com::{RecvGate, SendGate};
use core::intrinsics;
use core::ops;
use core::slice;
use dtu;
use errors::Error;
use libc;
use serialize::{Sink, Source, Marshallable, Unmarshallable};
use util;

const MAX_MSG_SIZE: usize = 512;

struct GateSink {
    arr: [u64; MAX_MSG_SIZE / 8],
    pos: usize,
}

impl GateSink {
    pub fn new() -> Self {
        GateSink {
            arr: unsafe { intrinsics::uninit() },
            pos: 0,
        }
    }
}

impl Sink for GateSink {
    fn size(&self) -> usize {
        self.pos * util::size_of::<u64>()
    }

    fn words(&self) -> &[u64] {
        &self.arr[0..self.pos]
    }

    fn push(&mut self, item: &Marshallable) {
        item.marshall(self);
    }
    fn push_word(&mut self, word: u64) {
        self.arr[self.pos] = word;
        self.pos += 1;
    }
    fn push_str(&mut self, b: &str) {
        self.push_word(b.len() as u64);

        unsafe {
            libc::memcpy(
                (&mut self.arr[self.pos..]).as_mut_ptr() as *mut libc::c_void,
                b.as_bytes().as_ptr() as *const libc::c_void,
                b.len(),
            );
        }

        self.pos += (b.len() + 7) / 8;
    }
}

pub struct VecSink {
    vec: Vec<u64>,
}

impl VecSink {
    pub fn new() -> Self {
        VecSink {
            vec: Vec::new(),
        }
    }
}

impl Sink for VecSink {
    fn size(&self) -> usize {
        self.vec.len() * util::size_of::<u64>()
    }

    fn words(&self) -> &[u64] {
        &self.vec
    }

    fn push(&mut self, item: &Marshallable) {
        item.marshall(self);
    }
    fn push_word(&mut self, word: u64) {
        self.vec.push(word);
    }
    fn push_str(&mut self, b: &str) {
        self.push_word(b.len() as u64);

        let elems = (b.len() + 7) / 8;
        let cur = self.vec.len();
        self.vec.reserve_exact(elems);

        unsafe {
            self.vec.set_len(cur + elems);
            libc::memcpy(
                (&mut self.vec.as_mut_slice()[cur..cur + elems]).as_mut_ptr() as *mut libc::c_void,
                b.as_bytes().as_ptr() as *const libc::c_void,
                b.len(),
            );
        }
    }
}

struct GateSource {
    msg: &'static dtu::Message,
    pos: usize,
}

impl GateSource {
    pub fn new(msg: &'static dtu::Message) -> Self {
        GateSource {
            msg: msg,
            pos: 0,
        }
    }

    pub fn data(&self) -> &'static [u64] {
        unsafe {
            let ptr = self.msg.data.as_ptr() as *const u64;
            slice::from_raw_parts(ptr, (self.msg.header.length / 8) as usize)
        }
    }
}

fn copy_str_from(s: &[u64], len: usize) -> String {
    unsafe {
        let bytes: *mut libc::c_void = intrinsics::transmute((s).as_ptr());
        let copy = ::base::heap::heap_alloc(len + 1);
        libc::memcpy(copy, bytes, len);
        String::from_raw_parts(copy as *mut u8, len, len)
    }
}

impl Source for GateSource {
    fn pop_word(&mut self) -> u64 {
        self.pos += 1;
        self.data()[self.pos - 1]
    }
    fn pop_str(&mut self) -> String {
        let len = self.pop_word() as usize;
        let res = copy_str_from(&self.data()[self.pos..], len);
        self.pos += (len + 7) / 8;
        res
    }
}

pub struct SliceSource<'s> {
    slice: &'s [u64],
    pos: usize,
}

impl<'s> SliceSource<'s> {
    pub fn new(s: &'s [u64]) -> SliceSource<'s> {
        SliceSource {
            slice: s,
            pos: 0,
        }
    }

    pub fn pop<T : Unmarshallable>(&mut self) -> T {
        T::unmarshall(self)
    }
}

impl<'s> Source for SliceSource<'s> {
    fn pop_word(&mut self) -> u64 {
        self.pos += 1;
        self.slice[self.pos - 1]
    }
    fn pop_str(&mut self) -> String {
        let len = self.pop_word() as usize;
        let res = copy_str_from(&self.slice[self.pos..], len);
        self.pos += (len + 7) / 8;
        res
    }
}

pub struct GateOStream {
    buf: GateSink,
}

impl GateOStream {
    pub fn new() -> Self {
        GateOStream {
            buf: GateSink::new(),
        }
    }

    pub fn size(&self) -> usize {
        self.buf.size()
    }

    pub fn push<T : Marshallable>(&mut self, item: &T) {
        item.marshall(&mut self.buf);
    }

    pub fn send(&self, gate: &SendGate, reply_gate: &RecvGate) -> Result<(), Error> {
        gate.send(self.buf.words(), reply_gate)
    }
}

pub struct GateIStream<'r> {
    source: GateSource,
    rgate: &'r RecvGate,
    ack: bool,
}

impl<'r> GateIStream<'r> {
    pub fn new(msg: &'static dtu::Message, rgate: &'r RecvGate) -> Self {
        GateIStream {
            source: GateSource::new(msg),
            rgate: rgate,
            ack: true,
        }
    }

    pub fn label(&self) -> u64 {
        self.source.msg.header.label
    }

    pub fn size(&self) -> usize {
        self.source.data().len() * util::size_of::<u64>()
    }

    pub fn pop<T : Unmarshallable>(&mut self) -> T {
        T::unmarshall(&mut self.source)
    }

    pub fn reply<T>(&mut self, reply: &[T]) -> Result<(), Error> {
        match self.rgate.reply(reply, self.source.msg) {
            Ok(_)   => {
                self.ack = false;
                Ok(())
            },
            Err(e)  => Err(e),
        }
    }

    pub fn reply_os(&mut self, os: &GateOStream) -> Result<(), Error> {
        self.reply(os.buf.words())
    }
}

impl<'r> ops::Drop for GateIStream<'r> {
    fn drop(&mut self) {
        if self.ack {
            dtu::DTU::mark_read(self.rgate.ep().unwrap(), self.source.msg);
        }
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
        $is.reply_os(&os)
    });
}

pub fn recv_msg<'r>(rgate: &'r RecvGate) -> Result<GateIStream<'r>, Error> {
    recv_msg_from(rgate, None)
}

pub fn recv_msg_from<'r>(rgate: &'r RecvGate, sgate: Option<&SendGate>) -> Result<GateIStream<'r>, Error> {
    rgate.wait(sgate)
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
    let mut reply = recv_msg_from(rgate, sgate)?;
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
