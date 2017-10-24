use cfg;
use errors::Error;
use kif::pedesc::PEDesc;

pub const RECVBUF_SPACE: usize      = 0x3FC00000;
pub const RECVBUF_SIZE: usize       = 4 * cfg::PAGE_SIZE;
pub const RECVBUF_SIZE_SPM: usize   = 16384;

pub const SYSC_RBUF_SIZE: usize     = 1 << 9;
pub const UPCALL_RBUF_SIZE: usize   = 1 << 9;
pub const DEF_RBUF_SIZE: usize      = 1 << 8;

#[repr(C, packed)]
#[derive(Debug)]
pub struct RBufSpace {
    pub cur: usize,
    pub end: usize,
}

impl RBufSpace {
    pub fn new(cur: usize, end: usize) -> Self {
        RBufSpace {
            cur: cur,
            end: end,
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
            Err(Error::NoSpace)
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
