use io::serial::Serial;
use vfs::{Fd, FileRef, BufReader, BufWriter};
use vpe::VPE;

pub const STDIN_FILENO: Fd      = 0;
pub const STDOUT_FILENO: Fd     = 1;
pub const STDERR_FILENO: Fd     = 2;

static mut STDIN : Option<BufReader<FileRef>> = None;
static mut STDOUT: Option<BufWriter<FileRef>> = None;
static mut STDERR: Option<BufWriter<FileRef>> = None;

pub fn stdin() -> &'static mut BufReader<FileRef> {
    unsafe { STDIN.as_mut().unwrap() }
}
pub fn stdout() -> &'static mut BufWriter<FileRef> {
    unsafe { STDOUT.as_mut().unwrap() }
}
pub fn stderr() -> &'static mut BufWriter<FileRef> {
    unsafe { STDERR.as_mut().unwrap() }
}

pub fn init() {
    let create_in = |fd| {
        match VPE::cur().files().get(fd) {
            Some(f) => Some(BufReader::new(FileRef::new(f, fd))),
            None    => Some(BufReader::new(FileRef::new(Serial::get().clone(), fd))),
        }
    };
    let create_out = |fd| {
        match VPE::cur().files().get(fd) {
            Some(f) => Some(BufWriter::new(FileRef::new(f, fd))),
            None    => Some(BufWriter::new(FileRef::new(Serial::get().clone(), fd))),
        }
    };

    unsafe {
        STDIN  = create_in(STDIN_FILENO);
        STDOUT = create_out(STDOUT_FILENO);
        STDERR = create_out(STDERR_FILENO);
    }
}
