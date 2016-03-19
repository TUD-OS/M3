/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <base/tracing/Tracing.h>
#include <base/Log.h>

#include "pes/PEManager.h"
#include "com/Services.h"
#include "com/RecvBufs.h"
#include "SyscallHandler.h"

// #define SIMPLE_SYSC_LOG

#if defined(__host__)
extern int int_target;
#endif

namespace kernel {

SyscallHandler SyscallHandler::_inst INIT_PRIORITY(121);

#if defined(SIMPLE_SYSC_LOG)
#   define LOG_SYS(vpe, sysname, expr) \
        LOG(KSYSC, (vpe)->name() << (sysname))
#else
#   define LOG_SYS(vpe, sysname, expr) \
        LOG(KSYSC, (vpe)->name() << "@" << m3::fmt((vpe)->core(), "X") << (sysname) << expr)
#endif

#define SYS_ERROR(vpe, gate, error, msg) { \
        LOG(KERR, (vpe)->name() << ": " << msg << " (" << error << ")"); \
        reply_vmsg((gate), (error)); \
        return; \
    }

struct ReplyInfo {
    explicit ReplyInfo(const m3::DTU::Message &msg)
        : replylbl(msg.replylabel), replyep(msg.reply_epid()), crdep(msg.send_epid()),
          replycrd(msg.length) {
    }

    label_t replylbl;
    int replyep;
    int crdep;
    word_t replycrd;
};

static void reply_to_vpe(VPE &vpe, const ReplyInfo &info, const void *msg, size_t size) {
    DTU::get().reply_to(vpe, info.replyep, info.crdep, info.replycrd, info.replylbl, msg, size);
}

static m3::Errors::Code do_activate(VPE *vpe, size_t epid, MsgCapability *oldcapobj, MsgCapability *newcapobj) {
    if(newcapobj) {
        LOG_SYS(vpe, ": syscall::activate", ": setting ep[" << epid << "] to lbl="
                << m3::fmt(newcapobj->obj->label, "#0x", sizeof(label_t) * 2) << ", core=" << newcapobj->obj->core
                << ", ep=" << newcapobj->obj->epid
                << ", crd=#" << m3::fmt(newcapobj->obj->credits, "x"));
    }
    else
        LOG_SYS(vpe, ": syscall::activate", ": setting ep[" << epid << "] to NUL");

    m3::Errors::Code res = vpe->xchg_ep(epid, oldcapobj, newcapobj);
    if(res != m3::Errors::NO_ERROR)
        return res;

    if(oldcapobj)
        oldcapobj->localepid = -1;
    if(newcapobj)
        newcapobj->localepid = epid;
    return m3::Errors::NO_ERROR;
}

SyscallHandler::SyscallHandler() : _serv_ep(DTU::get().alloc_ep()) {
#if !defined(__t2__)
    // configure both receive buffers (we need to do that manually in the kernel)
    int buford = m3::nextlog2<AVAIL_PES>::val + VPE::SYSC_CREDIT_ORD;
    size_t bufsize = static_cast<size_t>(1) << buford;
    DTU::get().config_recv_local(epid(),reinterpret_cast<uintptr_t>(new uint8_t[bufsize]),
        buford, VPE::SYSC_CREDIT_ORD, 0);

    buford = m3::nextlog2<1024>::val;
    bufsize = static_cast<size_t>(1) << buford;
    DTU::get().config_recv_local(srvepid(), reinterpret_cast<uintptr_t>(new uint8_t[bufsize]),
        buford, m3::nextlog2<256>::val, 0);
#endif

    // add a dummy item to workloop; we handle everything manually anyway
    // but one item is needed to not stop immediately
    m3::env()->workloop()->add(nullptr, false);

    add_operation(m3::Syscalls::PAGEFAULT, &SyscallHandler::pagefault);
    add_operation(m3::Syscalls::CREATESRV, &SyscallHandler::createsrv);
    add_operation(m3::Syscalls::CREATESESS, &SyscallHandler::createsess);
    add_operation(m3::Syscalls::CREATEGATE, &SyscallHandler::creategate);
    add_operation(m3::Syscalls::CREATEVPE, &SyscallHandler::createvpe);
    add_operation(m3::Syscalls::CREATEMAP, &SyscallHandler::createmap);
    add_operation(m3::Syscalls::ATTACHRB, &SyscallHandler::attachrb);
    add_operation(m3::Syscalls::DETACHRB, &SyscallHandler::detachrb);
    add_operation(m3::Syscalls::EXCHANGE, &SyscallHandler::exchange);
    add_operation(m3::Syscalls::VPECTRL, &SyscallHandler::vpectrl);
    add_operation(m3::Syscalls::DELEGATE, &SyscallHandler::delegate);
    add_operation(m3::Syscalls::OBTAIN, &SyscallHandler::obtain);
    add_operation(m3::Syscalls::ACTIVATE, &SyscallHandler::activate);
    add_operation(m3::Syscalls::REQMEM, &SyscallHandler::reqmem);
    add_operation(m3::Syscalls::DERIVEMEM, &SyscallHandler::derivemem);
    add_operation(m3::Syscalls::REVOKE, &SyscallHandler::revoke);
    add_operation(m3::Syscalls::EXIT, &SyscallHandler::exit);
    add_operation(m3::Syscalls::NOOP, &SyscallHandler::noop);
#if defined(__host__)
    add_operation(m3::Syscalls::COUNT, &SyscallHandler::init);
#endif
}

