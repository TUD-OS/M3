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
#include <base/log/Kernel.h>
#include <base/Init.h>
#include <base/Panic.h>

#include <thread/ThreadManager.h>

#include "com/RecvBufs.h"
#include "com/Services.h"
#include "pes/ContextSwitcher.h"
#include "pes/PEManager.h"
#include "pes/VPEManager.h"
#include "Platform.h"
#include "SyscallHandler.h"
#include "WorkLoop.h"

#if defined(__host__)
extern int int_target;
#endif

namespace kernel {

INIT_PRIO_USER(3) SyscallHandler SyscallHandler::_inst;

#define LOG_SYS(vpe, sysname, expr)                                                         \
        KLOG(SYSC, (vpe)->id() << ":" << (vpe)->name() << "@" << m3::fmt((vpe)->pe(), "X")  \
            << (sysname) << expr)

#define LOG_ERROR(vpe, error, msg)                                                          \
    do {                                                                                    \
        KLOG(ERR, "\e[37;41m"                                                               \
            << (vpe)->id() << ":" << (vpe)->name() << "@" << m3::fmt((vpe)->pe(), "X")      \
            << ": " << msg << " (" << error << ")\e[0m");                                   \
    }                                                                                       \
    while(0)

#define SYS_ERROR(vpe, is, error, msg) {                                                    \
        LOG_ERROR(vpe, error, msg);                                                         \
        kreply_result((vpe), (is), (error));                                                \
        return;                                                                             \
    }

static inline void kreply_msg(VPE *vpe, GateIStream &is, const void *msg, size_t size) {
    while(vpe->state() != VPE::RUNNING) {
        if(!vpe->resume())
            return;
    }

    is.reply(msg, size);
}

static inline void kreply_result(VPE *vpe, GateIStream &is, m3::Errors::Code code) {
    m3::KIF::DefaultReply reply;
    reply.error = code;
    return kreply_msg(vpe, is, &reply, sizeof(reply));
}

template<typename... Args>
static inline void kreply_vmsg(VPE *vpe, GateIStream &is, const Args &... args) {
    auto msg = kernel::create_vmsg(args...);
    kreply_msg(vpe, is, msg.bytes(), msg.total());
}

template<typename T>
static const T *get_message(GateIStream &is) {
    return reinterpret_cast<const T*>(is.message().data);
}

SyscallHandler::SyscallHandler() : _serv_ep(DTU::get().alloc_ep()) {
#if !defined(__t2__)
    // configure both receive buffers (we need to do that manually in the kernel)
    int buford = m3::getnextlog2(Platform::pe_count()) + VPE::SYSC_MSGSIZE_ORD;
    size_t bufsize = static_cast<size_t>(1) << buford;
    DTU::get().recv_msgs(ep(),reinterpret_cast<uintptr_t>(new uint8_t[bufsize]),
        buford, VPE::SYSC_MSGSIZE_ORD);

    buford = m3::getnextlog2(Platform::pe_count()) + VPE::NOTIFY_MSGSIZE_ORD;
    bufsize = static_cast<size_t>(1) << buford;
    DTU::get().recv_msgs(m3::DTU::NOTIFY_SEP, reinterpret_cast<uintptr_t>(new uint8_t[bufsize]),
        buford, VPE::NOTIFY_MSGSIZE_ORD);

    buford = m3::nextlog2<1024>::val;
    bufsize = static_cast<size_t>(1) << buford;
    DTU::get().recv_msgs(srvep(), reinterpret_cast<uintptr_t>(new uint8_t[bufsize]),
        buford, m3::nextlog2<256>::val);
#endif

    // add a dummy item to workloop; we handle everything manually anyway
    // but one item is needed to not stop immediately
    m3::env()->workloop()->add(nullptr, false);

    add_operation(m3::KIF::Syscall::PAGEFAULT, &SyscallHandler::pagefault);
    add_operation(m3::KIF::Syscall::CREATESRV, &SyscallHandler::createsrv);
    add_operation(m3::KIF::Syscall::CREATESESS, &SyscallHandler::createsess);
    add_operation(m3::KIF::Syscall::CREATESESSAT, &SyscallHandler::createsessat);
    add_operation(m3::KIF::Syscall::CREATERBUF, &SyscallHandler::createrbuf);
    add_operation(m3::KIF::Syscall::CREATEGATE, &SyscallHandler::creategate);
    add_operation(m3::KIF::Syscall::CREATEVPE, &SyscallHandler::createvpe);
    add_operation(m3::KIF::Syscall::CREATEMAP, &SyscallHandler::createmap);
    add_operation(m3::KIF::Syscall::EXCHANGE, &SyscallHandler::exchange);
    add_operation(m3::KIF::Syscall::VPECTRL, &SyscallHandler::vpectrl);
    add_operation(m3::KIF::Syscall::DELEGATE, &SyscallHandler::delegate);
    add_operation(m3::KIF::Syscall::OBTAIN, &SyscallHandler::obtain);
    add_operation(m3::KIF::Syscall::ACTIVATE, &SyscallHandler::activate);
    add_operation(m3::KIF::Syscall::FORWARDMSG, &SyscallHandler::forwardmsg);
    add_operation(m3::KIF::Syscall::FORWARDMEM, &SyscallHandler::forwardmem);
    add_operation(m3::KIF::Syscall::FORWARDREPLY, &SyscallHandler::forwardreply);
    add_operation(m3::KIF::Syscall::REQMEM, &SyscallHandler::reqmem);
    add_operation(m3::KIF::Syscall::DERIVEMEM, &SyscallHandler::derivemem);
    add_operation(m3::KIF::Syscall::REVOKE, &SyscallHandler::revoke);
    add_operation(m3::KIF::Syscall::IDLE, &SyscallHandler::idle);
    add_operation(m3::KIF::Syscall::EXIT, &SyscallHandler::exit);
    add_operation(m3::KIF::Syscall::NOOP, &SyscallHandler::noop);
#if defined(__host__)
    add_operation(m3::KIF::Syscall::INIT, &SyscallHandler::init);
#else
    add_operation(m3::KIF::Syscall::INIT, &SyscallHandler::noop);
#endif
}

