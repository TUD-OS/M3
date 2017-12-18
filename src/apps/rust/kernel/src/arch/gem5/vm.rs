use base::cfg;
use base::dtu::{self, EpId};
use base::errors::Error;
use base::kif::{CapSel, Perm, PEDesc};
use base::GlobAddr;
use base::util;

use arch::kdtu::KDTU;
use mem;
use pes::VPEDesc;

pub struct AddrSpace {
    pe: PEDesc,
    root: GlobAddr,
    sep: Option<EpId>,
    rep: Option<EpId>,
    sgate: Option<CapSel>,
}

impl AddrSpace {
    pub fn new(pe: &PEDesc) -> Result<Self, Error> {
        Ok(AddrSpace {
            pe: pe.clone(),
            root: Self::get_root_pt()?,
            sep: None,
            rep: None,
            sgate: None,
        })
    }

    pub fn new_with_pager(pe: &PEDesc, sep: EpId, rep: EpId, sgate: CapSel) -> Result<Self, Error> {
        Ok(AddrSpace {
            pe: pe.clone(),
            root: Self::get_root_pt()?,
            sep: Some(sep),
            rep: Some(rep),
            sgate: Some(sgate),
        })
    }

    pub fn sep(&self) -> Option<EpId> {
        self.sep
    }
    pub fn sgate_sel(&self) -> Option<CapSel> {
        self.sgate
    }

    pub fn setup(&self, vpe: &VPEDesc) {
        // insert recursive entry
        let off = self.root.offset();
        let pte = self.to_mmu_pte(self.root.raw() | dtu::PTEFlags::RWX.bits());
        KDTU::get().write_slice(
            &VPEDesc::new_mem(self.root.pe()), off + dtu::PTE_REC_IDX * 8, &[pte]
        );

        assert!(self.pe.has_dtuvm());

        let (status, pf_ep) = match (self.sep, self.rep) {
            (Some(sep), Some(rep))  => (dtu::StatusFlags::PAGEFAULTS.bits(), sep | rep << 8),
            (Some(sep), None)       => (dtu::StatusFlags::PAGEFAULTS.bits(), sep),
            _                       => (0, 0),
        };

        // init DTU registers
        let regs = [status, self.root.raw(), pf_ep as dtu::Reg];
        KDTU::get().try_write_slice(vpe, dtu::DTU::dtu_reg_addr(dtu::DtuReg::STATUS), &regs).unwrap();

        // invalidate TLB, because we have changed the root PT
        KDTU::get().invalidate_tlb(vpe).unwrap();
    }

    pub fn map_pages(&self, vpe: &VPEDesc, virt: usize, mut phys: GlobAddr,
                     mut pages: usize, attr: Perm) -> Result<(), Error> {
        while pages > 0 {
            for lvl in (0..dtu::LEVEL_CNT).rev() {
                let pte_addr = Self::get_pte_addr(virt, lvl);

                let pte: dtu::PTE = KDTU::get().read_obj(vpe, pte_addr);
                if lvl > 0 {
                    self.create_pt(vpe, virt, pte_addr, pte, attr, lvl)?;
                }
                else {
                    self.create_ptes(vpe, virt, pte_addr, pte, &mut phys, &mut pages, attr);
                }
            }
        }
        Ok(())
    }

    pub fn unmap_pages(&self, virt: usize, pages: usize) {
    }

    fn get_pte_addr(mut virt: usize, lvl: usize) -> usize {
        const REC_MASK: usize =
            dtu::PTE_REC_IDX << (cfg::PAGE_BITS + dtu::LEVEL_BITS * 3) |
            dtu::PTE_REC_IDX << (cfg::PAGE_BITS + dtu::LEVEL_BITS * 2) |
            dtu::PTE_REC_IDX << (cfg::PAGE_BITS + dtu::LEVEL_BITS * 1) |
            dtu::PTE_REC_IDX << (cfg::PAGE_BITS + dtu::LEVEL_BITS * 0);

        // at first, just shift it accordingly.
        virt >>= cfg::PAGE_BITS + lvl * dtu::LEVEL_BITS;
        virt <<= dtu::PTE_BITS;

        // now put in one PTE_REC_IDX's for each loop that we need to take
        let shift = lvl + 1;
        let rem_mask = (1 << (cfg::PAGE_BITS + dtu::LEVEL_BITS * (dtu::LEVEL_CNT - shift))) - 1;
        virt |= REC_MASK & !rem_mask;

        // finally, make sure that we stay within the bounds for virtual addresses
        // this is because of recMask, that might actually have too many of those.
        virt &= (1 << (dtu::LEVEL_CNT * dtu::LEVEL_BITS + cfg::PAGE_BITS)) - 1;
        virt
    }

