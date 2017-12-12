use base::cell::RefCell;
use base::cfg;
use base::col::ToString;
use base::dtu;
use base::errors::{Error, Code};
use base::kif::{self, CapRngDesc, CapSel, CapType};
use base::rc::Rc;
use base::util;
use core::intrinsics;
use thread;

use arch::kdtu;
use cap::{Capability, KObject};
use cap::{MGateObject, RGateObject, SGateObject, ServObject, SessObject};
use com::{Service, ServiceList};
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

fn send_reply<T>(msg: &'static dtu::Message, rep: *const T) {
    dtu::DTU::reply(kdtu::KSYS_EP, rep as *const u8, util::size_of::<T>(), msg)
        .expect("Reply failed");
}

fn reply_result(msg: &'static dtu::Message, code: u64) {
    let rep = kif::syscalls::DefaultReply {
        error: code,
    };
    send_reply(msg, &rep);
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
        kif::syscalls::Operation::CREATE_SRV    => create_srv(&vpe, msg),
        kif::syscalls::Operation::CREATE_SESS   => create_sess(&vpe, msg),
        kif::syscalls::Operation::CREATE_VPE    => create_vpe(&vpe, msg),
        kif::syscalls::Operation::DERIVE_MEM    => derive_mem(&vpe, msg),
        kif::syscalls::Operation::EXCHANGE      => exchange(&vpe, msg),
        kif::syscalls::Operation::DELEGATE      => exchange_over_sess(&vpe, msg, false),
        kif::syscalls::Operation::OBTAIN        => exchange_over_sess(&vpe, msg, true),
        kif::syscalls::Operation::VPE_CTRL      => vpectrl(&vpe, msg),
        kif::syscalls::Operation::REVOKE        => revoke(&vpe, msg),
        kif::syscalls::Operation::NOOP          => noop(&vpe, msg),
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

    let alloc: mem::Allocation = if addr == !0 {
        mem::get().allocate(size, cfg::PAGE_SIZE)
    }
    else {
        mem::get().allocate_at(addr, size)
    }?;

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
        vpe, "create_sgate(dst={}, rgate={}, label={:#x}, credits={:#x})",
        dst_sel, rgate_sel, label, credits
    );

    if !vpe.borrow().obj_caps().unused(dst_sel) {
        sysc_err!(vpe, Code::InvArgs, "Selector {} already in use", dst_sel);
    }

    {
        let rgate: Rc<RefCell<RGateObject>> = get_kobj!(vpe, rgate_sel, RGate);
        let cap = Capability::new(dst_sel, KObject::SGate(SGateObject::new(
            &rgate, label, credits
        )));

        let mut vpe_mut = vpe.borrow_mut();
        vpe_mut.obj_caps_mut().insert_as_child(cap, rgate_sel);
    }

    reply_success(msg);
    Ok(())
}

fn create_srv(vpe: &Rc<RefCell<VPE>>, msg: &'static dtu::Message) -> Result<(), Error> {
    let req: &kif::syscalls::CreateSrv = get_message(msg);
    let dst_sel = req.dst_sel as CapSel;
    let rgate_sel = req.rgate_sel as CapSel;
    let name: &str = unsafe { intrinsics::transmute(&req.name[0..req.namelen as usize]) };

    sysc_log!(
        vpe, "create_srv(dst={}, rgate={}, name={})",
        dst_sel, rgate_sel, name
    );

    if !vpe.borrow().obj_caps().unused(dst_sel) {
        sysc_err!(vpe, Code::InvArgs, "Selector {} already in use", dst_sel);
    }
    if ServiceList::get().find(&name).is_some() {
        sysc_err!(vpe, Code::Exists, "Selector {} does already exist", name);
    }

    let rgate: Rc<RefCell<RGateObject>> = get_kobj!(vpe, rgate_sel, RGate);

    vpe.borrow_mut().obj_caps_mut().insert(
        Capability::new(dst_sel, KObject::Serv(ServObject::new(vpe, name.to_string(), rgate)))
    );

    ServiceList::get().add(vpe, dst_sel);
    vpemng::get().start_pending();

    reply_success(msg);
    Ok(())
}

