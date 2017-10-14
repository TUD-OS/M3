use cap;
use com::gate::Gate;
use errors::Error;
use dtu;
use kif::INVALID_SEL;
use syscalls;
use vpe;

pub use kif::Perm;

#[derive(Debug)]
pub struct MemGate {
    gate: Gate,
}

pub struct MGateArgs {
    size: usize,
    addr: usize,
    perm: Perm,
    sel: cap::Selector,
    flags: cap::Flags,
}

impl MGateArgs {
    pub fn new(size: usize, perm: Perm) -> MGateArgs {
        MGateArgs {
            size: size,
            addr: !0,
            perm: perm,
            sel: INVALID_SEL,
            flags: cap::Flags::empty(),
        }
    }

    pub fn addr(mut self, addr: usize) -> Self {
        self.addr = addr;
        self
    }

    pub fn sel(mut self, sel: cap::Selector) -> Self {
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
            vpe::VPE::cur().alloc_cap()
        }
        else {
            args.sel
        };

        try!(syscalls::create_mgate(sel, args.addr, args.size, args.perm));
        Ok(MemGate {
            gate: Gate::new(sel, args.flags)
        })
    }

    pub fn new_bind(sel: cap::Selector) -> Self {
        MemGate {
            gate: Gate::new(sel, cap::Flags::KEEP_CAP)
        }
    }

    pub fn derive(&self, offset: usize, size: usize, perm: Perm) -> Result<Self, Error> {
        let sel = vpe::VPE::cur().alloc_cap();
        self.derive_with_sel(offset, size, perm, sel)
    }

    pub fn derive_with_sel(&self, offset: usize, size: usize, perm: Perm, sel: cap::Selector) -> Result<Self, Error> {
        try!(syscalls::derive_mem(sel, self.sel(), offset, size, perm));
        Ok(MemGate {
            gate: Gate::new(sel, cap::Flags::empty())
        })
    }

    pub fn rebind(&mut self, sel: cap::Selector) -> Result<(), Error> {
        self.gate.rebind(sel)
    }

    pub fn sel(&self) -> cap::Selector {
        self.gate.cap.sel()
    }

    pub fn read<T>(&mut self, data: &mut [T], off: usize) -> Result<(), Error> {
        let ep = try!(self.gate.activate());
        dtu::DTU::read(ep, data, off, 0)
    }

    pub fn write<T>(&mut self, data: &[T], off: usize) -> Result<(), Error> {
        let ep = try!(self.gate.activate());
        dtu::DTU::write(ep, data, off, 0)
    }
}

pub mod tests {
    use super::*;

    pub fn run(t: &mut ::test::Tester) {
        run_test!(t, create);
        run_test!(t, create_readonly);
        run_test!(t, create_writeonly);
        run_test!(t, derive);
        run_test!(t, read_write);
    }

    fn create() {
        assert_err!(MemGate::new_with(MGateArgs::new(0x1000, Perm::R).sel(1)), Error::InvArgs);
    }

    fn create_readonly() {
        let mut mgate = assert_ok!(MemGate::new(0x1000, Perm::R));
        let mut data = [0u8; 8];
        assert_err!(mgate.write(&data, 0), Error::InvEP);
        assert_ok!(mgate.read(&mut data, 0));
    }

    fn create_writeonly() {
        let mut mgate = assert_ok!(MemGate::new(0x1000, Perm::W));
        let mut data = [0u8; 8];
        assert_err!(mgate.read(&mut data, 0), Error::InvEP);
        assert_ok!(mgate.write(&data, 0));
    }

    fn derive() {
        let mgate = assert_ok!(MemGate::new(0x1000, Perm::RW));
        assert_err!(mgate.derive(0x0, 0x2000, Perm::RW), Error::InvArgs);
        assert_err!(mgate.derive(0x1000, 0x10, Perm::RW), Error::InvArgs);
        assert_err!(mgate.derive(0x800, 0x1000, Perm::RW), Error::InvArgs);
        let mut dgate = assert_ok!(mgate.derive(0x800, 0x800, Perm::R));
        let mut data = [0u8; 8];
        assert_err!(dgate.write(&data, 0), Error::InvEP);
        assert_ok!(dgate.read(&mut data, 0));
    }

    fn read_write() {
        let mut mgate = assert_ok!(MemGate::new(0x1000, Perm::RW));
        let refdata = [0u8, 1, 2, 3, 4, 5, 6, 7];
        let mut data = refdata.clone();
        assert_ok!(mgate.write(&data, 0));
        assert_ok!(mgate.read(&mut data, 0));
        assert_eq!(data, refdata);

        assert_ok!(mgate.read(&mut data[0..4], 4));
        assert_eq!(&data[0..4], &refdata[4..8]);
        assert_eq!(&data[4..8], &refdata[4..8]);
    }
}
