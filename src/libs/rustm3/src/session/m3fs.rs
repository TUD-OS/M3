use cap::Selector;
use cell::RefCell;
use col::Vec;
use com::*;
use core::any::Any;
use core::{fmt, intrinsics};
use errors::Error;
use kif;
use rc::{Rc, Weak};
use serialize::Sink;
use session::Session;
use util;
use vfs::{FileHandle, FileInfo, FileMode, FileSystem, FSHandle, GenericFile, OpenFlags};
use vpe::VPE;

pub type ExtId = u16;

pub struct M3FS {
    self_weak: Weak<RefCell<M3FS>>,
    sess: Session,
    sgate: SendGate,
}

int_enum! {
    struct Operation : u32 {
        const STAT      = 0x4;
        const MKDIR     = 0x5;
        const RMDIR     = 0x6;
        const LINK      = 0x7;
        const UNLINK    = 0x8;
    }
}

bitflags! {
    pub struct LocFlags : u32 {
        const BYTE_OFF  = 0b01;
        const EXTEND    = 0b10;
    }
}

impl M3FS {
    fn create(sess: Session, sgate: SendGate) -> FSHandle {
        let inst = Rc::new(RefCell::new(M3FS {
            self_weak: Weak::new(),
            sess: sess,
            sgate: sgate,
        }));
        inst.borrow_mut().self_weak = Rc::downgrade(&inst);
        inst
    }

    pub fn new(name: &str) -> Result<FSHandle, Error> {
        let sels = VPE::cur().alloc_sels(2);
        let sess = Session::new_with_sel(name, 0, sels + 1)?;

        let crd = kif::CapRngDesc::new(kif::CapType::OBJECT, sels + 0, 1);
        let mut args = kif::syscalls::ExchangeArgs::default();
        sess.obtain_for(VPE::cur().sel(), crd, &mut args)?;
        let sgate = SendGate::new_bind(sels + 0);
        Ok(Self::create(sess, sgate))
    }

    pub fn new_bind(sels: Selector) -> FSHandle {
        Self::create(Session::new_bind(sels + 0), SendGate::new_bind(sels + 1))
    }

    pub fn sess(&self) -> &Session {
        &self.sess
    }
}

impl FileSystem for M3FS {
    fn as_any(&self) -> &Any {
        self
    }

    fn open(&self, path: &str, flags: OpenFlags) -> Result<FileHandle, Error> {
        let mut args = kif::syscalls::ExchangeArgs {
            count: 1,
            vals: kif::syscalls::ExchangeUnion {
                s: kif::syscalls::ExchangeUnionStr {
                    i: [flags.bits() as u64, 0],
                    s: unsafe { intrinsics::uninit() },
                },
            },
        };

        // copy path
        unsafe {
            for (a, c) in args.vals.s.s.iter_mut().zip(path.bytes()) {
                *a = c as u8;
            }
            args.vals.s.s[path.len()] = '\0' as u8;
        }

        let crd = self.sess.obtain(2, &mut args)?;
        Ok(Rc::new(RefCell::new(GenericFile::new(crd.start()))))
    }

    fn stat(&self, path: &str) -> Result<FileInfo, Error> {
        let mut reply = send_recv_res!(
            &self.sgate, RecvGate::def(),
            Operation::STAT, path
        )?;
        Ok(reply.pop())
    }

    fn mkdir(&self, path: &str, mode: FileMode) -> Result<(), Error> {
        send_recv_res!(
            &self.sgate, RecvGate::def(),
            Operation::MKDIR, path, mode
        ).map(|_| ())
    }
    fn rmdir(&self, path: &str) -> Result<(), Error> {
        send_recv_res!(
            &self.sgate, RecvGate::def(),
            Operation::RMDIR, path
        ).map(|_| ())
    }

    fn link(&self, old_path: &str, new_path: &str) -> Result<(), Error> {
        send_recv_res!(
            &self.sgate, RecvGate::def(),
            Operation::LINK, old_path, new_path
        ).map(|_| ())
    }
    fn unlink(&self, path: &str) -> Result<(), Error> {
        send_recv_res!(
            &self.sgate, RecvGate::def(),
            Operation::UNLINK, path
        ).map(|_| ())
    }

    fn fs_type(&self) -> u8 {
        b'M'
    }
    fn exchange_caps(&self, vpe: Selector, dels: &mut Vec<Selector>, max_sel: &mut Selector) {
        dels.push(self.sess.sel());

        // TODO error case?
        let crd = kif::CapRngDesc::new(kif::CapType::OBJECT, self.sess.sel() + 1, 1);
        let mut args = kif::syscalls::ExchangeArgs::default();
        self.sess.obtain_for(vpe, crd, &mut args).ok();
        *max_sel = util::max(*max_sel, self.sess.sel() + 2);
    }
    fn serialize(&self, s: &mut VecSink) {
        s.push(&self.sess.sel());
        s.push(&self.sgate.sel());
    }
}

impl M3FS {
    pub fn unserialize(s: &mut SliceSource) -> FSHandle {
        let sels: Selector = s.pop();
        M3FS::new_bind(sels)
    }
}

impl fmt::Debug for M3FS {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "M3FS[sess={:?}, sgate={:?}]", self.sess, self.sgate)
    }
}