void SyscallHandler::handle_message(GateIStream &is) {
    auto req = get_message<m3::KIF::DefaultRequest>(is);
    m3::KIF::Syscall::Operation op = static_cast<m3::KIF::Syscall::Operation>(req->opcode);
    is.ignore(sizeof(word_t));

    if(static_cast<size_t>(op) < sizeof(_callbacks) / sizeof(_callbacks[0])) {
        (this->*_callbacks[op])(is);
        return;
    }

    VPE *vpe = is.gate().session<VPE>();
    kreply_result(vpe, is, m3::Errors::INV_ARGS);
}

void SyscallHandler::createsrv(GateIStream &is) {
    EVENT_TRACER_Syscall_createsrv();
    VPE *vpe = is.gate().session<VPE>();

    auto req = get_message<m3::KIF::Syscall::CreateSrv>(is);
    capsel_t srv = req->srv;
    label_t label = req->label;
    m3::String name(req->name, m3::Math::min(req->namelen, sizeof(req->name)));

    LOG_SYS(vpe, ": syscall::createsrv", "(srv=" << srv << ", label="
        << m3::fmt(label, "#0x", sizeof(label_t) * 2) << ", name=" << name << ")");

    if(ServiceList::get().find(name) != nullptr)
        SYS_ERROR(vpe, is, m3::Errors::EXISTS, "Service does already exist");

    Service *s = ServiceList::get().add(*vpe, srv, name, label);
    vpe->objcaps().set(srv, new ServiceCapability(&vpe->objcaps(), srv, s));

#if defined(__host__)
    // TODO ugly hack
    if(name == "interrupts")
        int_target = vpe->pid();
#endif

    // maybe there are VPEs that now have all requirements fullfilled
    VPEManager::get().start_pending(ServiceList::get());

    kreply_result(vpe, is, m3::Errors::NO_ERROR);
}

void SyscallHandler::pagefault(UNUSED GateIStream &is) {
    EVENT_TRACER_Syscall_pagefault();
    VPE *vpe = is.gate().session<VPE>();

#if defined(__gem5__)
    auto req = get_message<m3::KIF::Syscall::Pagefault>(is);
    uint64_t virt = req->virt;
    uint access = req->access;

    LOG_SYS(vpe, ": syscall::pagefault", "(virt=" << m3::fmt(virt, "p")
        << ", access " << m3::fmt(access, "#x") << ")");

    if(!vpe->address_space())
        SYS_ERROR(vpe, is, m3::Errors::NOT_SUP, "No address space / PF handler");

    // if we don't have a pager, it was probably because of speculative execution. just return an
    // error in this case and don't print anything
    capsel_t gcap = vpe->address_space()->gate();
    MsgCapability *msg = static_cast<MsgCapability*>(vpe->objcaps().get(gcap, Capability::MSG));
    if(msg == nullptr) {
        kreply_result(vpe, is, m3::Errors::INV_ARGS);
        return;
    }

    // TODO this might also indicates that the pf handler is not available (ctx switch, migrate, ...)
    vpe->config_snd_ep(vpe->address_space()->ep(), *msg->obj);
#endif

    kreply_result(vpe, is, m3::Errors::NO_ERROR);
}

void SyscallHandler::createsess(GateIStream &is) {
    EVENT_TRACER_Syscall_createsess();
    VPE *vpe = is.gate().session<VPE>();

    auto req = get_message<m3::KIF::Syscall::CreateSess>(is);
    capsel_t cap = req->sess;
    word_t arg = req->arg;
    m3::String name(req->name, m3::Math::min(req->namelen, sizeof(req->name)));

    LOG_SYS(vpe, ": syscall::createsess",
        "(name=" << name << ", cap=" << cap << ", arg=" << arg << ")");

    if(!vpe->objcaps().unused(cap))
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid cap");

    Service *s = ServiceList::get().find(name);
    if(!s || s->closing)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Unknown service");

    m3::Reference<Service> rsrv(s);

    vpe->start_wait();
    while(s->vpe().state() != VPE::RUNNING) {
        if(!s->vpe().resume()) {
            vpe->stop_wait();
            SYS_ERROR(vpe, is, m3::Errors::VPE_GONE, "VPE does no longer exist");
        }
    }

    m3::KIF::Service::Open msg;
    msg.opcode = m3::KIF::Service::OPEN;
    msg.arg = arg;

    const m3::DTU::Message *srvreply = s->send_receive(&msg, sizeof(msg), false);

    vpe->stop_wait();
    EVENT_TRACER_Syscall_createsess();
    auto reply = reinterpret_cast<const m3::KIF::Service::OpenReply*>(srvreply->data);

    m3::Errors::Code res = static_cast<m3::Errors::Code>(reply->error);

    LOG_SYS(vpe, ": syscall::createsess-cb", "(res=" << res << ")");

    if(res != m3::Errors::NO_ERROR)
        LOG_ERROR(vpe, res, "Server denied session creation");
    else {
        // inherit the session-cap from the service-cap. this way, it will be automatically
        // revoked if the service-cap is revoked
        Capability *srvcap = rsrv->vpe().objcaps().get(rsrv->selector(), Capability::SERVICE);
        assert(srvcap != nullptr);
        SessionCapability *sesscap = new SessionCapability(
            &vpe->objcaps(), cap, const_cast<Service*>(&*rsrv), reply->sess);
        vpe->objcaps().inherit(srvcap, sesscap);
        vpe->objcaps().set(cap, sesscap);
    }

    kreply_result(vpe, is, res);
}

