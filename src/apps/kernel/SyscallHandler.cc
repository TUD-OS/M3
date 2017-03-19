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
#include <base/util/Math.h>
#include <base/Init.h>
#include <base/Panic.h>

#include <thread/ThreadManager.h>

#include "com/Services.h"
#include "pes/PEManager.h"
#include "pes/VPEManager.h"
#include "DTU.h"
#include "Platform.h"
#include "SyscallHandler.h"
#include "WorkLoop.h"

#if defined(__host__)
extern int int_target;
#endif

namespace kernel {

SyscallHandler::handler_func SyscallHandler::_callbacks[m3::KIF::Syscall::COUNT];

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

#define SYS_ERROR(vpe, msg, errcode, errmsg) {                                              \
        LOG_ERROR(vpe, errcode, errmsg);                                                    \
        reply_result((vpe), (msg), (errcode));                                              \
        return;                                                                             \
    }

template<typename T>
static const T *get_message(const m3::DTU::Message *msg) {
    return reinterpret_cast<const T*>(msg->data);
}

void SyscallHandler::init() {
#if !defined(__t2__)
    // configure both receive buffers (we need to do that manually in the kernel)
    int buford = m3::getnextlog2(Platform::pe_count()) + VPE::SYSC_MSGSIZE_ORD;
    size_t bufsize = static_cast<size_t>(1) << buford;
    DTU::get().recv_msgs(ep(),reinterpret_cast<uintptr_t>(new uint8_t[bufsize]),
        buford, VPE::SYSC_MSGSIZE_ORD);

    buford = m3::nextlog2<1024>::val;
    bufsize = static_cast<size_t>(1) << buford;
    DTU::get().recv_msgs(srvep(), reinterpret_cast<uintptr_t>(new uint8_t[bufsize]),
        buford, m3::nextlog2<256>::val);
#endif

    // add a dummy item to workloop; we handle everything manually anyway
    // but one item is needed to not stop immediately
    m3::env()->workloop()->add(nullptr, false);

    add_operation(m3::KIF::Syscall::PAGEFAULT,      &SyscallHandler::pagefault);
    add_operation(m3::KIF::Syscall::CREATE_SRV,     &SyscallHandler::createsrv);
    add_operation(m3::KIF::Syscall::CREATE_SESS,    &SyscallHandler::createsess);
    add_operation(m3::KIF::Syscall::CREATE_SESS_AT, &SyscallHandler::createsessat);
    add_operation(m3::KIF::Syscall::CREATE_RGATE,   &SyscallHandler::creatergate);
    add_operation(m3::KIF::Syscall::CREATE_SGATE,   &SyscallHandler::createsgate);
    add_operation(m3::KIF::Syscall::CREATE_MGATE,   &SyscallHandler::createmgate);
    add_operation(m3::KIF::Syscall::CREATE_VPE,     &SyscallHandler::createvpe);
    add_operation(m3::KIF::Syscall::CREATE_MAP,     &SyscallHandler::createmap);
    add_operation(m3::KIF::Syscall::ACTIVATE,       &SyscallHandler::activate);
    add_operation(m3::KIF::Syscall::VPE_CTRL,       &SyscallHandler::vpectrl);
    add_operation(m3::KIF::Syscall::DERIVE_MEM,     &SyscallHandler::derivemem);
    add_operation(m3::KIF::Syscall::EXCHANGE,       &SyscallHandler::exchange);
    add_operation(m3::KIF::Syscall::DELEGATE,       &SyscallHandler::delegate);
    add_operation(m3::KIF::Syscall::OBTAIN,         &SyscallHandler::obtain);
    add_operation(m3::KIF::Syscall::REVOKE,         &SyscallHandler::revoke);
    add_operation(m3::KIF::Syscall::FORWARD_MSG,    &SyscallHandler::forwardmsg);
    add_operation(m3::KIF::Syscall::FORWARD_MEM,    &SyscallHandler::forwardmem);
    add_operation(m3::KIF::Syscall::FORWARD_REPLY,  &SyscallHandler::forwardreply);
    add_operation(m3::KIF::Syscall::NOOP,           &SyscallHandler::noop);
}

void SyscallHandler::reply_msg(VPE *vpe, const m3::DTU::Message *msg, const void *reply, size_t size) {
    while(vpe->state() != VPE::RUNNING) {
        if(!vpe->resume())
            return;
    }

    DTU::get().reply(ep(), reply, size, m3::DTU::get().get_msgoff(ep(), msg));
}

void SyscallHandler::reply_result(VPE *vpe, const m3::DTU::Message *msg, m3::Errors::Code code) {
    m3::KIF::DefaultReply reply;
    reply.error = code;
    return reply_msg(vpe, msg, &reply, sizeof(reply));
}

void SyscallHandler::handle_message(VPE *vpe, const m3::DTU::Message *msg) {
    auto req = get_message<m3::KIF::DefaultRequest>(msg);
    m3::KIF::Syscall::Operation op = static_cast<m3::KIF::Syscall::Operation>(req->opcode);

    if(static_cast<size_t>(op) < sizeof(_callbacks) / sizeof(_callbacks[0]))
        _callbacks[op](vpe, msg);
    else
        reply_result(vpe, msg, m3::Errors::INV_ARGS);
}

