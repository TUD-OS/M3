use core::intrinsics;
use base::dtu;
use base::kif;
use base::util;

use pes::vpemng;

fn get_message<R: 'static>(msg: &'static dtu::Message) -> &'static R {
    let data: &[R] = unsafe { intrinsics::transmute(&msg.data) };
    &data[0]
}

fn reply<T>(msg: &'static dtu::Message, rep: *const T) {
    dtu::DTU::reply(0, rep as *const u8, util::size_of::<T>(), msg)
        .expect("Reply failed");
}

pub fn handle(msg: &'static dtu::Message) {
    let opcode: &u64 = get_message(msg);
    match kif::syscalls::Operation::from(*opcode) {
        kif::syscalls::Operation::VPE_CTRL  => vpectrl(msg),
        _                                   => panic!("Unexpected operation: {}", opcode),
    }
}

fn vpectrl(msg: &'static dtu::Message) {
    let vpe = vpemng::get().vpe(msg.header.label as usize).unwrap();
    let req: &kif::syscalls::VPECtrl = get_message(msg);

    let vpe_sel = req.vpe_sel;
    let op = kif::syscalls::VPEOp::from(req.op);
    let arg = req.arg;

    klog!(SYSC, "{}:{}@{}: syscall::vpectrl(vpe={}, op={:?}, arg={:#x})",
        vpe.borrow().id(), vpe.borrow().name(), vpe.borrow().pe_id(),
        vpe_sel, op, arg);

    match op {
        kif::syscalls::VPEOp::INIT  => vpe.borrow_mut().set_eps_addr(arg as usize),
        kif::syscalls::VPEOp::STOP  => {
            vpemng::get().remove(vpe.borrow().id());
            return;
        },
        _                           => panic!("VPEOp unsupported: {:?}", op),
    }

    let rep = kif::syscalls::DefaultReply {
        error: 0,
    };
    reply(msg, &rep);
}
