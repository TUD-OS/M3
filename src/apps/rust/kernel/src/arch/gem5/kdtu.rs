use base::cell::StaticCell;
use base::cfg;
use base::col::Vec;
use base::dtu::*;
use base::errors::{Code, Error};
use base::goff;
use base::kif::Perm;
use base::libc;
use base::util;
use core::intrinsics;

use arch::platform;
use pes::{INVALID_VPE, VPEId, VPEDesc};
use pes::rctmux;
use pes::vpemng;

pub struct State {
    dtu: [Reg; DTU_REGS],
    cmd: [Reg; CMD_REGS],
    eps: [Reg; EP_COUNT * EP_REGS],
    header: [ReplyHeader; HEADER_COUNT],
    header_off: usize,
}

impl State {
    pub fn new() -> State {
        State {
            dtu: [0; DTU_REGS],
            cmd: [0; CMD_REGS],
            eps: [0; EP_COUNT * EP_REGS],
            header: [ReplyHeader::default(); HEADER_COUNT],
            header_off: 0,
        }
    }

    pub fn set_dtu_reg(&mut self, reg: DtuReg, val: Reg) {
        self.dtu[reg.val as usize] = val;
    }

    pub fn get_ep(&self, ep: EpId) -> &[Reg] {
        &self.eps[ep * EP_REGS..(ep + 1) * EP_REGS]
    }
    pub fn get_ep_mut(&mut self, ep: EpId) -> &mut [Reg] {
        &mut self.eps[ep * EP_REGS..(ep + 1) * EP_REGS]
    }

    pub fn config_send(&mut self, ep: EpId, lbl: Label, pe: PEId, vpe: VPEId,
                       dst_ep: EpId, msg_size: usize, credits: u64) {
        let regs: &mut [Reg] = self.get_ep_mut(ep);
        regs[0] = (EpType::SEND.val << 61) |
                  ((vpe as Reg & 0xFFFF) << 16) |
                  (msg_size as Reg & 0xFFFF);
        regs[1] = ((pe as Reg & 0xFF) << 40) |
                  ((dst_ep as Reg & 0xFF) << 32) |
                  (credits << 16) |
                  (credits << 0);
        regs[2] = lbl;
    }

    pub fn config_recv(&mut self, ep: EpId, buf: goff, ord: i32, msg_ord: i32, header: usize) {
        let regs: &mut [Reg] = self.get_ep_mut(ep);
        let buf_size = 1 << (ord - msg_ord);
        let msg_size = 1 << msg_ord;
        regs[0] = (EpType::RECEIVE.val << 61) |
                  ((msg_size as Reg & 0xFFFF) << 32) |
                  ((buf_size as Reg & 0xFFFF) << 16) |
                  ((header as Reg) << 5);
        regs[1] = buf as Reg;
        regs[2] = 0;
    }

    pub fn config_mem(&mut self, ep: EpId, pe: PEId, vpe: VPEId, addr: goff, size: usize, perm: Perm) {
        let regs: &mut [Reg] = self.get_ep_mut(ep);
        regs[0] = (EpType::MEMORY.val << 61) |
                  (size as Reg & 0x1FFF_FFFF_FFFF_FFFF);
        regs[1] = addr as Reg;
        regs[2] = ((vpe as Reg & 0xFFFF) << 12) |
                  ((pe as Reg & 0xFF) << 4) |
                  (perm.bits() as Reg & 0x7);
    }

    pub fn invalidate(&mut self, ep: EpId, check: bool) -> Result<(), Error> {
        let regs: &mut [Reg] = self.get_ep_mut(ep);
        if check && (regs[0] >> 61) == EpType::SEND.val {
            if (regs[1] >> 16) & 0xFFFF != (regs[1] & 0xFFFF) {
                return Err(Error::new(Code::InvArgs));
            }
        }

        regs[0] = 0;
        regs[1] = 0;
        regs[2] = 0;
        Ok(())
    }

    pub fn restore(&mut self, vpe: &VPEDesc, vpe_id: VPEId) {
        // TODO copy the receive buffers back to the SPM

        // TODO reenable pagefaults
        self.set_dtu_reg(DtuReg::STATUS, 0);
        self.set_dtu_reg(DtuReg::VPE_ID, vpe_id as Reg);
        self.set_dtu_reg(DtuReg::IDLE_TIME, 0);

        // restore registers
        let size = util::size_of_val(&self.dtu) +
                   util::size_of_val(&self.cmd) +
                   util::size_of_val(&self.eps);
        KDTU::get().try_write_mem(vpe, BASE_ADDR as goff,
            self.dtu.as_mut_ptr() as *mut u8, size).unwrap();

        // restore headers (we've already set the VPE id)
        let hdr_size = util::size_of_val(&self.header);
        KDTU::get().state.config_mem(KTMP_EP, vpe.pe_id(), vpe_id,
            (BASE_ADDR + size) as goff, hdr_size, Perm::W);
        KDTU::get().write_ep_local(KTMP_EP);

        DTU::write(KTMP_EP, self.header.as_mut_ptr() as *mut u8, hdr_size, 0, CmdFlags::NOPF).unwrap();
    }