void SyscallHandler::pagefault(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_pagefault();

#if defined(__gem5__)
    auto req = get_message<m3::KIF::Syscall::Pagefault>(msg);
    uint64_t virt = req->virt;
    uint access = req->access;

    LOG_SYS(vpe, ": syscall::pagefault", "(virt=" << m3::fmt(virt, "p")
        << ", access " << m3::fmt(access, "#x") << ")");

    if(!vpe->address_space())
        SYS_ERROR(vpe, msg, m3::Errors::NOT_SUP, "No address space / PF handler");

    // if we don't have a pager, it was probably because of speculative execution. just return an
    // error in this case and don't print anything
    capsel_t gcap = vpe->address_space()->gate();
    auto sgatecap = static_cast<SGateCapability*>(vpe->objcaps().get(gcap, Capability::SGATE));
    if(sgatecap == nullptr) {
        reply_result(vpe, msg, m3::Errors::INV_ARGS);
        return;
    }

    // TODO this might also indicates that the pf handler is not available (ctx switch, migrate, ...)
    vpe->config_snd_ep(vpe->address_space()->ep(), *sgatecap->obj);
#endif

    reply_result(vpe, msg, m3::Errors::NONE);
}

void SyscallHandler::createsrv(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_createsrv();

    auto req = get_message<m3::KIF::Syscall::CreateSrv>(msg);
    capsel_t dst = req->dst_sel;
    capsel_t rgate = req->rgate_sel;
    m3::String name(req->name, m3::Math::min(static_cast<size_t>(req->namelen), sizeof(req->name)));

    LOG_SYS(vpe, ": syscall::createsrv", "(dst=" << dst << ", rgate=" << rgate << ", name=" << name << ")");

    auto rgatecap = static_cast<RGateCapability*>(vpe->objcaps().get(rgate, Capability::RGATE));
    if(rgatecap == nullptr || !rgatecap->obj->activated())
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "RGate capability invalid");

    if(ServiceList::get().find(name) != nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::EXISTS, "Service does already exist");

    Service *s = ServiceList::get().add(*vpe, dst, name, rgatecap->obj);
    vpe->objcaps().set(dst, new ServCapability(&vpe->objcaps(), dst, s));

#if defined(__host__)
    // TODO ugly hack
    if(name == "interrupts")
        int_target = vpe->pid();
#endif

    // maybe there are VPEs that now have all requirements fullfilled
    VPEManager::get().start_pending(ServiceList::get());

    reply_result(vpe, msg, m3::Errors::NONE);
}

void SyscallHandler::createsess(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_createsess();

    auto req = get_message<m3::KIF::Syscall::CreateSess>(msg);
    capsel_t dst = req->dst_sel;
    word_t arg = req->arg;
    m3::String name(req->name, m3::Math::min(static_cast<size_t>(req->namelen), sizeof(req->name)));

    LOG_SYS(vpe, ": syscall::createsess", "(dst=" << dst << ", name=" << name << ", arg=" << arg << ")");

    if(!vpe->objcaps().unused(dst))
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid cap");

    Service *s = ServiceList::get().find(name);
    if(!s)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Unknown service");

    m3::Reference<Service> rsrv(s);

    vpe->start_wait();
    while(s->vpe().state() != VPE::RUNNING) {
        if(!s->vpe().resume()) {
            vpe->stop_wait();
            SYS_ERROR(vpe, msg, m3::Errors::VPE_GONE, "VPE does no longer exist");
        }
    }

    m3::KIF::Service::Open smsg;
    smsg.opcode = m3::KIF::Service::OPEN;
    smsg.arg = arg;

    const m3::DTU::Message *srvreply = s->send_receive(&smsg, sizeof(smsg), false);
    vpe->stop_wait();

    if(srvreply == nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::RECV_GONE, "Service unreachable");

    auto reply = reinterpret_cast<const m3::KIF::Service::OpenReply*>(srvreply->data);

    m3::Errors::Code res = static_cast<m3::Errors::Code>(reply->error);

    LOG_SYS(vpe, ": syscall::createsess-cont", "(res=" << res << ")");

    if(res != m3::Errors::NONE)
        LOG_ERROR(vpe, res, "Server denied session creation");
    else {
        // inherit the session-cap from the service-cap. this way, it will be automatically
        // revoked if the service-cap is revoked
        Capability *srvcap = rsrv->vpe().objcaps().get(rsrv->selector(), Capability::SERV);
        assert(srvcap != nullptr);
        auto sesscap = new SessCapability(&vpe->objcaps(), dst, const_cast<Service*>(&*rsrv), reply->sess);
        vpe->objcaps().inherit(srvcap, sesscap);
        vpe->objcaps().set(dst, sesscap);
    }

    reply_result(vpe, msg, res);
}

