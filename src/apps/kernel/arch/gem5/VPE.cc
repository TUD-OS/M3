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

#include <base/util/Sync.h>
#include <base/util/Math.h>
#include <base/log/Kernel.h>
#include <base/ELF.h>
#include <base/Panic.h>

#include "cap/Capability.h"
#include "mem/MainMemory.h"
#include "pes/VPE.h"
#include "DTU.h"
#include "Platform.h"

namespace kernel {

struct BootModule {
    char name[128];
    uint64_t addr;
    uint64_t size;
} PACKED;

static size_t count = 0;
static BootModule mods[Platform::MAX_MODS];
static uint64_t loaded = 0;
static BootModule *idles[Platform::MAX_PES];
static char buffer[4096];

static BootModule *get_mod(const char *name, bool *first) {
    static_assert(sizeof(loaded) * 8 >= Platform::MAX_MODS, "Too few bits for modules");

    if(count == 0) {
        for(size_t i = 0; i < Platform::MAX_MODS && Platform::mod(i); ++i) {
            uintptr_t addr = m3::DTU::noc_to_virt(reinterpret_cast<uintptr_t>(Platform::mod(i)));
            peid_t pe = m3::DTU::noc_to_pe(reinterpret_cast<uintptr_t>(Platform::mod(i)));
            DTU::get().read_mem(VPEDesc(pe, VPE::INVALID_ID), addr, &mods[i], sizeof(mods[i]));

            KLOG(KENV, "Module '" << mods[i].name << "':");
            KLOG(KENV, "  addr: " << m3::fmt(mods[i].addr, "p"));
            KLOG(KENV, "  size: " << m3::fmt(mods[i].size, "p"));

            count++;
        }

        static const char *types[] = {"imem", "emem", " mem"};
        static const char *isas[] = {"non", "x86", "xte", "acc"};
        for(size_t i = 0; i < Platform::pe_count(); ++i) {
            KLOG(KENV, "PE" << m3::fmt(i, 2) << ": "
                << types[static_cast<size_t>(Platform::pe(i).type())] << " "
                << isas[static_cast<size_t>(Platform::pe(i).isa())] << " "
                << (Platform::pe(i).mem_size() / 1024) << " KiB memory");
        }
    }

    size_t len = strlen(name);
    for(size_t i = 0; i < count; ++i) {
        if(mods[i].name[len] == ' ' || mods[i].name[len] == '\0') {
            if(strncmp(name, mods[i].name, len) == 0) {
                *first = (loaded & (1 << i)) == 0;
                loaded |= 1 << i;
                return mods + i;
            }
        }
    }
    return nullptr;
}

static uint64_t alloc_mem(size_t size) {
    MainMemory::Allocation alloc = MainMemory::get().allocate(size);
    if(!alloc)
        PANIC("Not enough memory");
    return m3::DTU::build_noc_addr(alloc.pe(), alloc.addr);
}

static void read_from_mod(BootModule *mod, void *data, size_t size, size_t offset) {
    if(offset + size < offset || offset + size > mod->size)
        PANIC("Invalid ELF file");

    uintptr_t addr = m3::DTU::noc_to_virt(mod->addr + offset);
    peid_t pe = m3::DTU::noc_to_pe(mod->addr + offset);
    DTU::get().read_mem(VPEDesc(pe, VPE::INVALID_ID), addr, data, size);
}

static void copy_clear(const VPEDesc &vpe, uintptr_t dst, uintptr_t src, size_t size, bool clear) {
    if(clear)
        memset(buffer, 0, sizeof(buffer));

    size_t rem = size;
    while(rem > 0) {
        size_t amount = m3::Math::min(rem, sizeof(buffer));
        // read it from src, if necessary
        if(!clear) {
            DTU::get().read_mem(VPEDesc(m3::DTU::noc_to_pe(src), VPE::INVALID_ID),
                m3::DTU::noc_to_virt(src), buffer, amount);
        }
        DTU::get().write_mem(vpe, m3::DTU::noc_to_virt(dst), buffer, amount);
        src += amount;
        dst += amount;
        rem -= amount;
    }
}

static void map_segment(VPE &vpe, uint64_t phys, uintptr_t virt, size_t size, uint perms) {
    if(Platform::pe(vpe.pe()).has_virtmem()) {
        capsel_t dst = virt >> PAGE_BITS;
        size_t pages = m3::Math::round_up(size, PAGE_SIZE) >> PAGE_BITS;
        MapCapability *mapcap = new MapCapability(&vpe.mapcaps(), dst, phys, pages, perms);
        vpe.mapcaps().set(dst, mapcap);
    }
    else
        copy_clear(vpe.desc(), virt, phys, size, false);
}

static uintptr_t load_mod(VPE &vpe, BootModule *mod, bool copy, bool needs_heap) {
    // load and check ELF header
    m3::ElfEh header;
    read_from_mod(mod, &header, sizeof(header), 0);

    if(header.e_ident[0] != '\x7F' || header.e_ident[1] != 'E' || header.e_ident[2] != 'L' ||
        header.e_ident[3] != 'F')
        PANIC("Invalid ELF file");

    // map load segments
    uintptr_t end = 0;
    off_t off = header.e_phoff;
    for(uint i = 0; i < header.e_phnum; ++i, off += header.e_phentsize) {
        /* load program header */
        m3::ElfPh pheader;
        read_from_mod(mod, &pheader, sizeof(pheader), off);

        // we're only interested in non-empty load segments
        if(pheader.p_type != PT_LOAD || pheader.p_memsz == 0)
            continue;

        int perms = 0;
        if(pheader.p_flags & PF_R)
            perms |= m3::DTU::PTE_R;
        if(pheader.p_flags & PF_W)
            perms |= m3::DTU::PTE_W;
        if(pheader.p_flags & PF_X)
            perms |= m3::DTU::PTE_X;

        uintptr_t offset = m3::Math::round_dn(pheader.p_offset, PAGE_SIZE);
        uintptr_t virt = m3::Math::round_dn(pheader.p_vaddr, PAGE_SIZE);

        // do we need new memory for this segment?
        if((copy && (perms & m3::DTU::PTE_W)) || pheader.p_filesz == 0) {
            // allocate memory
            size_t size = m3::Math::round_up((pheader.p_vaddr & PAGE_BITS) + pheader.p_memsz, PAGE_SIZE);
            uintptr_t phys = alloc_mem(size);

            // map it
            map_segment(vpe, phys, virt, size, perms);
            end = virt + size;

            copy_clear(vpe.desc(), virt, mod->addr + offset, size, pheader.p_filesz == 0);
        }
        else {
            assert(pheader.p_memsz == pheader.p_filesz);
            size_t size = (pheader.p_offset & PAGE_BITS) + pheader.p_filesz;
            map_segment(vpe, mod->addr + offset, virt, size, perms);
            end = virt + size;
        }
    }

    if(needs_heap) {
        // create initial heap
        uintptr_t phys = alloc_mem(MOD_HEAP_SIZE);
        map_segment(vpe, phys, m3::Math::round_up(end, PAGE_SIZE), MOD_HEAP_SIZE, m3::DTU::PTE_RW);
    }

    return header.e_entry;
}

static uintptr_t map_idle(VPE &vpe) {
    BootModule *idle = idles[vpe.pe()];
    if(!idle) {
        bool first;
        BootModule *tmp = get_mod("rctmux", &first);
        idle = new BootModule;

        // copy the ELF file
        size_t size = m3::Math::round_up(tmp->size, PAGE_SIZE);
        uintptr_t phys = alloc_mem(size);
        copy_clear(VPEDesc(m3::DTU::noc_to_pe(phys), VPE::INVALID_ID), phys, tmp->addr, tmp->size, false);

        // remember the copy
        strcpy(idle->name, "rctmux");
        idle->addr = phys;
        idle->size = tmp->size;
        idles[vpe.pe()] = idle;
    }
    if(!idle)
        PANIC("Unable to find boot module 'idle'");

    // load idle
    return load_mod(vpe, idle, false, false);
}

void VPE::load_app(const char *name) {
    assert(_flags & F_BOOTMOD);

    bool appFirst;
    BootModule *mod = get_mod(name, &appFirst);
    if(!mod)
        PANIC("Unable to find boot module '" << name << "'");

    KLOG(KENV, "Loading mod '" << mod->name << "':");

    if(Platform::pe(pe()).has_virtmem()) {
        // map runtime space
        uintptr_t virt = RT_START;
        uintptr_t phys = alloc_mem(STACK_TOP - virt);
        map_segment(*this, phys, virt, STACK_TOP - virt, m3::DTU::PTE_RW);
    }

    // load app
    uint64_t entry = load_mod(*this, mod, !appFirst, true);

    // count arguments
    int argc = 1;
    for(size_t i = 0; mod->name[i]; ++i) {
        if(mod->name[i] == ' ')
            argc++;
    }

    // copy arguments and arg pointers to buffer
    char **argptr = (char**)buffer;
    char *args = buffer + argc * sizeof(char*);
    char c;
    size_t i, off = args - buffer;
    *argptr++ = (char*)(RT_SPACE_START + off);
    for(i = 0; i < sizeof(buffer) && (c = mod->name[i]); ++i) {
        if(c == ' ') {
            args[i] = '\0';
            *argptr++ = (char*)(RT_SPACE_START + off + i + 1);
        }
        else
            args[i] = c;
    }
    if(i == sizeof(buffer))
        PANIC("Not enough space for arguments");

    // write buffer to the target PE
    size_t argssize = m3::Math::round_up(off + i, DTU_PKG_SIZE);
    DTU::get().write_mem(desc(), RT_SPACE_START, buffer, argssize);

    // write env to targetPE
    m3::Env senv;
    memset(&senv, 0, sizeof(senv));

    senv.argc = argc;
    senv.argv = reinterpret_cast<char**>(RT_SPACE_START);
    senv.sp = STACK_TOP - sizeof(word_t);
    senv.entry = entry;
    senv.pe = Platform::pe(pe());
    senv.heapsize = MOD_HEAP_SIZE;

    DTU::get().write_mem(desc(), RT_START, &senv, sizeof(senv));

    // we can start now
    assert(_flags & F_INIT);
    _flags |= F_START;

    // add a reference, like VPE::start() does
    ref();
}

void VPE::init_memory() {
    bool vm = Platform::pe(pe()).has_virtmem();
    if(vm) {
        dtustate().config_pf(address_space()->root_pt(), address_space()->ep());
        DTU::get().config_pf_remote(desc(), address_space()->root_pt(), address_space()->ep());
    }

    if(Platform::pe(pe()).is_programmable())
        map_idle(*this);

    if(vm) {
        // map receive buffer
        uintptr_t phys = alloc_mem(RECVBUF_SIZE);
        map_segment(*this, phys, RECVBUF_SPACE, RECVBUF_SIZE, m3::DTU::PTE_RW);
    }

    uintptr_t barrier = Platform::rw_barrier(pe());
    dtustate().config_rwb(barrier);
    DTU::get().config_rwb_remote(desc(), barrier);
}

}
