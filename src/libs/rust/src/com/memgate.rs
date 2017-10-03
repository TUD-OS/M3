use cap;
use com::gate::Gate;
use errors::Error;
use dtu;
use syscalls;

pub use kif::Perm;

pub struct MemGate {
    gate: Gate,
}

impl MemGate {
    pub fn new(size: usize, perm: Perm) -> Result<MemGate, Error> {
        let sel = cap::SelSpace::get().alloc();
        Self::new_with_sel(size, perm, sel)
    }

    pub fn new_with_sel(size: usize, perm: Perm, sel: cap::Selector) -> Result<MemGate, Error> {
        try!(syscalls::create_mgate(sel, 0, size, perm));
        Ok(MemGate {
            gate: Gate::new(sel, cap::Flags::empty())
        })
    }

    pub fn new_bind(sel: cap::Selector) -> MemGate {
        MemGate {
            gate: Gate::new(sel, cap::Flags::KEEP_SEL | cap::Flags::KEEP_CAP)
        }
    }

    pub fn derive(&self, offset: usize, size: usize, perm: Perm) -> Result<MemGate, Error> {
        let sel = cap::SelSpace::get().alloc();
        self.derive_with_sel(offset, size, perm, sel)
    }

    pub fn derive_with_sel(&self, offset: usize, size: usize, perm: Perm, sel: cap::Selector) -> Result<MemGate, Error> {
        try!(syscalls::derive_mem(sel, self.sel(), offset, size, perm));
        Ok(MemGate {
            gate: Gate::new(sel, cap::Flags::empty())
        })
    }

    pub fn sel(&self) -> cap::Selector {
        self.gate.cap.sel()
    }

    pub fn read<T>(&mut self, data: &mut [T], off: usize) -> Result<(), Error> {
        try!(self.gate.activate());
        dtu::DTU::read(self.gate.ep, data, off, 0)
    }

    pub fn write<T>(&mut self, data: &[T], off: usize) -> Result<(), Error> {
        try!(self.gate.activate());
        dtu::DTU::write(self.gate.ep, data, off, 0)
    }
}