void SyscallHandler::createsrv(RecvGate &gate, GateIStream &is) {
    EVENT_TRACER_Syscall_createsrv();
    VPE *vpe = gate.session<VPE>();
    m3::String name;
    capsel_t gatesel, srv;
    is >> gatesel >> srv >> name;
    LOG_SYS(vpe, ": syscall::createsrv", "(gate=" << gatesel
        << ", srv=" << srv << ", name=" << name << ")");

    MsgCapability *gatecap = static_cast<MsgCapability*>(vpe->objcaps().get(gatesel, Capability::MSG));
    if(gatecap == nullptr || name.length() == 0)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Invalid cap or name");
    if(ServiceList::get().find(name) != nullptr)
        SYS_ERROR(vpe, gate, m3::Errors::EXISTS, "Service does already exist");

    int capacity = 1;   // TODO this depends on the credits that the kernel has
    Service *s = ServiceList::get().add(*vpe, srv, name,
        gatecap->obj->epid, gatecap->obj->label, capacity);
    vpe->objcaps().set(srv, new ServiceCapability(&vpe->objcaps(), srv, s));

#if defined(__host__)
    // TODO ugly hack
    if(name == "interrupts")
        int_target = vpe->pid();
#endif

    // maybe there are VPEs that now have all requirements fullfilled
    PEManager::get().start_pending(ServiceList::get());

    reply_vmsg(gate, m3::Errors::NO_ERROR);
}

void SyscallHandler::pagefault(RecvGate &gate, UNUSED GateIStream &is) {
#if defined(__gem5__)
    VPE *vpe = gate.session<VPE>();
    uint64_t virt, access;
    is >> virt >> access;
    LOG_SYS(vpe, ": syscall::pagefault", "(virt=" << m3::fmt(virt, "p")
        << ", access " << m3::fmt(access, "#x") << ")");

    // TODO this might also indicates that the pf handler is not available (ctx switch, migrate, ...)

    if(!vpe->address_space())
        SYS_ERROR(vpe, gate, m3::Errors::NOT_SUP, "No address space / PF handler");

    capsel_t gcap = vpe->address_space()->gate();
    MsgCapability *msg = static_cast<MsgCapability*>(vpe->objcaps().get(gcap, Capability::MSG));
    if(msg == nullptr || msg->localepid != -1)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Invalid cap(s)");

    m3::Errors::Code res = do_activate(vpe, vpe->address_space()->ep(), nullptr, msg);
    if(res != m3::Errors::NO_ERROR)
        SYS_ERROR(vpe, gate, res, "Activate failed");
#endif

    reply_vmsg(gate, m3::Errors::NO_ERROR);
}

