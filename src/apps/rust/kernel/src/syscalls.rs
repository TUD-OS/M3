use core::intrinsics;
use base::cell::RefCell;
use base::cfg;
use base::dtu;
use base::errors::{Error, Code};
use base::kif::{self, CapRngDesc, CapSel, CapType};
use base::rc::Rc;
use base::util;

use cap::{Capability, KObject, MGateObject};
use mem;
use pes::{INVALID_VPE, vpemng};
use pes::VPE;

fn get_message<R: 'static>(msg: &'static dtu::Message) -> &'static R {
    let data: &[R] = unsafe { intrinsics::transmute(&msg.data) };
    &data[0]
}

fn reply<T>(msg: &'static dtu::Message, rep: *const T) {
    dtu::DTU::reply(0, rep as *const u8, util::size_of::<T>(), msg)
        .expect("Reply failed");
}

pub fn handle(msg: &'static dtu::Message) {
    let vpe: Rc<RefCell<VPE>> = vpemng::get().vpe(msg.header.label as usize).unwrap();
    let opcode: &u64 = get_message(msg);

    let res = match kif::syscalls::Operation::from(*opcode) {
        kif::syscalls::Operation::CREATE_MGATE  => create_mgate(vpe, msg),
        kif::syscalls::Operation::VPE_CTRL      => vpectrl(vpe, msg),
        kif::syscalls::Operation::REVOKE        => revoke(vpe, msg),
        _                                       => panic!("Unexpected operation: {}", opcode),
    };

    if let Err(e) = res {
        let rep = kif::syscalls::DefaultReply {
            error: e.code() as u64,
        };
        reply(msg, &rep);
    }
}

fn create_mgate(vpe: Rc<RefCell<VPE>>, msg: &'static dtu::Message) -> Result<(), Error> {
    let req: &kif::syscalls::CreateMGate = get_message(msg);
    let dst_sel = req.dst_sel as CapSel;
    let addr = req.addr as usize;
    let size = req.size as usize;
    let perms = kif::Perm::from_bits_truncate(req.perms as u8);

    klog!(SYSC, "{}:{}@{}: syscall::create_mgate(dst={}, addr={:#x}, size={:#x}, perms={:?})",
        vpe.borrow().id(), vpe.borrow().name(), vpe.borrow().pe_id(),
        dst_sel, addr, size, perms);

    if !vpe.borrow().obj_caps().unused(dst_sel) {
        return Err(Error::new(Code::InvArgs));
    }
    if size == 0 || (size & kif::Perm::RWX.bits() as usize) != 0 || perms.is_empty() {
        return Err(Error::new(Code::InvArgs));
    }

    let alloc = mem::get().allocate(size, cfg::PAGE_SIZE)?;

    vpe.borrow_mut().obj_caps_mut().insert(
        Capability::new(dst_sel, KObject::MGate(MGateObject::new(
            alloc.global().pe(), INVALID_VPE, alloc.global().offset(), alloc.size(), perms)
        ))
    );

    let rep = kif::syscalls::DefaultReply {
        error: 0,
    };
    reply(msg, &rep);
    Ok(())
}

fn vpectrl(vpe: Rc<RefCell<VPE>>, msg: &'static dtu::Message) -> Result<(), Error> {
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
            return Ok(());
        },
        _                           => panic!("VPEOp unsupported: {:?}", op),
    }

    let rep = kif::syscalls::DefaultReply {
        error: 0,
    };
    reply(msg, &rep);
    Ok(())
}

fn revoke(vpe: Rc<RefCell<VPE>>, msg: &'static dtu::Message) -> Result<(), Error> {
    let req: &kif::syscalls::Revoke = get_message(msg);
    let vpe_sel = req.vpe_sel as CapSel;
    let crd = CapRngDesc::new_from(req.crd);
    let own = req.own == 1;

    klog!(SYSC, "{}:{}@{}: syscall::revoke(vpe={}, crd={}, own={})",
        vpe.borrow().id(), vpe.borrow().name(), vpe.borrow().pe_id(),
        vpe_sel, crd, own);

    if crd.cap_type() == CapType::OBJECT && crd.start() < 2 {
        return Err(Error::new(Code::InvArgs));
    }

    // TODO use vpe_sel
    vpe.borrow_mut().obj_caps_mut().revoke(crd, own);

    let rep = kif::syscalls::DefaultReply {
        error: 0,
    };
    reply(msg, &rep);
    Ok(())
}
