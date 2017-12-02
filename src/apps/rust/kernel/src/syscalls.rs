use base::cell::RefCell;
use base::cfg;
use base::dtu;
use base::errors::{Error, Code};
use base::kif::{self, CapRngDesc, CapSel, CapType};
use base::rc::Rc;
use base::util;
use core::intrinsics;
use core::ptr::Shared;

use cap::{Capability, CapTable, KObject, MGateObject, RGateObject, SGateObject};
use mem;
use pes::{INVALID_VPE, vpemng};
use pes::VPE;

macro_rules! sysc_log {
    ($vpe:expr, $fmt:tt, $($args:tt)*) => (
        klog!(
            SYSC,
            concat!("{}:{}@{}: syscall::", $fmt),
            $vpe.borrow().id(), $vpe.borrow().name(), $vpe.borrow().pe_id(), $($args)*
        )
    )
}

macro_rules! sysc_err {
    ($vpe:expr, $e:expr, $fmt:tt, $($args:tt)*) => ({
        klog!(
            ERR,
            concat!("\x1B[37;41m{}:{}@{}: ", $fmt, "\x1B[0m"),
            $vpe.borrow().id(), $vpe.borrow().name(), $vpe.borrow().pe_id(), $($args)*
        );
        return Err(Error::new($e));
    })
}

macro_rules! get_kobj {
    ($vpe:expr, $sel:expr, $ty:ident) => ({
        let kobj = match $vpe.borrow().obj_caps().get($sel) {
            Some(c)         => c.get().clone(),
            None            => sysc_err!($vpe, Code::InvArgs, "Invalid capability",),
        };
        match kobj {
            KObject::$ty(k) => k,
            _               => sysc_err!($vpe, Code::InvArgs, "Expected {:?} cap", stringify!($ty)),
        }
    })
}

fn get_message<R: 'static>(msg: &'static dtu::Message) -> &'static R {
    let data: &[R] = unsafe { intrinsics::transmute(&msg.data) };
    &data[0]
}

fn reply<T>(msg: &'static dtu::Message, rep: *const T) {
    dtu::DTU::reply(0, rep as *const u8, util::size_of::<T>(), msg)
        .expect("Reply failed");
}

fn reply_result(msg: &'static dtu::Message, code: u64) {
    let rep = kif::syscalls::DefaultReply {
        error: code,
    };
    reply(msg, &rep);
}

fn reply_success(msg: &'static dtu::Message) {
    reply_result(msg, 0);
}

pub fn handle(msg: &'static dtu::Message) {
    let vpe: Rc<RefCell<VPE>> = vpemng::get().vpe(msg.header.label as usize).unwrap();
    let opcode: &u64 = get_message(msg);

    let res = match kif::syscalls::Operation::from(*opcode) {
        kif::syscalls::Operation::ACTIVATE      => activate(&vpe, msg),
        kif::syscalls::Operation::CREATE_MGATE  => create_mgate(&vpe, msg),
        kif::syscalls::Operation::CREATE_RGATE  => create_rgate(&vpe, msg),
        kif::syscalls::Operation::CREATE_SGATE  => create_sgate(&vpe, msg),
        kif::syscalls::Operation::VPE_CTRL      => vpectrl(&vpe, msg),
        kif::syscalls::Operation::REVOKE        => revoke(&vpe, msg),
        _                                       => panic!("Unexpected operation: {}", opcode),
    };

    if let Err(e) = res {
        reply_result(msg, e.code() as u64);
    }
}

