use base::cell::RefCell;
use base::col::{DList, ToString, Vec};
use base::dtu::PEId;
use base::env;
use base::errors::{Code, Error};
use base::rc::Rc;

use arch::loader::Loader;
use arch::platform;
use com::ServiceList;
use pes::{VPE, VPEId, VPEFlags};

pub const MAX_VPES: usize   = 1024;
pub const KERNEL_VPE: usize = MAX_VPES;

pub struct VPEMng {
    vpes: Vec<Option<Rc<RefCell<VPE>>>>,
    pending: DList<VPEId>,
    count: usize,
    daemons: usize,
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
            pending: DList::new(),
            count: 0,
            daemons: 0,
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
            klog!(VPES, "Created VPE {} [id={}, pe={}]", &arg, id, pe_id);
            pe_id += 1;

            // find end of arguments
            let mut karg = false;
            vpe.borrow_mut().add_arg(&argv[i]);
            for j in i + 1..argv.len() {
                if argv[j] == "daemon" {
                    vpe.borrow_mut().make_daemon();
                    self.daemons += 1;
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
                    vpe.borrow_mut().add_arg(&argv[j]);
                }

                i += 1;
            }

            if vpe.borrow().requirements().len() > 0 {
                self.pending.push_back(id);
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

    pub fn start_pending(&mut self) {
        let mut it = self.pending.iter_mut();
        while let Some(id) = it.next() {
            let vpe = self.vpes[*id as usize].as_ref().unwrap();
            let mut fullfilled = true;
            for r in vpe.borrow().requirements() {
                if ServiceList::get().find(r).is_none() {
                    fullfilled = false;
                    break;
                }
            }

            if fullfilled {
                let loader = Loader::get();
                let pid = loader.load_app(vpe.borrow_mut()).unwrap();
                vpe.borrow_mut().set_pid(pid);

                it.remove();
            }
        }
    }

    pub fn count(&self) -> usize {
        self.count
    }
    pub fn daemons(&self) -> usize {
        self.daemons
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
            Some(ref v) => {
                v.borrow_mut().destroy();
                klog!(
                    VPES, "Removed VPE {} [id={}, pe={}]",
                    v.borrow().name(), v.borrow().id(), v.borrow().pe_id()
                );
            },
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