void SyscallHandler::createsessat(GateIStream &is) {
    EVENT_TRACER_Syscall_createsessat();
    VPE *vpe = is.gate().session<VPE>();

    auto req = get_message<m3::KIF::Syscall::CreateSessAt>(is);
    capsel_t srvcap = req->srv;
    capsel_t sesscap = req->sess;
    word_t ident = req->ident;

    LOG_SYS(vpe, ": syscall::createsessat",
        "(service=" << srvcap << ", session=" << sesscap << ", ident=#" << m3::fmt(ident, "0x") << ")");

    if(!vpe->objcaps().unused(sesscap))
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid session selector");

    ServiceCapability *scapobj = static_cast<ServiceCapability*>(
        vpe->objcaps().get(srvcap, Capability::SERVICE));
    if(scapobj == nullptr || scapobj->inst->closing)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Service capability is invalid");

    SessionCapability *sess = new SessionCapability(&vpe->objcaps(), sesscap,
        const_cast<Service*>(&*scapobj->inst), ident);
    sess->obj->servowned = true;
    vpe->objcaps().set(sesscap, sess);

    kreply_result(vpe, is, m3::Errors::NO_ERROR);
}

void SyscallHandler::createrbuf(GateIStream &is) {
    EVENT_TRACER_Syscall_createrbuf();
    VPE *vpe = is.gate().session<VPE>();

    auto req = get_message<m3::KIF::Syscall::CreateRBuf>(is);
    capsel_t rbuf = req->rbuf;
    int order = req->order;
    int msgorder = req->msgorder;

    LOG_SYS(vpe, ": syscall::createrbuf", "(rbuf=" << rbuf
        << ", size=" << m3::fmt(1UL << order, "#x")
        << ", msgsize=" << m3::fmt(1UL << msgorder, "#x") << ")");

    if(msgorder > order)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid arguments");
    if((1UL << (order - msgorder)) > MAX_RB_SIZE)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Too many receive buffer slots");

    RBufCapability *cap = new RBufCapability(&vpe->objcaps(), rbuf, order, msgorder);
    vpe->objcaps().set(rbuf, cap);

    kreply_result(vpe, is, m3::Errors::NO_ERROR);
}

void SyscallHandler::creategate(GateIStream &is) {
    EVENT_TRACER_Syscall_creategate();
    VPE *vpe = is.gate().session<VPE>();

    auto req = get_message<m3::KIF::Syscall::CreateGate>(is);
    capsel_t rbuf = req->rbuf;
    capsel_t dstcap = req->gate;
    label_t label = req->label;
    word_t credits = req->credits;

    LOG_SYS(vpe, ": syscall::creategate", "(rbuf=" << rbuf << ", cap=" << dstcap
        << ", label=" << m3::fmt(label, "#0x", sizeof(label_t) * 2)
        << ", crd=#" << m3::fmt(credits, "0x") << ")");

#if defined(__gem5__)
    if(credits == m3::KIF::UNLIM_CREDITS)
        PANIC("Unlimited credits are not yet supported on gem5");
#endif

    RBufCapability *rbufcap = static_cast<RBufCapability*>(vpe->objcaps().get(rbuf, Capability::RBUF));
    if(rbufcap == nullptr)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "RBuf capability is invalid");

    if(!vpe->objcaps().unused(dstcap))
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid cap");

    vpe->objcaps().set(dstcap,
        new MsgCapability(&vpe->objcaps(), dstcap, &*rbufcap->obj, label, credits));

    kreply_result(vpe, is, m3::Errors::NO_ERROR);
}

void SyscallHandler::createvpe(GateIStream &is) {
    EVENT_TRACER_Syscall_createvpe();
    VPE *vpe = is.gate().session<VPE>();

    auto req = get_message<m3::KIF::Syscall::CreateVPE>(is);
    capsel_t tcap = req->vpe;
    capsel_t mcap = req->mem;
    capsel_t gcap = req->gate;
    m3::PEDesc::value_t pe = req->pe;
    epid_t ep = req->ep;
    bool tmuxable = req->muxable;
    m3::String name(req->name, m3::Math::min(req->namelen, sizeof(req->name)));

    LOG_SYS(vpe, ": syscall::createvpe", "(name=" << name
        << ", pe=" << static_cast<int>(m3::PEDesc(pe).type())
        << ", tcap=" << tcap << ", mcap=" << mcap << ", pfgate=" << gcap
        << ", pfep=" << ep << ", tmuxable=" << tmuxable << ")");

    if(!vpe->objcaps().unused(tcap) || !vpe->objcaps().unused(mcap))
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid VPE or memory cap");

    // if it has a pager, we need a gate cap
    MsgCapability *msg = nullptr;
    if(gcap != m3::KIF::INV_SEL) {
        msg = static_cast<MsgCapability*>(vpe->objcaps().get(gcap, Capability::MSG));
        if(msg == nullptr)
            SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid cap(s)");
    }
    else
        ep = -1;

    // create VPE
    VPE *nvpe = VPEManager::get().create(std::move(name), m3::PEDesc(pe), ep, gcap, tmuxable);
    if(nvpe == nullptr)
        SYS_ERROR(vpe, is, m3::Errors::NO_FREE_PE, "No free and suitable PE found");

    // childs of daemons are daemons
    if(vpe->is_daemon())
        nvpe->make_daemon();

    // inherit VPE and mem caps to the parent
    vpe->objcaps().obtain(tcap, nvpe->objcaps().get(0));
    vpe->objcaps().obtain(mcap, nvpe->objcaps().get(1));

    // delegate pf gate to the new VPE
    if(gcap != m3::KIF::INV_SEL)
        nvpe->objcaps().obtain(gcap, msg);

    kreply_vmsg(vpe, is, m3::Errors::NO_ERROR, Platform::pe(nvpe->pe()).value());
}