fn create_mgate(vpe: &Rc<RefCell<VPE>>, msg: &'static dtu::Message) -> Result<(), Error> {
    let req: &kif::syscalls::CreateMGate = get_message(msg);
    let dst_sel = req.dst_sel as CapSel;
    let addr = req.addr as usize;
    let size = req.size as usize;
    let perms = kif::Perm::from_bits_truncate(req.perms as u8);

    sysc_log!(
        vpe, "create_mgate(dst={}, addr={:#x}, size={:#x}, perms={:?})",
        dst_sel, addr, size, perms
    );

    if !vpe.borrow().obj_caps().unused(dst_sel) {
        sysc_err!(vpe, Code::InvArgs, "Selector {} already in use", dst_sel);
    }
    if size == 0 || (size & kif::Perm::RWX.bits() as usize) != 0 || perms.is_empty() {
        sysc_err!(vpe, Code::InvArgs, "Invalid size or permissions",);
    }

    let alloc = mem::get().allocate(size, cfg::PAGE_SIZE)?;

    vpe.borrow_mut().obj_caps_mut().insert(
        Capability::new(dst_sel, KObject::MGate(MGateObject::new(
            alloc.global().pe(), INVALID_VPE, alloc.global().offset(), alloc.size(), perms
        )))
    );

    reply_success(msg);
    Ok(())
}

fn create_rgate(vpe: &Rc<RefCell<VPE>>, msg: &'static dtu::Message) -> Result<(), Error> {
    let req: &kif::syscalls::CreateRGate = get_message(msg);
    let dst_sel = req.dst_sel as CapSel;
    let order = req.order as i32;
    let msg_order = req.msgorder as i32;

    sysc_log!(
        vpe, "create_rgate(dst={}, size={:#x}, msg_size={:#x})",
        dst_sel, 1 << order, 1 << msg_order
    );

    if !vpe.borrow().obj_caps().unused(dst_sel) {
        sysc_err!(vpe, Code::InvArgs, "Selector {} already in use", dst_sel);
    }
    if msg_order > order || (1 << (order - msg_order)) > cfg::MAX_RB_SIZE {
        sysc_err!(vpe, Code::InvArgs, "Invalid size",);
    }

    vpe.borrow_mut().obj_caps_mut().insert(
        Capability::new(dst_sel, KObject::RGate(RGateObject::new(order, msg_order)))
    );

    reply_success(msg);
    Ok(())
}

fn create_sgate(vpe: &Rc<RefCell<VPE>>, msg: &'static dtu::Message) -> Result<(), Error> {
    let req: &kif::syscalls::CreateSGate = get_message(msg);
    let dst_sel = req.dst_sel as CapSel;
    let rgate_sel = req.rgate_sel as CapSel;
    let label = req.label as dtu::Label;
    let credits = req.credits;

    sysc_log!(
        vpe, "create_sgate(dst={}, rgate={:#x}, label={:#x}, credits={:#x})",
        dst_sel, rgate_sel, label, credits
    );

    if !vpe.borrow().obj_caps().unused(dst_sel) {
        sysc_err!(vpe, Code::InvArgs, "Selector {} already in use", dst_sel);
    }

    // TODO that's not nice
    let cap = {
        let rgate = get_kobj!(vpe, rgate_sel, RGate);
        Capability::new(dst_sel, KObject::SGate(SGateObject::new(
            &rgate, label, credits
        )))
    };

    {
        let mut vpe_mut = vpe.borrow_mut();
        let captbl: &mut CapTable = vpe_mut.obj_caps_mut();
        unsafe {
            let parent: Option<Shared<Capability>> = captbl.get_shared(rgate_sel);
            captbl.insert_as_child(cap, parent);
        }
    }

    reply_success(msg);
    Ok(())
}

