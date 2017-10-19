use core::intrinsics;
use core::fmt;

#[derive(Debug, PartialEq, Clone, Copy)]
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
    WriteFailed,
}

impl From<u32> for Error {
    fn from(error: u32) -> Self {
        // TODO better way?
        unsafe { intrinsics::transmute(error as u8) }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}", self)
    }
}