fn create_sess(vpe: &Rc<RefCell<VPE>>, msg: &'static dtu::Message) -> Result<(), Error> {
    let req: &kif::syscalls::CreateSess = get_message(msg);
    let dst_sel = req.dst_sel as CapSel;
    let arg = req.arg;
    let name: &str = unsafe { intrinsics::transmute(&req.name[0..req.namelen as usize]) };

    sysc_log!(
        vpe, "create_sess(dst={}, arg={:#x}, name={})",
        dst_sel, arg, name
    );

    if !vpe.borrow().obj_caps().unused(dst_sel) {
        sysc_err!(vpe, Code::InvArgs, "Selector {} already in use", dst_sel);
    }

    let sentry: Option<&Service> = ServiceList::get().find(name);
    if sentry.is_none() {
        sysc_err!(vpe, Code::Exists, "Selector {} does already exist", name);
    }

    let smsg = kif::service::Open {
        opcode: kif::service::Operation::OPEN.val as u64,
        arg: arg,
    };

    let serv: Rc<RefCell<ServObject>> = sentry.unwrap().get_kobj();
    let res = ServObject::send_receive(&serv, util::object_to_bytes(&smsg));

    match res {
        None        => sysc_err!(vpe, Code::Exists, "Service {} unreachable", name),

        Some(rmsg)  => {
            let reply: &kif::service::OpenReply = get_message(rmsg);

            sysc_log!(vpe, "create_sess continue with res={}", {reply.res});

            if reply.res != 0 {
                sysc_err!(vpe, Code::from(reply.res as u32), "Server denied session creation",);
            }
            else {
                let cap = Capability::new(dst_sel, KObject::Sess(SessObject::new(
                    &serv, reply.sess, false
                )));

                // inherit the session-cap from the service-cap. this way, it will be automatically
                // revoked if the service-cap is revoked
                sentry.map(|se| {
                    vpe.borrow_mut().obj_caps_mut().insert_as_child_from(
                        cap, se.vpe().borrow_mut().obj_caps_mut(), se.sel()
                    );
                });
            }
        }
    }

    reply_success(msg);
    Ok(())
}

fn create_vpe(vpe: &Rc<RefCell<VPE>>, msg: &'static dtu::Message) -> Result<(), Error> {
    let req: &kif::syscalls::CreateVPE = get_message(msg);
    let dst_sel = req.dst_sel as CapSel;
    let mgate_sel = req.mgate_sel as CapSel;
    let sgate_sel = req.sgate_sel as CapSel;
    let rgate_sel = req.rgate_sel as CapSel;
    let pedesc = kif::PEDesc::new_from(req.pe as u32);
    let sep = req.sep;
    let rep = req.rep;
    let muxable = req.muxable == 1;
    let name: &str = unsafe { intrinsics::transmute(&req.name[0..req.namelen as usize]) };

    // TODO support that
    assert!(sgate_sel == kif::INVALID_SEL);
    assert!(rgate_sel == kif::INVALID_SEL);

    sysc_log!(
        vpe, "create_vpe(dst={}, mgate={}, sgate={}, rgate={}, name={}, pe={:?}, sep={}, rep={}, muxable={})",
        dst_sel, mgate_sel, sgate_sel, rgate_sel, name, pedesc, sep, rep, muxable
    );

    if !vpe.borrow().obj_caps().unused(dst_sel) || !vpe.borrow().obj_caps().unused(mgate_sel) {
        sysc_err!(vpe, Code::InvArgs, "Selector {} or {} already in use", dst_sel, mgate_sel);
    }

    let nvpe: Rc<RefCell<VPE>> = vpemng::get().create(name, &pedesc, muxable)?;

    // childs of daemons are daemons
    if vpe.borrow().is_daemon() {
        nvpe.borrow_mut().make_daemon();
    }

    // inherit VPE and mem caps to the parent
    {
        let mut vpe_ref  = vpe.borrow_mut();
        let mut nvpe_ref = nvpe.borrow_mut();
        {
            let vpe_cap: Option<&mut Capability> = nvpe_ref.obj_caps_mut().get_mut(0);
            vpe_cap.map(|c| vpe_ref.obj_caps_mut().obtain(dst_sel, c, false));
        }

        {
            let mem_cap: Option<&mut Capability> = nvpe_ref.obj_caps_mut().get_mut(1);
            mem_cap.map(|c| vpe_ref.obj_caps_mut().obtain(mgate_sel, c, true));
        }
    }

    let kreply = kif::syscalls::CreateVPEReply {
        error: 0,
        pe: nvpe.borrow().pe_desc().value() as u64,
    };
    send_reply(msg, &kreply);

    Ok(())
}

