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

use base::cell::StaticCell;
use base::dtu::*;
use base::errors::{Code, Error};
use base::goff;
use base::kif::Perm;
use base::util;

use pes::{VPEId, VPEDesc};

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
        let regs: &mut [Reg] = self.get_ep_mut(ep);
        regs[EpReg::LABEL.val as usize] = lbl;
        regs[EpReg::PE_ID.val as usize] = pe as Reg;
        regs[EpReg::EP_ID.val as usize] = dst_ep as Reg;
        regs[EpReg::CREDITS.val as usize] = credits;
        regs[EpReg::MSGORDER.val as usize] = util::next_log2(msg_size) as Reg;
    }

    pub fn config_recv(&mut self, ep: EpId, buf: goff, ord: i32, msg_ord: i32, _header: usize) {
        let regs: &mut [Reg] = self.get_ep_mut(ep);
        regs[EpReg::BUF_ADDR.val as usize]       = buf as Reg;
        regs[EpReg::BUF_ORDER.val as usize]      = ord as Reg;
        regs[EpReg::BUF_MSGORDER.val as usize]   = msg_ord as Reg;
        regs[EpReg::BUF_ROFF.val as usize]       = 0;
        regs[EpReg::BUF_WOFF.val as usize]       = 0;
        regs[EpReg::BUF_MSG_CNT.val as usize]    = 0;
        regs[EpReg::BUF_UNREAD.val as usize]     = 0;
        regs[EpReg::BUF_OCCUPIED.val as usize]   = 0;
    }

    pub fn config_mem(&mut self, ep: EpId, pe: PEId, _vpe: VPEId, addr: goff, size: usize, perm: Perm) {
        let regs: &mut [Reg] = self.get_ep_mut(ep);
        assert!((addr & perm.bits() as goff) == 0);
        regs[EpReg::LABEL.val as usize]         = (addr | perm.bits() as goff) as Reg;
        regs[EpReg::PE_ID.val as usize]         = pe as Reg;
        regs[EpReg::EP_ID.val as usize]         = 0;
        regs[EpReg::CREDITS.val as usize]       = size as Reg;
        regs[EpReg::MSGORDER.val as usize]      = 0;
    }

    pub fn invalidate(&mut self, ep: EpId, _check: bool) -> Result<(), Error> {
        let regs: &mut [Reg] = self.get_ep_mut(ep);
        for r in regs.iter_mut() {
            *r = 0;
        }
        Ok(())
    }

    pub fn restore(&mut self, _vpe: &VPEDesc, _vpe_id: VPEId) {
    }

    pub fn reset(&mut self) {
    }
}

pub struct KDTU {
    state: State,
}

pub const KSYS_EP: EpId   = 0;
pub const KSRV_EP: EpId   = 1;
pub const KTMP_EP: EpId   = 2;

static INST: StaticCell<Option<KDTU>> = StaticCell::new(None);

impl KDTU {
    pub fn init() {
        INST.set(Some(KDTU {
            state: State::new(),
        }));
    }

    pub fn get() -> &'static mut KDTU {
        INST.get_mut().as_mut().unwrap()
    }

    pub fn write_ep_local(&mut self, ep: EpId) {
        DTU::set_ep_regs(ep, self.state.get_ep(ep));
    }

    pub fn invalidate_ep_remote(&mut self, vpe: &VPEDesc, ep: EpId) -> Result<(), Error> {
        let regs = [0 as Reg; EPS_RCNT * EP_COUNT];
        self.write_ep_remote(vpe, ep, &regs)
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
        self.state.config_send(KTMP_EP, lbl, vpe.pe_id(), vpe.vpe_id(), ep, msg_size, !0);
        self.write_ep_local(KTMP_EP);

        DTU::send(KTMP_EP, msg, size, rpl_lbl, rpl_ep)
    }

    pub fn clear(&mut self, _dst_vpe: &VPEDesc, mut _dst_addr: goff, _size: usize) -> Result<(), Error> {
        Err(Error::new(Code::NotSup))
    }

    pub fn copy(&mut self, _dst_vpe: &VPEDesc, mut _dst_addr: goff,
                           _src_vpe: &VPEDesc, mut _src_addr: goff,
                           _size: usize) -> Result<(), Error> {
        Err(Error::new(Code::NotSup))
    }

    pub fn recv_msgs(&mut self, ep: EpId, buf: goff, ord: i32, msg_ord: i32) -> Result<(), Error> {
        self.state.config_recv(ep, buf, ord, msg_ord, 0);
        self.write_ep_local(ep);

        Ok(())
    }

    pub fn write_ep_remote(&mut self, vpe: &VPEDesc, ep: EpId, regs: &[Reg]) -> Result<(), Error> {
        let eps = vpe.vpe().unwrap().eps_addr();
        let addr = eps + ep * EPS_RCNT * util::size_of::<Reg>();
        let bytes = EPS_RCNT * util::size_of::<Reg>();
        self.try_write_mem(vpe, addr as goff, regs.as_ptr() as *const u8, bytes)
    }

    pub fn reset(&mut self, _vpe: &VPEDesc) -> Result<(), Error> {
        // nothing to do
        Ok(())
    }
}
