use arch::env;
use col::Vec;
use errors::Error;
use kif::PEDesc;

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
        self.alloc(&env::get().pe_desc(), size).unwrap()
    }

    pub fn alloc(&mut self, _pe: &PEDesc, size: usize) -> Result<usize, Error> {
        let buf = vec![0u8; size];
        let res = buf.as_ptr() as usize;
        self.bufs.push(buf);
        Ok(res)
    }

    pub fn free(&mut self, addr: usize, _size: usize) {
        self.bufs.retain(|ref b| b.as_ptr() as usize != addr);
    }
}
