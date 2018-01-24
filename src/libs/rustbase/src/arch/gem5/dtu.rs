use cfg;
use core::intrinsics;
use core::ptr;
use arch;
use errors::Error;
use kif::PEDesc;
use util;

/// A DTU register
pub type Reg    = u64;
/// An endpoint id
pub type EpId   = usize;
/// A DTU label used in send EPs
pub type Label  = u64;
/// A PE id
pub type PEId   = usize;

/// The number of endpoints in each DTU
pub const EP_COUNT: EpId        = 12;

/// The send EP for system calls
pub const SYSC_SEP: EpId        = 0;
/// The receive EP for system calls
pub const SYSC_REP: EpId        = 1;
/// The receive EP for upcalls from the kernel
pub const UPCALL_REP: EpId      = 2;
/// The default receive EP
pub const DEF_REP: EpId         = 3;
/// The first free EP id
pub const FIRST_FREE_EP: EpId   = 4;

/// The base address of the DTU's MMIO area
pub const BASE_ADDR: usize      = 0xF0000000;
/// The base address of the DTU's MMIO area for external requests
pub const BASE_REQ_ADDR: usize  = BASE_ADDR + cfg::PAGE_SIZE;
/// The number of DTU registers
pub const DTU_REGS: usize       = 8;
// const REQ_REGS: usize        = 3;
/// The number of command registers
pub const CMD_REGS: usize       = 5;
/// The number of registers per EP
pub const EP_REGS: usize        = 3;
/// The number of headers per DTU
pub const HEADER_COUNT: usize   = 128;

/// Represents unlimited credits
pub const CREDITS_UNLIM: u64    = 0xFFFF;

// actual max is 64k - 1; use less for better alignment
const MAX_PKT_SIZE: usize       = 60 * 1024;

