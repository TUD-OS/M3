use cell::MutCell;
use io::Serial;
use vfs::{Fd, FileRef, BufReader, BufWriter};
use vpe::VPE;

pub const STDIN_FILENO: Fd      = 0;
pub const STDOUT_FILENO: Fd     = 1;
pub const STDERR_FILENO: Fd     = 2;

static STDIN : MutCell<Option<BufReader<FileRef>>> = MutCell::new(None);
static STDOUT: MutCell<Option<BufWriter<FileRef>>> = MutCell::new(None);
static STDERR: MutCell<Option<BufWriter<FileRef>>> = MutCell::new(None);

pub fn stdin() -> &'static mut BufReader<FileRef> {
    STDIN.get_mut().as_mut().unwrap()
}
pub fn stdout() -> &'static mut BufWriter<FileRef> {
    STDOUT.get_mut().as_mut().unwrap()
}
pub fn stderr() -> &'static mut BufWriter<FileRef> {
    STDERR.get_mut().as_mut().unwrap()
}

pub fn init() {
    let create_in = |fd| {
        match VPE::cur().files().get(fd) {
            Some(f) => Some(BufReader::new(FileRef::new(f, fd))),
            None    => Some(BufReader::new(FileRef::new(Serial::new(), fd))),
        }
    };
    let create_out = |fd| {
        match VPE::cur().files().get(fd) {
            Some(f) => Some(BufWriter::new(FileRef::new(f, fd))),
            None    => Some(BufWriter::new(FileRef::new(Serial::new(), fd))),
        }
    };

    STDIN .set(create_in(STDIN_FILENO));
    STDOUT.set(create_out(STDOUT_FILENO));
    STDERR.set(create_out(STDERR_FILENO));
}
