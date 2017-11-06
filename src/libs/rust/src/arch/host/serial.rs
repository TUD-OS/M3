use errors::Error;
use libc;

static mut LOG_FD: i32 = -1;

pub fn read(buf: &mut [u8]) -> Result<usize, Error> {
    match unsafe { libc::read(0, buf.as_mut_ptr() as *mut libc::c_void, buf.len()) } {
        res if res < 0 => Err(Error::ReadFailed),
        res            => Ok(res as usize),
    }
}

pub fn write(buf: &[u8]) -> Result<usize, Error> {
    match unsafe { libc::write(LOG_FD, buf.as_ptr() as *const libc::c_void, buf.len()) } {
        res if res < 0 => Err(Error::WriteFailed),
        res            => Ok(res as usize),
    }
}

pub fn init() {
    unsafe {
        LOG_FD = libc::open(
            "run/log.txt\0".as_ptr() as *const libc::c_char,
            libc::O_WRONLY | libc::O_APPEND
        );
        assert!(LOG_FD != -1);
    }
}
