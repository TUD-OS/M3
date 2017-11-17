use base::dtu::*;
use base::errors::{Code, Error};
use base::kif::Perm;
use base::libc;
use base::util;

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
        let regs = self.get_ep_mut(ep);
        regs[0] = (EpType::SEND.val << 61) |
                  ((vpe as Reg & 0xFFFF) << 16) |
                  (msg_size as Reg & 0xFFFF);
        regs[1] = ((pe as Reg & 0xFF) << 40) |
                  ((dst_ep as Reg & 0xFF) << 32) |
                  (credits << 16) |
                  (credits << 0);
        regs[2] = lbl;
    }

    pub fn config_recv(&mut self, ep: EpId, buf: usize, ord: i32, msg_ord: i32, header: usize) {
        let regs = self.get_ep_mut(ep);
        let buf_size = 1 << (ord - msg_ord);
        let msg_size = 1 << msg_ord;
        regs[0] = (EpType::RECEIVE.val << 61) |
                  ((msg_size & 0xFFFF) << 32) |
                  ((buf_size & 0xFFFF) << 16) |
                  ((header as Reg) << 5);
        regs[1] = buf as Reg;
        regs[2] = 0;
    }

    pub fn config_mem(&mut self, ep: EpId, pe: PEId, vpe: VPEId, addr: usize, size: usize, perm: Perm) {
        let regs = self.get_ep_mut(ep);
        regs[0] = (EpType::MEMORY.val << 61) |
                  (size & 0x1FFF_FFFF_FFFF_FFFF) as Reg;
        regs[1] = addr as Reg;
        regs[2] = ((vpe & 0xFFFF) << 12) as Reg |
                  ((pe & 0xFF) << 4) as Reg |
                  (perm.bits() & 0x7) as Reg;
    }

    pub fn restore(&mut self, vpe: &VPEDesc, vpe_id: VPEId) {
        // TODO copy the receive buffers back to the SPM

        // TODO reenable pagefaults
        self.set_dtu_reg(DtuReg::FEATURES, 0);
        self.set_dtu_reg(DtuReg::VPE_ID, vpe_id as Reg);
        self.set_dtu_reg(DtuReg::IDLE_TIME, 0);

        // restore registers
        let size = util::size_of_val(&self.dtu) + util::size_of_val(&self.cmd) + util::size_of_val(&self.eps);
        KDTU::get().try_write_mem(vpe, BASE_ADDR, self.dtu.as_mut_ptr() as *mut u8, size).unwrap();

        // restore headers (we've already set the VPE id)
        KDTU::get().try_write_mem(&VPEDesc::new(vpe.pe(), vpe_id),
                              BASE_ADDR + size, self.header.as_mut_ptr() as *mut u8,
                              util::size_of_val(&self.header)).unwrap();
    }

    pub fn reset(&mut self) {
        self.set_dtu_reg(DtuReg::EXT_CMD, ExtCmdOpCode::RESET.val as Reg);
    }
}

pub struct KDTU {
    state: State,
    ep: EpId,
    buf: [u8; 4096],
}

static mut INST: Option<KDTU> = None;

impl KDTU {
    pub fn init() {
        unsafe {
            INST = Some(KDTU {
                state: State::new(),
                ep: 1,  // TODO
                buf: [0u8; 4096],
            });
        }

        // set our own VPE id
        Self::get().do_set_vpe_id(
            &VPEDesc::new(platform::kernel_pe(), INVALID_VPE),
            vpemng::KERNEL_VPE
        ).unwrap();
    }

