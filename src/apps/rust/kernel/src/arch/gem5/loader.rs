use base::cfg::{MOD_HEAP_SIZE, RT_START, STACK_TOP, PAGE_BITS, PAGE_SIZE};
use base::cell::{Cell, MutCell, RefMut};
use base::col::{String, ToString, Vec};
use base::elf;
use base::envdata;
use base::errors::{Code, Error};
use base::GlobAddr;
use base::kif;
use base::util;
use core::intrinsics;

use arch::kdtu::KDTU;
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
    fn new(name: &str, addr: GlobAddr, size: usize) -> Self {
        let mut bm = BootModule {
            name: unsafe { intrinsics::uninit() },
            addr: addr,
            size: size as u64,
        };
        for (a, c) in bm.name.iter_mut().zip(name.bytes()) {
            *a = c as i8;
        }
        bm.name[name.len()] = '\0' as i8;
        bm
    }

    fn name(&self) -> &'static str {
        unsafe {
            util::cstr_to_str(self.name.as_ptr())
        }
    }
}

pub struct Loader {
    mods: Vec<BootModule>,
    loaded: Cell<u64>,
    idles: Vec<Option<BootModule>>,
}

static LOADER: MutCell<Option<Loader>> = MutCell::new(None);

pub fn init() {
    let mut mods = Vec::new();
    for m in platform::mods() {
        let mut bmod: BootModule = unsafe { intrinsics::uninit() };
        KDTU::get().read_mem(
            &VPEDesc::new_mem(m.pe()),
            m.offset(), &mut bmod as *mut BootModule as *mut u8, util::size_of::<BootModule>()
        );

        klog!(KENV, "Module '{}':", bmod.name());
        klog!(KENV, "  addr: {:#x}", bmod.addr.raw());
        klog!(KENV, "  size: {:#x}", bmod.size);

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
        idles: vec![None; platform::MAX_PES],
    }));
}

