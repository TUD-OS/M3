use arch::dtu;
use errors::Error;

extern {
    // pub fn gem5_shutdown(delay: u64);
    pub fn gem5_writefile(src: *const u8, len: u64, offset: u64, file: u64);
    pub fn gem5_readfile(dst: *mut u8, max: u64, offset: u64) -> i64;
}

pub fn read(buf: &mut [u8]) -> Result<usize, Error> {
    unsafe {
        Ok(gem5_readfile(buf.as_mut_ptr(), buf.len() as u64, 0) as usize)
    }
}

pub fn write(buf: &[u8]) -> Result<usize, Error> {
    dtu::DTU::print(buf);
    unsafe {
        // put the string on the stack to prevent that gem5_writefile causes a pagefault
        let file: [u8; 7] = *b"stdout\0";
        gem5_writefile(buf.as_ptr(), buf.len() as u64, 0, file.as_ptr() as u64);
    }
    Ok(buf.len())
}

pub fn init() {
}
