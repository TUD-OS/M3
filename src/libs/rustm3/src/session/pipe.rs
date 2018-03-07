use cap::Selector;
use cell::RefCell;
use com::MemGate;
use errors::Error;
use kif;
use rc::Rc;
use session::Session;
use vfs::{FileHandle, GenericFile};

pub struct Pipe {
    sess: Session,
}

impl Pipe {
    pub fn new(name: &str, mem: &MemGate, mem_size: usize) -> Result<Self, Error> {
        let sess = Session::new(name, mem_size as u64)?;
        sess.delegate_obj(mem.sel())?;
        Ok(Pipe {
            sess: sess,
        })
    }

    pub fn sel(&self) -> Selector {
        self.sess.sel()
    }

    pub fn create_chan(&self, read: bool) -> Result<FileHandle, Error> {
        let mut args = kif::syscalls::ExchangeArgs {
            count: 1,
            vals: kif::syscalls::ExchangeUnion {
                i: [read as u64, 0, 0, 0, 0, 0, 0, 0]
            },
        };
        let crd = self.sess.obtain(2, &mut args)?;
        Ok(Rc::new(RefCell::new(GenericFile::new(crd.start()))))
    }
}