void SyscallHandler::createmap(UNUSED GateIStream &is) {
    EVENT_TRACER_Syscall_createmap();
    VPE *vpe = is.gate().session<VPE>();

#if defined(__gem5__)
    auto req = get_message<m3::KIF::Syscall::CreateMap>(is);
    capsel_t tcap = req->vpe;
    capsel_t mcap = req->mem;
    capsel_t first = req->first;
    capsel_t pages = req->pages;
    capsel_t dst = req->dest;
    int perms = req->perms;

    LOG_SYS(vpe, ": syscall::createmap", "(vpe=" << tcap << ", mem=" << mcap
        << ", first=" << first << ", pages=" << pages << ", dst=" << dst
        << ", perms=" << perms << ")");

    VPECapability *tcapobj = static_cast<VPECapability*>(vpe->objcaps().get(tcap, Capability::VIRTPE));
    if(tcapobj == nullptr)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "VPE capability is invalid");
    MemCapability *mcapobj = static_cast<MemCapability*>(vpe->objcaps().get(mcap, Capability::MEM));
    if(mcapobj == nullptr)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Memory capability is invalid");

    if((mcapobj->obj->addr & PAGE_MASK) || (mcapobj->obj->size & PAGE_MASK))
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Memory capability is not page aligned");
    if(perms & ~mcapobj->obj->perms)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid permissions");

    size_t total = mcapobj->obj->size >> PAGE_BITS;
    if(first >= total || first + pages <= first || first + pages > total)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Region of memory capability is invalid");

    uintptr_t phys = m3::DTU::build_noc_addr(mcapobj->obj->pe, mcapobj->obj->addr + PAGE_SIZE * first);
    CapTable &mcaps = tcapobj->vpe->mapcaps();

    MapCapability *mapcap = static_cast<MapCapability*>(mcaps.get(dst, Capability::MAP));
    if(mapcap == nullptr) {
        MapCapability *mapcap = new MapCapability(&mcaps, dst, phys, pages, perms);
        mcaps.inherit(mcapobj, mapcap);
        mcaps.set(dst, mapcap);
    }
    else {
        if(mapcap->length != pages) {
            SYS_ERROR(vpe, is, m3::Errors::INV_ARGS,
                "Map capability exists with different number of pages ("
                    << mapcap->length << " vs. " << pages << ")");
        }
        mapcap->remap(phys, perms);
    }
#endif

    kreply_result(vpe, is, m3::Errors::NO_ERROR);
}

void SyscallHandler::exchange(GateIStream &is) {
    EVENT_TRACER_Syscall_exchange();
    VPE *vpe = is.gate().session<VPE>();

    auto req = get_message<m3::KIF::Syscall::Exchange>(is);
    capsel_t tcap = req->vpe;
    m3::KIF::CapRngDesc own(req->own);
    m3::KIF::CapRngDesc other(own.type(), req->other, own.count());
    bool obtain = req->obtain;

    LOG_SYS(vpe, ": syscall::exchange", "(vpe=" << tcap << ", own=" << own
        << ", other=" << other << ", obtain=" << obtain << ")");

    VPECapability *vpecap = static_cast<VPECapability*>(
            vpe->objcaps().get(tcap, Capability::VIRTPE));
    if(vpecap == nullptr)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid VPE cap");

    VPE *t1 = obtain ? vpecap->vpe : vpe;
    VPE *t2 = obtain ? vpe : vpecap->vpe;
    m3::Errors::Code res = do_exchange(t1, t2, own, other, obtain);

    kreply_result(vpe, is, res);
}

void SyscallHandler::vpectrl(GateIStream &is) {
    EVENT_TRACER_Syscall_vpectrl();
    VPE *vpe = is.gate().session<VPE>();

    auto req = get_message<m3::KIF::Syscall::VPECtrl>(is);
    capsel_t tcap = req->vpe;
    m3::KIF::Syscall::VPEOp op = static_cast<m3::KIF::Syscall::VPEOp>(req->op);
    int pid = req->pid;

    LOG_SYS(vpe, ": syscall::vpectrl", "(vpe=" << tcap << ", op=" << op
            << ", pid=" << pid << ")");

    VPECapability *vpecap = static_cast<VPECapability*>(
            vpe->objcaps().get(tcap, Capability::VIRTPE));
    if(vpecap == nullptr)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid VPE cap");
    if(vpe == vpecap->vpe)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "VPE can't ctrl itself");

#if defined(__host__)
    vpecap->vpe->set_pid(pid);
#endif

    switch(op) {
        case m3::KIF::Syscall::VCTRL_START: {
            vpecap->vpe->start_app();
            kreply_result(vpe, is, m3::Errors::NO_ERROR);
            break;
        }

        case m3::KIF::Syscall::VCTRL_STOP:
            vpecap->vpe->stop_app();
            kreply_result(vpe, is, m3::Errors::NO_ERROR);
            break;

        case m3::KIF::Syscall::VCTRL_WAIT:
            m3::KIF::Syscall::VPECtrlReply reply;
            reply.error = m3::Errors::NO_ERROR;

            if(vpecap->vpe->has_app()) {
                vpe->start_wait();
                vpecap->vpe->wait_for_exit();
                vpe->stop_wait();

                LOG_SYS(vpe, ": syscall::vpectrl-cb", "(exitcode=" << vpecap->vpe->exitcode() << ")");
            }

            reply.exitcode = vpecap->vpe->exitcode();
            kreply_msg(vpe, is, &reply, sizeof(reply));
            break;
    }
}

