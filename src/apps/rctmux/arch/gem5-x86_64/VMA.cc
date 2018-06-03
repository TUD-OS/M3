/**
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
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
#include "../../Print.h"
#include "Exceptions.h"
#include "VMA.h"

namespace RCTMux {

enum class PfState {
    IDLE,
    SEND_PAGER,
    WAIT_PAGER,
    SEND_YIELD,
    SEND_KERNEL,
    WAIT_KERNEL,
};

static volatile bool inpf = false;
static PfState pfstate = PfState::IDLE;

static volatile size_t req_count = 0;
static volatile bool ctxsw = false;
static m3::DTU::reg_t cmdXferBuf = static_cast<m3::DTU::reg_t>(-1);
static m3::DTU::reg_t reqs[4];
static m3::DTU::reg_t cmdregs[2] = {0, 0};

// store messages in static data to ensure that we don't pagefault
alignas(64) static uint64_t pfmsg[3] = {
    m3::KIF::Syscall::PAGEFAULT, 0, 0
};
alignas(64) static uint64_t yieldmsg[4] = {
    m3::KIF::Syscall::VPE_CTRL, 0, m3::KIF::Syscall::VCTRL_YIELD, 0
};

uintptr_t VMA::get_pte_addr(uintptr_t virt, int level) {
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

m3::DTU::pte_t VMA::to_dtu_pte(uint64_t pte) {
    m3::DTU::pte_t res = pte & ~static_cast<m3::DTU::pte_t>(PAGE_MASK);
    // translate physical address to NoC address
    res = (res & ~0x0000FF0000000000ULL) | ((res & 0x0000FF0000000000ULL) << 16);
    if(pte & 0x1) // present
        res |= m3::DTU::PTE_R;
    if(pte & 0x2) // writable
        res |= m3::DTU::PTE_W;
    if(pte & 0x4) // not-supervisor
        res |= m3::DTU::PTE_I;
    if(pte & 0x80)
        res |= m3::DTU::PTE_LARGE;
    return res;
}

void VMA::resume_cmd() {
    static_assert(static_cast<int>(m3::DTU::CmdOpCode::IDLE) == 0, "IDLE not 0");

    if(cmdregs[0] != 0) {
        // if there was a command, restore DATA register and retry command
        m3::DTU::get().write_reg(m3::DTU::CmdRegs::DATA, cmdregs[1]);
        m3::CPU::compiler_barrier();
        m3::DTU::get().retry(cmdregs[0]);
        cmdregs[0] = 0;
    }
}

bool VMA::handle_pf(m3::Exceptions::State *state, m3::DTU::reg_t xlate_req, uintptr_t virt, uint perm) {
    m3::DTU &dtu = m3::DTU::get();

    if(pfstate != PfState::IDLE) {
        for(size_t i = 0; i < ARRAY_SIZE(reqs); ++i) {
            if(reqs[i] == 0) {
                reqs[i] = xlate_req;
                break;
            }
        }
        req_count++;
        return false;
    }

    // abort the current command, if there is any
    cmdXferBuf = dtu.abort(m3::DTU::ABORT_CMD, cmdregs + 0);
    // if a command was being executed, save the DATA register, because we'll overwrite it
    if(cmdregs[0] != static_cast<m3::DTU::reg_t>(m3::DTU::CmdOpCode::IDLE))
        cmdregs[1] = dtu.read_reg(m3::DTU::CmdRegs::DATA);

    // execute PF state machine
    pfmsg[1] = virt;
    pfmsg[2] = perm;
    pfstate = PfState::SEND_PAGER;
    execute_fsm(state);
    return true;
}

void VMA::execute_fsm(m3::Exceptions::State *state) {
    if(inpf || pfstate == PfState::IDLE)
        return;

    m3::DTU &dtu = m3::DTU::get();
    // get PF EPs
    m3::DTU::reg_t pfeps = dtu.get_pfep();
    epid_t sep = pfeps & 0xFF;
    epid_t rep = pfeps >> 8;
    inpf = true;

    // allow other translation requests and context switches in the meantime
    asm volatile ("sti" : : : "memory");

    m3::Errors::Code res;
    bool sleep = false;
    while(pfstate != PfState::IDLE) {
        if(sleep) {
            dtu.sleep();
            // ensure that we stay here until the IRQ was handled (we set EXT_REQ to 0).
            // otherwise we might continue execution (O3 CPU) and miss the ctxsw request, for example
            m3::CPU::memory_barrier();
            while(dtu.get_ext_req() != 0)
                ;
            sleep = false;
        }

        if(ctxsw) {
            asm volatile ("cli" : : : "memory");
            // the following call may return or not; if not, we will continue the FSM later
            ctxsw = false;
            inpf = false;
            ctxsw_protocol(state, false);
            inpf = true;
            asm volatile ("sti" : : : "memory");
        }

        switch(pfstate) {
            case PfState::SEND_PAGER: {
                res = dtu.send(sep, pfmsg, sizeof(pfmsg), 0, rep);
                // if the pager isn't available, let the kernel forward the PF msg
                if(res == m3::Errors::VPE_GONE)
                    pfstate = PfState::SEND_KERNEL;
                else {
                    panic_if(res != m3::Errors::NONE, "RCTMux: unexpected result: %u\n", res);
                    pfstate = PfState::WAIT_PAGER;
                }
                break;
            }

            case PfState::WAIT_PAGER: {
                m3::DTU::Message *reply = dtu.fetch_msg(rep);
                if(reply) {
                    dtu.mark_read(rep, reinterpret_cast<size_t>(reply));
                    pfstate = PfState::IDLE;
                }
                // notify the kernel about idling (TODO: only after the given delay)
                else if(report_time() > 0)
                    pfstate = PfState::SEND_YIELD;
                else
                    sleep = true;
                break;
            }

            case PfState::SEND_YIELD: {
                res = dtu.send(m3::DTU::SYSC_SEP, yieldmsg, sizeof(yieldmsg), 0, m3::DTU::SYSC_REP);
                if(res == m3::Errors::MISS_CREDITS) {
                    // if we have no credits (maybe the app is currently doing a syscall), don't
                    // notify the kernel about our idling.
                    sleep = true;
                    pfstate = PfState::WAIT_PAGER;
                }
                else {
                    panic_if(res != m3::Errors::NONE, "RCTMux: unexpected result: %u\n", res);
                    pfstate = PfState::WAIT_KERNEL;
                }
                break;
            }

            case PfState::SEND_KERNEL: {
                res = dtu.send(m3::DTU::SYSC_SEP, pfmsg, sizeof(pfmsg), 0, m3::DTU::SYSC_REP);
                if(res == m3::Errors::MISS_CREDITS) {
                    // if we have no credits (maybe the app is currently doing a syscall), just
                    // wait until we receive a message and try to send to the pager again
                    sleep = true;
                    pfstate = PfState::SEND_PAGER;
                }
                else {
                    panic_if(res != m3::Errors::NONE, "RCTMux: unexpected result: %u\n", res);
                    pfstate = PfState::WAIT_KERNEL;
                }
                break;
            }

            case PfState::WAIT_KERNEL: {
                m3::DTU::Message *reply = dtu.fetch_msg(m3::DTU::SYSC_REP);
                if(reply) {
                    dtu.mark_read(m3::DTU::SYSC_REP, reinterpret_cast<size_t>(reply));
                    // we still need to get the reply from the pager
                    pfstate = PfState::WAIT_PAGER;
                }
                else
                    sleep = true;
                break;
            }

            case PfState::IDLE:
                break;
        }
    }

    asm volatile ("cli" : : : "memory");
    inpf = false;

    // check for ctxsw requests again, now that interrupts are disabled again
    if(ctxsw) {
        ctxsw = false;
        ctxsw_protocol(state, false);
    }

    // if we didn't came from an interrupt, resume the command here
    if(state == nullptr)
        resume_cmd();
}

void VMA::abort_pf() {
    // forget the requests, the DTU's abort command aborts them as well
    req_count = 0;
    for(size_t i = 0; i < ARRAY_SIZE(reqs); ++i)
        reqs[i] = 0;
}

bool VMA::handle_xlate(m3::Exceptions::State *state, m3::DTU::reg_t xlate_req) {
    m3::DTU &dtu = m3::DTU::get();

    uintptr_t virt = xlate_req & ~PAGE_MASK;
    uint perm = xlate_req & 0xF;
    uint xferbuf = (xlate_req >> 5) & 0x7;

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
            if((~(pte & 0xF) & perm) || (pte & m3::DTU::PTE_LARGE))
                break;
        }
    }

    bool pf = false;
    if(~(pte & 0xF) & perm) {
        // the first xfer buffer can't raise pagefaults
        if(xferbuf == 0) {
            // the xlate response has to be non-zero, but have no permission bits set
            pte = PAGE_SIZE;
        }
        else {
            if(!handle_pf(state, xlate_req, virt, perm))
                return false;

            // read PTE again
            pte = to_dtu_pte(*reinterpret_cast<uint64_t*>(get_pte_addr(virt, 0)));
            pf = true;
        }
    }

    // tell DTU the result; but only if the command has not been aborted or the aborted command
    // did not trigger the translation (in this case, the translation is already aborted, too).
    // TODO that means that aborted commands cause another TLB miss in the DTU, which can then
    // (hopefully) be handled with a simple PT walk. we could improve that by setting the TLB entry
    // right away without continuing the transfer (because that's aborted)
    if(!pf || cmdregs[0] == 0 || cmdXferBuf != xferbuf)
        dtu.set_xlate_resp(pte | (xferbuf << 5));

    if(pf)
        resume_cmd();

    return pf;
}

void *VMA::handle_ext_req(m3::Exceptions::State *state, m3::DTU::reg_t mst_req) {
    m3::DTU &dtu = m3::DTU::get();

    uint cmd = mst_req & 0x3;
    mst_req &= ~static_cast<m3::DTU::reg_t>(0x3);

    // ack
    dtu.set_ext_req(0);

    switch(cmd) {
        case m3::DTU::ExtReqOpCode::INV_PAGE:
            asm volatile ("invlpg (%0)" : : "r" (mst_req));
            break;

        case m3::DTU::ExtReqOpCode::RCTMUX: {
            ctxsw_protocol(state, inpf);
            // if we're currently handling a PF, just remember that there was a ctxsw request and
            // call ctxsw_protocol() again as soon as we can.
            if(inpf)
                ctxsw = true;
            break;
        }
    }

    return state;
}

void *VMA::dtu_irq(m3::Exceptions::State *state) {
    m3::DTU &dtu = m3::DTU::get();

    // translation request from DTU?
    m3::DTU::reg_t xlate_req = dtu.get_xlate_req();
    if(xlate_req) {
        // acknowledge the translation
        dtu.set_xlate_req(0);

        if(handle_xlate(state, xlate_req)) {
            // handle other requests that pagefaulted in the meantime
            while(req_count > 0) {
                for(size_t i = 0; i < ARRAY_SIZE(reqs); ++i) {
                    xlate_req = reqs[i];
                    if(xlate_req) {
                        req_count--;
                        reqs[i] = 0;
                        handle_xlate(state, xlate_req);
                    }
                }
            }
        }
    }

    // request from kernel?
    m3::DTU::reg_t ext_req = dtu.get_ext_req();
    if(ext_req != 0)
        handle_ext_req(state, ext_req);

    // if we return to the application, resume the DTU command, if necessary
    if(state->cs & 0x3)
        ctxsw_resume();
    return state;
}

void *VMA::mmu_pf(m3::Exceptions::State *state) {
    uintptr_t cr2;
    asm volatile ("mov %%cr2, %0" : "=r"(cr2));

    // rctmux isn't causing PFs
    panic_if(state->cs != ((Exceptions::SEG_UCODE << 3) | 3),
        "RCTMux: pagefault from ourself for 0x%x @ 0x%x; stopping\n", cr2, state->rip);

    // if we don't use the MMU, we shouldn't get here
    panic_if(!m3::env()->pedesc.has_mmu(),
        "RCTMux: unexpected pagefault for 0x%x @ 0x%x; stopping\n", cr2, state->rip);

    bool res = handle_pf(state, 0, cr2, to_dtu_pte(state->errorCode & 0x7));
    // if we can't handle the PF, there is something wrong
    panic_if(!res, "RCTMux: nested pagefault for 0x%x @ 0x%x; stopping\n", cr2, state->rip);

    return state;
}

}
