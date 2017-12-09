use base::cell::MutCell;
use base::col::Vec;
use base::dtu::PEId;
use base::kif;

use platform;

pub struct PEMng {
    pes: Vec<bool>,
}

static INST: MutCell<Option<PEMng>> = MutCell::new(None);

pub fn init() {
    INST.set(Some(PEMng::new()));
}

pub fn get() -> &'static mut PEMng {
    INST.get_mut().as_mut().unwrap()
}

impl PEMng {
    fn new() -> Self {
        PEMng {
            pes: vec![false; platform::pe_count()],
        }
    }

    pub fn alloc_pe(&mut self, pedesc: &kif::PEDesc, except: Option<PEId>, _muxable: bool) -> Option<PEId> {
        for pe in platform::user_pes() {
            if platform::pe_desc(pe).isa() != pedesc.isa() ||
               platform::pe_desc(pe).pe_type() != pedesc.pe_type() {
                continue;
            }
            if let Some(ex) = except {
                if pe == ex {
                    continue;
                }
            }

            if !self.pes[pe] {
                self.pes[pe] = true;
                return Some(pe);
            }
        }

        None
    }

    pub fn free(&mut self, pe: PEId) {
        // TODO this doesn't work yet
        // self.pes[pe] = false;
    }
}
