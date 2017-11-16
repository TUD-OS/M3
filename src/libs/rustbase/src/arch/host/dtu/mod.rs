use arch;
use core::intrinsics;
use core::ptr;
use errors::Error;
use libc;
use util;

mod backend;
mod thread;

pub type Reg    = u64;
pub type EpId   = usize;
pub type Label  = u64;
pub type PEId   = usize;

const PE_COUNT: usize           = 16;
const MAX_MSG_SIZE: usize       = 16 * 1024;

pub const EP_COUNT: EpId        = 12;

pub const SYSC_SEP: EpId        = 0;
pub const SYSC_REP: EpId        = 1;
pub const UPCALL_REP: EpId      = 2;
pub const DEF_REP: EpId         = 3;
pub const FIRST_FREE_EP: EpId   = 4;

int_enum! {
    struct CmdReg : Reg {
        const ADDR          = 0;
        const SIZE          = 1;
        const EPID          = 2;
        const CTRL          = 3;
        const OFFSET        = 4;
        const REPLY_LBL     = 5;
        const REPLY_EPID    = 6;
        const LENGTH        = 7;
    }
}

int_enum! {
    struct EpReg : Reg {
        // receive buffer registers
        const BUF_ADDR      = 0;
        const BUF_ORDER     = 1;
        const BUF_MSGORDER  = 2;
        const BUF_ROFF      = 3;
        const BUF_WOFF      = 4;
        const BUF_MSG_CNT   = 5;
        const BUF_MSG_ID    = 6;
        const BUF_UNREAD    = 7;
        const BUF_OCCUPIED  = 8;

        // for sending message and accessing memory
        const PE_ID         = 9;
        const EP_ID         = 10;
        const LABEL         = 11;
        const CREDITS       = 12;
        const MSGORDER      = 13;
    }
}

int_enum! {
    struct Command : Reg {
        const READ          = 1;
        const WRITE         = 2;
        const SEND          = 3;
        const REPLY         = 4;
        const RESP          = 5;
        const FETCH_MSG     = 6;
        const ACK_MSG       = 7;
    }
}

impl From<u8> for Command {
    fn from(cmd: u8) -> Self {
        unsafe { intrinsics::transmute(cmd as Reg) }
    }
}

bitflags! {
    struct Control : Reg {
        const NONE        = 0b000;
        const START       = 0b001;
        const REPLY_CAP   = 0b010;
    }
}

bitflags! {
    pub struct CmdFlags : u64 {
        const NOPF        = 0x1;
    }
}

#[repr(C, packed)]
#[derive(Debug)]
pub struct Header {
    pub length: usize,
    pub opcode: u8,
    pub label: Label,
    pub has_replycap: u8,
    pub pe: u16,
    pub rpl_ep: u8,
    pub snd_ep: u8,
    pub reply_label: Label,
    pub credits: u8,
    pub crd_ep: u8,
}

impl Header {
    const fn new() -> Header {
        Header {
            length: 0,
            opcode: 0,
            label: 0,
            has_replycap: 0,
            pe: 0,
            rpl_ep: 0,
            snd_ep: 0,
            reply_label: 0,
            credits: 0,
            crd_ep: 0,
        }
    }
}

#[repr(C, packed)]
#[derive(Debug)]
pub struct Message {
    pub header: Header,
    pub data: [u8],
}

const CMD_RCNT: usize = 8;
const EPS_RCNT: usize = 14;

static mut CMD_REGS: [Reg; CMD_RCNT] = [0; CMD_RCNT];
static mut EP_REGS: [Reg; EPS_RCNT * EP_COUNT] = [0; EPS_RCNT * EP_COUNT];

pub struct DTU {
}

impl DTU {
    pub fn send(ep: EpId, msg: *const u8, size: usize, reply_lbl: Label, reply_ep: EpId) -> Result<(), Error> {
        Self::fire(ep, Command::SEND, msg, size, 0, 0, reply_lbl, reply_ep)
    }

    pub fn reply(ep: EpId, reply: *const u8, size: usize, msg: &'static Message) -> Result<(), Error> {
        let msg_addr = msg as *const Message as *const u8 as usize;
        Self::fire(ep, Command::REPLY, reply, size, msg_addr, 0, 0, 0)?;
        Self::mark_read(ep, msg);
        Ok(())
    }

    pub fn read(ep: EpId, data: *mut u8, size: usize, off: usize, _flags: CmdFlags) -> Result<(), Error> {
        Self::fire(ep, Command::READ, data, size, off, size, 0, 0)
    }

    pub fn write(ep: EpId, data: *const u8, size: usize, off: usize, _flags: CmdFlags) -> Result<(), Error> {
        Self::fire(ep, Command::WRITE, data, size, off, size, 0, 0)
    }

    pub fn fetch_msg(ep: EpId) -> Option<&'static Message> {
        if Self::get_ep(ep, EpReg::BUF_MSG_CNT) == 0 {
            return None;
        }

