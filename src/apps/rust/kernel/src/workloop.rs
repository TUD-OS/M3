use base::dtu;
use core::intrinsics;
use thread;

use com;
use pes;
use syscalls;

pub fn workloop() {
    let thmng = thread::ThreadManager::get();
    let vpemng = pes::vpemng::get();

    while vpemng.count() > vpemng.daemons() {
        dtu::DTU::try_sleep(false, 0).unwrap();

        if let Some(msg) = dtu::DTU::fetch_msg(0) {
            syscalls::handle(msg);
        }

        if let Some(msg) = dtu::DTU::fetch_msg(2) {
            unsafe {
                let squeue: *mut com::SendQueue = intrinsics::transmute(msg.header.label);
                (*squeue).received_reply(msg);
            }
        }

        if thmng.ready_count() > 0 {
            thmng.try_yield();
        }
    }
}