fn derive_mem(vpe: &Rc<RefCell<VPE>>, msg: &'static dtu::Message) -> Result<(), Error> {
    let req: &kif::syscalls::DeriveMem = get_message(msg);
    let dst_sel = req.dst_sel as CapSel;
    let src_sel = req.src_sel as CapSel;
    let offset = req.offset as usize;
    let size = req.size as usize;
    let perms = kif::Perm::from_bits_truncate(req.perms as u8);

    sysc_log!(
        vpe, "derive_mem(src={}, dst={}, size={:#x}, offset={:#x}, perms={:?})",
        src_sel, dst_sel, size, offset, perms
    );

    if !vpe.borrow().obj_caps().unused(dst_sel) {
        sysc_err!(vpe, Code::InvArgs, "Selector {} already in use", dst_sel);
    }

    let cap = {
        let mgate: Rc<RefCell<MGateObject>> = get_kobj!(vpe, src_sel, MGate);

        if offset + size < offset || offset + size > mgate.borrow().size || size == 0 {
            sysc_err!(vpe, Code::InvArgs, "Size or offset invalid",);
        }

        let mgate_ref = mgate.borrow();
        let mgate_obj = MGateObject::new(
            mgate_ref.pe, mgate_ref.vpe, mgate_ref.addr + offset, size, perms & mgate_ref.perms
        );
        mgate_obj.borrow_mut().derived = true;
        Capability::new(dst_sel, KObject::MGate(mgate_obj))
    };

    {
        let mut vpe_mut = vpe.borrow_mut();
        vpe_mut.obj_caps_mut().insert_as_child(cap, src_sel);
    }

    reply_success(msg);
    Ok(())
}

fn do_exchange(vpe1: &Rc<RefCell<VPE>>, vpe2: &Rc<RefCell<VPE>>,
               c1: &kif::CapRngDesc, c2: &kif::CapRngDesc, obtain: bool) -> Result<(), Error> {
    let src = if obtain { vpe2 } else { vpe1 };
    let dst = if obtain { vpe1 } else { vpe2 };
    let src_rng = if obtain { c2 } else { c1 };
    let dst_rng = if obtain { c1 } else { c2 };

    if vpe1.borrow().id() == vpe2.borrow().id() {
        return Err(Error::new(Code::InvArgs));
    }
    if c1.cap_type() != c2.cap_type() {
        return Err(Error::new(Code::InvArgs));
    }
    if (obtain && c2.count() > c1.count()) || (!obtain && c2.count() != c1.count()) {
        return Err(Error::new(Code::InvArgs));
    }
    if !dst.borrow().obj_caps().range_unused(dst_rng) {
        return Err(Error::new(Code::InvArgs));
    }

    let mut src_ref = src.borrow_mut();
    let mut dst_ref = dst.borrow_mut();
    for i in 0..c2.count() {
        let src_sel = src_rng.start() + i;
        let dst_sel = dst_rng.start() + i;
        let src_cap = src_ref.obj_caps_mut().get_mut(src_sel);
        src_cap.map(|c| dst_ref.obj_caps_mut().obtain(dst_sel, c, true));
    }

    Ok(())
}

fn exchange(vpe: &Rc<RefCell<VPE>>, msg: &'static dtu::Message) -> Result<(), Error> {
    let req: &kif::syscalls::Exchange = get_message(msg);
    let vpe_sel = req.vpe_sel as CapSel;
    let own_crd = CapRngDesc::new_from(req.own_crd);
    let other_crd = CapRngDesc::new(own_crd.cap_type(), req.other_sel as CapSel, own_crd.count());
    let obtain = req.obtain == 1;

    sysc_log!(
        vpe, "exchange(vpe={}, own={}, other={}, obtain={})",
        vpe_sel, own_crd, other_crd, obtain
    );

    let vpe_ref: Rc<RefCell<VPE>> = get_kobj!(vpe, vpe_sel, VPE);

    do_exchange(vpe, &vpe_ref, &own_crd, &other_crd, obtain)?;

    reply_success(msg);
    Ok(())
}