void SyscallHandler::createsess(RecvGate &gate, GateIStream &is) {
    EVENT_TRACER_Syscall_createsess();
    VPE *vpe = gate.session<VPE>();
    capsel_t tvpe, cap;
    m3::String name;
    is >> tvpe >> cap >> name;
    LOG_SYS(vpe, ": syscall::createsess",
        "(vpe=" << tvpe << ", name=" << name << ", cap=" << cap << ")");

    VPECapability *tvpeobj = static_cast<VPECapability*>(vpe->objcaps().get(tvpe, Capability::VIRTPE));
    if(tvpeobj == nullptr)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "VPE capability is invalid");

    if(!tvpeobj->vpe->objcaps().unused(cap))
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Invalid cap");

    Service *s = ServiceList::get().find(name);
    if(!s || s->closing)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Unknown service");

    ReplyInfo rinfo(is.message());
    m3::Reference<Service> rsrv(s);
    vpe->service_gate().subscribe([this, rsrv, cap, vpe, tvpeobj, rinfo]
            (RecvGate &sgate, m3::Subscriber<RecvGate&> *sub) {
        EVENT_TRACER_Syscall_createsess();
        GateIStream reply(sgate);
        m3::Errors::Code res;
        reply >> res;
        if(res != m3::Errors::NO_ERROR) {
            LOG(KSYSC, vpe->id() << ": Server denied session creation (" << res << ")");
            auto reply = kernel::create_vmsg(res);
            reply_to_vpe(*vpe, rinfo, reply.bytes(), reply.total());
        }
        else {
            word_t sess;
            reply >> sess;

            // inherit the session-cap from the service-cap. this way, it will be automatically
            // revoked if the service-cap is revoked
            Capability *srvcap = rsrv->vpe().objcaps().get(rsrv->selector(), Capability::SERVICE);
            assert(srvcap != nullptr);
            SessionCapability *sesscap = new SessionCapability(
                &tvpeobj->vpe->objcaps(), cap, const_cast<Service*>(&*rsrv), sess);
            tvpeobj->vpe->objcaps().inherit(srvcap, sesscap);
            tvpeobj->vpe->objcaps().set(cap, sesscap);

            auto reply = kernel::create_vmsg(res);
            reply_to_vpe(*vpe, rinfo, reply.bytes(), reply.total());
        }

        const_cast<m3::Reference<Service>&>(rsrv)->received_reply();
        vpe->service_gate().unsubscribe(sub);
    });

    AutoGateOStream msg(m3::vostreamsize(m3::ostreamsize<server_type::Command>(), is.remaining()));
    msg << server_type::OPEN;
    msg.put(is);
    s->send(&vpe->service_gate(), msg.bytes(), msg.total());
    msg.claim();
}

void SyscallHandler::creategate(RecvGate &gate, GateIStream &is) {
    EVENT_TRACER_Syscall_creategate();
    VPE *vpe = gate.session<VPE>();
    capsel_t tcap,dstcap;
    label_t label;
    size_t epid;
    word_t credits;
    is >> tcap >> dstcap >> label >> epid >> credits;
    LOG_SYS(vpe, ": syscall::creategate", "(vpe=" << tcap << ", cap=" << dstcap
        << ", label=" << m3::fmt(label, "#0x", sizeof(label_t) * 2)
        << ", ep=" << epid << ", crd=#" << m3::fmt(credits, "0x") << ")");

#if defined(__gem5__)
    if(credits == m3::SendGate::UNLIMITED)
        PANIC("Unlimited credits are not yet supported on gem5");
#endif

    VPECapability *tcapobj = static_cast<VPECapability*>(vpe->objcaps().get(tcap, Capability::VIRTPE));
    if(tcapobj == nullptr)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "VPE capability is invalid");

    // 0 points to the SEPs and can't be delegated to someone else
    if(epid == 0 || epid >= EP_COUNT || !vpe->objcaps().unused(dstcap))
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Invalid cap or ep");

    vpe->objcaps().set(dstcap,
        new MsgCapability(&vpe->objcaps(), dstcap, label, tcapobj->vpe->core(),
            tcapobj->vpe->id(), epid, credits));
    reply_vmsg(gate, m3::Errors::NO_ERROR);
}

