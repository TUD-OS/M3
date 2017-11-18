use base::cfg;
use base::envdata;
use base::GlobAddr;
use base::kif::{PEDesc, PEType};
use base::util;
use base::dtu::PEId;
use core::intrinsics;

use arch::kdtu::KDTU;
use mem;
use pes::{INVALID_VPE, VPEDesc};
use platform;

const USABLE_MEM: usize     = 256 * 1024 * 1024;

static mut LAST_PE: PEId = 0;

pub fn init() -> platform::KEnv {
    // read kernel env
    let mut kenv: platform::KEnv = unsafe { intrinsics::uninit() };
    let kenv_addr = GlobAddr::new(envdata::get().kenv);
    KDTU::get().read_mem(
        &VPEDesc::new(kenv_addr.pe(), INVALID_VPE), kenv_addr.offset(),
        &mut kenv as *mut platform::KEnv as *mut u8, util::size_of::<platform::KEnv>()
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
            unsafe { LAST_PE = i };
        }
    }

    klog!(DEF, "mem = {:?}", mem);

    kenv
}

pub fn kernel_pe() -> PEId {
    envdata::get().pe_id as PEId
}
pub fn first_user_pe() -> PEId {
    kernel_pe() + 1
}
pub fn last_user_pe() -> PEId {
    unsafe { LAST_PE }
}

pub fn default_rcvbuf(pe: PEId) -> usize {
    platform::pe_desc(pe).mem_size() - cfg::RECVBUF_SIZE_SPM
}
pub fn rcvbufs_size(pe: PEId) -> usize {
    match platform::pe_desc(pe).has_virtmem() {
        true    => cfg::RECVBUF_SIZE,
        false   => cfg::RECVBUF_SIZE_SPM,
    }
}