fn activate(vpe: &Rc<RefCell<VPE>>, msg: &'static dtu::Message) -> Result<(), Error> {
    let req: &kif::syscalls::Activate = get_message(msg);
    let vpe_sel = req.vpe_sel as CapSel;
    let gate_sel = req.gate_sel as CapSel;
    let ep = req.ep as usize;
    let addr = req.addr as usize;

    sysc_log!(
        vpe, "activate(vpe={}, gate={}, ep={}, addr={:#x})",
        vpe_sel, gate_sel, ep, addr
    );

    if ep <= dtu::UPCALL_REP || ep >= dtu::EP_COUNT {
        sysc_err!(vpe, Code::InvArgs, "Invalid EP",);
    }

    let vpe_ref = get_kobj!(vpe, vpe_sel, VPE);

    {
        let mut vpe_mut = vpe_ref.borrow_mut();
        if let Some(mut old) = vpe_mut.get_ep_cap(ep) {
            if let Some(rgate) = old.as_rgate_mut() {
                rgate.addr = 0;
            }

            vpe_mut.invalidate_ep(ep, true)?;
        }
    }

    let maybe_kobj = vpe_ref.borrow().obj_caps().get(gate_sel).map(|cap| cap.get().clone());

    if let Some(kobj) = maybe_kobj {
        match kobj {
            KObject::RGate(ref r)    => {
                let mut rgate = r.borrow_mut();
                if rgate.activated() {
                    sysc_err!(vpe, Code::InvArgs, "Receive gate is already activated",);
                }

                rgate.vpe = vpe_ref.borrow().id();
                rgate.addr = addr;
                rgate.ep = Some(ep);

                if let Err(e) = vpe_ref.borrow_mut().config_rcv_ep(ep, &mut rgate) {
                    rgate.addr = 0;
                    sysc_err!(vpe, e.code(), "Unable to configure recv EP",);
                }
            },

            KObject::MGate(ref r)    => {
                if let Err(e) = vpe_ref.borrow_mut().config_mem_ep(ep, &r.borrow(), addr) {
                    sysc_err!(vpe, e.code(), "Unable to configure mem EP",);
                }
            },

            KObject::SGate(ref s)    => {
                let sgate = s.borrow();
                // TODO
                assert!(sgate.rgate.borrow().activated());
                let pe_id = vpemng::get().pe_of(sgate.rgate.borrow().vpe);
                vpe_ref.borrow_mut().config_snd_ep(ep, &sgate, pe_id.unwrap());
            },

            _                        => sysc_err!(vpe, Code::InvArgs, "Invalid capability",),
        };
        vpe_ref.borrow_mut().set_ep_cap(ep, Some(kobj));
    }
    else {
        vpe_ref.borrow_mut().invalidate_ep(ep, false)?;
        vpe_ref.borrow_mut().set_ep_cap(ep, None);
    }

    reply_success(msg);
    Ok(())
}

fn vpectrl(vpe: &Rc<RefCell<VPE>>, msg: &'static dtu::Message) -> Result<(), Error> {
    let req: &kif::syscalls::VPECtrl = get_message(msg);
    let vpe_sel = req.vpe_sel as CapSel;
    let op = kif::syscalls::VPEOp::from(req.op);
    let arg = req.arg;

    sysc_log!(
        vpe, "vpectrl(vpe={}, op={:?}, arg={:#x})",
        vpe_sel, op, arg
    );

    let vpe_ref = get_kobj!(vpe, vpe_sel, VPE);

    match op {
        kif::syscalls::VPEOp::INIT  => vpe_ref.borrow_mut().set_eps_addr(arg as usize),
        kif::syscalls::VPEOp::STOP  => {
            let id = vpe_ref.borrow().id();
            vpemng::get().remove(id);
            return Ok(());
        },
        _                           => panic!("VPEOp unsupported: {:?}", op),
    }

    reply_success(msg);
    Ok(())
}

fn revoke(vpe: &Rc<RefCell<VPE>>, msg: &'static dtu::Message) -> Result<(), Error> {
    let req: &kif::syscalls::Revoke = get_message(msg);
    let vpe_sel = req.vpe_sel as CapSel;
    let crd = CapRngDesc::new_from(req.crd);
    let own = req.own == 1;

    sysc_log!(
        vpe, "revoke(vpe={}, crd={}, own={})",
        vpe_sel, crd, own
    );

    if crd.cap_type() == CapType::OBJECT && crd.start() < 2 {
        sysc_err!(vpe, Code::InvArgs, "Cap 0 and 1 are not revokeable",);
    }

    let vpe_ref = get_kobj!(vpe, vpe_sel, VPE);

    vpe_ref.borrow_mut().obj_caps_mut().revoke(crd, own);

    reply_success(msg);
    Ok(())
}