void SyscallHandler::createvpe(RecvGate &gate, GateIStream &is) {
    EVENT_TRACER_Syscall_createvpe();
    VPE *vpe = gate.session<VPE>();
    capsel_t tcap, mcap, gcap;
    m3::String name, core;
    size_t ep;
    is >> tcap >> mcap >> name >> core >> gcap >> ep;
    LOG_SYS(vpe, ": syscall::createvpe", "(name=" << name << ", core=" << core
        << ", tcap=" << tcap << ", mcap=" << mcap << ", pfgate=" << gcap
        << ", pfep=" << ep << ")");

    if(!vpe->objcaps().unused(tcap) || !vpe->objcaps().unused(mcap))
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Invalid VPE or memory cap");

    // if it has a pager, we need a gate cap
    MsgCapability *msg = nullptr;
    if(gcap != m3::ObjCap::INVALID) {
        msg = static_cast<MsgCapability*>(vpe->objcaps().get(gcap, Capability::MSG));
        if(msg == nullptr)
            SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Invalid cap(s)");
    }

    // create VPE
    const char *corename = core.c_str()[0] == '\0'
        ? PEManager::get().type(vpe->core() - APP_CORES)
        : core.c_str();
    VPE *nvpe = PEManager::get().create(std::move(name), corename,
        gcap != m3::ObjCap::INVALID, ep, gcap);
    if(nvpe == nullptr)
        SYS_ERROR(vpe, gate, m3::Errors::NO_FREE_CORE, "No free and suitable core found");

    // inherit VPE and mem caps to the parent
    vpe->objcaps().obtain(tcap, nvpe->objcaps().get(0));
    vpe->objcaps().obtain(mcap, nvpe->objcaps().get(1));

    // initialize paging
    if(gcap != m3::ObjCap::INVALID) {
        // delegate pf gate to the new VPE
        nvpe->objcaps().obtain(gcap, msg);

        DTU::get().config_pf_remote(*nvpe, ep);
    }

    reply_vmsg(gate, m3::Errors::NO_ERROR);
}

void SyscallHandler::createmap(RecvGate &gate, UNUSED GateIStream &is) {
#if defined(__gem5__)
    EVENT_TRACER_Syscall_createmap();
    VPE *vpe = gate.session<VPE>();
    capsel_t tcap, mcap;
    capsel_t first, pages, dst;
    int perms;
    is >> tcap >> mcap >> first >> pages >> dst >> perms;
    LOG_SYS(vpe, ": syscall::createmap", "(vpe=" << tcap << ", mem=" << mcap
        << ", first=" << first << ", pages=" << pages << ", dst=" << dst
        << ", perms=" << perms << ")");

    VPECapability *tcapobj = static_cast<VPECapability*>(vpe->objcaps().get(tcap, Capability::VIRTPE));
    if(tcapobj == nullptr)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "VPE capability is invalid");
    MemCapability *mcapobj = static_cast<MemCapability*>(vpe->objcaps().get(mcap, Capability::MEM));
    if(mcapobj == nullptr)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Memory capability is invalid");

    if(!tcapobj->vpe->mapcaps().range_unused(m3::CapRngDesc(m3::CapRngDesc::MAP, dst, pages)))
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Map capability range is not unused");
    if((mcapobj->addr() & PAGE_MASK) || (mcapobj->size() & PAGE_MASK))
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Memory capability is not page aligned");
    if(perms & ~mcapobj->perms())
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Invalid permissions");

    size_t total = mcapobj->size() >> PAGE_BITS;
    if(first >= total || first + pages <= first || first + pages > total)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Region of memory capability is invalid");

    uintptr_t phys = m3::DTU::build_noc_addr(mcapobj->obj->core, mcapobj->addr() + PAGE_SIZE * first);
    for(capsel_t i = 0; i < pages; ++i) {
        MapCapability *mapcap = new MapCapability(&tcapobj->vpe->mapcaps(), dst + i, phys, perms);
        tcapobj->vpe->mapcaps().inherit(mcapobj, mapcap);
        tcapobj->vpe->mapcaps().set(dst + i, mapcap);
        phys += PAGE_SIZE;
    }
#endif

    reply_vmsg(gate, m3::Errors::NO_ERROR);
}

void SyscallHandler::attachrb(RecvGate &gate, GateIStream &is) {
    EVENT_TRACER_Syscall_attachrb();
    VPE *vpe = gate.session<VPE>();
    capsel_t tcap;
    uintptr_t addr;
    size_t ep;
    int order, msgorder;
    uint flags;
    is >> tcap >> ep >> addr >> order >> msgorder >> flags;
    LOG_SYS(vpe, ": syscall::attachrb", "(vpe=" << tcap << ", ep=" << ep
        << ", addr=" << m3::fmt(addr, "p") << ", size=" << m3::fmt(1UL << order, "#x")
        << ", msgsize=" << m3::fmt(1UL << msgorder, "#x") << ", flags=" << m3::fmt(flags, "#x") << ")");

    VPECapability *tcapobj = static_cast<VPECapability*>(vpe->objcaps().get(tcap, Capability::VIRTPE));
    if(tcapobj == nullptr)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "VPE capability is invalid");

    m3::Errors::Code res = RecvBufs::attach(*tcapobj->vpe, ep, addr, order, msgorder, flags);
    if(res != m3::Errors::NO_ERROR)
        SYS_ERROR(vpe, gate, res, "Unable to attach receive buffer");

    reply_vmsg(gate, m3::Errors::NO_ERROR);
}

