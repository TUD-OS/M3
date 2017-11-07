use backtrace;
use boxed::Box;
use core::intrinsics;
use core::fmt;

const MAX_BT_LEN: usize = 16;

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum Code {
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
    ReadFailed,
    WriteFailed,
}

#[derive(Clone, Copy)]
pub struct ErrorInfo {
    code: Code,
    bt_len: usize,
    bt: [usize; MAX_BT_LEN],
}

impl ErrorInfo {
    pub fn new(code: Code) -> Self {
        let mut bt = [0usize; MAX_BT_LEN];
        let count = backtrace::collect(bt.as_mut());

        ErrorInfo {
            code: code,
            bt_len: count,
            bt: bt,
        }
    }
}

#[derive(Clone)]
pub struct Error {
    info: Box<ErrorInfo>,
}

impl Error {
    pub fn new(code: Code) -> Self {
        Error {
            info: Box::new(ErrorInfo::new(code)),
        }
    }

    pub fn code(&self) -> Code {
        self.info.code
    }
    pub fn backtrace(&self) -> &[usize] {
        self.info.bt.as_ref()
    }

    fn debug(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?} at:\n", self.code())?;
        for i in 0..self.info.bt_len {
            write!(f, "  {:#x}\n", self.info.bt[i as usize])?;
        }
        Ok(())
    }
}

impl From<u32> for Error {
    fn from(error: u32) -> Self {
        Self::new(Code::from(error))
    }
}

impl From<u32> for Code {
    fn from(error: u32) -> Self {
        // TODO better way?
        unsafe { intrinsics::transmute(error as u8) }
    }
}

impl PartialEq for Error {
    fn eq(&self, other: &Error) -> bool {
        self.code() == other.code()
    }
}

impl fmt::Debug for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.debug(f)
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.debug(f)
    }
}
