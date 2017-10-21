use com::Unmarshallable;
use com::GateIStream;
use core::fmt;
use core::intrinsics;
use kif::syscalls;

int_enum! {
    pub struct Operation : u64 {
        const OPEN          = 0x0;
        const OBTAIN        = 0x1;
        const DELEGATE      = 0x2;
        const CLOSE         = 0x3;
        const SHUTDOWN      = 0x4;
    }
}

#[repr(C, packed)]
pub struct Open {
    pub opcode: u64,
    pub arg: u64,
}

#[repr(C, packed)]
pub struct OpenReply {
    pub res: u64,
    pub sess: u64,
}

#[repr(C, packed)]
pub struct ExchangeData {
    pub caps: u64,
    pub argcount: u64,
    pub args: [u64; syscalls::MAX_EXCHG_ARGS],
}

#[repr(C, packed)]
pub struct Exchange {
    pub opcode: u64,
    pub sess: u64,
    pub data: ExchangeData,
}

#[repr(C, packed)]
pub struct ExchangeReply {
    pub res: u64,
    pub data: ExchangeData,
}

#[repr(C, packed)]
pub struct Close {
    pub opcode: u64,
    pub sess: u64,
}

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
    fn unmarshall(is: &mut GateIStream) -> Self {
        let mut res = ExchangeData {
            caps: is.pop(),
            argcount: is.pop(),
            args: unsafe { intrinsics::uninit() },
        };
        for i in 0..res.argcount {
            res.args[i as usize] = is.pop();
        }
        res
    }
}
