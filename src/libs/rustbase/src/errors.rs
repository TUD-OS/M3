/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

//! Contains the error handling types

use backtrace;
use boxed::Box;
use core::intrinsics;
use core::fmt;

const MAX_BT_LEN: usize = 16;

/// The error codes
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
    InvalidFs,
}

/// The struct that stores information about an occurred error
#[derive(Clone, Copy)]
pub struct ErrorInfo {
    code: Code,
    bt_len: usize,
    bt: [usize; MAX_BT_LEN],
}

impl ErrorInfo {
    /// Creates a new object for given error code
    ///
    /// Note that this gathers and stores the backtrace
    #[inline(never)]
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

/// The error struct that is passed around
#[derive(Clone)]
pub struct Error {
    info: Box<ErrorInfo>,
}

impl Error {
    /// Creates a new object for given error code
    ///
    /// Note that this gathers and stores the backtrace
    pub fn new(code: Code) -> Self {
        Error {
            info: Box::new(ErrorInfo::new(code)),
        }
    }

    /// Returns the error code
    pub fn code(&self) -> Code {
        self.info.code
    }
    /// Returns the backtrace to the location where the error occurred
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