void SyscallHandler::detachrb(RecvGate &gate, GateIStream &is) {
    EVENT_TRACER_Syscall_detachrb();
    VPE *vpe = gate.session<VPE>();
    capsel_t tcap;
    size_t ep;
    is >> tcap >> ep;
    LOG_SYS(vpe, ": syscall::detachrb", "(vpe=" << tcap << ", ep=" << ep << ")");

    VPECapability *tcapobj = static_cast<VPECapability*>(vpe->objcaps().get(tcap, Capability::VIRTPE));
    if(tcapobj == nullptr)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "VPE capability is invalid");

    RecvBufs::detach(*tcapobj->vpe, ep);
    reply_vmsg(gate, m3::Errors::NO_ERROR);
}

void SyscallHandler::exchange(RecvGate &gate, GateIStream &is) {
    EVENT_TRACER_Syscall_exchange();
    VPE *vpe = gate.session<VPE>();
    capsel_t tcap;
    m3::CapRngDesc own, other;
    bool obtain;
    is >> tcap >> own >> other >> obtain;
    LOG_SYS(vpe, ": syscall::exchange", "(vpe=" << tcap << ", own=" << own
        << ", other=" << other << ", obtain=" << obtain << ")");

    VPECapability *vpecap = static_cast<VPECapability*>(
            vpe->objcaps().get(tcap, Capability::VIRTPE));
    if(vpecap == nullptr)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Invalid VPE cap");

    VPE *t1 = obtain ? vpecap->vpe : vpe;
    VPE *t2 = obtain ? vpe : vpecap->vpe;
    m3::Errors::Code res = do_exchange(t1, t2, own, other, obtain);
    reply_vmsg(gate, res);
}

void SyscallHandler::vpectrl(RecvGate &gate, GateIStream &is) {
    EVENT_TRACER_Syscall_vpectrl();
    VPE *vpe = gate.session<VPE>();
    capsel_t tcap;
    m3::Syscalls::VPECtrl op;
    int pid;
    is >> tcap >> op >> pid;
    LOG_SYS(vpe, ": syscall::vpectrl", "(vpe=" << tcap << ", op=" << op
            << ", pid=" << pid << ")");

    VPECapability *vpecap = static_cast<VPECapability*>(
            vpe->objcaps().get(tcap, Capability::VIRTPE));
    if(vpecap == nullptr)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Invalid cap");

    switch(op) {
        case m3::Syscalls::VCTRL_START:
            vpecap->vpe->start(0, nullptr, pid);
            reply_vmsg(gate, m3::Errors::NO_ERROR);
            break;

        case m3::Syscalls::VCTRL_WAIT:
            if(vpecap->vpe->state() == VPE::DEAD)
                reply_vmsg(gate, m3::Errors::NO_ERROR, vpecap->vpe->exitcode());
            else {
                ReplyInfo rinfo(is.message());
                vpecap->vpe->subscribe_exit([vpe, is, rinfo] (int exitcode, m3::Subscriber<int> *) {
                    EVENT_TRACER_Syscall_vpectrl();
                    auto reply = kernel::create_vmsg(m3::Errors::NO_ERROR,exitcode);
                    reply_to_vpe(*vpe, rinfo, reply.bytes(), reply.total());
                });
            }
            break;
    }
}