    pub fn get() -> &'static mut KDTU {
        unsafe {
            INST.as_mut().unwrap()
        }
    }

    fn do_set_vpe_id(&mut self, vpe: &VPEDesc, nid: VPEId) -> Result<(), Error> {
        let vpe_id: Reg = nid as Reg;
        self.try_write_mem(vpe, DTU::dtu_reg_addr(DtuReg::VPE_ID), &vpe_id as *const Reg as *const u8, 8)
    }

    pub fn write_ep_local(&mut self, ep: EpId) {
        DTU::set_ep(ep, self.state.get_ep(ep));
    }

    pub fn read_mem(&mut self, vpe: &VPEDesc, addr: usize, data: *mut u8, size: usize) {
        assert!(vpe.vpe() == INVALID_VPE);
        self.try_read_mem(vpe, addr, data, size).unwrap();
    }

    pub fn try_read_mem(&mut self, vpe: &VPEDesc, addr: usize, data: *mut u8, size: usize) -> Result<(), Error> {
        let ep = self.ep;
        self.state.config_mem(ep, vpe.pe(), vpe.vpe(), addr, size, Perm::R);
        self.write_ep_local(ep);

        return DTU::read(ep, data, size, 0, CmdFlags::NOPF);
    }

    pub fn write_mem(&mut self, vpe: &VPEDesc, addr: usize, data: *const u8, size: usize) {
        assert!(vpe.vpe() == INVALID_VPE);
        self.try_write_mem(vpe, addr, data, size).unwrap();
    }

    pub fn try_write_mem(&mut self, vpe: &VPEDesc, addr: usize, data: *const u8, size: usize) -> Result<(), Error> {
        let ep = self.ep;
        self.state.config_mem(ep, vpe.pe(), vpe.vpe(), addr, size, Perm::W);
        self.write_ep_local(ep);

        return DTU::write(ep, data, size, 0, CmdFlags::NOPF);
    }

    pub fn copy_clear(&mut self, dst_vpe: &VPEDesc, mut dst_addr: usize,
                                 src_vpe: &VPEDesc, mut src_addr: usize,
                                 size: usize, clear: bool) -> Result<(), Error> {
        if clear {
            unsafe {
                libc::memset(self.buf.as_ptr() as *mut libc::c_void, 0, self.buf.len());
            }
        }

        let mut rem = size;
        let mut buf = self.buf;
        while rem > 0 {
            let amount = util::min(rem, self.buf.len());
            if !clear {
                self.try_read_mem(src_vpe, src_addr, buf.as_mut_ptr(), amount)?;
            }
            self.try_write_mem(dst_vpe, dst_addr, buf.as_ptr(), amount)?;
            src_addr += amount;
            dst_addr += amount;
            rem -= amount;
        }
        Ok(())
    }

    pub fn recv_msgs(&mut self, ep: EpId, buf: usize, ord: i32, msg_ord: i32) -> Result<(), Error> {
        if self.state.header_off + 1 << (ord - msg_ord) > HEADER_COUNT {
            return Err(Error::new(Code::NoSpace));
        }

        let off = self.state.header_off;
        self.state.config_recv(ep, buf, ord, msg_ord, off);
        self.state.header_off += 1 << (ord - msg_ord);
        self.write_ep_local(ep);

        Ok(())
    }

    pub fn write_ep_remote(&mut self, vpe: &VPEDesc, ep: EpId, regs: &[Reg]) -> Result<(), Error> {
        self.try_write_mem(vpe, DTU::ep_regs_addr(ep), regs.as_ptr() as *const u8, EP_REGS * 8)
    }

    fn do_ext_cmd(&mut self, vpe: &VPEDesc, cmd: Reg) -> Result<(), Error> {
        self.try_write_mem(vpe, DTU::dtu_reg_addr(DtuReg::EXT_CMD), &cmd as *const u64 as *const u8, 8)
    }

    pub fn wakeup(&mut self, vpe: &VPEDesc, addr: usize) -> Result<(), Error> {
        let cmd = ExtCmdOpCode::WAKEUP_CORE.val | (addr << 3) as u64;
        self.do_ext_cmd(vpe, cmd)
    }

    pub fn write_swstate(&mut self, vpe: &VPEDesc, flags: u64, notify: u64) -> Result<(), Error> {
        let vals = [notify, flags];
        self.try_write_mem(vpe, rctmux::YIELD_ADDR, vals.as_ptr() as *const u8, 16)
    }

    pub fn write_swflags(&mut self, vpe: &VPEDesc, flags: u64) -> Result<(), Error> {
        self.try_write_mem(vpe, rctmux::FLAGS_ADDR, &flags as *const u64 as *const u8, 8)
    }

    pub fn read_swflags(&mut self, vpe: &VPEDesc) -> Result<u64, Error> {
        let mut flags = 0u64;
        self.try_read_mem(vpe, rctmux::FLAGS_ADDR, &mut flags as *mut u64 as *mut u8, 8)?;
        Ok(flags)
    }
}
