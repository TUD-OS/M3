use arch::env;
use cfg;
use errors::{Code, Error};
use kif::PEDesc;

pub const RECVBUF_SPACE: usize      = 0x3FC00000;
pub const RECVBUF_SIZE: usize       = 4 * cfg::PAGE_SIZE;
pub const RECVBUF_SIZE_SPM: usize   = 16384;

pub const SYSC_RBUF_ORD: i32        = 9;
pub const UPCALL_RBUF_ORD: i32      = 9;
pub const DEF_RBUF_ORD: i32         = 8;

pub const SYSC_RBUF_SIZE: usize     = 1 << SYSC_RBUF_ORD;
pub const UPCALL_RBUF_SIZE: usize   = 1 << UPCALL_RBUF_ORD;
pub const DEF_RBUF_SIZE: usize      = 1 << DEF_RBUF_ORD;

#[repr(C, packed)]
#[derive(Debug)]
pub struct RBufSpace {
    pub cur: usize,
    pub end: usize,
}

impl RBufSpace {
    pub fn new() -> Self {
        Self::new_with(0, 0)
    }

    pub fn new_with(cur: usize, end: usize) -> Self {
        RBufSpace {
            cur: cur,
            end: end,
        }
    }

    pub fn get_std(&mut self, off: usize, _size: usize) -> usize {
        let pe = env::data().pedesc();
        if pe.has_virtmem() {
            RECVBUF_SPACE + off
        }
        else {
            (pe.mem_size() - RECVBUF_SIZE_SPM) + off
        }
    }

    pub fn alloc(&mut self, pe: &PEDesc, size: usize) -> Result<usize, Error> {
        if self.end == 0 {
            let buf_sizes = SYSC_RBUF_SIZE + UPCALL_RBUF_SIZE + DEF_RBUF_SIZE;
            if pe.has_virtmem() {
                self.cur = RECVBUF_SPACE + buf_sizes;
                self.end = RECVBUF_SPACE + RECVBUF_SIZE;
            }
            else {
                self.cur = pe.mem_size() - RECVBUF_SIZE_SPM + buf_sizes;
                self.end = pe.mem_size();
            }
        }

        // TODO atm, the kernel allocates the complete receive buffer space
        let left = self.end - self.cur;
        if size > left {
            Err(Error::new(Code::NoSpace))
        }
        else {
            let res = self.cur;
            self.cur += size;
            Ok(res)
        }
    }

    pub fn free(&mut self, _addr: usize) {
        // TODO implement me
    }
}