void SyscallHandler::createsessat(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_createsessat();

    auto req = get_message<m3::KIF::Syscall::CreateSessAt>(msg);
    capsel_t dst = req->dst_sel;
    capsel_t srv = req->srv_sel;
    word_t ident = req->ident;

    LOG_SYS(vpe, ": syscall::createsessat",
        "(dst=" << dst << ", srv=" << srv << ", ident=#" << m3::fmt(ident, "0x") << ")");

    if(!vpe->objcaps().unused(dst))
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid session selector");

    auto srvcap = static_cast<ServCapability*>(vpe->objcaps().get(srv, Capability::SERV));
    if(srvcap == nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Service capability is invalid");

    auto sesscap = new SessCapability(&vpe->objcaps(), dst, const_cast<Service*>(&*srvcap->inst), ident);
    sesscap->obj->servowned = true;
    vpe->objcaps().set(dst, sesscap);

    reply_result(vpe, msg, m3::Errors::NONE);
}

void SyscallHandler::creatergate(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_creatergate();

    auto req = get_message<m3::KIF::Syscall::CreateRGate>(msg);
    capsel_t dst = req->dst_sel;
    int order = req->order;
    int msgorder = req->msgorder;

    LOG_SYS(vpe, ": syscall::creatergate", "(dst=" << dst
        << ", size=" << m3::fmt(1UL << order, "#x")
        << ", msgsize=" << m3::fmt(1UL << msgorder, "#x") << ")");

    if(msgorder > order)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid arguments");
    if((1UL << (order - msgorder)) > MAX_RB_SIZE)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Too many receive buffer slots");

    auto rgatecap = new RGateCapability(&vpe->objcaps(), dst, order, msgorder);
    vpe->objcaps().set(dst, rgatecap);

    reply_result(vpe, msg, m3::Errors::NONE);
}

void SyscallHandler::createsgate(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_createsgate();

    auto req = get_message<m3::KIF::Syscall::CreateSGate>(msg);
    capsel_t dst = req->dst_sel;
    capsel_t rgate = req->rgate_sel;
    label_t label = req->label;
    word_t credits = req->credits;

    LOG_SYS(vpe, ": syscall::createsgate", "(dst=" << dst << ", rgate=" << rgate
        << ", label=" << m3::fmt(label, "#0x", sizeof(label_t) * 2)
        << ", crd=#" << m3::fmt(credits, "0x") << ")");

#if defined(__gem5__)
    if(credits == m3::KIF::UNLIM_CREDITS)
        PANIC("Unlimited credits are not yet supported on gem5");
#endif

    auto rgatecap = static_cast<RGateCapability*>(vpe->objcaps().get(rgate, Capability::RGATE));
    if(rgatecap == nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "RGate capability is invalid");

    if(!vpe->objcaps().unused(dst))
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid cap");

    auto sgcap = new SGateCapability(&vpe->objcaps(), dst, &*rgatecap->obj, label, credits);
    vpe->objcaps().inherit(rgatecap, sgcap);
    vpe->objcaps().set(dst, sgcap);

    reply_result(vpe, msg, m3::Errors::NONE);
}

void SyscallHandler::createmgate(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_createmgate();

    auto req = get_message<m3::KIF::Syscall::CreateMGate>(msg);
    capsel_t dst = req->dst_sel;
    uintptr_t addr = req->addr;
    size_t size = req->size;
    int perms = req->perms;

    LOG_SYS(vpe, ": syscall::createmgate", "(dst=" << dst
        << ", addr=#" << m3::fmt(addr, "x") << ", size=#" << m3::fmt(size, "x")
        << ", perms=" << perms << ")");

    if(!vpe->objcaps().unused(dst))
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid cap");
    if(size == 0 || (size & m3::KIF::Perm::RWX) || perms == 0 || (perms & ~(m3::KIF::Perm::RWX)))
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Size or permissions invalid");

    MainMemory &mem = MainMemory::get();
    MainMemory::Allocation alloc = addr == static_cast<uintptr_t>(-1) ? mem.allocate(size, PAGE_SIZE)
                                                                      : mem.allocate_at(addr, size);
    if(!alloc)
        SYS_ERROR(vpe, msg, m3::Errors::OUT_OF_MEM, "Not enough memory");

    // TODO if addr was 0, we don't want to free it on revoke
    vpe->objcaps().set(dst, new MGateCapability(&vpe->objcaps(), dst,
        alloc.pe(), VPE::INVALID_ID, alloc.addr, alloc.size, perms));

    reply_result(vpe, msg, m3::Errors::NONE);
}

void SyscallHandler::createvpe(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_createvpe();

    auto req = get_message<m3::KIF::Syscall::CreateVPE>(msg);
    capsel_t dst = req->dst_sel;
    capsel_t mgate = req->mgate_sel;
    capsel_t sgate = req->sgate_sel;
    m3::PEDesc::value_t pe = req->pe;
    epid_t ep = req->ep;
    bool tmuxable = req->muxable;
    m3::String name(req->name, m3::Math::min(static_cast<size_t>(req->namelen), sizeof(req->name)));

    LOG_SYS(vpe, ": syscall::createvpe", "(dst=" << dst << ", mgate=" << mgate
        << ", sgate=" << sgate << ", name=" << name
        << ", pe=" << static_cast<int>(m3::PEDesc(pe).type())
        << ", pfep=" << ep << ", tmuxable=" << tmuxable << ")");

    if(!vpe->objcaps().unused(dst) || !vpe->objcaps().unused(mgate))
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid VPE or memory cap");

    // if it has a pager, we need an sgate cap
    SGateCapability *sgatecap = nullptr;
    if(sgate != m3::KIF::INV_SEL) {
        sgatecap = static_cast<SGateCapability*>(vpe->objcaps().get(sgate, Capability::SGATE));
        if(sgatecap == nullptr)
            SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid cap(s)");
    }
    else
        ep = VPE::INVALID_EP;

    // create VPE
    VPE *nvpe = VPEManager::get().create(m3::Util::move(name), m3::PEDesc(pe), ep, sgate, tmuxable);
    if(nvpe == nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::NO_FREE_PE, "No free and suitable PE found");

    // childs of daemons are daemons
    if(vpe->is_daemon())
        nvpe->make_daemon();

    // inherit VPE and mem caps to the parent
    vpe->objcaps().obtain(dst, nvpe->objcaps().get(0));
    vpe->objcaps().obtain(mgate, nvpe->objcaps().get(1));

    // delegate pf gate to the new VPE
    if(sgate != m3::KIF::INV_SEL)
        nvpe->objcaps().obtain(sgate, sgatecap);

    m3::KIF::Syscall::CreateVPEReply reply;
    reply.error = m3::Errors::NONE;
    reply.pe = Platform::pe(nvpe->pe()).value();
    reply_msg(vpe, msg, &reply, sizeof(reply));
}

void SyscallHandler::createmap(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_createmap();

#if defined(__gem5__)
    auto req = get_message<m3::KIF::Syscall::CreateMap>(msg);
    capsel_t dst = req->dst_sel;
    capsel_t mgate = req->mgate_sel;
    capsel_t tvpe = req->vpe_sel;
    capsel_t first = req->first;
    capsel_t pages = req->pages;
    int perms = req->perms;

    LOG_SYS(vpe, ": syscall::createmap", "(dst=" << dst << ", tvpe=" << tvpe << ", mgate=" << mgate
        << ", first=" << first << ", pages=" << pages << ", perms=" << perms << ")");

    auto vpecap = static_cast<VPECapability*>(vpe->objcaps().get(tvpe, Capability::VIRTPE));
    if(vpecap == nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "VPE capability is invalid");
    auto mgatecap = static_cast<MGateCapability*>(vpe->objcaps().get(mgate, Capability::MGATE));
    if(mgatecap == nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Memory capability is invalid");

    if((mgatecap->obj->addr & PAGE_MASK) || (mgatecap->obj->size & PAGE_MASK))
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Memory capability is not page aligned");
    if(perms & ~mgatecap->obj->perms)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid permissions");

    size_t total = mgatecap->obj->size >> PAGE_BITS;
    if(first >= total || first + pages <= first || first + pages > total)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Region of memory capability is invalid");

    gaddr_t phys = m3::DTU::build_gaddr(mgatecap->obj->pe, mgatecap->obj->addr + PAGE_SIZE * first);
    CapTable &mcaps = vpecap->obj->mapcaps();

    auto mapcap = static_cast<MapCapability*>(mcaps.get(dst, Capability::MAP));
    if(mapcap == nullptr) {
        MapCapability *mapcap = new MapCapability(&mcaps, dst, phys, pages, perms);
        mcaps.inherit(mgatecap, mapcap);
        mcaps.set(dst, mapcap);
    }
    else {
        if(mapcap->length != pages) {
            SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS,
                "Map capability exists with different number of pages ("
                    << mapcap->length << " vs. " << pages << ")");
        }
        mapcap->remap(phys, perms);
    }
#endif

    reply_result(vpe, msg, m3::Errors::NONE);
}

void SyscallHandler::activate(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_activate();

    auto *req = get_message<m3::KIF::Syscall::Activate>(msg);
    capsel_t tvpe = req->vpe_sel;
    capsel_t gate = req->gate_sel;
    epid_t ep = req->ep;
    uintptr_t addr = req->addr;

    LOG_SYS(vpe, ": syscall::activate", "(vpe=" << tvpe << ", gate=" << gate << ", ep=" << ep
        << ", addr=#" << m3::fmt(addr, "x") << ")");

    auto vpecap = static_cast<VPECapability*>(vpe->objcaps().get(tvpe, Capability::VIRTPE));
    if(vpecap == nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "VPE capability is invalid");

    if(ep <= m3::DTU::UPCALL_REP || ep >= EP_COUNT)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalidate EP");

    Capability *gatecap = nullptr;
    if(gate != m3::KIF::INV_SEL) {
        gatecap = vpe->objcaps().get(gate, Capability::SGATE | Capability::MGATE | Capability::RGATE);
        if(gatecap == nullptr)
            SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalidate capability");
    }

    Capability *oldcap = vpecap->obj->ep_cap(ep);
    if(oldcap) {
        if(oldcap->type == Capability::RGATE) {
            auto rgatecap = static_cast<RGateCapability*>(gatecap);
            rgatecap->obj->addr = 0;
        }

        if(!vpecap->obj->invalidate_ep(ep, true))
            SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Unable to invalidate EP");
    }

    if(gatecap) {
        epid_t oldep = vpecap->obj->cap_ep(gatecap);
        if(oldep && oldep != ep)
            SYS_ERROR(vpe, msg, m3::Errors::EXISTS, "Capability already in use");

        if(gatecap->type == Capability::MGATE) {
            auto mgatecap = static_cast<MGateCapability*>(gatecap);
            vpecap->obj->config_mem_ep(ep, *mgatecap->obj);
        }
        else if(gatecap->type == Capability::SGATE) {
            auto sgatecap = static_cast<SGateCapability*>(gatecap);

            if(!sgatecap->obj->rgate->activated()) {
                LOG_SYS(vpe, ": syscall::activate",
                    ": waiting for rgate " << &sgatecap->obj->rgate);

                vpe->start_wait();
                m3::ThreadManager::get().wait_for(reinterpret_cast<event_t>(&*sgatecap->obj->rgate));
                vpe->stop_wait();

                LOG_SYS(vpe, ": syscall::activate-cont",
                    ": rgate " << &sgatecap->obj->rgate << " activated");
            }

            vpecap->obj->config_snd_ep(ep, *sgatecap->obj);
        }
        else {
            auto rgatecap = static_cast<RGateCapability*>(gatecap);
            if(rgatecap->obj->activated())
                SYS_ERROR(vpe, msg, m3::Errors::EXISTS, "RGate already activated");

            rgatecap->obj->vpe = vpecap->obj->id();
            rgatecap->obj->addr = addr;
            rgatecap->obj->ep = ep;

            m3::Errors::Code res = vpecap->obj->config_rcv_ep(ep, *rgatecap->obj);
            if(res != m3::Errors::NONE) {
                rgatecap->obj->addr = 0;
                SYS_ERROR(vpe, msg, res, "Unable to invalidate EP");
            }
        }
    }
    else
        vpecap->obj->invalidate_ep(ep);

    vpecap->obj->ep_cap(ep, gatecap);

    reply_result(vpe, msg, m3::Errors::NONE);
}

void SyscallHandler::vpectrl(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_vpectrl();

    auto req = get_message<m3::KIF::Syscall::VPECtrl>(msg);
    capsel_t tvpe = req->vpe_sel;
    m3::KIF::Syscall::VPEOp op = static_cast<m3::KIF::Syscall::VPEOp>(req->op);
    word_t arg = req->arg;

    static const char *opnames[] = {
        "INIT", "START", "YIELD", "STOP", "WAIT"
    };

    LOG_SYS(vpe, ": syscall::vpectrl", "(vpe=" << tvpe
        << ", op=" << (static_cast<size_t>(op) < ARRAY_SIZE(opnames) ? opnames[op] : "??")
        << ", arg=" << m3::fmt(arg, "#x") << ")");

    auto vpecap = static_cast<VPECapability*>(vpe->objcaps().get(tvpe, Capability::VIRTPE));
    if(vpecap == nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid VPE cap");

    switch(op) {
        case m3::KIF::Syscall::VCTRL_INIT:
            vpecap->obj->set_ep_addr(arg);
            break;

        case m3::KIF::Syscall::VCTRL_START:
            if(vpe == &*vpecap->obj)
                SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "VPE can't start itself");
            vpecap->obj->start_app(static_cast<int>(arg));
            break;

        case m3::KIF::Syscall::VCTRL_YIELD:
            if(vpe != &*vpecap->obj)
                SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Yield for other VPEs is prohibited");

            // reply before the context switch
            reply_result(vpe, msg, m3::Errors::NONE);
            vpecap->obj->yield();
            return;

        case m3::KIF::Syscall::VCTRL_STOP: {
            bool self = vpe == &*vpecap->obj;
            vpecap->obj->stop_app(static_cast<int>(arg), self);
            if(self) {
                // if we don't reply, we need to mark it read manually
                m3::DTU::get().mark_read(ep(), reinterpret_cast<uintptr_t>(msg));
                return;
            }
            break;
        }

        case m3::KIF::Syscall::VCTRL_WAIT:
            if(vpe == &*vpecap->obj)
                SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "VPE can't wait for itself");

            m3::KIF::Syscall::VPECtrlReply reply;
            reply.error = m3::Errors::NONE;

            if(vpecap->obj->has_app()) {
                vpe->start_wait();
                vpecap->obj->wait_for_exit();
                vpe->stop_wait();

                LOG_SYS(vpe, ": syscall::vpectrl-cont", "(exitcode=" << vpecap->obj->exitcode() << ")");
            }

            reply.exitcode = static_cast<xfer_t>(vpecap->obj->exitcode());
            reply_msg(vpe, msg, &reply, sizeof(reply));
            return;
    }

    reply_result(vpe, msg, m3::Errors::NONE);
}

