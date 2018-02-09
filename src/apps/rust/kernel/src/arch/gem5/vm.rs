use base::cfg;
use base::dtu::{self, EpId};
use base::errors::Error;
use base::GlobAddr;
use base::heap;
use base::kif::{CapSel, PEDesc};
use base::util;
use core::intrinsics;

use arch::kdtu::KDTU;
use cap::MapFlags;
use mem;
use pes::{self, State, VPEId, VPEDesc};
use platform;

bitflags! {
    pub struct X86PTE : u64 {
        const PRESENT  = 0x1;
        const WRITE    = 0x2;
        const USER     = 0x4;
        const UNCACHED = 0x10;
        const LARGE    = 0x80;
        const NOEXEC   = 0x8000_0000_0000_0000;
    }
}

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

    pub fn new_kernel() -> Self {
        AddrSpace {
            pe: platform::pe_desc(platform::kernel_pe()),
            root: GlobAddr::new_with(0, 0), // unused
            sep: None,
            rep: None,
            sgate: None,
        }
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

        let (status, pf_ep) = match (self.sep, self.rep) {
            (Some(sep), Some(rep))  => (dtu::StatusFlags::PAGEFAULTS.bits(), sep | rep << 8),
            (Some(sep), None)       => (dtu::StatusFlags::PAGEFAULTS.bits(), sep),
            _                       => (0, 0),
        };

        if self.pe.has_dtuvm() {
            const_assert!(dtu::DtuReg::STATUS.val == 0);
            const_assert!(dtu::DtuReg::ROOT_PT.val == 1);
            const_assert!(dtu::DtuReg::PF_EP.val == 2);

            // init DTU registers
            let regs = [status, self.root.raw(), pf_ep as dtu::Reg];
            KDTU::get().try_write_slice(vpe, dtu::DTU::dtu_reg_addr(dtu::DtuReg::STATUS), &regs).unwrap();
        }
        else {
            // set PF_EP register
            let reg: dtu::Reg = pf_ep as dtu::Reg;
            KDTU::get().try_write_slice(vpe, dtu::DTU::dtu_reg_addr(dtu::DtuReg::PF_EP), &[reg]).unwrap();

            // set root PT
            let rootpt = self.to_mmu_pte(self.root.raw());
            Self::mmu_cmd_remote(vpe, rootpt | dtu::ExtReqOpCode::SET_ROOTPT.val);
        }

        // invalidate TLB, because we have changed the root PT
        KDTU::get().invalidate_tlb(vpe).unwrap();
    }

    pub fn map_pages(&self, vpe: &VPEDesc, mut virt: usize, mut phys: GlobAddr,
                     mut pages: usize, flags: MapFlags) -> Result<(), Error> {
        let running = vpe.pe_id() == platform::kernel_pe() ||
            vpe.vpe().unwrap().state() == State::RUNNING;

        let mem_vpe = VPEDesc::new_mem(self.root.pe());
        let rvpe = if !running { &mem_vpe } else { vpe };

        while pages > 0 {
            for lvl in (0..dtu::LEVEL_CNT).rev() {
                let pte_addr = if !running {
                    self.get_pte_addr_mem(rvpe, self.root, virt, lvl)
                }
                else {
                    Self::get_pte_addr(virt, lvl)
                };

                let pte: dtu::PTE = self.to_dtu_pte(KDTU::get().read_obj(rvpe, pte_addr));
                if lvl > 0 {
                    self.create_pt(
                        rvpe, vpe.vpe_id(), virt, pte_addr, pte, flags, lvl
                    )?;
                }
                else {
                    self.create_ptes(
                        rvpe, vpe.vpe_id(), &mut virt, pte_addr, pte, &mut phys, &mut pages, flags
                    );
                }
            }
        }
        Ok(())
    }

    pub fn unmap_pages(&self, vpe: &VPEDesc, virt: usize, pages: usize) -> Result<(), Error> {
        // don't do anything if the VPE is already dead (or if it's the kernel)
        if vpe.vpe().map_or(true, |v| v.state() == State::DEAD) {
            return Ok(());
        }

        self.map_pages(vpe, virt, GlobAddr::new(0), pages, MapFlags::empty())
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

    fn get_pte_addr_mem(&self, vpe: &VPEDesc, root: GlobAddr, virt: usize, lvl: usize) -> usize {
        let mut pt = root.offset();

        for l in (0..dtu::LEVEL_CNT).rev() {
            let idx = (virt >> (cfg::PAGE_BITS + dtu::LEVEL_BITS * l)) & dtu::LEVEL_MASK;
            pt += idx * dtu::PTE_SIZE;

            if lvl == l {
                return pt;
            }

            let pte: dtu::PTE = self.to_dtu_pte(KDTU::get().read_obj(vpe, pt));

            pt = GlobAddr::new(pte).offset() & !cfg::PAGE_MASK;
        }

        unsafe {
            intrinsics::unreachable()
        }
    }

    fn create_pt(&self, vpe: &VPEDesc, vpe_id: VPEId, virt: usize, pte_addr: usize, pte: dtu::PTE,
                 flags: MapFlags, lvl: usize) -> Result<bool, Error> {
        // create the pagetable on demand
        if pte == 0 {
            // if we don't have a pagetable for that yet, unmapping is a noop
            if flags.is_empty() {
                return Ok(true);
            }

            // TODO this is prelimilary
            let mut alloc = mem::get().allocate(cfg::PAGE_SIZE, cfg::PAGE_SIZE)?;

            // clear PT
            let mut npte = alloc.global().raw();
            KDTU::get().clear(
                &VPEDesc::new_mem(alloc.global().pe()), alloc.global().offset(), cfg::PAGE_SIZE
            ).unwrap();
            alloc.claim();

            // insert PTE
            npte |= dtu::PTEFlags::IRWX.bits();
            npte = self.to_mmu_pte(npte);
            let pt_size = (1 << (dtu::LEVEL_BITS * lvl)) * cfg::PAGE_SIZE;
            klog!(PTES, "VPE{}: lvl {} PTE for {:#x}: {:#x}",
                vpe_id, lvl, virt & !(pt_size - 1), npte);
            KDTU::get().try_write_slice(vpe, pte_addr, &[npte]).unwrap();
        }

        Ok(false)
    }

    fn create_ptes(&self, vpe: &VPEDesc, vpe_id: VPEId, virt: &mut usize, mut pte_addr: usize,
                   pte: dtu::PTE, phys: &mut GlobAddr, pages: &mut usize, flags: MapFlags) -> bool {
        // note that we can assume here that map_pages is always called for the same set of
        // pages. i.e., it is not possible that we map page 1 and 2 and afterwards remap
        // only page 1. this is because we call map_pages with MapCapability, which can't
        // be resized. thus, we know that a downgrade for the first, is a downgrade for all
        // and that an existing mapping for the first is an existing mapping for all.

        let pte_dtu = self.to_dtu_pte(pte);
        // TODO don't automatically set the I bit
        let npte = phys.raw() | flags.bits() | dtu::PTEFlags::I.bits();

        let end_pte = util::min(pte_addr + *pages * 8, util::round_up(pte_addr + 8, cfg::PAGE_SIZE));
        let count = (end_pte - pte_addr) / 8;
        assert!(count > 0);
        *pages -= count;
        *phys = *phys + (count << cfg::PAGE_BITS);

        if npte == pte_dtu {
            return true;
        }

        let mut downgrade = false;
        let rwx = dtu::PTEFlags::RWX.bits();
        // do not invalidate pages if we are writing to a memory PE
        if (pte_dtu & rwx) != 0 && platform::pe_desc(vpe.pe_id()).has_virtmem() {
            downgrade = ((pte_dtu & rwx) & (!npte & rwx)) != 0;
        }

        let mut npte = self.to_mmu_pte(npte);
        while pte_addr < end_pte {
            let start_addr = pte_addr;
            let mut buf = [0 as dtu::PTE; 16];

            let mut i = 0;
            while pte_addr < end_pte && i < buf.len() {
                klog!(PTES, "VPE{}: lvl 0 PTE for {:#x}: {:#x}{}",
                    vpe_id, virt, npte, if downgrade { " (invalidating)"} else { "" });

                buf[i] = npte;

                pte_addr += 8;
                *virt += cfg::PAGE_SIZE;
                npte += cfg::PAGE_SIZE as dtu::PTE;
                i += 1;
            }

            KDTU::get().try_write_slice(vpe, start_addr, &buf[0..i]).unwrap();

            if downgrade {
                let mut vaddr = *virt - i * cfg::PAGE_SIZE;
                while vaddr < *virt {
                    self.invalidate_page(vpe, vaddr).unwrap();
                    vaddr += cfg::PAGE_SIZE;
                }
            }
        }

        false
    }

    fn to_mmu_pte(&self, pte: dtu::PTE) -> dtu::PTE {
        // the current implementation is based on some equal properties of MMU and DTU paging
        const_assert!(dtu::LEVEL_CNT == 4);
        const_assert!(cfg::PAGE_SIZE == 4096);
        const_assert!(dtu::LEVEL_BITS == 9);

        if self.pe.has_dtuvm() {
            return pte;
        }

        let dtu_pte = dtu::PTEFlags::from_bits_truncate(pte);
        let addr = pte & !cfg::PAGE_MASK as dtu::PTE;

        let mut flags = X86PTE::empty();
        if dtu_pte.intersects(dtu::PTEFlags::RWX) {
            flags.insert(X86PTE::PRESENT);
        }
        if dtu_pte.contains(dtu::PTEFlags::W) {
            flags.insert(X86PTE::WRITE);
        }
        if dtu_pte.contains(dtu::PTEFlags::I) {
            flags.insert(X86PTE::USER);
        }
        if dtu_pte.contains(dtu::PTEFlags::UNCACHED) {
            flags.insert(X86PTE::UNCACHED);
        }
        if dtu_pte.contains(dtu::PTEFlags::LARGE) {
            flags.insert(X86PTE::LARGE);
        }
        if !dtu_pte.contains(dtu::PTEFlags::X) {
            flags.insert(X86PTE::NOEXEC);
        }

        // translate NoC address to physical address
        let addr = (addr & !0xFF00_0000_0000_0000) | ((addr & 0xFF00_0000_0000_0000) >> 16);
        return addr | flags.bits()
    }

    fn to_dtu_pte(&self, pte: dtu::PTE) -> dtu::PTE {
        if self.pe.has_dtuvm() || pte == 0 {
            return pte;
        }

        let mut res = pte & !cfg::PAGE_MASK as dtu::PTE;

        let x86_pte = X86PTE::from_bits_truncate(pte);
        if x86_pte.contains(X86PTE::PRESENT) {
            res |= dtu::PTEFlags::R.bits();
        }
        if x86_pte.contains(X86PTE::WRITE) {
            res |= dtu::PTEFlags::W.bits();
        }
        if x86_pte.contains(X86PTE::USER) {
            res |= dtu::PTEFlags::I.bits();
        }
        if x86_pte.contains(X86PTE::LARGE) {
            res |= dtu::PTEFlags::LARGE.bits();
        }
        if !x86_pte.contains(X86PTE::NOEXEC) {
            res |= dtu::PTEFlags::X.bits();
        }

        // translate physical address to NoC address
        res = (res & !0x0000_FF00_0000_0000) | ((res & 0x0000_FF00_0000_0000) << 16);
        return res;
    }

    fn invalidate_page(&self, vpe: &VPEDesc, mut virt: usize) -> Result<(), Error> {
        virt &= !cfg::PAGE_MASK;
        if self.pe.has_mmu() {
            Self::mmu_cmd_remote(vpe, virt as dtu::Reg | dtu::ExtReqOpCode::INV_PAGE.val);
        }
        KDTU::get().invlpg_remote(vpe, virt)
    }

    fn mmu_cmd_remote(vpe: &VPEDesc, cmd: dtu::Reg) {
        KDTU::get().inject_irq(vpe, cmd).unwrap();

        // wait until the remote core sends us an ACK (writes 0 to MASTER_REQ)
        let mut mstreq: dtu::Reg = 1;
        let extarg_addr = dtu::DTU::dtu_req_addr(dtu::ReqReg::EXT_REQ);
        while mstreq != 0 {
            mstreq = KDTU::get().read_obj(vpe, extarg_addr);
        }
    }

    fn get_root_pt() -> Result<GlobAddr, Error> {
        let mut root_pt = mem::get().allocate(cfg::PAGE_SIZE, cfg::PAGE_SIZE)?;
        KDTU::get().clear(
            &VPEDesc::new_mem(root_pt.global().pe()), root_pt.global().offset(), cfg::PAGE_SIZE
        )?;
        root_pt.claim();
        Ok(root_pt.global())
    }
}

extern {
    fn heap_set_oom_callback(cb: extern fn(size: usize) -> bool);

    static mut heap_end: *mut heap::HeapArea;
}

extern fn kernel_oom_callback(size: usize) -> bool {
    if !platform::pe_desc(platform::kernel_pe()).has_virtmem() {
        panic!("Unable to allocate {} bytes on the heap: out of memory", size);
    }

    // allocate memory
    let pages = util::max(256, util::round_up(size, cfg::PAGE_SIZE) >> cfg::PAGE_BITS);
    let mut alloc = mem::get().allocate(pages * cfg::PAGE_SIZE, cfg::PAGE_SIZE).unwrap();

    // map the memory
    let virt = unsafe { util::round_up(heap_end as usize, cfg::PAGE_SIZE) };
    let space = AddrSpace::new_kernel();
    let vpe = &pes::VPEDesc::new_kernel(pes::vpemng::KERNEL_VPE);
    space.map_pages(vpe, virt, alloc.global(), pages, MapFlags::RW).unwrap();
    alloc.claim();

    // append to heap
    heap::append(pages);
    return true;
}

pub fn init() {
    unsafe {
        heap_set_oom_callback(kernel_oom_callback);
    }
}
