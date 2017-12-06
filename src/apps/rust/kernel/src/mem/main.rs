use base::cell::MutCell;
use base::col::Vec;
use base::errors::{Code, Error};
use base::GlobAddr;
use core::fmt;
use mem::MemMod;

pub struct MainMemory {
    mods: Vec<MemMod>,
}

pub struct Allocation {
    gaddr: GlobAddr,
    size: usize,
}

impl Allocation {
    fn new(gaddr: GlobAddr, size: usize) -> Self {
        Allocation {
            gaddr: gaddr,
            size: size,
        }
    }

    pub fn global(&self) -> GlobAddr {
        self.gaddr
    }
    pub fn size(&self) -> usize {
        self.size
    }
}

impl MainMemory {
    fn new() -> Self {
        MainMemory {
            mods: Vec::new(),
        }
    }

    pub fn add(&mut self, m: MemMod) {
        self.mods.push(m)
    }

    pub fn allocate(&mut self, size: usize, align: usize) -> Result<Allocation, Error> {
        for m in &mut self.mods {
            if let Ok(gaddr) = m.allocate(size, align) {
                klog!(MEM, "Allocated {:#x} bytes at {:?}", size, gaddr);
                return Ok(Allocation::new(gaddr, size))
            }
        }
        Err(Error::new(Code::NoSpace))
    }
    pub fn allocate_at(&mut self, offset: usize, size: usize) -> Result<Allocation, Error> {
        // TODO check if that's actually ok
        Ok(Allocation::new(self.mods[0].addr() + offset, size))
    }

    pub fn free(&mut self, alloc: &Allocation) {
        for m in &mut self.mods {
            if m.free(alloc.gaddr, alloc.size) {
                klog!(MEM, "Freed {:#x} bytes at {:?}", alloc.size, alloc.gaddr);
                break;
            }
        }
    }

    pub fn capacity(&self) -> usize {
        self.mods.iter().fold(0, |total, ref m| total + m.capacity())
    }
    pub fn available(&self) -> usize {
        self.mods.iter().fold(0, |total, ref m| total + m.available())
    }
}

impl fmt::Debug for MainMemory {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "size: {} MiB, available: {} MiB, mods: [\n",
            self.capacity() / (1024 * 1024), self.available() / (1024 * 1024))?;
        for m in &self.mods {
            write!(f, "  {:?}\n", m)?;
        }
        write!(f, "]")
    }
}

static MEM: MutCell<Option<MainMemory>> = MutCell::new(None);

pub fn init() {
    MEM.set(Some(MainMemory::new()));
}

pub fn get() -> &'static mut MainMemory {
    MEM.get_mut().as_mut().unwrap()
}