fn exchange_over_sess(vpe: &Rc<RefCell<VPE>>, msg: &'static dtu::Message, obtain: bool) -> Result<(), Error> {
    let req: &kif::syscalls::ExchangeSess = get_message(msg);
    let sess_sel = req.sess_sel as CapSel;
    let crd = CapRngDesc::new_from(req.crd);

    sysc_log!(
        vpe, "{}(sess={}, crd={})",
        if obtain { "obtain" } else { "delegate" }, sess_sel, crd
    );

    let sess: Rc<RefCell<SessObject>> = get_kobj!(vpe, sess_sel, Sess);

    let mut smsg = kif::service::Exchange {
        opcode: if obtain {
            kif::service::Operation::OBTAIN.val as u64
        }
        else {
            kif::service::Operation::DELEGATE.val as u64
        },
        sess: sess.borrow().ident,
        data: kif::service::ExchangeData {
            caps: crd.count() as u64,
            argcount: req.argcount,
            args: unsafe { intrinsics::uninit() },
        },
    };
    for i in 0..req.argcount as usize {
        smsg.data.args[i] = req.args[i];
    }

    let serv: &Rc<RefCell<ServObject>> = &sess.borrow().srv;
    let res = ServObject::send_receive(serv, util::object_to_bytes(&smsg));

    match res {
        None        => sysc_err!(vpe, Code::Exists, "Service {} unreachable", serv.borrow().name),

        Some(rmsg)  => {
            let reply: &kif::service::ExchangeReply = get_message(rmsg);

            sysc_log!(
                vpe, "{} continue with res={}",
                if obtain { "obtain" } else { "delegate" }, {reply.res}
            );

            if reply.res != 0 {
                sysc_err!(vpe, Code::from(reply.res as u32), "Server denied cap exchange",);
            }
            else {
                let err = do_exchange(
                    vpe, &serv.borrow().vpe(),
                    &crd, &CapRngDesc::new_from(reply.data.caps), obtain
                );
                // TODO improve that
                if let Err(e) = err {
                    sysc_err!(vpe, e.code(), "Cap exchange failed",);
                }
            }

            let mut kreply = kif::syscalls::ExchangeSessReply {
                error: 0,
                argcount: reply.data.argcount,
                args: unsafe { intrinsics::uninit() },
            };
            for i in 0..reply.data.argcount as usize {
                kreply.args[i] = reply.data.args[i];
            }
            send_reply(msg, &kreply);
        }
    }

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

    let vpe_ref: Rc<RefCell<VPE>> = get_kobj!(vpe, vpe_sel, VPE);

    {
        let mut vpe_mut = vpe_ref.borrow_mut();
        if let Some(old_sel) = vpe_mut.get_ep_sel(ep) {
            if let Some(old_cap) = vpe_mut.obj_caps_mut().get_mut(old_sel) {
                if let Some(rgate) = old_cap.get_mut().as_rgate_mut() {
                    rgate.addr = 0;
                }
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
                let rgate: Rc<RefCell<RGateObject>> = s.borrow().rgate.clone();

                if !rgate.borrow().activated() {
                    sysc_log!(vpe, "activate: waiting for rgate {:?}", rgate.borrow());

                    let event = rgate.borrow().get_event();
                    thread::ThreadManager::get().wait_for(event);

                    sysc_log!(vpe, "activate: rgate {:?} is activated", rgate.borrow());
                }

                let pe_id = vpemng::get().pe_of(rgate.borrow().vpe);
                if let Err(e) = vpe_ref.borrow_mut().config_snd_ep(ep, &s.borrow(), pe_id.unwrap()) {
                    sysc_err!(vpe, e.code(), "Unable to configure send EP",);
                }
            },

            _                        => sysc_err!(vpe, Code::InvArgs, "Invalid capability",),
        };
        vpe_ref.borrow_mut().set_ep_sel(ep, Some(gate_sel));
    }
    else {
        vpe_ref.borrow_mut().invalidate_ep(ep, false)?;
        vpe_ref.borrow_mut().set_ep_sel(ep, None);
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

    let vpe_ref: Rc<RefCell<VPE>> = get_kobj!(vpe, vpe_sel, VPE);

    let exitcode = match op {
        kif::syscalls::VPEOp::INIT  => {
            vpe_ref.borrow_mut().set_eps_addr(arg as usize);
            0
        },

        kif::syscalls::VPEOp::START => {
            vpe_ref.borrow_mut().start(arg as i32)?;
            0
        },

        kif::syscalls::VPEOp::WAIT  => {
            let exit_code = VPE::wait(vpe_ref);
            if exit_code.is_none() {
                sysc_err!(vpe, Code::InvArgs, "VPE was not running",);
            }
            exit_code.unwrap()
        },

        kif::syscalls::VPEOp::STOP  => {
            VPE::stop(vpe_ref, arg as i32);
            if vpe_sel == 0 {
                dtu::DTU::mark_read(kdtu::KSYS_EP, msg);
                return Ok(());
            }
            0
        },

        _                           => panic!("VPEOp unsupported: {:?}", op),
    };

    let reply = kif::syscalls::VPECtrlReply {
        error: 0,
        exitcode: exitcode as u64,
    };
    send_reply(msg, &reply);
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

    let mut kobj = match vpe.borrow().obj_caps().get(vpe_sel) {
        Some(c)  => c.get().clone(),
        None     => sysc_err!(vpe, Code::InvArgs, "Invalid capability",),
    };
    match kobj.as_vpe_mut() {
        Some(ref mut v) => v.obj_caps_mut().revoke(crd, own),
        None            => sysc_err!(vpe, Code::InvArgs, "Invalid capability",),
    }

    reply_success(msg);
    Ok(())
}

fn noop(vpe: &Rc<RefCell<VPE>>, msg: &'static dtu::Message) -> Result<(), Error> {
    sysc_log!(
        vpe, "noop()",
    );

    reply_success(msg);
    Ok(())
}
