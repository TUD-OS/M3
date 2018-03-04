use cap::{CapFlags, Selector};
use com::gate::Gate;
use core::fmt;
use core::intrinsics;
use dtu;
use errors::{Code, Error};
use goff;
use kif::{self, INVALID_SEL};
use syscalls;
use util;
use vpe;

pub use kif::Perm;

pub struct MemGate {
    gate: Gate,
}

pub struct MGateArgs {
    size: usize,
    addr: goff,
    perm: Perm,
    sel: Selector,
    flags: CapFlags,
}

impl MGateArgs {
    pub fn new(size: usize, perm: Perm) -> MGateArgs {
        MGateArgs {
            size: size,
            addr: !0,
            perm: perm,
            sel: INVALID_SEL,
            flags: CapFlags::empty(),
        }
    }

    pub fn addr(mut self, addr: goff) -> Self {
        self.addr = addr;
        self
    }

    pub fn sel(mut self, sel: Selector) -> Self {
        self.sel = sel;
        self
    }
}

impl MemGate {
    pub fn new(size: usize, perm: Perm) -> Result<Self, Error> {
        Self::new_with(MGateArgs::new(size, perm))
    }

    pub fn new_with(args: MGateArgs) -> Result<Self, Error> {
        let sel = if args.sel == INVALID_SEL {
            vpe::VPE::cur().alloc_sel()
        }
        else {
            args.sel
        };

        syscalls::create_mgate(sel, args.addr, args.size, args.perm)?;
        Ok(MemGate {
            gate: Gate::new(sel, args.flags)
        })
    }

    pub fn new_bind(sel: Selector) -> Self {
        MemGate {
            gate: Gate::new(sel, CapFlags::KEEP_CAP)
        }
    }

    pub fn ep(&self) -> Option<dtu::EpId> {
        self.gate.ep()
    }
    pub fn set_ep(&mut self, ep: dtu::EpId) {
        self.gate.set_ep(ep);
    }
    pub fn unset_ep(&mut self) {
        self.gate.unset_ep();
    }
    pub fn sel(&self) -> Selector {
        self.gate.sel()
    }

    pub fn derive(&self, offset: goff, size: usize, perm: Perm) -> Result<Self, Error> {
        let sel = vpe::VPE::cur().alloc_sel();
        self.derive_with_sel(offset, size, perm, sel)
    }

    pub fn derive_with_sel(&self, offset: goff, size: usize,
                           perm: Perm, sel: Selector) -> Result<Self, Error> {
        syscalls::derive_mem(sel, self.sel(), offset, size, perm)?;
        Ok(MemGate {
            gate: Gate::new(sel, CapFlags::empty())
        })
    }

    pub fn rebind(&mut self, sel: Selector) -> Result<(), Error> {
        self.gate.rebind(sel)
    }

    pub fn read<T>(&self, data: &mut [T], off: goff) -> Result<(), Error> {
        self.read_bytes(data.as_mut_ptr() as *mut u8, data.len() * util::size_of::<T>(), off)
    }

    pub fn read_obj<T>(&self, off: goff) -> Result<T, Error> {
        let mut obj: T = unsafe { intrinsics::uninit() };
        self.read_bytes(&mut obj as *mut T as *mut u8, util::size_of::<T>(), off)?;
        Ok(obj)
    }

    pub fn read_bytes(&self, mut data: *mut u8, mut size: usize, mut off: goff) -> Result<(), Error> {
        let ep = self.gate.activate()?;

        loop {
            match dtu::DTU::read(ep, data, size, off, dtu::CmdFlags::empty()) {
                Ok(_)                                   => return Ok(()),
                Err(ref e) if e.code() == Code::VPEGone => {
                    // simply retry the write if the forward failed (pagefault)
                    if self.forward_read(&mut data, &mut size, &mut off).is_ok() && size == 0 {
                        break Ok(())
                    }
                },
                Err(e)                                  => return Err(e),
            }
        }
    }

    pub fn write<T>(&self, data: &[T], off: goff) -> Result<(), Error> {
        self.write_bytes(data.as_ptr() as *const u8, data.len() * util::size_of::<T>(), off)
    }

    pub fn write_obj<T>(&self, obj: *const T, off: goff) -> Result<(), Error> {
        self.write_bytes(obj as *const u8, util::size_of::<T>(), off)
    }

    pub fn write_bytes(&self, mut data: *const u8, mut size: usize, mut off: goff) -> Result<(), Error> {
        let ep = self.gate.activate()?;

        loop {
            match dtu::DTU::write(ep, data, size, off, dtu::CmdFlags::empty()) {
                Ok(_)                                   => return Ok(()),
                Err(ref e) if e.code() == Code::VPEGone => {
                    // simply retry the write if the forward failed (pagefault)
                    if self.forward_write(&mut data, &mut size, &mut off).is_ok() && size == 0 {
                        break Ok(());
                    }
                },
                Err(e)                                  => return Err(e),
            }
        }
    }

    fn forward_read(&self, data: &mut *mut u8, size: &mut usize, off: &mut goff) -> Result<(), Error> {
        let amount = util::min(kif::syscalls::MAX_MSG_SIZE, *size);
        syscalls::forward_read(
            self.sel(), unsafe { util::slice_for_mut(*data, amount) }, *off,
            kif::syscalls::ForwardMemFlags::empty(), 0
        )?;
        *data = unsafe { (*data).offset(amount as isize) };
        *off += amount as goff;
        *size -= amount;
        Ok(())
    }

    fn forward_write(&self, data: &mut *const u8, size: &mut usize, off: &mut goff) -> Result<(), Error> {
        let amount = util::min(kif::syscalls::MAX_MSG_SIZE, *size);
        syscalls::forward_write(
            self.sel(), unsafe { util::slice_for(*data, amount) }, *off,
            kif::syscalls::ForwardMemFlags::empty(), 0
        )?;
        *data = unsafe { (*data).offset(amount as isize) };
        *off += amount as goff;
        *size -= amount;
        Ok(())
    }
}

impl fmt::Debug for MemGate {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "MemGate[sel: {}, ep: {:?}]", self.sel(), self.gate.ep())
    }
}
