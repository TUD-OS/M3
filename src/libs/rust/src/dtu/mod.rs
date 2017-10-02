use core::intrinsics;
use core::ptr;
use errors::Error;
use util;

pub type Reg    = u64;
pub type EpId   = usize;
pub type Label  = u64;

const BASE_ADDR: usize      = 0xF0000000;
// TODO move that elsewhere
pub const PAGE_SIZE: usize  = 0x1000;
const DTU_REGS: usize       = 8;
// const REQ_REGS: usize    = 3;
const CMD_REGS: usize       = 5;
const EP_REGS: usize        = 3;

pub enum DtuReg {
    Features    = 0,
    RootPt      = 1,
    PfEp        = 2,
    VpeId       = 3,
    CurTime     = 4,
    IdleTime    = 5,
    MsgCnt      = 6,
    ExtCmd      = 7,
}

pub enum ReqReg {
    ExtReq      = 0,
    XlateReq    = 1,
    XlateResp   = 2,
}

pub enum CmdReg {
    Command     = 0,
    Abort       = 1,
    Data        = 2,
    Offset      = 3,
    ReplyLabel  = 4,
}

#[allow(dead_code)]
#[derive(PartialEq)]
enum CmdOpCode {
    Idle        = 0,
    Send        = 1,
    Reply       = 2,
    Read        = 3,
    Write       = 4,
    FetchMsg    = 5,
    AckMsg      = 6,
    Sleep       = 7,
    ClearIrq    = 8,
    Print       = 9,
}

impl From<u64> for CmdOpCode {
    fn from(error: u64) -> Self {
        // TODO better way?
        unsafe { intrinsics::transmute(error as u8) }
    }
}

#[repr(C, packed)]
#[derive(Debug)]
pub struct Header {
    pub flags: u8,      // if bit 0 is set its a reply, if bit 1 is set we grant credits
    pub sender_pe: u8,
    pub sender_ep: u8,
    pub reply_ep: u8,   // for a normal message this is the reply epId
                        // for a reply this is the enpoint that receives credits
    pub length: u16,
    pub sender_vpe_id: u16,

    pub reply_label: u64,
    pub label: u64,
}

#[repr(C, packed)]
#[derive(Debug)]
pub struct Message {
    pub header: Header,
    pub data: [char],
}

pub struct DTU {
}

impl DTU {
    pub fn send<T>(ep: EpId, msg: T, reply_lbl: Label, reply_ep: EpId) -> Result<(), Error> {
        let ptr: *const T = &msg as *const T;
        Self::write_cmd_reg(CmdReg::Data, Self::build_data(
            ptr as *const u8, util::size_of_val(&msg)
        ));
        if reply_lbl != 0 {
            Self::write_cmd_reg(CmdReg::ReplyLabel, reply_lbl);
        }
        Self::write_cmd_reg(CmdReg::Command, Self::build_cmd(
            ep, CmdOpCode::Send, 0, reply_ep as Reg
        ));

        Self::get_error()
    }

    pub fn fetch_msg(ep: EpId) -> Option<&'static Message> {
        Self::write_cmd_reg(CmdReg::Command, Self::build_cmd(ep, CmdOpCode::FetchMsg, 0, 0));
        let msg = Self::read_cmd_reg(CmdReg::Offset);
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

    pub fn mark_read(ep: EpId, msg: &Message) {
        let off = (msg as *const Message) as *const u8 as usize as Reg;
        Self::write_cmd_reg(CmdReg::Command, Self::build_cmd(ep, CmdOpCode::AckMsg, 0, off));
    }

    pub fn get_error() -> Result<(), Error> {
        loop {
            let cmd = Self::read_cmd_reg(CmdReg::Command);
            let opcode = CmdOpCode::from(cmd & 0xF);
            if opcode == CmdOpCode::Idle {
                let err = (cmd >> 13) & 0x7;
                if err != 0 {
                    return Err(Error::from(err))
                }
                return Ok(())
            }
        }
    }

    pub fn read_dtu_reg(reg: DtuReg) -> Reg {
        Self::read_reg(reg as usize)
    }
    pub fn read_req_reg(reg: ReqReg) -> Reg {
        Self::read_reg((PAGE_SIZE / 8) + (reg as usize))
    }
    pub fn read_cmd_reg(reg: CmdReg) -> Reg {
        Self::read_reg(DTU_REGS + reg as usize)
    }
    pub fn read_ep_reg(ep: EpId, reg: usize) -> Reg {
        Self::read_reg(DTU_REGS + CMD_REGS + EP_REGS * ep + reg)
    }

    pub fn write_cmd_reg(reg: CmdReg, val: Reg) {
        Self::write_reg(DTU_REGS + reg as usize, val)
    }

    pub fn read_reg(idx: usize) -> Reg {
        unsafe {
            let addr: *const Reg = (BASE_ADDR + idx * 8) as *const Reg;
            ptr::read_volatile(addr)
        }
    }

    pub fn write_reg(idx: usize, val: Reg) {
        unsafe {
            let addr: *mut Reg = (BASE_ADDR + idx * 8) as *mut Reg;
            ptr::write_volatile(addr, val);
        }
    }

    fn build_data(addr: *const u8, size: usize) -> Reg {
        (addr as usize | (size << 48)) as Reg
    }

    fn build_cmd(ep: EpId, c: CmdOpCode, flags: Reg, arg: Reg) -> Reg {
        c as Reg | ((ep as Reg) << 4) | (flags << 12) | (arg << 16)
    }
}
