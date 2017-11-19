use base::cell::RefCell;
use base::col::{ToString, Vec};
use base::dtu::PEId;
use base::env;
use base::errors::{Code, Error};
use base::rc::Rc;

use arch::loader::Loader;
use arch::platform;
use pes::{VPE, VPEId, VPEFlags};

pub const MAX_VPES: usize   = 1024;
pub const KERNEL_VPE: usize = MAX_VPES;

pub struct VPEMng {
    vpes: Vec<Option<Rc<RefCell<VPE>>>>,
    count: usize,
    next_id: usize,
}

static mut INST: Option<VPEMng> = None;

pub fn get() -> &'static mut VPEMng {
    unsafe {
        INST.as_mut().unwrap()
    }
}

pub fn init() {
    unsafe {
        INST = Some(VPEMng {
            vpes: vec![None; MAX_VPES],
            count: 0,
            next_id: 0,
        })
    }
}

impl VPEMng {
    pub fn start(&mut self, args: env::Args) -> Result<(), Error> {
        let mut pe_id = 1;  // TODO

        let mut argv = Vec::new();
        for arg in args {
            argv.push(arg.to_string());
        }

        let mut i = 0;
        while i < argv.len() {
            let arg = &argv[i];
            if arg == "--" {
                i += 1;
                continue;
            }

            let id = self.get_id()?;
            let vpe = VPE::new(&arg, id, pe_id, VPEFlags::BOOTMOD);
            pe_id += 1;

            // find end of arguments
            let mut karg = false;
            vpe.borrow_mut().add_arg(&argv[i]);
            for j in i + 1..argv.len() {
                if argv[j] == "daemon" {
                    vpe.borrow_mut().make_daemon();
                    karg = true;
                }
                else if argv[j].starts_with("requires=") {
                    vpe.borrow_mut().add_requirement(&argv[j]["requires=".len()..]);
                    karg = true;
                }
                else if argv[j] == "--" {
                    break;
                }
                else if karg {
                    panic!("Kernel argument before program argument");
                }
                else {
                    vpe.borrow_mut().add_arg(&argv[i]);
                }

                i += 1;
            }

            if vpe.borrow().requirements().len() > 0 {
                // TODO self.pending.push(Pending::new(id));
            }
            else {
                let loader = Loader::get();
                let pid = loader.load_app(vpe.borrow_mut())?;
                vpe.borrow_mut().set_pid(pid);
            }

            self.vpes[id] = Some(vpe);
            self.count += 1;

            i += 1;
        }

        Ok(())
    }

    pub fn count(&self) -> usize {
        self.count
    }

    pub fn vpe(&self, id: VPEId) -> Option<Rc<RefCell<VPE>>> {
        self.vpes[id].as_ref().map(|v| v.clone())
    }

    pub fn pe_of(&self, id: VPEId) -> Option<PEId> {
        if id == KERNEL_VPE {
            Some(platform::kernel_pe())
        }
        else {
            self.vpe(id).map(|v| v.borrow().pe_id())
        }
    }

    pub fn remove(&mut self, id: VPEId) {
        match self.vpes[id] {
            Some(ref v) => v.borrow_mut().destroy(),
            None        => panic!("Removing nonexisting VPE with id {}", id),
        };
        self.vpes[id] = None;
        self.count -= 1;
    }

    fn get_id(&mut self) -> Result<usize, Error> {
        for id in self.next_id..MAX_VPES {
            if self.vpes[id].is_none() {
                self.next_id = id + 1;
                return Ok(id)
            }
        }

        for id in 0..self.next_id {
            if self.vpes[id].is_none() {
                self.next_id = id + 1;
                return Ok(id)
            }
        }

        Err(Error::new(Code::NoSpace))
    }
}