    pub fn reset(&mut self) {
        self.set_dtu_reg(DtuReg::EXT_CMD, ExtCmdOpCode::RESET.val as Reg);
    }
}

pub struct KDTU {
    state: State,
    buf: Vec<u8>,
}

pub const KSYS_EP: EpId   = 0;
pub const KSRV_EP: EpId   = 1;
pub const KTMP_EP: EpId   = 2;

static INST: StaticCell<Option<KDTU>> = StaticCell::new(None);

impl KDTU {
    pub fn init() {
        INST.set(Some(KDTU {
            state: State::new(),
            buf: vec![0u8; 4096],
        }));

        // set our own VPE id
        Self::get().do_set_vpe_id(
            &VPEDesc::new_kernel(INVALID_VPE),
            vpemng::KERNEL_VPE
        ).unwrap();
    }

    pub fn get() -> &'static mut KDTU {
        INST.get_mut().as_mut().unwrap()
    }

    fn do_set_vpe_id(&mut self, vpe: &VPEDesc, nid: VPEId) -> Result<(), Error> {
        let vpe_id = nid as Reg;
        self.try_write_slice(vpe, DTU::dtu_reg_addr(DtuReg::VPE_ID) as goff, &[vpe_id])
    }

    pub fn write_ep_local(&mut self, ep: EpId) {
        DTU::set_ep(ep, self.state.get_ep(ep));
    }

    pub fn invalidate_ep_remote(&mut self, vpe: &VPEDesc, ep: EpId) -> Result<(), Error> {
        let reg = ExtCmdOpCode::INV_EP.val | (ep << 3) as Reg;
        self.try_write_slice(vpe, DTU::dtu_reg_addr(DtuReg::EXT_CMD) as goff, &[reg])
    }

    pub fn read_obj<T>(&mut self, vpe: &VPEDesc, addr: goff) -> T {
        self.try_read_obj(vpe, addr).unwrap()
    }

    pub fn try_read_obj<T>(&mut self, vpe: &VPEDesc, addr: goff) -> Result<T, Error> {
        let mut obj: T = unsafe { intrinsics::uninit() };
        self.try_read_mem(vpe, addr, &mut obj as *mut T as *mut u8, util::size_of::<T>())?;
        Ok(obj)
    }

    pub fn read_mem(&mut self, vpe: &VPEDesc, addr: goff, data: *mut u8, size: usize) {
        assert!(vpe.vpe().is_none());
        self.try_read_mem(vpe, addr, data, size).unwrap();
    }

    pub fn try_read_mem(&mut self, vpe: &VPEDesc, addr: goff, data: *mut u8, size: usize) -> Result<(), Error> {
        self.state.config_mem(KTMP_EP, vpe.pe_id(), vpe.vpe_id(), addr, size, Perm::R);
        self.write_ep_local(KTMP_EP);

        DTU::read(KTMP_EP, data, size, 0, CmdFlags::NOPF)
    }

    pub fn write_slice<T>(&mut self, vpe: &VPEDesc, addr: goff, sl: &[T]) {
        self.write_mem(vpe, addr, sl.as_ptr() as *const u8, sl.len() * util::size_of::<T>());
    }

    pub fn try_write_slice<T>(&mut self, vpe: &VPEDesc, addr: goff, sl: &[T]) -> Result<(), Error> {
        self.try_write_mem(vpe, addr, sl.as_ptr() as *const u8, sl.len() * util::size_of::<T>())
    }

    pub fn write_mem(&mut self, vpe: &VPEDesc, addr: goff, data: *const u8, size: usize) {
        assert!(vpe.vpe().is_none());
        self.try_write_mem(vpe, addr, data, size).unwrap();
    }

    pub fn try_write_mem(&mut self, vpe: &VPEDesc, addr: goff, data: *const u8, size: usize) -> Result<(), Error> {
        self.state.config_mem(KTMP_EP, vpe.pe_id(), vpe.vpe_id(), addr, size, Perm::W);
        self.write_ep_local(KTMP_EP);

        DTU::write(KTMP_EP, data, size, 0, CmdFlags::NOPF)
    }

    pub fn send_to(&mut self, vpe: &VPEDesc, ep: EpId, lbl: Label, msg: *const u8, size: usize,
                   rpl_lbl: Label, rpl_ep: EpId) -> Result<(), Error> {
        let msg_size = size + util::size_of::<Header>();
        self.state.config_send(KTMP_EP, lbl, vpe.pe_id(), vpe.vpe_id(), ep, msg_size, CREDITS_UNLIM);
        self.write_ep_local(KTMP_EP);

        let sender = (platform::kernel_pe() as Reg) |
                     ((vpemng::KERNEL_VPE as Reg) << 8) |
                     ((EP_COUNT as Reg) << 24) |
                     ((rpl_ep as Reg) << 32);

        DTU::send_to(KTMP_EP, msg, size, rpl_lbl, rpl_ep, sender as u64)
    }

    pub fn clear(&mut self, dst_vpe: &VPEDesc, mut dst_addr: goff, size: usize) -> Result<(), Error> {
        unsafe {
            libc::memset(self.buf.as_ptr() as *mut libc::c_void, 0, self.buf.len());
        }

        let mut rem = size;
        while rem > 0 {
            let amount = util::min(rem, self.buf.len());
            let buf = self.buf.as_ptr();
            self.try_write_mem(dst_vpe, dst_addr, buf, amount)?;
            dst_addr += amount as goff;
            rem -= amount;
        }
        Ok(())
    }

    pub fn copy(&mut self, dst_vpe: &VPEDesc, mut dst_addr: goff,
                           src_vpe: &VPEDesc, mut src_addr: goff,
                           size: usize) -> Result<(), Error> {
        let mut rem = size;
        while rem > 0 {
            let amount = util::min(rem, self.buf.len());
            let buf = self.buf.as_mut_ptr();
            self.try_read_mem(src_vpe, src_addr, buf, amount)?;
            self.try_write_mem(dst_vpe, dst_addr, buf, amount)?;
            src_addr += amount as goff;
            dst_addr += amount as goff;
            rem -= amount;
        }
        Ok(())
    }

    pub fn recv_msgs(&mut self, ep: EpId, buf: goff, ord: i32, msg_ord: i32) -> Result<(), Error> {
        if self.state.header_off + (1 << (ord - msg_ord)) > HEADER_COUNT {
            return Err(Error::new(Code::NoSpace));
        }

        let off = self.state.header_off;
        self.state.config_recv(ep, buf, ord, msg_ord, off);
        self.state.header_off += 1 << (ord - msg_ord);
        self.write_ep_local(ep);

        Ok(())
    }

    pub fn write_ep_remote(&mut self, vpe: &VPEDesc, ep: EpId, regs: &[Reg]) -> Result<(), Error> {
        self.try_write_slice(vpe, DTU::ep_regs_addr(ep) as goff, regs)
    }

    fn do_ext_cmd(&mut self, vpe: &VPEDesc, cmd: Reg) -> Result<(), Error> {
        self.try_write_slice(vpe, DTU::dtu_reg_addr(DtuReg::EXT_CMD) as goff, &[cmd])
    }

    pub fn invalidate_tlb(&mut self, vpe: &VPEDesc) -> Result<(), Error> {
        self.do_ext_cmd(vpe, ExtCmdOpCode::INV_TLB.val)
    }

    pub fn invlpg_remote(&mut self, vpe: &VPEDesc, mut virt: goff) -> Result<(), Error> {
        virt &= !cfg::PAGE_MASK as goff;
        let cmd = ExtCmdOpCode::INV_PAGE.val | (virt << 3) as u64;
        self.do_ext_cmd(vpe, cmd)
    }

    pub fn reset(&mut self, vpe: &VPEDesc) -> Result<(), Error> {
        // TODO temporary
        let id = INVALID_VPE as Reg;
        self.try_write_slice(vpe, DTU::dtu_reg_addr(DtuReg::VPE_ID) as goff, &[id])
    }

    pub fn wakeup(&mut self, vpe: &VPEDesc, addr: goff) -> Result<(), Error> {
        let cmd = ExtCmdOpCode::WAKEUP_CORE.val | (addr << 3) as u64;
        self.do_ext_cmd(vpe, cmd)
    }

    pub fn inject_irq(&mut self, vpe: &VPEDesc, cmd: Reg) -> Result<(), Error> {
        self.try_write_slice(vpe, DTU::dtu_req_addr(ReqReg::EXT_REQ) as goff, &[cmd])
    }

    pub fn write_swstate(&mut self, vpe: &VPEDesc, flags: u64, notify: u64) -> Result<(), Error> {
        let vals = [notify, flags];
        self.try_write_slice(vpe, rctmux::YIELD_ADDR, &vals)
    }

    pub fn write_swflags(&mut self, vpe: &VPEDesc, flags: u64) -> Result<(), Error> {
        self.try_write_slice(vpe, rctmux::FLAGS_ADDR, &[flags])
    }

    pub fn read_swflags(&mut self, vpe: &VPEDesc) -> Result<u64, Error> {
        self.try_read_obj(vpe, rctmux::FLAGS_ADDR)
    }
}