void SyscallHandler::derivemem(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_derivemem();

    auto req = get_message<m3::KIF::Syscall::DeriveMem>(msg);
    capsel_t dst = req->dst_sel;
    capsel_t src = req->src_sel;
    size_t offset = req->offset;
    size_t size = req->size;
    int perms = req->perms;

    LOG_SYS(vpe, ": syscall::derivemem", "(src=" << src << ", dst=" << dst
        << ", size=" << size << ", off=" << offset << ", perms=" << perms << ")");

    auto srccap = static_cast<MGateCapability*>(vpe->objcaps().get(src, Capability::MGATE));
    if(srccap == nullptr || !vpe->objcaps().unused(dst))
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid cap(s)");

    if(offset + size < offset || offset + size > srccap->obj->size || size == 0 ||
            (perms & ~(m3::KIF::Perm::RWX)))
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid args");

    auto dercap = static_cast<MGateCapability*>(vpe->objcaps().obtain(dst, srccap));
    dercap->obj = m3::Reference<MGateObject>(new MGateObject(
        srccap->obj->pe,
        srccap->obj->vpe,
        srccap->obj->addr + offset,
        size,
        perms & srccap->obj->perms
    ));
    dercap->obj->derived = true;

    reply_result(vpe, msg, m3::Errors::NONE);
}