void SyscallHandler::reqmem(GateIStream &is) {
    EVENT_TRACER_Syscall_reqmem();
    VPE *vpe = is.gate().session<VPE>();

    auto req = get_message<m3::KIF::Syscall::ReqMem>(is);
    capsel_t cap = req->mem;
    uintptr_t addr = req->addr;
    size_t size = req->size;
    int perms = req->perms;

    LOG_SYS(vpe, ": syscall::reqmem", "(cap=" << cap
        << ", addr=#" << m3::fmt(addr, "x") << ", size=#" << m3::fmt(size, "x")
        << ", perms=" << perms << ")");

    if(!vpe->objcaps().unused(cap))
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid cap");
    if(size == 0 || (size & m3::KIF::Perm::RWX) || perms == 0 || (perms & ~(m3::KIF::Perm::RWX)))
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Size or permissions invalid");

    MainMemory &mem = MainMemory::get();
    MainMemory::Allocation alloc =
        addr == (uintptr_t)-1 ? mem.allocate(size, PAGE_SIZE) : mem.allocate_at(addr, size);
    if(!alloc)
        SYS_ERROR(vpe, is, m3::Errors::OUT_OF_MEM, "Not enough memory");

    // TODO if addr was 0, we don't want to free it on revoke
    vpe->objcaps().set(cap, new MemCapability(&vpe->objcaps(), cap,
        alloc.pe(), VPE::INVALID_ID, alloc.addr, alloc.size, perms));

    kreply_result(vpe, is, m3::Errors::NO_ERROR);
}

void SyscallHandler::derivemem(GateIStream &is) {
    EVENT_TRACER_Syscall_derivemem();
    VPE *vpe = is.gate().session<VPE>();

    auto req = get_message<m3::KIF::Syscall::DeriveMem>(is);
    capsel_t src = req->src;
    capsel_t dst = req->dst;
    size_t offset = req->offset;
    size_t size = req->size;
    int perms = req->perms;

    LOG_SYS(vpe, ": syscall::derivemem", "(src=" << src << ", dst=" << dst
        << ", size=" << size << ", off=" << offset << ", perms=" << perms << ")");

    MemCapability *srccap = static_cast<MemCapability*>(
            vpe->objcaps().get(src, Capability::MEM));
    if(srccap == nullptr || !vpe->objcaps().unused(dst))
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid cap(s)");

    if(offset + size < offset || offset + size > srccap->obj->size || size == 0 ||
            (perms & ~(m3::KIF::Perm::RWX)))
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid args");

    MemCapability *dercap = static_cast<MemCapability*>(vpe->objcaps().obtain(dst, srccap));
    dercap->obj = m3::Reference<MemObject>(new MemObject(
        srccap->obj->pe,
        srccap->obj->vpe,
        srccap->obj->addr + offset,
        size,
        perms & srccap->obj->perms
    ));
    dercap->obj->derived = true;

    kreply_result(vpe, is, m3::Errors::NO_ERROR);
}

void SyscallHandler::delegate(GateIStream &is) {
    EVENT_TRACER_Syscall_delegate();
    exchange_over_sess(is, false);
}

void SyscallHandler::obtain(GateIStream &is) {
    EVENT_TRACER_Syscall_obtain();
    exchange_over_sess(is, true);
}

m3::Errors::Code SyscallHandler::do_exchange(VPE *v1, VPE *v2, const m3::KIF::CapRngDesc &c1,
        const m3::KIF::CapRngDesc &c2, bool obtain) {
    VPE &src = obtain ? *v2 : *v1;
    VPE &dst = obtain ? *v1 : *v2;
    const m3::KIF::CapRngDesc &srcrng = obtain ? c2 : c1;
    const m3::KIF::CapRngDesc &dstrng = obtain ? c1 : c2;

    if(c1.type() != c2.type()) {
        LOG_ERROR(v1, m3::Errors::INV_ARGS, "Descriptor types don't match");
        return m3::Errors::INV_ARGS;
    }
    if((obtain && c2.count() > c1.count()) || (!obtain && c2.count() != c1.count())) {
        LOG_ERROR(v1, m3::Errors::INV_ARGS, "Server gave me invalid CRD");
        return m3::Errors::INV_ARGS;
    }
    if(!dst.objcaps().range_unused(dstrng)) {
        LOG_ERROR(v1, m3::Errors::INV_ARGS, "Invalid destination caps");
        return m3::Errors::INV_ARGS;
    }

    // TODO exchange map caps doesn't really work yet, because they might have a length > 1

    CapTable &srctab = c1.type() == m3::KIF::CapRngDesc::OBJ ? src.objcaps() : src.mapcaps();
    CapTable &dsttab = c1.type() == m3::KIF::CapRngDesc::OBJ ? dst.objcaps() : dst.mapcaps();
    for(uint i = 0; i < c2.count(); ++i) {
        capsel_t srccap = srcrng.start() + i;
        capsel_t dstcap = dstrng.start() + i;
        Capability *scapobj = srctab.get(srccap);
        assert(dsttab.get(dstcap) == nullptr);
        dsttab.obtain(dstcap, scapobj);
    }
    return m3::Errors::NO_ERROR;
}

