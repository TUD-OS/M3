use base::dtu::*;
use base::errors::{Code, Error};
use base::kif::Perm;
use base::util;

use pes::{INVALID_VPE, VPEId, VPEDesc};
use pes::vpemng;

pub struct State {
    cmd: [Reg; CMD_RCNT],
    eps: [Reg; EPS_RCNT * EP_COUNT],
}

impl State {
    pub fn new() -> State {
        State {
            cmd: [0; CMD_RCNT],
            eps: [0; EPS_RCNT * EP_COUNT],
        }
    }

    pub fn get_ep(&self, ep: EpId) -> &[Reg] {
        &self.eps[ep * EPS_RCNT..(ep + 1) * EPS_RCNT]
    }
    pub fn get_ep_mut(&mut self, ep: EpId) -> &mut [Reg] {
        &mut self.eps[ep * EPS_RCNT..(ep + 1) * EPS_RCNT]
    }

    pub fn config_send(&mut self, ep: EpId, lbl: Label, pe: PEId, _vpe: VPEId,
                       dst_ep: EpId, msg_size: usize, credits: u64) {
        let regs = self.get_ep_mut(ep);
        regs[EpReg::LABEL.val as usize] = lbl;
        regs[EpReg::PE_ID.val as usize] = pe as Reg;
        regs[EpReg::EP_ID.val as usize] = dst_ep as Reg;
        regs[EpReg::CREDITS.val as usize] = credits;
        regs[EpReg::MSGORDER.val as usize] = util::next_log2(msg_size) as Reg;
    }

    pub fn config_recv(&mut self, ep: EpId, buf: usize, ord: i32, msg_ord: i32, _header: usize) {
        let regs = self.get_ep_mut(ep);
        regs[EpReg::BUF_ADDR.val as usize]       = buf as Reg;
        regs[EpReg::BUF_ORDER.val as usize]      = ord as Reg;
        regs[EpReg::BUF_MSGORDER.val as usize]   = msg_ord as Reg;
        regs[EpReg::BUF_ROFF.val as usize]       = 0;
        regs[EpReg::BUF_WOFF.val as usize]       = 0;
        regs[EpReg::BUF_MSG_CNT.val as usize]    = 0;
        regs[EpReg::BUF_UNREAD.val as usize]     = 0;
        regs[EpReg::BUF_OCCUPIED.val as usize]   = 0;
    }

    pub fn config_mem(&mut self, ep: EpId, pe: PEId, _vpe: VPEId, addr: usize, size: usize, perm: Perm) {
        let regs = self.get_ep_mut(ep);
        assert!((addr & perm.bits() as usize) == 0);
        regs[EpReg::LABEL.val as usize]         = (addr | perm.bits() as usize) as Reg;
        regs[EpReg::PE_ID.val as usize]         = pe as Reg;
        regs[EpReg::EP_ID.val as usize]         = 0;
        regs[EpReg::CREDITS.val as usize]       = size as Reg;
        regs[EpReg::MSGORDER.val as usize]      = 0;
    }

    pub fn restore(&mut self, _vpe: &VPEDesc, _vpe_id: VPEId) {
    }

    pub fn reset(&mut self) {
    }
}

pub struct KDTU {
    state: State,
    ep: EpId,
}

static mut INST: Option<KDTU> = None;

impl KDTU {
    pub fn init() {
        unsafe {
            INST = Some(KDTU {
                state: State::new(),
                ep: 1,  // TODO
            });
        }
    }

    pub fn get() -> &'static mut KDTU {
        unsafe {
            INST.as_mut().unwrap()
        }
    }

    pub fn write_ep_local(&mut self, ep: EpId) {
        DTU::set_ep_regs(ep, self.state.get_ep(ep));
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

    pub fn copy_clear(&mut self, _dst_vpe: &VPEDesc, mut _dst_addr: usize,
                                 _src_vpe: &VPEDesc, mut _src_addr: usize,
                                 _size: usize, _clear: bool) -> Result<(), Error> {
        Err(Error::new(Code::NotSup))
    }

    pub fn recv_msgs(&mut self, ep: EpId, buf: usize, ord: i32, msg_ord: i32) -> Result<(), Error> {
        self.state.config_recv(ep, buf, ord, msg_ord, 0);
        self.write_ep_local(ep);

        Ok(())
    }

    pub fn write_ep_remote(&mut self, vpe: &VPEDesc, ep: EpId, regs: &[Reg]) -> Result<(), Error> {
        let vpeobj = vpemng::get().vpe(vpe.vpe()).unwrap();
        let eps = vpeobj.borrow().eps_addr();
        let addr = eps + ep * EPS_RCNT * util::size_of::<Reg>();
        self.try_write_mem(vpe, addr, regs.as_ptr() as *const u8, EPS_RCNT * util::size_of::<Reg>())
    }
}