impl Loader {
    pub fn get() -> &'static mut Loader {
        LOADER.get_mut().as_mut().unwrap()
    }

    fn get_mod(&self, args: &[String]) -> Option<(&BootModule, bool)> {
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

    fn read_from_mod<T>(bm: &BootModule, off: usize) -> Result<T, Error> {
        let size = util::size_of::<T>();
        if off + size > bm.size as usize {
            return Err(Error::new(Code::InvalidElf))
        }

        let mut obj: T = unsafe { intrinsics::uninit() };
        KDTU::get().read_mem(
            &VPEDesc::new_mem(bm.addr.pe()),
            bm.addr.offset() + off,
            &mut obj as *mut T as *mut u8,
            size
        );
        Ok(obj)
    }

    fn map_segment(vpe: &RefMut<VPE>, phys: GlobAddr, virt: usize, size: usize,
                   _perm: kif::Perm) -> Result<(), Error> {
        assert!(!vpe.pe_desc().has_virtmem());
        KDTU::get().copy_clear(
            // destination
            &vpe.desc(),
            virt,
            // source
            &VPEDesc::new_mem(phys.pe()),
            phys.offset(),
            size,
            false
        )
    }

    fn load_mod(&self, vpe: &RefMut<VPE>, bm: &BootModule,
                copy: bool, needs_heap: bool) -> Result<usize, Error> {
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
            let phdr: elf::Phdr = Self::read_from_mod(bm, off)?;

            // we're only interested in non-empty load segments
            if phdr.ty != elf::PT::LOAD.val || phdr.memsz == 0 {
                continue;
            }

            let perms = kif::Perm::from(elf::PF::from_bits_truncate(phdr.flags));
            let offset = util::round_dn(phdr.offset, PAGE_SIZE);
            let virt = util::round_dn(phdr.vaddr, PAGE_SIZE);

            // do we need new memory for this segment?
            if (copy && perms.contains(kif::Perm::W)) || phdr.filesz == 0 {
                let size = util::round_up((phdr.vaddr & PAGE_BITS) + phdr.memsz, PAGE_SIZE);

                // TODO with VM, allocate new memory and map it
                // let phys = mem::get().allocate(size, PAGE_SIZE)?;
                // Self::map_segment(&vpe, phys.global(), virt, size, perms)?;

                KDTU::get().copy_clear(
                    // destination
                    &vpe.desc(),
                    virt,
                    // source
                    &VPEDesc::new_mem(bm.addr.pe()),
                    bm.addr.offset() + offset,
                    size,
                    phdr.filesz == 0
                )?;

                end = virt + size;
            }
            else {
                assert!(phdr.memsz == phdr.filesz);
                let size = (phdr.offset & PAGE_BITS) + phdr.filesz;
                Self::map_segment(&vpe, bm.addr + offset, virt, size, perms)?;
                end = virt + size;
            }

            off += hdr.phentsize as usize;
        }

        if needs_heap {
            // create initial heap
            let phys = mem::get().allocate(MOD_HEAP_SIZE, PAGE_SIZE)?;
            let end = util::round_up(end, PAGE_SIZE);
            Self::map_segment(&vpe, phys.global(), end, MOD_HEAP_SIZE, kif::Perm::RW)?;
        }

        Ok(hdr.entry)
    }

    fn map_idle(&mut self, vpe: &RefMut<VPE>) -> Result<usize, Error> {
        let pe = vpe.pe_id();

        if self.idles[pe].is_none() {
            let (phys, size) = {
                let rctmux: &BootModule = self.get_mod(&["rctmux".to_string()]).unwrap().0;

                // copy the ELF file
                let size = util::round_up(rctmux.size as usize, PAGE_SIZE);
                let phys = mem::get().allocate(size, PAGE_SIZE)?;
                let bootvpe = VPEDesc::new_mem(phys.global().pe());
                KDTU::get().copy_clear(
                    // destination
                    &bootvpe,
                    phys.global().offset(),
                    // source
                    &VPEDesc::new_mem(rctmux.addr.pe()),
                    rctmux.addr.offset(),
                    rctmux.size as usize,
                    false
                )?;

                (phys, size)
            };

            self.idles[pe] = Some(BootModule::new("rctmux", phys.global(), size));
        }

        // load idle
        let idle: &BootModule = self.idles[pe].as_ref().unwrap();
        self.load_mod(vpe, idle, false, false)
    }

    fn write_arguments(vpe: &VPEDesc, args: &Vec<String>) -> Result<usize, Error> {
        let mut argptr: Vec<usize> = Vec::new();
        let mut argbuf: Vec<u8> = Vec::new();

        let off = RT_START + util::size_of::<envdata::EnvData>();
        let mut argoff = off;
        for s in args {
            // push argv entry
            argptr.push(argoff);

            // push string
            let arg = s.as_bytes();
            argbuf.extend_from_slice(arg);

            // 0-terminate it
            argbuf.push('\0' as u8);

            argoff += arg.len() + 1;
        }

        KDTU::get().try_write_mem(vpe, off, argbuf.as_ptr() as *const u8, argbuf.len())?;
        let argv_size = argptr.len() * util::size_of::<usize>();
        KDTU::get().try_write_mem(vpe, argoff, argptr.as_ptr() as *const u8, argv_size)?;

        Ok(argoff)
    }

    pub fn load_app(&mut self, mut vpe: RefMut<VPE>) -> Result<i32, Error> {
        {
            // get DTU into correct state first
            vpe.dtu_state().reset();
            let vpe_desc = VPEDesc::new_mem(vpe.pe_id());
            let vpe_id = vpe.id();
            vpe.dtu_state().restore(&vpe_desc, vpe_id);
        }

        self.map_idle(&vpe)?;

        let entry: usize = {
            let (app, first) = self.get_mod(vpe.args()).ok_or(Error::new(Code::NoSuchFile))?;
            klog!(KENV, "Loading mod '{}':", app.name());
            self.load_mod(&vpe, app, !first, true)?
        };

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
        KDTU::get().try_write_mem(
            &vpe.desc(),
            RT_START,
            &senv as *const envdata::EnvData as *const u8,
            util::size_of_val(&senv)
        )?;

        vpe.set_state(State::RUNNING);

        // notify rctmux
        {
            // let the VPE report idle times if there are other VPEs
            let report = 0u64;
            let mut flags: u64 = rctmux::Flags::WAITING.bits();

            flags |= rctmux::Flags::RESTORE.bits();
            flags |= (vpe.pe_id() as u64) << 32;

            KDTU::get().write_swstate(&vpe.desc(), flags, report)?;
            KDTU::get().wakeup(&vpe.desc(), rctmux::ENTRY_ADDR)?;
        }

        Ok(0)
    }
}
