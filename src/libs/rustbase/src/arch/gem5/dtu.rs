use core::intrinsics;
use core::ptr;
use errors::Error;
use util;

pub type Reg    = u64;
pub type EpId   = usize;
pub type Label  = u64;
pub type PEId   = usize;

pub const EP_COUNT: EpId        = 12;

pub const SYSC_SEP: EpId        = 0;
pub const SYSC_REP: EpId        = 1;
pub const UPCALL_REP: EpId      = 2;
pub const DEF_REP: EpId         = 3;
pub const FIRST_FREE_EP: EpId   = 4;

pub const BASE_ADDR: usize      = 0xF0000000;
pub const DTU_REGS: usize       = 8;
// const REQ_REGS: usize        = 3;
pub const CMD_REGS: usize       = 5;
pub const EP_REGS: usize        = 3;
pub const HEADER_COUNT: usize   = 128;

pub const CREDITS_UNLIM: u64    = 0xFFFF;

// actual max is 64k - 1; use less for better alignment
const MAX_PKT_SIZE: usize       = 60 * 1024;

int_enum! {
    pub struct DtuReg : Reg {
        const STATUS    = 0;
        const ROOT_PT     = 1;
        const PF_EP       = 2;
        const VPE_ID      = 3;
        const CUR_TIME    = 4;
        const IDLE_TIME   = 5;
        const MSG_CNT     = 6;
        const EXT_CMD     = 7;
    }
}

#[allow(dead_code)]
bitflags! {
    struct StatusFlags : Reg {
        const PRIV         = 1 << 0;
        const PAGEFAULTS   = 1 << 1;
        const COM_DISABLED = 1 << 2;
        const IRQ_WAKEUP   = 1 << 3;
    }
}

#[allow(dead_code)]
int_enum! {
    struct ReqReg : Reg {
        const EXT_REQ     = 0x0;
        const XLATE_REQ   = 0x1;
        const XLATER_ESP  = 0x2;
    }
}

#[allow(dead_code)]
int_enum! {
    struct CmdReg : Reg {
        const COMMAND     = 0x0;
        const ABORT       = 0x1;
        const DATA        = 0x2;
        const OFFSET      = 0x3;
        const REPLY_LABEL = 0x4;
    }
}

int_enum! {
    struct CmdOpCode : u64 {
        const IDLE        = 0x0;
        const SEND        = 0x1;
        const REPLY       = 0x2;
        const READ        = 0x3;
        const WRITE       = 0x4;
        const FETCH_MSG   = 0x5;
        const ACK_MSG     = 0x6;
        const SLEEP       = 0x7;
        const CLEAR_IRQ   = 0x8;
        const PRINT       = 0x9;
    }
}

bitflags! {
    pub struct CmdFlags : u64 {
        const NOPF        = 0x1;
    }
}

int_enum! {
    pub struct EpType : u64 {
        const INVALID     = 0x0;
        const SEND        = 0x1;
        const RECEIVE     = 0x2;
        const MEMORY      = 0x3;
    }
}

int_enum! {
    pub struct ExtCmdOpCode : Reg {
        const IDLE        = 0;
        const WAKEUP_CORE = 1;
        const INV_EP      = 2;
        const INV_PAGE    = 3;
        const INV_TLB     = 4;
        const RESET       = 5;
        const ACK_MSG     = 6;
    }
}

#[repr(C, packed)]
#[derive(Copy, Clone, Default, Debug)]
pub struct ReplyHeader {
    pub flags: u8,      // if bit 0 is set its a reply, if bit 1 is set we grant credits
    pub sender_pe: u8,
    pub sender_ep: u8,
    pub reply_ep: u8,   // for a normal message this is the reply epId
                        // for a reply this is the enpoint that receives credits
    pub length: u16,
    pub sender_vpe_id: u16,

    pub reply_label: u64,
}

#[repr(C, packed)]
#[derive(Copy, Clone, Default, Debug)]
pub struct Header {
    pub flags: u8,
    pub sender_pe: u8,
    pub sender_ep: u8,
    pub reply_ep: u8,

    pub length: u16,
    pub sender_vpe_id: u16,

    pub reply_label: u64,
    pub label: u64,
}

#[repr(C, packed)]
#[derive(Debug)]
pub struct Message {
    pub header: Header,
    pub data: [u8],
}

pub struct DTU {
}

impl DTU {
    pub fn send(ep: EpId, msg: *const u8, size: usize, reply_lbl: Label, reply_ep: EpId) -> Result<(), Error> {
        Self::write_cmd_reg(CmdReg::DATA, Self::build_data(msg, size));
        if reply_lbl != 0 {
            Self::write_cmd_reg(CmdReg::REPLY_LABEL, reply_lbl);
        }
        Self::write_cmd_reg(CmdReg::COMMAND, Self::build_cmd(
            ep, CmdOpCode::SEND, 0, reply_ep as Reg
        ));

        Self::get_error()
    }

    pub fn reply(ep: EpId, reply: *const u8, size: usize, msg: &'static Message) -> Result<(), Error> {
        Self::write_cmd_reg(CmdReg::DATA, Self::build_data(reply, size));
        let slice: u128 = unsafe { intrinsics::transmute(msg) };
        Self::write_cmd_reg(CmdReg::COMMAND, Self::build_cmd(
            ep, CmdOpCode::REPLY, 0, slice as u64
        ));

        Self::get_error()
    }

    pub fn read(ep: EpId, data: *mut u8, size: usize, off: usize, flags: CmdFlags) -> Result<(), Error> {
        let cmd = Self::build_cmd(ep, CmdOpCode::READ, flags.bits(), 0);
        let res = Self::transfer(cmd, data as usize, size, off);
        unsafe { intrinsics::atomic_fence_rel() };
        res
    }

