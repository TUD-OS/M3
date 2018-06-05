use base::cell::StaticCell;
use base::cfg;
use base::envdata;
use base::GlobAddr;
use base::goff;
use base::kif::{PEDesc, PEType};
use base::dtu::PEId;

use arch::kdtu::KDTU;
use mem;
use pes::VPEDesc;
use platform;

const USABLE_MEM: usize     = (1024 + 256) * 1024 * 1024;

static LAST_PE: StaticCell<PEId> = StaticCell::new(0);

pub fn init() -> platform::KEnv {
    // read kernel env
    let kenv_addr = GlobAddr::new(envdata::get().kenv);
    let kenv: platform::KEnv = KDTU::get().read_obj(
        &VPEDesc::new_mem(kenv_addr.pe()), kenv_addr.offset()
    );

    let mut count = 0;
    let mem: &mut mem::MainMemory = mem::get();
    for i in 0..kenv.pe_count as usize {
        let pedesc = PEDesc::new_from(kenv.pes[i]);
        if pedesc.pe_type() == PEType::MEM {
            let mut m = mem::MemMod::new(i, 0, pedesc.mem_size());
            if count == 0 {
                // allocate FS image
                m.allocate(USABLE_MEM, 1).expect("Not enough space for FS image");
            }
            mem.add(m);
            count += 1;
        }
        else {
            LAST_PE.set(i);
        }
    }

    kenv
}

pub fn kernel_pe() -> PEId {
    envdata::get().pe_id as PEId
}
pub fn first_user_pe() -> PEId {
    kernel_pe() + 1
}
pub fn last_user_pe() -> PEId {
    *LAST_PE
}

pub fn default_rcvbuf(pe: PEId) -> goff {
    if platform::pe_desc(pe).has_virtmem() {
        return cfg::RECVBUF_SPACE as goff
    }
    else {
        (platform::pe_desc(pe).mem_size() - cfg::RECVBUF_SIZE_SPM) as goff
    }
}
pub fn rcvbufs_size(pe: PEId) -> usize {
    match platform::pe_desc(pe).has_virtmem() {
        true    => cfg::RECVBUF_SIZE,
        false   => cfg::RECVBUF_SIZE_SPM,
    }
}
