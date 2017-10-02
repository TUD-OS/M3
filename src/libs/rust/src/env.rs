use core::intrinsics;
use core::iter;
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

    // TODO
    // PEDesc pedesc,
    // uintptr_t isrs,
}

pub fn data() -> &'static EnvData {
    unsafe {
        intrinsics::transmute(0x6000 as u64)
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