void SyscallHandler::exchange_over_sess(GateIStream &is, bool obtain) {
    VPE *vpe = is.gate().session<VPE>();

    auto req = get_message<m3::KIF::Syscall::ExchangeSess>(is);
    capsel_t sesscap = req->sess;
    m3::KIF::CapRngDesc caps(req->caps);

    // TODO compiler-bug? if I try to print caps, it hangs on T2!? doing it here manually works
    LOG_SYS(vpe, (obtain ? ": syscall::obtain" : ": syscall::delegate"),
            "(sess=" << sesscap << ", caps=" << caps.start() << ":" << caps.count() << ")");

    SessionCapability *sess = static_cast<SessionCapability*>(
        vpe->objcaps().get(sesscap, Capability::SESSION));
    if(sess == nullptr)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid session-cap");
    if(sess->obj->srv->closing)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Server is shutting down");

    // we can't be sure that the session will still exist when we receive the reply
    m3::Reference<Service> rsrv(sess->obj->srv);

    vpe->start_wait();
    while(rsrv->vpe().state() != VPE::RUNNING) {
        if(!rsrv->vpe().resume()) {
            vpe->stop_wait();
            SYS_ERROR(vpe, is, m3::Errors::VPE_GONE, "VPE does no longer exist");
        }
    }

    m3::KIF::Service::Exchange msg;
    msg.opcode = obtain ? m3::KIF::Service::OBTAIN : m3::KIF::Service::DELEGATE;
    msg.sess = sess->obj->ident;
    msg.data.caps = caps.count();
    msg.data.argcount = req->argcount;
    for(size_t i = 0; i < req->argcount; ++i)
        msg.data.args[i] = req->args[i];

    const m3::DTU::Message *srvreply = rsrv->send_receive(&msg, sizeof(msg), false);
    vpe->stop_wait();

    EVENT_TRACER_Syscall_delob_done();
    auto *reply = reinterpret_cast<const m3::KIF::Service::ExchangeReply*>(srvreply->data);

    m3::Errors::Code res = static_cast<m3::Errors::Code>(reply->error);

    LOG_SYS(vpe, (obtain ? ": syscall::obtain-cb" : ": syscall::delegate-cb"), "(res=" << res << ")");

    if(res != m3::Errors::NO_ERROR)
        LOG_ERROR(vpe, res, "Server denied cap-transfer");
    else {
        m3::KIF::CapRngDesc srvcaps(reply->data.caps);
        res = do_exchange(vpe, &rsrv->vpe(), caps, srvcaps, obtain);
    }

    m3::KIF::Syscall::ExchangeSessReply kreply;
    kreply.error = res;
    kreply.argcount = 0;
    if(res == m3::Errors::NO_ERROR) {
        kreply.argcount = m3::Math::min(reply->data.argcount, ARRAY_SIZE(kreply.args));
        for(size_t i = 0; i < kreply.argcount; ++i)
            kreply.args[i] = reply->data.args[i];
    }
    kreply_msg(vpe, is, &kreply, sizeof(kreply));
}

void SyscallHandler::activate(GateIStream &is) {
    EVENT_TRACER_Syscall_activate();
    VPE *vpe = is.gate().session<VPE>();

    auto *req = get_message<m3::KIF::Syscall::Activate>(is);
    capsel_t tvpe = req->vpe;
    epid_t ep = req->ep;
    capsel_t cap = req->cap;
    uintptr_t addr = req->addr;

    LOG_SYS(vpe, ": syscall::activate", "(vpe=" << tvpe << ", ep=" << ep << ", cap=" << cap
        << ", addr=#" << m3::fmt(addr, "x") << ")");

    VPECapability *tvpeobj = static_cast<VPECapability*>(vpe->objcaps().get(tvpe, Capability::VIRTPE));
    if(tvpeobj == nullptr)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "VPE capability is invalid");

    if(ep <= m3::DTU::UPCALL_REP || ep >= EP_COUNT)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalidate EP");

    Capability *capobj = nullptr;
    if(cap != m3::KIF::INV_SEL) {
        capobj = vpe->objcaps().get(cap, Capability::MSG | Capability::MEM | Capability::RBUF);
        if(capobj == nullptr)
            SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalidate capability");
    }

    Capability *oldobj = tvpeobj->vpe->ep_cap(ep);
    if(oldobj) {
        if(oldobj->type == Capability::RBUF) {
            RBufCapability *rbufcap = static_cast<RBufCapability*>(capobj);
            rbufcap->obj->addr = 0;
        }

        m3::Errors::Code res = DTU::get().inval_ep_remote(tvpeobj->vpe->desc(), ep);
        if(res != m3::Errors::NO_ERROR)
            SYS_ERROR(vpe, is, res, "Unable to invalidate EP");
    }

    if(capobj) {
        epid_t oldep = tvpeobj->vpe->cap_ep(capobj);
        if(oldep && oldep != ep)
            SYS_ERROR(vpe, is, m3::Errors::EXISTS, "Capability already in use");

        if(capobj->type == Capability::MEM)
            tvpeobj->vpe->config_mem_ep(ep, *static_cast<MemCapability*>(capobj)->obj);
        else if(capobj->type == Capability::MSG) {
            MsgCapability *msgcap = static_cast<MsgCapability*>(capobj);

            if(msgcap->obj->rbuf->addr == 0) {
                LOG_SYS(vpe, ": syscall::activate", ": waiting for rbuf " << &msgcap->obj->rbuf);

                vpe->start_wait();
                m3::ThreadManager::get().wait_for(&*msgcap->obj->rbuf);
                vpe->stop_wait();
            }

            tvpeobj->vpe->config_snd_ep(ep, *msgcap->obj);
        }
        else {
            RBufCapability *rbufcap = static_cast<RBufCapability*>(capobj);
            if(addr < Platform::rw_barrier(tvpeobj->vpe->pe()))
                SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Not in receive buffer space");
            if(rbufcap->obj->addr != 0)
                SYS_ERROR(vpe, is, m3::Errors::EXISTS, "Receive buffer already activated");

            LOG_SYS(vpe, ": syscall::activate", ": rbuf " << &*rbufcap->obj);

            rbufcap->obj->vpe = tvpeobj->vpe->id();
            rbufcap->obj->addr = addr;
            rbufcap->obj->ep = ep;

            m3::Errors::Code res = tvpeobj->vpe->rbufs().attach(*tvpeobj->vpe, &*rbufcap->obj);
            if(res != m3::Errors::NO_ERROR)
                SYS_ERROR(vpe, is, res, "Unable to invalidate EP");
        }
    }
    else
        tvpeobj->vpe->invalidate_ep(ep);

    tvpeobj->vpe->ep_cap(ep, capobj);

    kreply_result(vpe, is, m3::Errors::NO_ERROR);
}