    pub fn write(ep: EpId, data: *const u8, size: usize, off: usize, flags: CmdFlags) -> Result<(), Error> {
        let cmd = Self::build_cmd(ep, CmdOpCode::WRITE, flags.bits(), 0);
        Self::transfer(cmd, data as usize, size, off)
    }

    fn transfer(cmd: Reg, data: usize, size: usize, off: usize) -> Result<(), Error> {
        let mut left = size;
        let mut offset = off;
        let mut data_addr = data;
        while left > 0 {
            let amount = util::min(left, MAX_PKT_SIZE);
            Self::write_cmd_reg(CmdReg::DATA, (data_addr | (amount << 48)) as Reg);
            Self::write_cmd_reg(CmdReg::COMMAND, cmd | (offset << 16) as Reg);

            left -= amount;
            offset += amount;
            data_addr += amount;

            Self::get_error()?;
        }
        Ok(())
    }

    pub fn fetch_msg(ep: EpId) -> Option<&'static Message> {
        Self::write_cmd_reg(CmdReg::COMMAND, Self::build_cmd(ep, CmdOpCode::FETCH_MSG, 0, 0));
        let msg = Self::read_cmd_reg(CmdReg::OFFSET);
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

    pub fn is_valid(ep: EpId) -> bool {
        let r0 = Self::read_ep_reg(ep, 0);
        (r0 >> 61) != EpType::INVALID.val
    }

    pub fn mark_read(ep: EpId, msg: &Message) {
        let off = (msg as *const Message) as *const u8 as usize as Reg;
        Self::write_cmd_reg(CmdReg::COMMAND, Self::build_cmd(ep, CmdOpCode::ACK_MSG, 0, off));
    }

    pub fn get_error() -> Result<(), Error> {
        loop {
            let cmd = Self::read_cmd_reg(CmdReg::COMMAND);
            if (cmd & 0xF) == CmdOpCode::IDLE.val {
                let err = (cmd >> 13) & 0x7;
                return if err == 0 {
                    Ok(())
                }
                else {
                    Err(Error::from(err as u32))
                }
            }
        }
    }

    pub fn try_sleep(_yield: bool, cycles: u64) -> Result<(), Error> {
        for _ in 0..100 {
            if Self::read_dtu_reg(DtuReg::MSG_CNT) > 0 {
                return Ok(())
            }
        }

        // TODO yield

        Self::sleep(cycles)
    }

    pub fn sleep(cycles: u64) -> Result<(), Error> {
        Self::write_cmd_reg(CmdReg::COMMAND, Self::build_cmd(0, CmdOpCode::SLEEP, 0, cycles));
        Self::get_error()
    }

    pub fn print(s: &[u8]) {
        Self::write_cmd_reg(CmdReg::DATA, (s.as_ptr() as usize | (s.len() << 48)) as Reg);
        Self::write_cmd_reg(CmdReg::COMMAND, Self::build_cmd(0, CmdOpCode::PRINT, 0, 0));
    }

    fn read_dtu_reg(reg: DtuReg) -> Reg {
        Self::read_reg(reg.val as usize)
    }
    fn read_cmd_reg(reg: CmdReg) -> Reg {
        Self::read_reg(DTU_REGS + reg.val as usize)
    }
    fn read_ep_reg(ep: EpId, reg: usize) -> Reg {
        Self::read_reg(DTU_REGS + CMD_REGS + EP_REGS * ep + reg)
    }

    fn write_cmd_reg(reg: CmdReg, val: Reg) {
        Self::write_reg(DTU_REGS + reg.val as usize, val)
    }

    fn read_reg(idx: usize) -> Reg {
        unsafe {
            let addr: *const Reg = (BASE_ADDR + idx * 8) as *const Reg;
            ptr::read_volatile(addr)
        }
    }

    fn write_reg(idx: usize, val: Reg) {
        unsafe {
            let addr: *mut Reg = (BASE_ADDR + idx * 8) as *mut Reg;
            ptr::write_volatile(addr, val);
        }
    }

    fn build_data(addr: *const u8, size: usize) -> Reg {
        (addr as usize | (size << 48)) as Reg
    }

    fn build_cmd(ep: EpId, c: CmdOpCode, flags: Reg, arg: Reg) -> Reg {
        c.val as Reg | ((ep as Reg) << 4) | (flags << 12) | (arg << 16)
    }
}

#[cfg(feature = "kernel")]
impl DTU {
    pub fn set_ep(ep: EpId, regs: &[Reg]) {
        let off = DTU_REGS + CMD_REGS + EP_REGS * ep;
        let addr = (BASE_ADDR + off * 8) as *mut Reg;
        for i in 0..EP_REGS {
            unsafe {
                ptr::write_volatile(addr.offset(i as isize), regs[i]);
            }
        }
    }

    pub fn send_to(ep: EpId, msg: *const u8, size: usize, rlbl: Label,
                   rep: EpId, sender: u64) -> Result<(), Error> {
        Self::write_cmd_reg(CmdReg::DATA, Self::build_data(msg, size));
        if rlbl != 0 {
            Self::write_cmd_reg(CmdReg::REPLY_LABEL, rlbl);
        }
        Self::write_cmd_reg(CmdReg::OFFSET, sender);
        Self::write_cmd_reg(CmdReg::COMMAND, Self::build_cmd(
            ep, CmdOpCode::SEND, 0, rep as Reg
        ));

        Self::get_error()
    }

    pub fn dtu_reg_addr(reg: DtuReg) -> usize {
        BASE_ADDR + (reg.val as usize) * 8
    }
    pub fn ep_regs_addr(ep: EpId) -> usize {
        BASE_ADDR + (DTU_REGS + CMD_REGS + EP_REGS * ep) * 8
    }
}
