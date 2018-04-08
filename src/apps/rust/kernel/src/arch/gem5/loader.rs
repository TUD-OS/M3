use base::cfg::{MOD_HEAP_SIZE, RT_START, STACK_TOP, PAGE_BITS, PAGE_MASK, PAGE_SIZE};
use base::cfg::{RECVBUF_SPACE, RECVBUF_SIZE};
use base::cell::{Cell, StaticCell};
use base::col::{String, ToString, Vec};
use base::dtu;
use base::elf;
use base::envdata;
use base::errors::{Code, Error};
use base::GlobAddr;
use base::goff;
use base::kif;
use base::util;

use arch::kdtu::KDTU;
use cap::{Capability, KObject, MapFlags, MapObject, SelRange};
use pes::{State, VPE, VPEDesc};
use pes::rctmux;
use platform;
use mem;

#[repr(C, packed)]
#[derive(Copy, Clone)]
struct BootModule {
    name: [i8; 256],
    addr: GlobAddr,
    size: u64,
}

impl BootModule {
    fn name(&self) -> &'static str {
        unsafe {
            util::cstr_to_str(self.name.as_ptr())
        }
    }
}

pub struct Loader {
    mods: Vec<BootModule>,
    loaded: Cell<u64>,
}

static LOADER: StaticCell<Option<Loader>> = StaticCell::new(None);

pub fn init() {
    let mut mods = Vec::new();
    for m in platform::mods() {
        let bmod: BootModule = KDTU::get().read_obj(&VPEDesc::new_mem(m.pe()), m.offset());

        // TODO removing all prints breaks it!?
        klog!(DEF, "Module '{}':", bmod.name());
        klog!(KENV, "  addr: {:#x}", bmod.addr.raw());
        klog!(KENV, "  size: {:#x}", {bmod.size});

        mods.push(bmod);
    }

    for i in platform::pes() {
        let desc: kif::PEDesc = platform::pe_desc(i);
        klog!(KENV,
            "PE{:02}: {} {} {} KiB memory",
            i, desc.pe_type(), desc.isa(), desc.mem_size() / 1024
        );
    }

    LOADER.set(Some(Loader {
        mods: mods,
        loaded: Cell::new(0),
    }));
}