void SyscallHandler::reqmem(RecvGate &gate, GateIStream &is) {
    EVENT_TRACER_Syscall_reqmem();
    VPE *vpe = gate.session<VPE>();
    capsel_t cap;
    uintptr_t addr;
    size_t size;
    int perms;
    is >> cap >> addr >> size >> perms;
    LOG_SYS(vpe, ": syscall::reqmem", "(cap=" << cap
        << ", addr=#" << m3::fmt(addr, "x") << ", size=#" << m3::fmt(size, "x")
        << ", perms=" << perms << ")");

    if(!vpe->objcaps().unused(cap))
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Invalid cap");
    if(size == 0 || (size & m3::MemGate::RWX) || perms == 0 || (perms & ~(m3::MemGate::RWX)))
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Size or permissions invalid");

    MainMemory &mem = MainMemory::get();
    if(addr != (uintptr_t)-1 && m3::Math::overlap(addr, size, mem.addr(), mem.size()))
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Addr+size overlap with allocatable memory");

    if(addr == (uintptr_t)-1) {
        addr = mem.map().allocate(size);
        if(addr == (uintptr_t)-1)
            SYS_ERROR(vpe, gate, m3::Errors::OUT_OF_MEM, "Not enough memory");
    }
    else
        addr += mem.base();

    // TODO if addr was 0, we don't want to free it on revoke
    vpe->objcaps().set(cap,
        new MemCapability(&vpe->objcaps(), cap, addr, size, perms, MEMORY_CORE, 0, mem.epid()));
    reply_vmsg(gate, m3::Errors::NO_ERROR);
}

void SyscallHandler::derivemem(RecvGate &gate, GateIStream &is) {
    EVENT_TRACER_Syscall_derivemem();
    VPE *vpe = gate.session<VPE>();
    capsel_t src, dst;
    size_t offset, size;
    int perms;
    is >> src >> dst >> offset >> size >> perms;
    LOG_SYS(vpe, ": syscall::derivemem", "(src=" << src << ", dst=" << dst
        << ", size=" << size << ", off=" << offset << ", perms=" << perms << ")");

    MemCapability *srccap = static_cast<MemCapability*>(
            vpe->objcaps().get(src, Capability::MEM));
    if(srccap == nullptr || !vpe->objcaps().unused(dst))
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Invalid cap(s)");

    if(offset + size < offset || offset + size > srccap->size() || size == 0 ||
            (perms & ~(m3::MemGate::RWX)))
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Invalid args");

    MemCapability *dercap = static_cast<MemCapability*>(vpe->objcaps().obtain(dst, srccap));
    dercap->obj = m3::Reference<MsgObject>(new MemObject(
        srccap->addr() + offset,
        size,
        perms & srccap->perms(),
        srccap->obj->core,
        srccap->obj->vpe,
        srccap->obj->epid
    ));
    dercap->obj->derived = true;
    reply_vmsg(gate, m3::Errors::NO_ERROR);
}

void SyscallHandler::delegate(RecvGate &gate, GateIStream &is) {
    EVENT_TRACER_Syscall_delegate();
    exchange_over_sess(gate, is, false);
}

void SyscallHandler::obtain(RecvGate &gate, GateIStream &is) {
    EVENT_TRACER_Syscall_obtain();
    exchange_over_sess(gate, is, true);
}

m3::Errors::Code SyscallHandler::do_exchange(VPE *v1, VPE *v2, const m3::CapRngDesc &c1,
        const m3::CapRngDesc &c2, bool obtain) {
    VPE &src = obtain ? *v2 : *v1;
    VPE &dst = obtain ? *v1 : *v2;
    const m3::CapRngDesc &srcrng = obtain ? c2 : c1;
    const m3::CapRngDesc &dstrng = obtain ? c1 : c2;

    if(c1.type() != c2.type()) {
        LOG(KSYSC, v1->id() << ": Descriptor types don't match (" << m3::Errors::INV_ARGS << ")");
        return m3::Errors::INV_ARGS;
    }
    if((obtain && c2.count() > c1.count()) || (!obtain && c2.count() != c1.count())) {
        LOG(KSYSC, v1->id() << ": Server gave me invalid CRD (" << m3::Errors::INV_ARGS << ")");
        return m3::Errors::INV_ARGS;
    }
    if(!dst.objcaps().range_unused(dstrng)) {
        LOG(KSYSC, v1->id() << ": Invalid destination caps (" << m3::Errors::INV_ARGS << ")");
        return m3::Errors::INV_ARGS;
    }

    CapTable &srctab = c1.type() == m3::CapRngDesc::OBJ ? src.objcaps() : src.mapcaps();
    CapTable &dsttab = c1.type() == m3::CapRngDesc::OBJ ? dst.objcaps() : dst.mapcaps();
    for(uint i = 0; i < c2.count(); ++i) {
        capsel_t srccap = srcrng.start() + i;
        capsel_t dstcap = dstrng.start() + i;
        Capability *scapobj = srctab.get(srccap);
        assert(dsttab.get(dstcap) == nullptr);
        dsttab.obtain(dstcap, scapobj);
    }
    return m3::Errors::NO_ERROR;
}

