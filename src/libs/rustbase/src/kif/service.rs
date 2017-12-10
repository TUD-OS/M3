//! The service interface

use serialize::{Source, Unmarshallable};
use core::fmt;
use core::intrinsics;
use kif::syscalls;

int_enum! {
    /// The service calls
    pub struct Operation : u64 {
        const OPEN          = 0x0;
        const OBTAIN        = 0x1;
        const DELEGATE      = 0x2;
        const CLOSE         = 0x3;
        const SHUTDOWN      = 0x4;
    }
}

/// The open request message
#[repr(C, packed)]
pub struct Open {
    pub opcode: u64,
    pub arg: u64,
}

/// The open reply message
#[repr(C, packed)]
pub struct OpenReply {
    pub res: u64,
    pub sess: u64,
}

/// The data part of the delegate/obtain request messages
#[repr(C, packed)]
pub struct ExchangeData {
    pub caps: u64,
    pub argcount: u64,
    pub args: [u64; syscalls::MAX_EXCHG_ARGS],
}

/// The delegate/obtain request message
#[repr(C, packed)]
pub struct Exchange {
    pub opcode: u64,
    pub sess: u64,
    pub data: ExchangeData,
}

/// The delegate/obtain reply message
#[repr(C, packed)]
pub struct ExchangeReply {
    pub res: u64,
    pub data: ExchangeData,
}

/// The close request message
#[repr(C, packed)]
pub struct Close {
    pub opcode: u64,
    pub sess: u64,
}

/// The shutdown request message
#[repr(C, packed)]
pub struct Shutdown {
    pub opcode: u64,
}

impl fmt::Debug for ExchangeData {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "ExchangeData[")?;
        for i in 0..self.argcount {
            write!(f, "{}", self.args[i as usize])?;
            if i + 1 < self.argcount {
                write!(f, ", ")?;
            }
        }
        write!(f, "]")
    }
}

impl Unmarshallable for ExchangeData {
    fn unmarshall(s: &mut Source) -> Self {
        let mut res = ExchangeData {
            caps: s.pop_word(),
            argcount: s.pop_word(),
            args: unsafe { intrinsics::uninit() },
        };
        for i in 0..res.argcount {
            res.args[i as usize] = s.pop_word();
        }
        res
    }
}
