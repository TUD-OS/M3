use core::intrinsics;
use core::fmt;

#[derive(Debug)]
pub enum Error {
    // DTU errors
    MissCredits = 1,
    NoRingSpace,
    VPEGone,
    Pagefault,
    NoMapping,
    InvEP,
    Abort,
    // SW Errors
    InvArgs,
    OutOfMem,
    NoSuchFile,
    NoPerm,
    NotSup,
    NoFreePE,
    InvalidElf,
    NoSpace,
    Exists,
    XfsLink,
    DirNotEmpty,
    IsDir,
    IsNoDir,
    EPInvalid,
    RecvGone,
    EndOfFile,
    MsgsWaiting,
    UpcallReply,
    CommitFailed,
}

impl From<u64> for Error {
    fn from(error: u64) -> Self {
        // TODO better way?
        unsafe { intrinsics::transmute(error as u8) }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}", self)
    }
}
