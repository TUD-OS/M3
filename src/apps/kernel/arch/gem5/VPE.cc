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

#include <base/util/Math.h>
#include <base/log/Kernel.h>
#include <base/ELF.h>

#include "mem/MainMemory.h"
#include "pes/VPE.h"
#include "DTU.h"
#include "Platform.h"

namespace kernel {

struct BootModule {
    char name[256];
    uint64_t addr;
    uint64_t size;
} PACKED;

static size_t count = 0;
static BootModule mods[Platform::MAX_MODS];
static uint64_t loaded = 0;
static BootModule *idles[Platform::MAX_PES];

static BootModule *get_mod(size_t argc, char **argv, bool *first) {
    static_assert(sizeof(loaded) * 8 >= Platform::MAX_MODS, "Too few bits for modules");

    if(count == 0) {
        for(size_t i = 0; i < Platform::MAX_MODS && Platform::mod(i); ++i) {
            uintptr_t addr = m3::DTU::gaddr_to_virt(Platform::mod(i));
            peid_t pe = m3::DTU::gaddr_to_pe(Platform::mod(i));
            DTU::get().read_mem(VPEDesc(pe, VPE::INVALID_ID), addr, &mods[i], sizeof(mods[i]));

            KLOG(KENV, "Module '" << mods[i].name << "':");
            KLOG(KENV, "  addr: " << m3::fmt(mods[i].addr, "p"));
            KLOG(KENV, "  size: " << m3::fmt(mods[i].size, "p"));

            count++;
        }

        static const char *types[] = {"imem", "emem", " mem"};
        static const char *isas[] = {"non", "x86", "arm", "xte", "acc"};
        for(size_t i = 0; i < Platform::pe_count(); ++i) {
            KLOG(KENV, "PE" << m3::fmt(i, 2) << ": "
                << types[static_cast<size_t>(Platform::pe(i).type())] << " "
                << isas[static_cast<size_t>(Platform::pe(i).isa())] << " "
                << (Platform::pe(i).mem_size() / 1024) << " KiB memory");
        }
    }

    char buf[256];
    m3::OStringStream os(buf, sizeof(buf));
    for(size_t i = 0; i < argc; ++i) {
        os << argv[i];
        if(i + 1 < argc)
            os << ' ';
    }

    for(size_t i = 0; i < count; ++i) {
        if(strcmp(mods[i].name, os.str()) == 0) {
            *first = (loaded & (static_cast<uint64_t>(1) << i)) == 0;
            loaded |= static_cast<uint64_t>(1) << i;
            return mods + i;
        }
    }
    return nullptr;
}

static gaddr_t alloc_mem(size_t size) {
    MainMemory::Allocation alloc = MainMemory::get().allocate(size, PAGE_SIZE);
    if(!alloc)
        PANIC("Not enough memory");
    return m3::DTU::build_gaddr(alloc.pe(), alloc.addr);
}

static void read_from_mod(BootModule *mod, void *data, size_t size, size_t offset) {
    if(offset + size < offset || offset + size > mod->size)
        PANIC("Invalid ELF file");

    uintptr_t addr = m3::DTU::gaddr_to_virt(mod->addr + offset);
    peid_t pe = m3::DTU::gaddr_to_pe(mod->addr + offset);
    DTU::get().read_mem(VPEDesc(pe, VPE::INVALID_ID), addr, data, size);
}

static void map_segment(VPE &vpe, gaddr_t phys, uintptr_t virt, size_t size, int perms) {
    if(Platform::pe(vpe.pe()).has_virtmem()) {
        capsel_t dst = virt >> PAGE_BITS;
        size_t pages = m3::Math::round_up(size, PAGE_SIZE) >> PAGE_BITS;
        MapCapability *mapcap = new MapCapability(&vpe.mapcaps(), dst, phys, pages, perms);
        vpe.mapcaps().set(dst, mapcap);
    }
    else {
        DTU::get().copy_clear(vpe.desc(), virt,
            VPEDesc(m3::DTU::gaddr_to_pe(phys), VPE::INVALID_ID), m3::DTU::gaddr_to_virt(phys),
            size, false);
    }
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
    size_t off = header.e_phoff;
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
            gaddr_t phys = alloc_mem(size);

            // map it
            map_segment(vpe, phys, virt, size, perms);
            end = virt + size;

            DTU::get().copy_clear(vpe.desc(), virt,
                VPEDesc(m3::DTU::gaddr_to_pe(mod->addr), VPE::INVALID_ID),
                m3::DTU::gaddr_to_virt(mod->addr + offset),
                size, pheader.p_filesz == 0);
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
        gaddr_t phys = alloc_mem(MOD_HEAP_SIZE);
        map_segment(vpe, phys, m3::Math::round_up(end, PAGE_SIZE), MOD_HEAP_SIZE, m3::DTU::PTE_RW);
    }

