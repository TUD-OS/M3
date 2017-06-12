/**
 * Copyright (C) 2016, René Küttner <rene.kuettner@.tu-dresden.de>
 * Economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <base/DTU.h>
#include <base/Env.h>
#include <base/Exceptions.h>
#include <base/KIF.h>
#include <base/RCTMux.h>

#include "../../RCTMux.h"
#include "Exceptions.h"

EXTERN_C void _save(void *state);
EXTERN_C void *_restore();
EXTERN_C void _signal();

namespace RCTMux {

static volatile size_t req_count = 0;
static bool inpf = false;
static m3::DTU::reg_t reqs[4];
static m3::DTU::reg_t cmdregs[2] = {0, 0};

// store message in static data to ensure that we don't pagefault
alignas(64) static uint64_t pfmsg[3] = {m3::KIF::Syscall::PAGEFAULT, 0, 0};

struct PFHandler {

static uintptr_t get_pte_addr(uintptr_t virt, int level) {
    static uintptr_t recMask =
        (static_cast<uintptr_t>(m3::DTU::PTE_REC_IDX) << (PAGE_BITS + m3::DTU::LEVEL_BITS * 3)) |
        (static_cast<uintptr_t>(m3::DTU::PTE_REC_IDX) << (PAGE_BITS + m3::DTU::LEVEL_BITS * 2)) |
        (static_cast<uintptr_t>(m3::DTU::PTE_REC_IDX) << (PAGE_BITS + m3::DTU::LEVEL_BITS * 1)) |
        (static_cast<uintptr_t>(m3::DTU::PTE_REC_IDX) << (PAGE_BITS + m3::DTU::LEVEL_BITS * 0));

    // at first, just shift it accordingly.
    virt >>= PAGE_BITS + level * m3::DTU::LEVEL_BITS;
    virt <<= m3::DTU::PTE_BITS;

    // now put in one PTE_REC_IDX's for each loop that we need to take
    int shift = level + 1;
    uintptr_t remMask = (1UL << (PAGE_BITS + m3::DTU::LEVEL_BITS * (m3::DTU::LEVEL_CNT - shift))) - 1;
    virt |= recMask & ~remMask;

    // finally, make sure that we stay within the bounds for virtual addresses
    // this is because of recMask, that might actually have too many of those.
    virt &= (1UL << (m3::DTU::LEVEL_CNT * m3::DTU::LEVEL_BITS + PAGE_BITS)) - 1;
    return virt;
}

static m3::DTU::pte_t to_dtu_pte(uint64_t pte) {
    m3::DTU::pte_t res = pte & ~static_cast<m3::DTU::pte_t>(PAGE_MASK);
    if(pte & 0x1) // present
        res |= m3::DTU::PTE_R;
    if(pte & 0x2) // writable
        res |= m3::DTU::PTE_W;
    if(pte & 0x4) // not-supervisor
        res |= m3::DTU::PTE_I;
    return res;
}

static void resume_cmd() {
    static_assert(static_cast<int>(m3::DTU::CmdOpCode::IDLE) == 0, "IDLE not 0");

    if(cmdregs[0] != 0) {
        // if there was a command, restore DATA register and retry command
        m3::DTU::get().write_reg(m3::DTU::CmdRegs::DATA, cmdregs[1]);
        m3::CPU::compiler_barrier();
        m3::DTU::get().retry(cmdregs[0]);
        cmdregs[0] = 0;
    }
}

static bool handle_pf(m3::DTU::reg_t xlate_req, uintptr_t virt, uint perm) {
    m3::DTU &dtu = m3::DTU::get();

    if(inpf) {
        for(size_t i = 0; i < ARRAY_SIZE(reqs); ++i) {
            if(reqs[i] == 0) {
                reqs[i] = xlate_req;
                break;
            }
        }
        req_count++;
        return false;
    }

    inpf = true;

    // abort the current command, if there is any
    dtu.abort(m3::DTU::ABORT_CMD, cmdregs + 0);
    // if a command was being executed, save the DATA register, because we'll overwrite it
    if(cmdregs[0] != static_cast<m3::DTU::reg_t>(m3::DTU::CmdOpCode::IDLE))
        cmdregs[1] = dtu.read_reg(m3::DTU::CmdRegs::DATA);

    // allow other translation requests in the meantime
    asm volatile ("sti" : : : "memory");

retry:
    m3::DTU::reg_t pfeps = dtu.get_pfep();
    epid_t sep = pfeps & 0xFF;
    epid_t rep = pfeps >> 8;
    if(!dtu.ep_valid(sep))
        sep = m3::DTU::SYSC_SEP;

    pfmsg[1] = virt;
    pfmsg[2] = perm;

    // send PF to pager/kernel
    m3::Errors::Code res = m3::Errors::MISS_CREDITS;
    while(res == m3::Errors::MISS_CREDITS) {
        res = dtu.send(sep, pfmsg, sizeof(pfmsg), 0, rep);
        dtu.sleep();
    }

    // handle reply
    m3::DTU::Message *reply = dtu.fetch_msg(rep);
    while(!reply) {
        dtu.sleep();
        reply = dtu.fetch_msg(rep);
    }
    dtu.mark_read(rep, reinterpret_cast<size_t>(reply));
    if(sep == m3::DTU::SYSC_SEP)
        goto retry;

    asm volatile ("cli" : : : "memory");

    inpf = false;
    return true;
}

static bool handle_xlate(m3::DTU::reg_t xlate_req) {
    m3::DTU &dtu = m3::DTU::get();

    uintptr_t virt = xlate_req & ~PAGE_MASK;
    uint perm = xlate_req & 0xF;

    // translate to physical
    m3::DTU::pte_t pte;
    // special case for root pt
    if((virt & 0xFFFFFFFFF000) == 0x080402010000) {
        asm volatile ("mov %%cr3, %0" : "=r"(pte));
        pte = to_dtu_pte(pte | 0x3);
    }
    // in the PTE area, we can assume that all upper level PTEs are present
    else if((virt & 0xFFF000000000) == 0x080000000000)
        pte = to_dtu_pte(*reinterpret_cast<uint64_t*>(get_pte_addr(virt, 0)));
    // otherwise, walk through all levels
    else {
        for(int lvl = 3; lvl >= 0; lvl--) {
            pte = to_dtu_pte(*reinterpret_cast<uint64_t*>(get_pte_addr(virt, lvl)));
            if(~(pte & 0xF) & perm)
                break;
        }
    }

    bool pf = false;
    if(~(pte & 0xF) & perm) {
        // the first xfer buffer can't raise pagefaults
        if((xlate_req & 0x70) == 0) {
            // the xlate response has to be non-zero, but have no permission bits set
            pte = PAGE_SIZE;
        }
        else {
            if(!handle_pf(xlate_req, virt, perm))
                return false;

            // read PTE again
            pte = to_dtu_pte(*reinterpret_cast<uint64_t*>(get_pte_addr(virt, 0)));
            pf = true;
        }
    }

    // tell DTU the result; but only if the command has not been aborted
    // TODO that means that aborted commands cause another TLB miss in the DTU, which can then
    // (hopefully) be handled with a simple PT walk. we could improve that by setting the TLB entry
    // right away without continuing the transfer (because that's aborted)
    if(!pf || cmdregs[0] == 0)
        dtu.set_xlate_resp(pte | (xlate_req & 0x70));

    if(pf)
        resume_cmd();

    return pf;
}

static void handle_master_req(m3::DTU::reg_t mst_req) {
    m3::DTU &dtu = m3::DTU::get();

    uint cmd = mst_req & 0x1;
    mst_req &= ~static_cast<m3::DTU::reg_t>(0x1);

    switch(cmd) {
        case m3::DTU::MstReqOpCode::SET_ROOTPT:
            // TODO workaround to clear irqPending in DTU
            dtu.set_xlate_resp(0);

            // ack before jumping away
            dtu.set_master_req(0);

            asm volatile (
                "mov %0, %%cr3;"
                "jmp _start;"
                : : "r"(mst_req)
            );
            UNREACHED;
            break;

        case m3::DTU::MstReqOpCode::INV_PAGE:
            asm volatile ("invlpg (%0)" : : "r" (mst_req));
            break;
    }

    // ack to kernel
    dtu.set_master_req(0);
}

static void *dtu_irq(m3::Exceptions::State *state) {
    m3::DTU &dtu = m3::DTU::get();

    // translation request from DTU?
    m3::DTU::reg_t xlate_req = dtu.get_xlate_req();
    if(xlate_req) {
        // acknowledge the translation
        dtu.set_xlate_req(0);

        if(handle_xlate(xlate_req)) {
            // handle other requests that pagefaulted in the meantime
            while(req_count > 0) {
                for(size_t i = 0; i < ARRAY_SIZE(reqs); ++i) {
                    xlate_req = reqs[i];
                    if(xlate_req) {
                        req_count--;
                        reqs[i] = 0;
                        handle_xlate(xlate_req);
                    }
                }
            }
        }
    }

    // paging request from kernel?
    m3::DTU::reg_t mst_req = dtu.get_master_req();
    if(mst_req != 0)
        handle_master_req(mst_req);

    if(!inpf) {
        // context switch request from kernel?
        uint64_t flags = flags_get();

        if(flags & m3::RESTORE)
            return _restore();

        if(flags & m3::STORE) {
            _save(state);

            // stay here until reset
            asm volatile (
                "hlt;"
                "1: jmp 1b;"
            );
            UNREACHED;
        }

        if(flags & m3::WAITING)
            _signal();
    }

    return state;
}

static void *mmu_pf(m3::Exceptions::State *state) {
    uintptr_t cr2;
    asm volatile ("mov %%cr2, %0" : "=r"(cr2));
    if(!handle_pf(0, cr2, to_dtu_pte(state->errorCode & 0x7)))
        PRINTSTR("RCTMux: nested pagefault\n");
    resume_cmd();
    return state;
}

};

void init() {
    Exceptions::init();
    Exceptions::get_table()[14] = PFHandler::mmu_pf;
    Exceptions::get_table()[64] = PFHandler::dtu_irq;
}

void *init_state() {
    m3::Env *senv = m3::env();
    senv->isrs = reinterpret_cast<uintptr_t>(Exceptions::get_table());

    // put state at the stack top
    m3::Exceptions::State *state = reinterpret_cast<m3::Exceptions::State*>(senv->sp) - 1;

    // init State
    state->rax = 0xDEADBEEF;    // tell crt0 that we've set the SP
    state->rbx = 0;
    state->rcx = 0;
    state->rdx = 0;
    state->rsi = 0;
    state->rdi = 0;
    state->r8  = 0;
    state->r9  = 0;
    state->r10 = 0;
    state->r11 = 0;
    state->r12 = 0;
    state->r13 = 0;
    state->r14 = 0;
    state->r15 = 0;

    state->cs  = (Exceptions::SEG_UCODE << 3) | 3;
    state->ss  = (Exceptions::SEG_UDATA << 3) | 3;
    state->rip = senv->entry;
    state->rsp = reinterpret_cast<uintptr_t>(state);
    state->rbp = 0;
    state->rflags = 0x200;  // enable interrupts

    return state;
}

} /* namespace RCTMux */