void SyscallHandler::exchange_over_sess(RecvGate &gate, GateIStream &is, bool obtain) {
    VPE *vpe = gate.session<VPE>();
    capsel_t tvpe, sesscap;
    m3::CapRngDesc caps;
    is >> tvpe >> sesscap >> caps;
    // TODO compiler-bug? if I try to print caps, it hangs on T2!? doing it here manually works
    LOG_SYS(vpe, (obtain ? "syscall::obtain" : "syscall::delegate"),
            "(vpe=" << tvpe << ", sess=" << sesscap << ", caps="
            << caps.start() << ":" << caps.count() << ")");

    VPECapability *tvpeobj = static_cast<VPECapability*>(vpe->objcaps().get(tvpe, Capability::VIRTPE));
    if(tvpeobj == nullptr)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "VPE capability is invalid");

    SessionCapability *sess = static_cast<SessionCapability*>(
        tvpeobj->vpe->objcaps().get(sesscap, Capability::SESSION));
    if(sess == nullptr)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Invalid session-cap");
    if(sess->obj->srv->closing)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Server is shutting down");

    ReplyInfo rinfo(is.message());
    // only pass in the service-reference. we can't be sure that the session will still exist
    // when we receive the reply
    m3::Reference<Service> rsrv(sess->obj->srv);
    vpe->service_gate().subscribe([this, rsrv, caps, vpe, tvpeobj, obtain, rinfo]
            (RecvGate &sgate, m3::Subscriber<RecvGate&> *sub) {
        EVENT_TRACER_Syscall_delob_done();
        m3::CapRngDesc srvcaps;

        GateIStream reply(sgate);
        m3::Errors::Code res;
        reply >> res;
        if(res != m3::Errors::NO_ERROR) {
            LOG(KSYSC, tvpeobj->vpe->id() << ": Server denied cap-transfer (" << res << ")");

            auto reply = kernel::create_vmsg(res);
            reply_to_vpe(*vpe, rinfo, reply.bytes(), reply.total());
            goto error;
        }

        reply >> srvcaps;
        if((res = do_exchange(tvpeobj->vpe, &rsrv->vpe(), caps, srvcaps, obtain)) != m3::Errors::NO_ERROR) {
            auto reply = kernel::create_vmsg(res);
            reply_to_vpe(*vpe, rinfo, reply.bytes(), reply.total());
            goto error;
        }

        {
            AutoGateOStream msg(m3::vostreamsize(m3::ostreamsize<m3::Errors, m3::CapRngDesc>(),
                reply.remaining()));
            msg << m3::Errors::NO_ERROR;
            msg.put(reply);
            reply_to_vpe(*vpe, rinfo, msg.bytes(), msg.total());
        }

    error:
        const_cast<m3::Reference<Service>&>(rsrv)->received_reply();
        vpe->service_gate().unsubscribe(sub);
    });

    AutoGateOStream msg(m3::vostreamsize(m3::ostreamsize<server_type::Command, word_t,
        m3::CapRngDesc>(), is.remaining()));
    msg << (obtain ? server_type::OBTAIN : server_type::DELEGATE) << sess->obj->ident << caps.count();
    msg.put(is);
    sess->obj->srv->send(&vpe->service_gate(), msg.bytes(), msg.total());
    msg.claim();
}