impl Loader {
    pub fn get() -> &'static mut Loader {
        LOADER.get_mut().as_mut().unwrap()
    }

    fn get_mod(&self, args: &[String]) -> Option<(&BootModule, bool)> {
        const_assert!(util::size_of::<u64>() * 8 >= platform::MAX_MODS);

        let mut arg_str = String::new();
        for (i, a) in args.iter().enumerate() {
            arg_str.push_str(a);
            if i + 1 < args.len() {
                arg_str.push(' ');
            }
        }

        for (i, ref m) in self.mods.iter().enumerate() {
            if m.name() == arg_str {
                let first = (self.loaded.get() & (1 << i)) == 0;
                self.loaded.set(self.loaded.get() | 1 << i);
                return Some((m, first))
            }
        }

        None
    }

    fn read_from_mod<T>(bm: &BootModule, off: goff) -> Result<T, Error> {
        if off + util::size_of::<T>() as goff > bm.size {
            return Err(Error::new(Code::InvalidElf))
        }

        Ok(KDTU::get().read_obj(&VPEDesc::new_mem(bm.addr.pe()), bm.addr.offset() + off))
    }

    fn map_segment(vpe: &mut VPE, phys: GlobAddr, virt: goff, size: usize,
                   flags: MapFlags) -> Result<(), Error> {
        if vpe.pe_desc().has_virtmem() {
            let dst_sel = virt >> PAGE_BITS;
            let pages = util::round_up(size, PAGE_SIZE) >> PAGE_BITS;

            let map_obj = MapObject::new(phys, flags);
            map_obj.borrow().map(vpe, virt, pages)?;

            vpe.map_caps_mut().insert(
                Capability::new_range(
                    SelRange::new_range(dst_sel as kif::CapSel, pages as kif::CapSel),
                    KObject::Map(map_obj)
                )
            );
            Ok(())
        }
        else {
            KDTU::get().copy(
                // destination
                &vpe.desc(),
                virt as goff,
                // source
                &VPEDesc::new_mem(phys.pe()),
                phys.offset(),
                size
            )
        }
    }

    fn load_mod(&self, vpe: &mut VPE, bm: &BootModule,
                copy: bool, needs_heap: bool, to_mem: bool) -> Result<usize, Error> {
        let hdr: elf::Ehdr = Self::read_from_mod(bm, 0)?;

        if hdr.ident[0] != '\x7F' as u8 ||
           hdr.ident[1] != 'E' as u8 ||
           hdr.ident[2] != 'L' as u8 ||
           hdr.ident[3] != 'F' as u8 {
            return Err(Error::new(Code::InvalidElf))
        }

        // copy load segments to destination PE
        let mut end = 0;
        let mut off = hdr.phoff;
        for _ in 0..hdr.phnum {
            // load program header
            let phdr: elf::Phdr = Self::read_from_mod(bm, off as goff)?;
            off += hdr.phentsize as usize;

            // we're only interested in non-empty load segments
            if phdr.ty != elf::PT::LOAD.val || phdr.memsz == 0 {
                continue;
            }

            let flags = MapFlags::from(kif::Perm::from(elf::PF::from_bits_truncate(phdr.flags)));
            let offset = util::round_dn(phdr.offset as usize, PAGE_SIZE);
            let virt = util::round_dn(phdr.vaddr, PAGE_SIZE);

            // do we need new memory for this segment?
            if (copy && flags.contains(MapFlags::W)) || phdr.filesz == 0 {
                let size = util::round_up((phdr.vaddr & PAGE_MASK) + phdr.memsz as usize, PAGE_SIZE);

                let (dst_vpe, dst_virt) = if vpe.pe_desc().has_virtmem() {
                    let mut phys = mem::get().allocate(size, PAGE_SIZE)?;
                    Self::map_segment(vpe, phys.global(), virt as goff, size, flags)?;
                    phys.claim();

                    // workaround for ARM: if we push remotely into the cache, it gets loaded to the
                    // L1d cache. however, we push instructions which need to end up in L1i. Thus,
                    // write to mem.
                    if to_mem || virt == 0x0 {
                        (VPEDesc::new_mem(phys.global().pe()), phys.global().offset())
                    }
                    else {
                        (vpe.desc(), virt as goff)
                    }
                }
                else {
                    (vpe.desc(), virt as goff)
                };

                if phdr.filesz == 0 {
                    KDTU::get().clear(&dst_vpe, dst_virt, size)?;
                }
                else {
                    KDTU::get().copy(
                        // destination
                        &dst_vpe,
                        dst_virt,
                        // source
                        &VPEDesc::new_mem(bm.addr.pe()),
                        bm.addr.offset() + offset as goff,
                        size
                    )?;
                }

                end = virt + size;
            }
            else {
                assert!(phdr.memsz == phdr.filesz);
                let size = (phdr.offset as usize & PAGE_MASK) + phdr.filesz as usize;
                Self::map_segment(vpe, bm.addr + offset as goff, virt as goff, size, flags)?;
                end = virt + size;
            }
        }

        // create initial heap
        if needs_heap && vpe.pe_desc().has_virtmem() {
            let end = util::round_up(end, PAGE_SIZE);
            let mut phys = mem::get().allocate(MOD_HEAP_SIZE, PAGE_SIZE)?;
            Self::map_segment(vpe, phys.global(), end as goff, MOD_HEAP_SIZE, MapFlags::RW)?;
            phys.claim();
        }

        Ok(hdr.entry)
    }

    fn map_idle(&mut self, vpe: &mut VPE) -> Result<usize, Error> {
        // load rctmux
        let rctmux: &BootModule = self.get_mod(&["rctmux".to_string()]).unwrap().0;
        let to_mem = vpe.pe_desc().has_mmu();
        let entry = self.load_mod(vpe, rctmux, true, false, to_mem)?;

        if vpe.pe_desc().has_mmu() {
            // clear rctmux variables
            KDTU::get().clear(
                &VPEDesc::new_mem(rctmux.addr.pe()), rctmux.addr.offset() + rctmux::YIELD_ADDR, 16
            )?;

            // map DTU
            Self::map_segment(
                vpe, GlobAddr::new(dtu::BASE_ADDR as u64), dtu::BASE_ADDR as goff, PAGE_SIZE,
                MapFlags::RW | MapFlags::I | MapFlags::UNCACHED
            )?;
            // map the privileged registers only for ring 0
            Self::map_segment(
                vpe, GlobAddr::new(dtu::BASE_REQ_ADDR as u64), dtu::BASE_REQ_ADDR as goff, PAGE_SIZE,
                MapFlags::RW | MapFlags::UNCACHED
            )?;
        }

        Ok(entry)
    }

    fn write_arguments(vpe: &VPEDesc, args: &Vec<String>) -> Result<usize, Error> {
        let mut argptr: Vec<u64> = Vec::new();
        let mut argbuf: Vec<u8> = Vec::new();

        let off = RT_START + util::size_of::<envdata::EnvData>();
        let mut argoff = off;
        for s in args {
            // push argv entry
            argptr.push(argoff as u64);

            // push string
            let arg = s.as_bytes();
            argbuf.extend_from_slice(arg);

            // 0-terminate it
            argbuf.push('\0' as u8);

            argoff += arg.len() + 1;
        }

        KDTU::get().try_write_mem(vpe, off as goff, argbuf.as_ptr() as *const u8, argbuf.len())?;
        let argv_size = argptr.len() * util::size_of::<u64>();
        argoff = util::round_up(argoff, util::size_of::<u64>());
        KDTU::get().try_write_mem(vpe, argoff as goff, argptr.as_ptr() as *const u8, argv_size)?;

        Ok(argoff)
    }

    pub fn init_app(&mut self, vpe: &mut VPE) -> Result<(), Error> {
        {
            // get DTU into correct state first
            vpe.dtu_state().reset();
            let vpe_desc = VPEDesc::new_mem(vpe.pe_id());
            let vpe_id = vpe.id();
            vpe.dtu_state().restore(&vpe_desc, vpe_id);
        }

        if vpe.pe_desc().has_mmu() {
            // we have to init it in external memory first until we have rctmux running
            vpe.set_state(State::SUSPENDED);
            self.map_idle(vpe)?;
            vpe.set_state(State::RUNNING);

            if let Some(aspace) = vpe.addr_space() {
                aspace.setup(&vpe.desc());
            }
        }
        else {
            vpe.set_state(State::RUNNING);

            if let Some(aspace) = vpe.addr_space() {
                aspace.setup(&vpe.desc());
            }

            self.map_idle(vpe)?;
        }

        if vpe.is_bootmod() {
            let entry: usize = {
                let (app, first) = self.get_mod(vpe.args()).ok_or(Error::new(Code::NoSuchFile))?;
                klog!(KENV, "Loading mod '{}':", app.name());
                self.load_mod(vpe, app, !first, true, false)?
            };

            if vpe.pe_desc().has_virtmem() {
                // map runtime space
                let virt = RT_START;
                let size = STACK_TOP - virt;
                let mut phys = mem::get().allocate(size, PAGE_SIZE)?;
                Self::map_segment(vpe, phys.global(), virt as goff, size, MapFlags::RW)?;
                phys.claim();
            }

            let argv_off: usize = Self::write_arguments(&vpe.desc(), vpe.args())?;

            // build env
            let mut senv = envdata::EnvData::default();
            senv.argc = vpe.args().len() as u32;
            senv.argv = argv_off as u64;
            senv.sp = STACK_TOP as u64;
            senv.entry = entry as u64;
            senv.pe_desc = vpe.pe_desc().value();
            senv.heap_size = MOD_HEAP_SIZE as u64;

            // write env to target PE
            KDTU::get().try_write_slice(&vpe.desc(), RT_START as goff, &[senv])?;
        }

        if vpe.pe_desc().has_virtmem() {
            // map receive buffer
            let mut phys = mem::get().allocate(RECVBUF_SIZE, PAGE_SIZE)?;
            Self::map_segment(vpe, phys.global(), RECVBUF_SPACE as goff, RECVBUF_SIZE, MapFlags::RW)?;
            phys.claim();
        }

        Ok(())
    }

    pub fn load_app(&mut self, vpe: &mut VPE) -> Result<i32, Error> {
        if vpe.state() == State::DEAD {
            self.init_app(vpe)?;
        }

        // notify rctmux
        {
            // let the VPE report idle times if there are other VPEs
            let report = 0u64;
            let mut flags: u64 = rctmux::Flags::WAITING.bits();

            flags |= rctmux::Flags::RESTORE.bits();
            flags |= (vpe.pe_id() as u64) << 32;

            KDTU::get().write_swstate(&vpe.desc(), flags, report)?;
            if !vpe.pe_desc().has_mmu() && vpe.pe_desc().is_programmable() {
                KDTU::get().wakeup(&vpe.desc(), rctmux::ENTRY_ADDR as goff)?;
            }
            else {
                KDTU::get().inject_irq(&vpe.desc(), dtu::ExtReqOpCode::RCTMUX.val)?;
            }
        }

        Ok(0)
    }
}