m3::Errors::Code SyscallHandler::wait_for(const char *name, VPE &tvpe, VPE *cur) {
    m3::Errors::Code res = m3::Errors::NO_ERROR;
    while(tvpe.state() != VPE::RUNNING) {
        cur->start_wait();
        tvpe.add_forward();

        // TODO not required anymore
        if(tvpe.pe() == cur->pe())
            tvpe.migrate();

        LOG_SYS(cur, name, ": waiting for VPE " << tvpe.id() << " at " << tvpe.pe());

        if(!tvpe.resume(false))
            res = m3::Errors::VPE_GONE;

        tvpe.rem_forward();
        cur->stop_wait();
    }
    return res;
}

void SyscallHandler::forwardmsg(GateIStream &is) {
    EVENT_TRACER_Syscall_forwardmsg();
    VPE *vpe = is.gate().session<VPE>();

#if !defined(__gem5__)
    kreply_result(vpe, is, m3::Errors::NOT_SUP);
#else
    auto *req = get_message<m3::KIF::Syscall::ForwardMsg>(is);
    capsel_t cap = req->cap;
    size_t len = req->len;
    epid_t rep = req->repid;
    label_t rlabel = req->rlabel;
    word_t event = req->event;

    LOG_SYS(vpe, ": syscall::forwardmsg", "(cap=" << cap << ", len=" << len
        << ", rep=" << rep << ", rlabel=" << m3::fmt(rlabel, "0x") << ", event=" << event << ")");

    MsgCapability *capobj = static_cast<MsgCapability*>(vpe->objcaps().get(cap, Capability::MSG));
    if(capobj == nullptr)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid msg cap");

    epid_t ep = vpe->cap_ep(capobj);
    if(ep == 0)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Msg cap is not activated");
    if(!vpe->can_forward_msg(ep))
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Send did not fail");

    // TODO if we do that asynchronously, we need to buffer the message somewhere, because the
    // VPE might want to do other system calls in the meantime. probably, VPEs need to allocate
    // the memory beforehand and the kernel will simply use it afterwards.
    VPE &tvpe = VPEManager::get().vpe(capobj->obj->rbuf->vpe);
    bool async = tvpe.state() != VPE::RUNNING && event;
    if(async)
        kreply_result(vpe, is, m3::Errors::UPCALL_REPLY);

    m3::Errors::Code res = wait_for(": syscall::forwardmsg", tvpe, vpe);

    if(res == m3::Errors::NO_ERROR) {
        uint32_t sender = vpe->pe() | (vpe->id() << 8);
        DTU::get().send_to(tvpe.desc(), capobj->obj->rbuf->ep, capobj->obj->label, req->msg, req->len,
            req->rlabel, req->repid, sender);

        vpe->forward_msg(ep, tvpe.pe(), tvpe.id());
    }
    else
        LOG_ERROR(vpe, res, "forwardmsg failed");

    if(async)
        vpe->upcall_notify(res, event);
    else
        kreply_result(vpe, is, res);
#endif
}

void SyscallHandler::forwardmem(GateIStream &is) {
    EVENT_TRACER_Syscall_forwardmem();
    VPE *vpe = is.gate().session<VPE>();

#if !defined(__gem5__)
    kreply_result(vpe, is, m3::Errors::NOT_SUP);
#else
    auto *req = get_message<m3::KIF::Syscall::ForwardMem>(is);
    capsel_t cap = req->cap;
    size_t len = m3::Math::min(sizeof(req->data), req->len);
    size_t offset = req->offset;
    uint flags = req->flags;
    word_t event = req->event;

    LOG_SYS(vpe, ": syscall::forwardmem", "(cap=" << cap << ", len=" << len
        << ", offset=" << offset << ", flags=" << m3::fmt(flags, "0x") << ", event=" << event << ")");

    MemCapability *capobj = static_cast<MemCapability*>(vpe->objcaps().get(cap, Capability::MEM));
    if(capobj == nullptr)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid memory cap");

    epid_t ep = vpe->cap_ep(capobj);
    if(ep == 0)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Mem cap is not activated");

    if(capobj->obj->addr + offset < offset || offset >= capobj->obj->size ||
       offset + len < offset || offset + len > capobj->obj->size)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid offset/length");
    if((flags & m3::KIF::Syscall::ForwardMem::WRITE) && !(capobj->obj->perms & m3::KIF::Perm::W))
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "No write permission");
    if((~flags & m3::KIF::Syscall::ForwardMem::WRITE) && !(capobj->obj->perms & m3::KIF::Perm::R))
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "No read permission");

    VPE &tvpe = VPEManager::get().vpe(capobj->obj->vpe);
    bool async = tvpe.state() != VPE::RUNNING && event;
    if(async)
        kreply_result(vpe, is, m3::Errors::UPCALL_REPLY);

    m3::Errors::Code res = wait_for(": syscall::forwardmem", tvpe, vpe);

    m3::KIF::Syscall::ForwardMemReply reply;
    reply.error = res;

    if(res == m3::Errors::NO_ERROR) {
        if(flags & m3::KIF::Syscall::ForwardMem::WRITE)
            res = DTU::get().try_write_mem(tvpe.desc(), capobj->obj->addr + offset, req->data, len);
        else
            res = DTU::get().try_read_mem(tvpe.desc(), capobj->obj->addr + offset, reply.data, len);

        vpe->forward_mem(ep, tvpe.pe());
    }
    if(res != m3::Errors::NO_ERROR && res != m3::Errors::PAGEFAULT)
        LOG_ERROR(vpe, res, "forwardmem failed");

    if(async)
        vpe->upcall_notify(res, event);
    else
        kreply_result(vpe, is, res);
