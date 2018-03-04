use base::dtu;
use core::intrinsics;
use thread;

use arch::kdtu;
use com;
use pes;
use syscalls;

pub fn workloop() {
    let thmng = thread::ThreadManager::get();
    let vpemng = pes::vpemng::get();

    while vpemng.count() > vpemng.daemons() {
        dtu::DTU::try_sleep(false, 0).unwrap();

        if let Some(msg) = dtu::DTU::fetch_msg(kdtu::KSYS_EP) {
            syscalls::handle(msg);
        }

        if let Some(msg) = dtu::DTU::fetch_msg(kdtu::KSRV_EP) {
            unsafe {
                let squeue: *mut com::SendQueue = intrinsics::transmute(msg.header.label as usize);
                (*squeue).received_reply(msg);
            }
        }

        if thmng.ready_count() > 0 {
            thmng.try_yield();
        }

        #[cfg(target_os = "linux")]
        ::arch::loader::check_childs();
    }
}
