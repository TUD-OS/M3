use col::Vec;
use envdata;
use errors::Error;
use kif::PEDesc;

pub const SYSC_RBUF_ORD: i32        = 9;
pub const UPCALL_RBUF_ORD: i32      = 9;
pub const DEF_RBUF_ORD: i32         = 8;

pub const SYSC_RBUF_SIZE: usize     = 1 << SYSC_RBUF_ORD;
pub const UPCALL_RBUF_SIZE: usize   = 1 << UPCALL_RBUF_ORD;
pub const DEF_RBUF_SIZE: usize      = 1 << DEF_RBUF_ORD;

#[repr(C, packed)]
#[derive(Debug)]
pub struct RBufSpace {
    bufs: Vec<Vec<u8>>,
}

impl RBufSpace {
    pub fn new() -> Self {
        RBufSpace {
            bufs: vec![],
        }
    }

    pub fn get_std(&mut self, _off: usize, size: usize) -> usize {
        self.alloc(&envdata::get().pe_desc(), size).unwrap()
    }

    pub fn alloc(&mut self, _pe: &PEDesc, size: usize) -> Result<usize, Error> {
        let buf = vec![0u8; size];
        let res = buf.as_ptr() as usize;
        self.bufs.push(buf);
        Ok(res)
    }

    pub fn free(&mut self, _addr: usize) {
        // TODO implement me
    }
}