    fn create_pt(&self, vpe: &VPEDesc, virt: usize, pte_addr: usize, pte: dtu::PTE,
                 attr: Perm, lvl: usize) -> Result<bool, Error> {
        // create the pagetable on demand
        if pte == 0 {
            // if we don't have a pagetable for that yet, unmapping is a noop
            if attr.is_empty() {
                return Ok(true);
            }

            // TODO this is prelimilary
            let alloc = mem::get().allocate(cfg::PAGE_SIZE, cfg::PAGE_SIZE)?;

            // clear PT
            let mut npte = alloc.global().raw();
            KDTU::get().clear(
                &VPEDesc::new_mem(alloc.global().pe()), alloc.global().offset(), cfg::PAGE_SIZE
            ).unwrap();

            // insert PTE
            npte |= dtu::PTEFlags::IRWX.bits();
            npte = self.to_mmu_pte(npte);
            let pt_size = (1 << (dtu::LEVEL_BITS * lvl)) * cfg::PAGE_SIZE;
            klog!(PTES, "VPE{}: lvl {} PTE for {:#x}: {:#x}",
                vpe.vpe_id(), lvl, virt & !(pt_size - 1), npte);
            KDTU::get().try_write_slice(vpe, pte_addr, &[npte]).unwrap();
        }

        Ok(false)
    }

    fn create_ptes(&self, vpe: &VPEDesc, mut virt: usize, mut pte_addr: usize, pte: dtu::PTE,
                   phys: &mut GlobAddr, pages: &mut usize, attr: Perm) -> bool {
        // note that we can assume here that map_pages is always called for the same set of
        // pages. i.e., it is not possible that we map page 1 and 2 and afterwards remap
        // only page 1. this is because we call map_pages with MapCapability, which can't
        // be resized. thus, we know that a downgrade for the first, is a downgrade for all
        // and that an existing mapping for the first is an existing mapping for all.

        let pte_dtu = self.to_dtu_pte(pte);
        let npte = phys.raw() | attr.bits() as dtu::PTE | dtu::PTEFlags::I.bits();
        if npte == pte_dtu {
            return true;
        }

        let mut downgrade = false;
        let rwx = dtu::PTEFlags::RWX.bits();
        // do not invalidate pages if we are writing to a memory PE
        if (pte_dtu & rwx) != 0 && self.pe.has_virtmem() {
            downgrade = ((pte_dtu & rwx) & (!npte & rwx)) != 0;
        }

        let end_pte = util::min(pte_addr + *pages * 8, util::round_up(pte_addr + 8, cfg::PAGE_SIZE));

        let count = (end_pte - pte_addr) / 8;
        assert!(count > 0);
        *pages -= count;
        *phys = *phys + (count << cfg::PAGE_BITS);

        let mut npte = self.to_mmu_pte(npte);
        while pte_addr < end_pte {
            let start_addr = pte_addr;
            let mut buf = [0 as dtu::PTE; 16];

            let mut i = 0;
            while pte_addr < end_pte && i < buf.len() {
                klog!(PTES, "VPE{}: lvl 0 PTE for {:#x}: {:#x}{}",
                    vpe.vpe_id(), virt, npte, if downgrade { " (invalidating)"} else { "" });

                buf[i] = npte;

                pte_addr += 8;
                virt += cfg::PAGE_SIZE;
                npte += cfg::PAGE_SIZE as dtu::PTE;
                i += 1;
            }

            KDTU::get().try_write_slice(vpe, start_addr, &buf[0..i]).unwrap();

            if downgrade {
                let mut vaddr = virt - i * cfg::PAGE_SIZE;
                while vaddr < virt {
                    KDTU::get().invlpg_remote(vpe, vaddr).unwrap();
                    vaddr += cfg::PAGE_SIZE;
                }
            }
        }

        false
    }

    fn to_mmu_pte(&self, pte: dtu::PTE) -> dtu::PTE {
        // the current implementation is based on some equal properties of MMU and DTU paging
        assert!(self.pe.has_dtuvm());
        pte
    }

    fn to_dtu_pte(&self, pte: dtu::PTE) -> dtu::PTE {
        assert!(self.pe.has_dtuvm());
        pte
    }

    fn get_root_pt() -> Result<GlobAddr, Error> {
        let root_pt = mem::get().allocate(cfg::PAGE_SIZE, cfg::PAGE_SIZE)?;
        KDTU::get().clear(
            &VPEDesc::new_mem(root_pt.global().pe()), root_pt.global().offset(), cfg::PAGE_SIZE
        )?;
        Ok(root_pt.global())
    }
}