#endif
}

void SyscallHandler::forwardreply(GateIStream &is) {
    EVENT_TRACER_Syscall_forwardreply();
    VPE *vpe = is.gate().session<VPE>();

#if !defined(__gem5__)
    kreply_result(vpe, is, m3::Errors::NOT_SUP);
#else
    auto *req = get_message<m3::KIF::Syscall::ForwardReply>(is);
    capsel_t cap = req->cap;
    uintptr_t msgaddr = req->msgaddr;
    size_t len = m3::Math::min(sizeof(req->msg), req->len);
    word_t event = req->event;

    LOG_SYS(vpe, ": syscall::forwardreply", "(cap=" << cap << ", len=" << len
        << ", msgaddr=" << (void*)msgaddr << ", event=" << event << ")");

    RBufCapability *capobj = static_cast<RBufCapability*>(vpe->objcaps().get(cap, Capability::RBUF));
    if(capobj == nullptr)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid rbuf cap");

    // ensure that the VPE is running, because we need to access it's address space
    while(vpe->state() != VPE::RUNNING) {
        if(!vpe->resume())
            return;
    }

    m3::DTU::Header head;
    m3::Errors::Code res = vpe->rbufs().get_header(*vpe, &*capobj->obj, msgaddr, head);
    if(res != m3::Errors::NO_ERROR || !(head.flags & m3::DTU::Header::FL_REPLY_FAILED))
        SYS_ERROR(vpe, is, res, "Invalid arguments");

    VPE &tvpe = VPEManager::get().vpe(head.senderVpeId);
    bool async = tvpe.state() != VPE::RUNNING && event;
    if(async)
        kreply_result(vpe, is, m3::Errors::UPCALL_REPLY);

    res = wait_for(": syscall::forwardreply", tvpe, vpe);

    if(res == m3::Errors::NO_ERROR) {
        DTU::get().reply_to(tvpe.desc(), head.replyEp, head.senderEp, head.length,
            head.replylabel, req->msg, len);

        while(vpe->state() != VPE::RUNNING) {
            if(!vpe->resume())
                return;
        }

        head.flags &= ~(m3::DTU::Header::FL_REPLY_FAILED | m3::DTU::Header::FL_REPLY_ENABLED);
        res = vpe->rbufs().set_header(*vpe, &*capobj->obj, msgaddr, head);
    }
    if(res != m3::Errors::NO_ERROR)
        LOG_ERROR(vpe, res, "forwardreply failed");

    if(async)
        vpe->upcall_notify(res, event);
    else
        kreply_result(vpe, is, res);
#endif
}

void SyscallHandler::revoke(GateIStream &is) {
    EVENT_TRACER_Syscall_revoke();
    VPE *vpe = is.gate().session<VPE>();

    auto *req = get_message<m3::KIF::Syscall::Revoke>(is);
    capsel_t vcap = req->vpe;
    m3::KIF::CapRngDesc crd(req->crd);
    bool own = req->own;

    LOG_SYS(vpe, ": syscall::revoke", "(vpe=" << vcap << ", crd=" << crd << ", own=" << own << ")");

    VPECapability *vpecap = static_cast<VPECapability*>(vpe->objcaps().get(vcap, Capability::VIRTPE));
    if(vpecap == nullptr)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Invalid cap");

    if(crd.type() == m3::KIF::CapRngDesc::OBJ && crd.start() < 2)
        SYS_ERROR(vpe, is, m3::Errors::INV_ARGS, "Cap 0 and 1 are not revokeable");

    CapTable &table = crd.type() == m3::KIF::CapRngDesc::OBJ
        ? vpecap->vpe->objcaps()
        : vpecap->vpe->mapcaps();
    m3::Errors::Code res = table.revoke(crd, own);
    if(res != m3::Errors::NO_ERROR)
        SYS_ERROR(vpe, is, res, "Revoke failed");

    kreply_result(vpe, is, m3::Errors::NO_ERROR);
}

void SyscallHandler::idle(GateIStream &is) {
    EVENT_TRACER_Syscall_idle();
    VPE *vpe = is.gate().session<VPE>();
    LOG_SYS(vpe, ": syscall::idle", "()");

    vpe->yield();
}

void SyscallHandler::exit(GateIStream &is) {
    EVENT_TRACER_Syscall_exit();
    VPE *vpe = is.gate().session<VPE>();

    auto *req = get_message<m3::KIF::Syscall::Exit>(is);
    int exitcode = req->exitcode;

    LOG_SYS(vpe, ": syscall::exit", "(" << exitcode << ")");

    vpe->exit_app(exitcode);
}

void SyscallHandler::noop(GateIStream &is) {
    EVENT_TRACER_Syscall_noop();
    VPE *vpe = is.gate().session<VPE>();
    LOG_SYS(vpe, ": syscall::noop", "()");

    kreply_result(vpe, is, m3::Errors::NO_ERROR);
}

#if defined(__host__)
void SyscallHandler::init(GateIStream &is) {
    VPE *vpe = is.gate().session<VPE>();

    auto *req = get_message<m3::KIF::Syscall::Init>(is);
    uintptr_t addr = req->eps;

    LOG_SYS(vpe, ": syscall::init", "(" << (void*)addr << ")");

    vpe->set_ep_addr(addr);

    kreply_result(vpe, is, m3::Errors::NO_ERROR);
}
#endif

}
