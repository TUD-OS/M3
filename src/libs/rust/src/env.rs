use core::intrinsics;
use core::mem;
use boxed::{Box, FnBox};
use core::iter;
use kif::PEDesc;
use util;

#[repr(C, packed)]
pub struct EnvData {
    pub pe: u64,
    pub argc: u32,
    pub argv: u64,

    pub sp: u64,
    pub entry: u64,
    pub lambda: u64,
    pub pager_sess: u32,
    pub pager_sgate: u32,
    pub pager_rgate: u32,
    pub mounts_len: u32,
    pub mounts: u64,
    pub fds_len: u32,
    pub fds: u64,
    pub rbuf_cur: u64,
    pub rbuf_end: u64,
    pub eps: u64,
    pub caps: u64,
    pub exit_addr: u64,
    pub heap_size: u64,

    pub _backend: u64,
    pub kenv: u64,

    pub pedesc: PEDesc,
    pub isrs: u64,
}

impl EnvData {
    pub fn new() -> Self {
        EnvData {
            pe: 0,
            argc: 0,
            argv: 0,
            sp: 0,
            entry: 0,
            lambda: 0,
            pager_sess: 0,
            pager_sgate: 0,
            pager_rgate: 0,
            mounts_len: 0,
            mounts: 0,
            fds_len: 0,
            fds: 0,
            rbuf_cur: 0,
            rbuf_end: 0,
            eps: 0,
            caps: 0,
            exit_addr: 0,
            heap_size: 0,
            _backend: 0,
            kenv: 0,
            pedesc: PEDesc::new(),
            isrs: 0,
        }
    }
}

pub fn data() -> &'static mut EnvData {
    unsafe {
        intrinsics::transmute(0x6000 as u64)
    }
}

pub struct Closure {
    func: Option<Box<FnBox() -> i32 + Send>>,
}

impl Closure {
    pub fn new<F>(func: Box<F>) -> Self
                  where F: FnBox() -> i32, F: Send + 'static {
        Closure {
            func: Some(func),
        }
    }

    pub fn call(&mut self) -> i32 {
        let old = mem::replace(&mut self.func, None);
        old.unwrap().call_box(())
    }
}

pub fn closure() -> &'static mut Closure {
    unsafe {
        intrinsics::transmute((0x6000 + util::size_of::<EnvData>()) as u64)
    }
}

pub struct Args {
    pos: isize,
}

impl Args {
    fn arg(&self, idx: isize) -> &'static str {
        unsafe {
            let args: *const *const u8 = intrinsics::transmute(data().argv as *const u8);
            let arg = *args.offset(idx);
            util::cstr_to_str(arg)
        }
    }

    pub fn len(&self) -> usize {
        data().argc as usize
    }
}

impl iter::Iterator for Args {
    type Item = &'static str;

    fn next(&mut self) -> Option<Self::Item> {
        if self.pos < data().argc as isize {
            let arg = self.arg(self.pos);
            self.pos += 1;
            Some(arg)
        }
        else {
            None
        }
    }
}

pub fn args() -> Args {
    Args {
        pos: 0,
    }
}