int_enum! {
    /// The DTU registers
    pub struct DtuReg : Reg {
        /// Stores various status flags
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
    /// The status flag for the `DtuReg::STATUS` register
    pub struct StatusFlags : Reg {
        /// Whether the PE is privileged
        const PRIV         = 1 << 0;
        /// Whether page faults are send via `PF_EP`
        const PAGEFAULTS   = 1 << 1;
        /// Whether communication is currently disabled (for context switching)
        const COM_DISABLED = 1 << 2;
        /// Set by the DTU if the PE was woken up by an IRQ
        const IRQ_WAKEUP   = 1 << 3;
    }
}

#[allow(dead_code)]
int_enum! {
    /// The request registers
    pub struct ReqReg : Reg {
        /// For external requests
        const EXT_REQ     = 0x0;
        /// For translation requests
        const XLATE_REQ   = 0x1;
        /// For translation responses
        const XLATER_ESP  = 0x2;
    }
}

#[allow(dead_code)]
int_enum! {
    /// The command registers
    struct CmdReg : Reg {
        /// Starts commands and signals their completion
        const COMMAND     = 0x0;
        /// Aborts commands
        const ABORT       = 0x1;
        /// Specifies the data address and size
        const DATA        = 0x2;
        /// Specifies an offset
        const OFFSET      = 0x3;
        /// Specifies the reply label
        const REPLY_LABEL = 0x4;
    }
}

int_enum! {
    /// The commands
    struct CmdOpCode : u64 {
        /// The idle command has no effect
        const IDLE        = 0x0;
        /// Sends a message
        const SEND        = 0x1;
        /// Replies to a message
        const REPLY       = 0x2;
        /// Reads from external memory
        const READ        = 0x3;
        /// Writes to external memory
        const WRITE       = 0x4;
        /// Fetches a message
        const FETCH_MSG   = 0x5;
        /// Acknowledges a message
        const ACK_MSG     = 0x6;
        /// Puts the CU to sleep
        const SLEEP       = 0x7;
        /// For ARM only: clears the IRQ at the interrupt controller
        const CLEAR_IRQ   = 0x8;
        /// Prints a message
        const PRINT       = 0x9;
    }
}

bitflags! {
    /// The command flags
    pub struct CmdFlags : u64 {
        /// Specifies that a page fault should abort the command with an error
        const NOPF        = 0x1;
    }
}

int_enum! {
    /// The different endpoint types
    pub struct EpType : u64 {
        /// Invalid endpoint (unusable)
        const INVALID     = 0x0;
        /// Send endpoint
        const SEND        = 0x1;
        /// Receive endpoint
        const RECEIVE     = 0x2;
        /// Memory endpoint
        const MEMORY      = 0x3;
    }
}

int_enum! {
    /// The external requests
    pub struct ExtReqOpCode : Reg {
        /// Sets the root page table
        const SET_ROOTPT  = 0x0;
        /// Invalidates a TLB entry in the CU's MMU
        const INV_PAGE    = 0x1;
        /// Requests some rctmux action
        const RCTMUX      = 0x2;
    }
}

int_enum! {
    /// The external commands
    pub struct ExtCmdOpCode : Reg {
        /// The idle command has no effect
        const IDLE        = 0;
        /// Wake up the CU in case it's sleeping
        const WAKEUP_CORE = 1;
        /// Invalidate and endpoint, if possible
        const INV_EP      = 2;
        /// Invalidate a single TLB entry
        const INV_PAGE    = 3;
        /// Invalidate all TLB entries
        const INV_TLB     = 4;
        /// Reset the CU
        const RESET       = 5;
        /// Acknowledge a message
        const ACK_MSG     = 6;
    }
}

pub const PTE_BITS: usize   = 3;
pub const PTE_SIZE: usize   = 1 << PTE_BITS;
pub const PTE_REC_IDX: usize= 0x10;

pub const LEVEL_CNT: usize  = 4;
pub const LEVEL_BITS: usize = cfg::PAGE_BITS - PTE_BITS;
pub const LEVEL_MASK: usize = (1 << LEVEL_BITS) - 1;

pub const LPAGE_BITS: usize = cfg::PAGE_BITS + LEVEL_BITS;
pub const LPAGE_SIZE: usize = 1 << LPAGE_BITS;
pub const LPAGE_MASK: usize = LPAGE_SIZE - 1;

pub type PTE = u64;

bitflags! {
    /// The page table entry flags
    pub struct PTEFlags : u64 {
        /// Readable
        const R             = 0b000001;
        /// Writable
        const W             = 0b000010;
        /// Executable
        const X             = 0b000100;
        /// Internally accessible, i.e., by the CU
        const I             = 0b001000;
        /// Large page (2 MiB)
        const LARGE         = 0b010000;
        /// Unsupported by DTU, but used for MMU
        const UNCACHED      = 0b100000;
        /// Read+write
        const RW            = Self::R.bits | Self::W.bits;
        /// Read+write+execute
        const RWX           = Self::R.bits | Self::W.bits | Self::X.bits;
        /// Internal+read+write+execute
        const IRWX          = Self::R.bits | Self::W.bits | Self::X.bits | Self::I.bits;
    }
}

/// The DTU header including the reply label
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

/// The DTU header excluding the reply label
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

/// The DTU message consisting of the header and the payload
#[repr(C, packed)]
#[derive(Debug)]
pub struct Message {
    pub header: Header,
    pub data: [u8],
}

/// The DTU interface
pub struct DTU {
}

impl DTU {
    /// Sends `msg[0..size]` via given endpoint.
    ///
    /// The `reply_ep` specifies the endpoint the reply is sent to. The label of the reply will be
    /// `reply_lbl`.
    ///
    /// # Errors
    ///
    /// If the number of left credits is not sufficient, the function returns (`Code::MISS_CREDITS`).
    /// If the receiver is suspended, the function returns (`Code::VPE_GONE`).
    #[inline(always)]
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

    /// Sends `reply[0..size]` as reply to `msg`.
    ///
    /// # Errors
    ///
    /// If the receiver is suspended, the function returns (`Code::VPE_GONE`).
    #[inline(always)]
    pub fn reply(ep: EpId, reply: *const u8, size: usize, msg: &'static Message) -> Result<(), Error> {
        Self::write_cmd_reg(CmdReg::DATA, Self::build_data(reply, size));
        let slice: u128 = unsafe { intrinsics::transmute(msg) };
        Self::write_cmd_reg(CmdReg::COMMAND, Self::build_cmd(
            ep, CmdOpCode::REPLY, 0, slice as u64
        ));

        Self::get_error()
    }

    /// Reads `size` bytes from offset `off` in the memory region denoted by the endpoint into `data`.
    ///
    /// The `flags` can be used to control whether page faults should abort the command.
    ///
    /// # Errors
    ///
    /// If the receiver is suspended, the function returns (`Code::VPE_GONE`).
    pub fn read(ep: EpId, data: *mut u8, size: usize, off: usize, flags: CmdFlags) -> Result<(), Error> {
        let cmd = Self::build_cmd(ep, CmdOpCode::READ, flags.bits(), 0);
        let res = Self::transfer(cmd, data as usize, size, off);
        unsafe { intrinsics::atomic_fence() };
        res
    }

    /// Writes `size` bytes from `data` to offset `off` in the memory region denoted by the endpoint.
    ///
    /// The `flags` can be used to control whether page faults should abort the command.
    ///
    /// # Errors
    ///
    /// If the receiver is suspended, the function returns (`Code::VPE_GONE`).
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

    /// Tries to fetch a new message from the given endpoint.
    #[inline(always)]
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

    /// Returns true if the given endpoint is valid, i.e., a SEND, RECEIVE, or MEMORY endpoint
    #[inline(always)]
    pub fn is_valid(ep: EpId) -> bool {
        let r0 = Self::read_ep_reg(ep, 0);
        (r0 >> 61) != EpType::INVALID.val
    }

    /// Marks the given message for receive endpoint `ep` as read
    #[inline(always)]
    pub fn mark_read(ep: EpId, msg: &Message) {
        let off = (msg as *const Message) as *const u8 as usize as Reg;
        Self::write_cmd_reg(CmdReg::COMMAND, Self::build_cmd(ep, CmdOpCode::ACK_MSG, 0, off));
    }

    /// Waits until the current command is completed and returns the error, if any occurred
    #[inline(always)]
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

    /// Tries to put the CU to sleep after checking for new messages a few times. Additionally, the
    /// kernel is notified about it, if required.
    #[inline(always)]
    pub fn try_sleep(_yield: bool, cycles: u64) -> Result<(), Error> {
        let num = if PEDesc::new_from(arch::envdata::get().pe_desc).has_mmu() { 2 } else { 100 };
        for _ in 0..num {
            if Self::read_dtu_reg(DtuReg::MSG_CNT) > 0 {
                return Ok(())
            }
        }

        // TODO yield

        Self::sleep(cycles)
    }

    /// Puts the CU to sleep for at most `cycles` or until the CU is woken up (e.g., by a message
    /// reception).
    #[inline(always)]
    pub fn sleep(cycles: u64) -> Result<(), Error> {
        Self::write_cmd_reg(CmdReg::COMMAND, Self::build_cmd(0, CmdOpCode::SLEEP, 0, cycles));
        Self::get_error()
    }

    /// Prints the given message into the gem5 log
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
    /// Configures the given endpoint
    pub fn set_ep(ep: EpId, regs: &[Reg]) {
        let off = DTU_REGS + CMD_REGS + EP_REGS * ep;
        let addr = (BASE_ADDR + off * 8) as *mut Reg;
        for i in 0..EP_REGS {
            unsafe {
                ptr::write_volatile(addr.offset(i as isize), regs[i]);
            }
        }
    }

    /// Sends the message `msg[0..`size`] via the given endpoint using the specified sender.
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

    /// Returns the MMIO address of the given DTU register
    pub fn dtu_reg_addr(reg: DtuReg) -> usize {
        BASE_ADDR + (reg.val as usize) * 8
    }
    /// Returns the MMIO address of the given request register
    pub fn dtu_req_addr(reg: ReqReg) -> usize {
        BASE_REQ_ADDR + (reg.val as usize) * 8
    }
    /// Returns the MMIO address of the given endpoint registers
    pub fn ep_regs_addr(ep: EpId) -> usize {
        BASE_ADDR + (DTU_REGS + CMD_REGS + EP_REGS * ep) * 8
    }
}