void SyscallHandler::activate(RecvGate &gate, GateIStream &is) {
    EVENT_TRACER_Syscall_activate();
    VPE *vpe = gate.session<VPE>();
    size_t epid;
    capsel_t oldcap, newcap;
    is >> epid >> oldcap >> newcap;
    LOG_SYS(vpe, ": syscall::activate", "(ep=" << epid << ", old=" <<
        oldcap << ", new=" << newcap << ")");

    MsgCapability *oldcapobj = oldcap == m3::ObjCap::INVALID ? nullptr : static_cast<MsgCapability*>(
            vpe->objcaps().get(oldcap, Capability::MSG | Capability::MEM));
    MsgCapability *newcapobj = newcap == m3::ObjCap::INVALID ? nullptr : static_cast<MsgCapability*>(
            vpe->objcaps().get(newcap, Capability::MSG | Capability::MEM));
    // ep 0 can never be used for sending
    if(epid == 0 || (oldcapobj == nullptr && newcapobj == nullptr)) {
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Invalid cap(s) (old=" << oldcap << "," << oldcapobj
            << ", new=" << newcap << "," << newcapobj << ")");
    }

    if(newcapobj && newcapobj->type == Capability::MSG) {
        if(!RecvBufs::is_attached(newcapobj->obj->core, newcapobj->obj->epid)) {
            ReplyInfo rinfo(is.message());
            LOG_SYS(vpe, ": syscall::activate", ": waiting for receive buffer "
                << newcapobj->obj->core << ":" << newcapobj->obj->epid << " to get attached");

            auto callback = [rinfo, vpe, epid, oldcapobj, newcapobj](bool success, m3::Subscriber<bool> *) {
                EVENT_TRACER_Syscall_activate();
                m3::Errors::Code res = success ? m3::Errors::NO_ERROR : m3::Errors::RECV_GONE;
                if(success)
                    res = do_activate(vpe, epid, oldcapobj, newcapobj);
                if(res != m3::Errors::NO_ERROR)
                    LOG(KERR, vpe->name() << ": activate failed (" << res << ")");

                auto reply = kernel::create_vmsg(res);
                reply_to_vpe(*vpe, rinfo, reply.bytes(), reply.total());
            };
            RecvBufs::subscribe(newcapobj->obj->core, newcapobj->obj->epid, callback);
            return;
        }
    }

    m3::Errors::Code res = do_activate(vpe, epid, oldcapobj, newcapobj);
    if(res != m3::Errors::NO_ERROR)
        SYS_ERROR(vpe, gate, res, "cmpxchg failed");
    reply_vmsg(gate, m3::Errors::NO_ERROR);
}

void SyscallHandler::revoke(RecvGate &gate, GateIStream &is) {
    EVENT_TRACER_Syscall_revoke();
    VPE *vpe = gate.session<VPE>();
    m3::CapRngDesc crd;
    is >> crd;
    LOG_SYS(vpe, ": syscall::revoke", "(" << crd << ")");

    if(crd.type() == m3::CapRngDesc::OBJ && crd.start() < 2)
        SYS_ERROR(vpe, gate, m3::Errors::INV_ARGS, "Cap 0 and 1 are not revokeable");

    CapTable &table = crd.type() == m3::CapRngDesc::OBJ ? vpe->objcaps() : vpe->mapcaps();
    m3::Errors::Code res = table.revoke(crd);
    if(res != m3::Errors::NO_ERROR)
        SYS_ERROR(vpe, gate, res, "Revoke failed");

    if(crd.type() == m3::CapRngDesc::OBJ) {
        // maybe we have removed a VPE
        tryTerminate();
    }

    reply_vmsg(gate, m3::Errors::NO_ERROR);
}

void SyscallHandler::exit(RecvGate &gate, GateIStream &is) {
    EVENT_TRACER_Syscall_exit();
    VPE *vpe = gate.session<VPE>();
    int exitcode;
    is >> exitcode;
    LOG_SYS(vpe, ": syscall::exit", "(" << exitcode << ")");

    vpe->exit(exitcode);
    vpe->unref();

    tryTerminate();
}

void SyscallHandler::tryTerminate() {
    // if there are no VPEs left, we can stop everything
    if(PEManager::get().used() == 0) {
        PEManager::destroy();
        m3::env()->workloop()->stop();
    }
    // if there are only daemons left, start the shutdown-procedure
    else if(PEManager::get().used() == PEManager::get().daemons())
        PEManager::shutdown();
}

void SyscallHandler::noop(RecvGate &gate, GateIStream &) {
    reply_vmsg(gate, 0);
}

#if defined(__host__)
void SyscallHandler::init(RecvGate &gate,GateIStream &is) {
    VPE *vpe = gate.session<VPE>();
    void *addr;
    is >> addr;
    vpe->activate_sysc_ep(addr);
    LOG_SYS(vpe, "syscall::init", "(" << addr << ")");

    reply_vmsg(gate, m3::Errors::NO_ERROR);
}
#endif

}