    return header.e_entry;
}

static uintptr_t map_idle(VPE &vpe) {
    BootModule *idle = idles[vpe.pe()];
    if(!idle) {
        bool first;
        char *args[] = {const_cast<char*>("rctmux")};
        BootModule *tmp = get_mod(1, args, &first);
        idle = new BootModule;

        // copy the ELF file
        size_t size = m3::Math::round_up(static_cast<size_t>(tmp->size), PAGE_SIZE);
        gaddr_t phys = alloc_mem(size);
        VPEDesc bootvpe(m3::DTU::gaddr_to_pe(phys), VPE::INVALID_ID);
        DTU::get().copy_clear(bootvpe, m3::DTU::gaddr_to_virt(phys),
            VPEDesc(m3::DTU::gaddr_to_pe(tmp->addr), VPE::INVALID_ID),
            m3::DTU::gaddr_to_virt(tmp->addr),
            tmp->size, false);

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

void VPE::load_app() {
    assert(_flags & F_BOOTMOD);
    assert(_argc > 0 && _argv);

    bool appFirst;
    BootModule *mod = get_mod(_argc, _argv, &appFirst);
    if(!mod)
        PANIC("Unable to find boot module '" << _argv[0] << "'");

    KLOG(KENV, "Loading mod '" << mod->name << "':");

    if(Platform::pe(pe()).has_virtmem()) {
        // map runtime space
        uintptr_t virt = RT_START;
        gaddr_t phys = alloc_mem(STACK_TOP - virt);
        map_segment(*this, phys, virt, STACK_TOP - virt, m3::DTU::PTE_RW);
    }

    // load app
    uintptr_t entry = load_mod(*this, mod, !appFirst, true);

    // count arguments
    size_t argc = 1;
    for(size_t i = 0; mod->name[i]; ++i) {
        if(mod->name[i] == ' ')
            argc++;
    }

    // copy arguments and arg pointers to buffer
    char buffer[512];
    uint64_t *argptr = reinterpret_cast<uint64_t*>(buffer);
    char *args = buffer + argc * sizeof(uint64_t);
    char c;
    size_t i, off = static_cast<size_t>(args - buffer);
    *argptr++ = RT_SPACE_START + off;
    for(i = 0; i < sizeof(buffer) && (c = mod->name[i]); ++i) {
        if(c == ' ') {
            args[i] = '\0';
            *argptr++ = RT_SPACE_START + off + i + 1;
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
    senv.argv = RT_SPACE_START;
    senv.sp = STACK_TOP - sizeof(word_t);
    senv.entry = entry;
    senv.pedesc = Platform::pe(pe());
    senv.heapsize = MOD_HEAP_SIZE;

    DTU::get().write_mem(desc(), RT_START, &senv, sizeof(senv));
}

void VPE::init_memory() {
    bool vm = Platform::pe(pe()).has_virtmem();
    if(vm) {
        _dtustate.config_pf(address_space()->root_pt(), address_space()->ep());
        DTU::get().config_pf_remote(desc(), address_space()->root_pt(), address_space()->ep());
    }

    if(Platform::pe(pe()).is_programmable())
        map_idle(*this);
    // PEs with virtual memory still need the rctmux flags
    else if(vm) {
        gaddr_t phys = alloc_mem(PAGE_SIZE);
        map_segment(*this, phys, RCTMUX_FLAGS & ~PAGE_MASK, PAGE_SIZE, m3::DTU::PTE_RW);
    }

    if(vm) {
        // map receive buffer
        gaddr_t phys = alloc_mem(RECVBUF_SIZE);
        map_segment(*this, phys, RECVBUF_SPACE, RECVBUF_SIZE, m3::DTU::PTE_RW);
    }

    uintptr_t barrier = Platform::rw_barrier(pe());
    _dtustate.config_rwb(barrier);
    DTU::get().config_rwb_remote(desc(), barrier);

    // boot modules are started implicitly
    if(_flags & F_BOOTMOD)
        load_app();
}

}