        Self::set_cmd(CmdReg::EPID, ep as Reg);
        Self::set_cmd(CmdReg::CTRL, (Command::FETCH_MSG.val << 3) | Control::START.bits);
        if Self::wait_until_ready().is_err() {
            return None;
        }

        let msg = Self::get_cmd(CmdReg::OFFSET);
        if msg != 0 {
            unsafe {
                let head: *const Header = intrinsics::transmute(msg);
                let msg_len = (*head).length as usize;
                let fat_ptr: u128 = (msg as u128) | (msg_len as u128) << 64;
                Some(intrinsics::transmute(fat_ptr))
            }
        }
        else {
            None
        }
    }

    pub fn is_valid(_ep: EpId) -> bool {
        true
    }

    pub fn mark_read(ep: EpId, msg: &Message) {
        let msg_addr = msg as *const Message as *const u8 as usize;
        Self::set_cmd(CmdReg::EPID, ep as Reg);
        Self::set_cmd(CmdReg::OFFSET, msg_addr as Reg);
        Self::set_cmd(CmdReg::CTRL, (Command::ACK_MSG.val << 3) | Control::START.bits);
        Self::wait_until_ready().unwrap();
    }

    pub fn try_sleep(_yield: bool, _cycles: u64) -> Result<(), Error> {
        unsafe { libc::usleep(1) };
        Ok(())
    }

    pub fn configure(ep: EpId, lbl: Label, pe: PEId, dst_ep: EpId, crd: u64, msg_order: i32) {
        Self::set_ep(ep, EpReg::LABEL, lbl);
        Self::set_ep(ep, EpReg::PE_ID, pe as Reg);
        Self::set_ep(ep, EpReg::EP_ID, dst_ep as Reg);
        Self::set_ep(ep, EpReg::CREDITS, crd);
        Self::set_ep(ep, EpReg::MSGORDER, msg_order as Reg);
    }
    pub fn configure_recv(ep: EpId, buf: usize, order: i32, msg_order: i32) {
        Self::set_ep(ep, EpReg::BUF_ADDR, buf as Reg);
        Self::set_ep(ep, EpReg::BUF_ORDER, order as Reg);
        Self::set_ep(ep, EpReg::BUF_MSGORDER, msg_order as Reg);
        Self::set_ep(ep, EpReg::BUF_ROFF, 0);
        Self::set_ep(ep, EpReg::BUF_WOFF, 0);
        Self::set_ep(ep, EpReg::BUF_MSG_CNT, 0);
        Self::set_ep(ep, EpReg::BUF_UNREAD, 0);
        Self::set_ep(ep, EpReg::BUF_OCCUPIED, 0);
    }

    fn fire(ep: EpId, cmd: Command, msg: *const u8, size: usize, off: usize, len: usize,
            reply_lbl: Label, reply_ep: EpId) -> Result<(), Error> {
        Self::set_cmd(CmdReg::ADDR, msg as Reg);
        Self::set_cmd(CmdReg::SIZE, size as Reg);
        Self::set_cmd(CmdReg::EPID, ep as Reg);
        Self::set_cmd(CmdReg::OFFSET, off as Reg);
        Self::set_cmd(CmdReg::LENGTH, len as Reg);
        Self::set_cmd(CmdReg::REPLY_LBL, reply_lbl as Reg);
        Self::set_cmd(CmdReg::REPLY_EPID, reply_ep as Reg);
        if cmd == Command::REPLY {
            Self::set_cmd(CmdReg::CTRL, (cmd.val << 3) | Control::START.bits);
        }
        else {
            Self::set_cmd(CmdReg::CTRL, (cmd.val << 3) | (Control::START | Control::REPLY_CAP).bits);
        }
        Self::wait_until_ready()
    }

    fn wait_until_ready() -> Result<(), Error> {
        loop {
            let cmd = Self::get_cmd(CmdReg::CTRL);
            if (cmd & 0xFFFF) == 0 {
                let err = cmd >> 16;
                if err != 0 {
                    return Err(Error::from(err as u32))
                }
                return Ok(())
            }
        }
    }

    fn get_cmd(cmd: CmdReg) -> Reg {
        unsafe {
            ptr::read_volatile(&CMD_REGS[cmd.val as usize])
        }
    }
    fn set_cmd(cmd: CmdReg, val: Reg) {
        unsafe {
            ptr::write_volatile(&mut CMD_REGS[cmd.val as usize], val)
        }
    }

    fn get_ep(ep: EpId, reg: EpReg) -> Reg {
        unsafe {
            ptr::read_volatile(&EP_REGS[ep * EPS_RCNT + reg.val as usize])
        }
    }
    fn set_ep(ep: EpId, reg: EpReg, val: Reg) {
        unsafe {
            ptr::write_volatile(&mut EP_REGS[ep * EPS_RCNT + reg.val as usize], val)
        }
    }
}

pub fn ep_regs_addr() -> usize {
    unsafe {
        EP_REGS.as_ptr() as usize
    }
}

pub fn init() {
    thread::init();
}

pub fn deinit() {
    thread::deinit();
}