void SyscallHandler::delegate(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_delegate();
    exchange_over_sess(vpe, msg, false);
}

void SyscallHandler::obtain(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_obtain();
    exchange_over_sess(vpe, msg, true);
}

void SyscallHandler::exchange(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_exchange();

    auto req = get_message<m3::KIF::Syscall::Exchange>(msg);
    capsel_t tvpe = req->vpe_sel;
    m3::KIF::CapRngDesc own(req->own_crd);
    m3::KIF::CapRngDesc other(own.type(), req->other_sel, own.count());
    bool obtain = req->obtain;

    LOG_SYS(vpe, ": syscall::exchange", "(vpe=" << tvpe << ", own=" << own
        << ", other=" << other << ", obtain=" << obtain << ")");

    auto vpecap = static_cast<VPECapability*>(vpe->objcaps().get(tvpe, Capability::VIRTPE));
    if(vpecap == nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid VPE cap");

    VPE *t1 = obtain ? &*vpecap->obj : vpe;
    VPE *t2 = obtain ? vpe : &*vpecap->obj;
    m3::Errors::Code res = do_exchange(t1, t2, own, other, obtain);

    reply_result(vpe, msg, res);
}

void SyscallHandler::revoke(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_revoke();

    auto *req = get_message<m3::KIF::Syscall::Revoke>(msg);
    capsel_t tvpe = req->vpe_sel;
    m3::KIF::CapRngDesc crd(req->crd);
    bool own = req->own;

    LOG_SYS(vpe, ": syscall::revoke", "(vpe=" << tvpe << ", crd=" << crd << ", own=" << own << ")");

    auto vpecap = static_cast<VPECapability*>(vpe->objcaps().get(tvpe, Capability::VIRTPE));
    if(vpecap == nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid cap");

    if(crd.type() == m3::KIF::CapRngDesc::OBJ && crd.start() < 2)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Cap 0 and 1 are not revokeable");

    if(crd.type() == m3::KIF::CapRngDesc::OBJ)
        vpecap->obj->objcaps().revoke(crd, own);
    else
        vpecap->obj->mapcaps().revoke(crd, own);

    reply_result(vpe, msg, m3::Errors::NONE);
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
        capsel_t srcsel = srcrng.start() + i;
        capsel_t dstsel = dstrng.start() + i;
        Capability *srccap = srctab.get(srcsel);
        assert(dsttab.get(dstsel) == nullptr);
        dsttab.obtain(dstsel, srccap);
    }
    return m3::Errors::NONE;
}

void SyscallHandler::exchange_over_sess(VPE *vpe, const m3::DTU::Message *msg, bool obtain) {
    auto req = get_message<m3::KIF::Syscall::ExchangeSess>(msg);
    capsel_t sess = req->sess_sel;
    m3::KIF::CapRngDesc crd(req->crd);

    LOG_SYS(vpe, (obtain ? ": syscall::obtain" : ": syscall::delegate"),
            "(sess=" << sess << ", crd=" << crd << ")");

    auto sesscap = static_cast<SessCapability*>(vpe->objcaps().get(sess, Capability::SESS));
    if(sesscap == nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid session-cap");

    // we can't be sure that the session will still exist when we receive the reply
    m3::Reference<Service> rsrv(sesscap->obj->srv);

    vpe->start_wait();
    while(rsrv->vpe().state() != VPE::RUNNING) {
        if(!rsrv->vpe().resume()) {
            vpe->stop_wait();
            SYS_ERROR(vpe, msg, m3::Errors::VPE_GONE, "VPE does no longer exist");
        }
    }

    m3::KIF::Service::Exchange smsg;
    smsg.opcode = obtain ? m3::KIF::Service::OBTAIN : m3::KIF::Service::DELEGATE;
    smsg.sess = sesscap->obj->ident;
    smsg.data.caps = crd.count();
    smsg.data.argcount = req->argcount;
    for(size_t i = 0; i < req->argcount; ++i)
        smsg.data.args[i] = req->args[i];

    const m3::DTU::Message *srvreply = rsrv->send_receive(&smsg, sizeof(smsg), false);
    vpe->stop_wait();

    if(srvreply == nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::RECV_GONE, "Service unreachable");

    auto *reply = reinterpret_cast<const m3::KIF::Service::ExchangeReply*>(srvreply->data);

    m3::Errors::Code res = static_cast<m3::Errors::Code>(reply->error);

    LOG_SYS(vpe, (obtain ? ": syscall::obtain-cont" : ": syscall::delegate-cont"), "(res=" << res << ")");

    if(res != m3::Errors::NONE)
        LOG_ERROR(vpe, res, "Server denied cap-transfer");
    else {
        m3::KIF::CapRngDesc srvcaps(reply->data.caps);
        res = do_exchange(vpe, &rsrv->vpe(), crd, srvcaps, obtain);
    }

    m3::KIF::Syscall::ExchangeSessReply kreply;
    kreply.error = res;
    kreply.argcount = 0;
    if(res == m3::Errors::NONE) {
        kreply.argcount = m3::Math::min(static_cast<size_t>(reply->data.argcount),
            ARRAY_SIZE(kreply.args));
        for(size_t i = 0; i < kreply.argcount; ++i)
            kreply.args[i] = reply->data.args[i];
    }
    reply_msg(vpe, msg, &kreply, sizeof(kreply));
}

m3::Errors::Code SyscallHandler::wait_for(const char *name, VPE &tvpe, VPE *cur) {
    m3::Errors::Code res = m3::Errors::NONE;
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

    LOG_SYS(cur, name, "-cont: VPE " << tvpe.id() << " ready");
    return res;
}

void SyscallHandler::forwardmsg(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_forwardmsg();

#if !defined(__gem5__)
    reply_result(vpe, msg, m3::Errors::NOT_SUP);
#else
    auto *req = get_message<m3::KIF::Syscall::ForwardMsg>(msg);
    capsel_t sgate = req->sgate_sel;
    capsel_t rgate = req->rgate_sel;
    size_t len = req->len;
    label_t rlabel = req->rlabel;
    word_t event = req->event;

    LOG_SYS(vpe, ": syscall::forwardmsg", "(sgate=" << sgate << ", rgate=" << rgate
        << ", len=" << len << ", rlabel=" << m3::fmt(rlabel, "0x")
        << ", event=" << m3::fmt(event, "0x") << ")");

    auto sgatecap = static_cast<SGateCapability*>(vpe->objcaps().get(sgate, Capability::SGATE));
    if(sgatecap == nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid msg cap");
    epid_t rep = m3::DTU::DEF_REP;
    if(rgate != m3::KIF::INV_SEL) {
        auto rgatecap = static_cast<RGateCapability*>(vpe->objcaps().get(rgate, Capability::RGATE));
        if(rgatecap == nullptr)
            SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid rgate cap");
        rep = rgatecap->obj->ep;
    }

    epid_t ep = vpe->cap_ep(sgatecap);
    if(ep == 0)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Msg cap is not activated");
    if(!vpe->can_forward_msg(ep))
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Send did not fail");

    // TODO if we do that asynchronously, we need to buffer the message somewhere, because the
    // VPE might want to do other system calls in the meantime. probably, VPEs need to allocate
    // the memory beforehand and the kernel will simply use it afterwards.
    VPE &tvpe = VPEManager::get().vpe(sgatecap->obj->rgate->vpe);
    bool async = tvpe.state() != VPE::RUNNING && event;
    if(async)
        reply_result(vpe, msg, m3::Errors::UPCALL_REPLY);

    m3::Errors::Code res = wait_for(": syscall::forwardmsg", tvpe, vpe);

    if(res == m3::Errors::NONE) {
        // re-enable the EP first, because the reply to the sent message below might otherwise
        // pass credits back BEFORE we overwrote the EP
        vpe->forward_msg(ep, tvpe.pe(), tvpe.id());

        uint64_t sender = vpe->pe() | (vpe->id() << 8) | (ep << 24) | (static_cast<uint64_t>(rep) << 32);
        DTU::get().send_to(tvpe.desc(), sgatecap->obj->rgate->ep, sgatecap->obj->label, req->msg,
            req->len, req->rlabel, rep, sender);
    }
    else
        LOG_ERROR(vpe, res, "forwardmsg failed");

    if(async)
        vpe->upcall_notify(res, event);
    else
        reply_result(vpe, msg, res);
#endif
}

void SyscallHandler::forwardmem(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_forwardmem();

#if !defined(__gem5__)
    reply_result(vpe, msg, m3::Errors::NOT_SUP);
#else
    auto *req = get_message<m3::KIF::Syscall::ForwardMem>(msg);
    capsel_t mgate = req->mgate_sel;
    size_t len = m3::Math::min(sizeof(req->data), static_cast<size_t>(req->len));
    size_t offset = req->offset;
    uint flags = req->flags;
    word_t event = req->event;

    LOG_SYS(vpe, ": syscall::forwardmem", "(mgate=" << mgate << ", len=" << len
        << ", offset=" << offset << ", flags=" << m3::fmt(flags, "0x")
        << ", event=" << m3::fmt(event, "0x") << ")");

    auto mgatecap = static_cast<MGateCapability*>(vpe->objcaps().get(mgate, Capability::MGATE));
    if(mgatecap == nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid memory cap");

    epid_t ep = vpe->cap_ep(mgatecap);
    if(ep == 0)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Mem cap is not activated");

    if(mgatecap->obj->addr + offset < offset || offset >= mgatecap->obj->size ||
       offset + len < offset || offset + len > mgatecap->obj->size)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid offset/length");
    if((flags & m3::KIF::Syscall::ForwardMem::WRITE) && !(mgatecap->obj->perms & m3::KIF::Perm::W))
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "No write permission");
    if((~flags & m3::KIF::Syscall::ForwardMem::WRITE) && !(mgatecap->obj->perms & m3::KIF::Perm::R))
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "No read permission");

    VPE &tvpe = VPEManager::get().vpe(mgatecap->obj->vpe);
    bool async = tvpe.state() != VPE::RUNNING && event;
    if(async)
        reply_result(vpe, msg, m3::Errors::UPCALL_REPLY);

    m3::Errors::Code res = wait_for(": syscall::forwardmem", tvpe, vpe);

    m3::KIF::Syscall::ForwardMemReply reply;
    reply.error = res;

    if(res == m3::Errors::NONE) {
        if(flags & m3::KIF::Syscall::ForwardMem::WRITE)
            res = DTU::get().try_write_mem(tvpe.desc(), mgatecap->obj->addr + offset, req->data, len);
        else
            res = DTU::get().try_read_mem(tvpe.desc(), mgatecap->obj->addr + offset, reply.data, len);

        vpe->forward_mem(ep, tvpe.pe());
    }
    if(res != m3::Errors::NONE && res != m3::Errors::PAGEFAULT)
        LOG_ERROR(vpe, res, "forwardmem failed");

    if(async)
        vpe->upcall_notify(res, event);
    else
        reply_result(vpe, msg, res);
#endif
}

void SyscallHandler::forwardreply(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_forwardreply();

#if !defined(__gem5__)
    reply_result(vpe, msg, m3::Errors::NOT_SUP);
#else
    auto *req = get_message<m3::KIF::Syscall::ForwardReply>(msg);
    capsel_t rgate = req->rgate_sel;
    uintptr_t msgaddr = req->msgaddr;
    size_t len = m3::Math::min(sizeof(req->msg), static_cast<size_t>(req->len));
    word_t event = req->event;

    LOG_SYS(vpe, ": syscall::forwardreply", "(rgate=" << rgate << ", len=" << len
        << ", msgaddr=" << (void*)msgaddr << ", event=" << m3::fmt(event, "0x") << ")");

    auto rgatecap = static_cast<RGateCapability*>(vpe->objcaps().get(rgate, Capability::RGATE));
    if(rgatecap == nullptr)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "Invalid rgate cap");

    epid_t ep = vpe->cap_ep(rgatecap);
    if(ep == 0)
        SYS_ERROR(vpe, msg, m3::Errors::INV_ARGS, "RGate cap is not activated");

    // ensure that the VPE is running, because we need to access it's address space
    while(vpe->state() != VPE::RUNNING) {
        if(!vpe->resume())
            return;
    }

    m3::DTU::Header head;
    m3::Errors::Code res = DTU::get().get_header(vpe->desc(), &*rgatecap->obj, msgaddr, head);
    if(res != m3::Errors::NONE || !(head.flags & m3::DTU::Header::FL_REPLY_FAILED))
        SYS_ERROR(vpe, msg, res, "Invalid arguments");

    // we have read the header. thus, we can mark the message as read (and have to do that before
    // doing the reply)
    DTU::get().mark_read_remote(vpe->desc(), ep, msgaddr);

    VPE &tvpe = VPEManager::get().vpe(head.senderVpeId);
    bool async = tvpe.state() != VPE::RUNNING && event;
    if(async)
        reply_result(vpe, msg, m3::Errors::UPCALL_REPLY);

    res = wait_for(": syscall::forwardreply", tvpe, vpe);

    if(res == m3::Errors::NONE) {
        uint64_t sender = vpe->pe() | (vpe->id() << 8) |
                        (static_cast<uint64_t>(head.senderEp) << 32) |
                        (static_cast<uint64_t>(1) << 40);
        DTU::get().reply_to(tvpe.desc(), head.replyEp, head.replylabel, req->msg, len, sender);

        while(vpe->state() != VPE::RUNNING) {
            if(!vpe->resume())
                return;
        }

        head.flags &= ~(m3::DTU::Header::FL_REPLY_FAILED | m3::DTU::Header::FL_REPLY_ENABLED);
        res = DTU::get().set_header(vpe->desc(), &*rgatecap->obj, msgaddr, head);
    }
    if(res != m3::Errors::NONE)
        LOG_ERROR(vpe, res, "forwardreply failed");

    if(async)
        vpe->upcall_notify(res, event);
    else
        reply_result(vpe, msg, res);
#endif
}

void SyscallHandler::noop(VPE *vpe, const m3::DTU::Message *msg) {
    EVENT_TRACER_Syscall_noop();
    LOG_SYS(vpe, ": syscall::noop", "()");

    reply_result(vpe, msg, m3::Errors::NONE);
}

}
