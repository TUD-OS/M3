use arch::cfg;
use com::MemGate;
use col::Vec;
use core::iter;
use elf;
use errors::{Code, Error};
use heap;
use kif;
use session::Pager;
use util;
use vfs::{BufReader, FileRef, Map, Read, Seek, SeekMode, read_object};

pub struct Loader<'l> {
    pager: Option<&'l Pager>,
    pager_inherited: bool,
    mem: &'l MemGate,
}

impl<'l> Loader<'l> {
    pub fn new(pager: Option<&'l Pager>, pager_inherited: bool, mem: &'l MemGate) -> Loader<'l> {
        Loader {
            pager: pager,
            pager_inherited: pager_inherited,
            mem: mem,
        }
    }

    pub fn copy_regions(&mut self, sp: usize) -> Result<usize, Error> {
        extern {
            static _text_start: u8;
            static _text_end: u8;
            static _data_start: u8;
            static _bss_end: u8;
            static heap_end: usize;
        }

        let addr = |sym: &u8| {
            (sym as *const u8) as usize
        };

        // use COW if both have a pager
        if let Some(pg) = self.pager {
            if self.pager_inherited {
                return pg.clone().map(|_| unsafe { addr(&_text_start) })
            }
            // TODO handle that case
            unimplemented!();
        }

        unsafe {
            // copy text
            let text_start = addr(&_text_start);
            let text_end = addr(&_text_end);
            self.mem.write_bytes(&_text_start, text_end - text_start, text_start)?;

            // copy data and heap
            let data_start = addr(&_data_start);
            self.mem.write_bytes(&_data_start, heap::heap_used_end() - data_start, data_start)?;

            // copy end-area of heap
            let heap_area_size = util::size_of::<heap::HeapArea>();
            self.mem.write_bytes(heap_end as *const u8, heap_area_size, heap_end)?;

            // copy stack
            self.mem.write_bytes(sp as *const u8, cfg::STACK_TOP - sp, sp)?;

            Ok(text_start)
        }
    }

    pub fn clear_mem(&self, buf: &mut [u8], mut count: usize, mut dst: usize) -> Result<(), Error> {
        if count == 0 {
            return Ok(())
        }

        for i in 0..buf.len() {
            buf[i] = 0;
        }

        while count > 0 {
            let amount = util::min(count, buf.len());
            self.mem.write(&buf[0..amount], dst)?;
            count -= amount;
            dst += amount;
        }

        Ok(())
    }

    pub fn load_segment(&self, file: &mut BufReader<FileRef>,
                        phdr: &elf::Phdr, buf: &mut [u8]) -> Result<(), Error> {
        file.seek(phdr.offset, SeekMode::SET)?;

        let mut count = phdr.filesz;
        let mut segoff = phdr.vaddr;
        while count > 0 {
            let amount = util::min(count, buf.len());
            let amount = file.read(&mut buf[0..amount])?;

            self.mem.write(&buf[0..amount], segoff)?;

            count -= amount;
            segoff += amount;
        }

        self.clear_mem(buf, phdr.memsz - phdr.filesz, segoff)
    }

    pub fn map_segment(&self, file: &mut BufReader<FileRef>, pager: &Pager,
                       phdr: &elf::Phdr) -> Result<(), Error> {
        let mut prot = kif::Perm::empty();
        if (phdr.flags & elf::PF::R.bits()) != 0 {
            prot |= kif::Perm::R;
        }
        if (phdr.flags & elf::PF::W.bits()) != 0 {
            prot |= kif::Perm::W;
        }
        if (phdr.flags & elf::PF::X.bits()) != 0 {
            prot |= kif::Perm::X;
        }

        let size = util::round_up(phdr.memsz, cfg::PAGE_SIZE);
        if phdr.memsz == phdr.filesz {
            file.get_ref().map(pager, phdr.vaddr, phdr.offset, size, prot)
        }
        else {
            assert!(phdr.filesz == 0);
            pager.map_anon(phdr.vaddr, size, prot).map(|_| ())
        }
    }

    pub fn load_program(&self, file: &mut BufReader<FileRef>) -> Result<usize, Error> {
        let mut buf = vec![0u8; 4096];
        let hdr: elf::Ehdr = read_object(file)?;

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
            file.seek(off, SeekMode::SET)?;
            let phdr: elf::Phdr = read_object(file)?;

            // we're only interested in non-empty load segments
            if phdr.ty != elf::PT::LOAD.val || phdr.memsz == 0 {
                continue;
            }

            if let Some(ref pg) = self.pager {
                self.map_segment(file, pg, &phdr)?;
            }
            else {
                self.load_segment(file, &phdr, &mut *buf)?;
            }
            off += hdr.phentsize as usize;

            end = phdr.vaddr + phdr.memsz;
        }

        if let Some(ref pg) = self.pager {
            // create area for boot/runtime stuff
            pg.map_anon(cfg::RT_START, cfg::RT_SIZE, kif::Perm::RW)?;

            // create area for stack
            pg.map_anon(cfg::STACK_BOTTOM, cfg::STACK_SIZE, kif::Perm::RW)?;

            // create heap
            pg.map_anon(util::round_up(end, cfg::PAGE_SIZE), cfg::APP_HEAP_SIZE, kif::Perm::RW)?;
        }

        Ok(hdr.entry)
    }

    pub fn write_arguments<I, S>(&self, off: &mut usize, args: I) -> Result<usize, Error>
                                 where I: iter::IntoIterator<Item = S>, S: AsRef<str> {
        let mut argptr = Vec::new();
        let mut argbuf = Vec::new();

        let mut argoff = *off;
        for s in args {
            // push argv entry
            argptr.push(argoff);

            // push string
            let arg = s.as_ref().as_bytes();
            argbuf.extend_from_slice(arg);

            // 0-terminate it
            argbuf.push('\0' as u8);

            argoff += arg.len() + 1;
        }

        self.mem.write(&argbuf, *off)?;
        self.mem.write(&argptr, argoff)?;
        *off = argoff + argptr.len() * util::size_of::<usize>();
        Ok(argoff)
    }
}

pub fn get_sp() -> usize {
    let res: usize;
    unsafe {
        asm!("mov %rsp, $0" : "=r"(res));
    }
    res
}
