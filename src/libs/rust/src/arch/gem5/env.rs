use core::intrinsics;
use env;
use kif::PEDesc;
use util;

#[derive(Default)]
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

pub fn data() -> &'static mut EnvData {
    unsafe {
        intrinsics::transmute(0x6000 as u64)
    }
}

pub fn closure() -> &'static mut env::Closure {
    unsafe {
        intrinsics::transmute((0x6000 + util::size_of::<EnvData>()) as u64)
    }
}
