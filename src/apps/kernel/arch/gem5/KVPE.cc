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

#include <m3/util/Sync.h>
#include <m3/ELF.h>
#include <m3/Log.h>

#include "../../Capability.h"
#include "../../KVPE.h"
#include "../../KDTU.h"
#include "../../MainMemory.h"

namespace m3 {

struct BootModule {
    char name[128];
    uint64_t addr;
    uint64_t size;
} PACKED;

static size_t count = 0;
static BootModule mods[MODS_MAX];
static uint32_t loaded = 0;
static BootModule *idles[MAX_CORES];
static char buffer[4096];

static BootModule *get_mod(const char *name, bool *first) {
    static_assert(sizeof(loaded) * 8 >= MODS_MAX, "Too few bits for modules");

    if(count == 0) {
        void **marray = reinterpret_cast<void**>(MODS_ADDR);
        for(size_t i = 0; i < MODS_MAX && marray[i]; ++i) {
            uintptr_t addr = DTU::noc_to_virt(reinterpret_cast<uintptr_t>(marray[i]));
            KDTU::get().read_mem_at(MEMORY_CORE, 0, addr, &mods[i], sizeof(mods[i]));

            Serial::get() << "Module '" << mods[i].name << "':\n";
            Serial::get() << "  addr: " << fmt(mods[i].addr, "p") << "\n";
            Serial::get() << "  size: " << fmt(mods[i].size, "p") << "\n";

            count++;
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

static void read_from_mod(BootModule *mod, void *data, size_t size, size_t offset) {
    if(offset + size < offset || offset + size > mod->size)
        PANIC("Invalid ELF file");

    KDTU::get().read_mem_at(MEMORY_CORE, 0, DTU::noc_to_virt(mod->addr + offset), data, size);
}

static void map_segment(KVPE &vpe, uint64_t phys, uintptr_t virt, size_t size, uint perms) {
    capsel_t kcap = VPE::self().alloc_cap();
    MemCapability *mcapobj = new MemCapability(&CapTable::kernel_table(), kcap,
        DTU::noc_to_virt(phys), size, perms, MEMORY_CORE, 0, -1);
    CapTable::kernel_table().set(kcap, mcapobj);

    capsel_t dst = virt >> PAGE_BITS;
    size_t pages = Math::round_up(size, PAGE_SIZE) >> PAGE_BITS;
    for(capsel_t i = 0; i < pages; ++i) {
        MapCapability *mapcap = new MapCapability(&vpe.mapcaps(), dst + i, phys, perms);
        vpe.mapcaps().inherit(mcapobj, mapcap);
        vpe.mapcaps().set(dst + i, mapcap);
        phys += PAGE_SIZE;
    }
}

static void copy_clear(int core, int vpe, uintptr_t dst, uintptr_t src, size_t size, bool clear) {
    if(clear)
        memset(buffer, 0, sizeof(buffer));

    size_t rem = size;
    while(rem > 0) {
        size_t amount = Math::min(rem, sizeof(buffer));
        // read it from src, if necessary
        if(!clear)
            KDTU::get().read_mem_at(MEMORY_CORE, 0, DTU::noc_to_virt(src), buffer, amount);
        KDTU::get().write_mem_at(core, vpe, DTU::noc_to_virt(dst), buffer, amount);
        src += amount;
        dst += amount;
        rem -= amount;
    }
}

static uintptr_t load_mod(KVPE &vpe, BootModule *mod, bool copy, bool needs_heap) {
    // load and check ELF header
    ElfEh header;
    read_from_mod(mod, &header, sizeof(header), 0);

    if(header.e_ident[0] != '\x7F' || header.e_ident[1] != 'E' || header.e_ident[2] != 'L' ||
        header.e_ident[3] != 'F')
        PANIC("Invalid ELF file");

    // map load segments
    uintptr_t end = 0;
    off_t off = header.e_phoff;
    for(uint i = 0; i < header.e_phnum; ++i, off += header.e_phentsize) {
        /* load program header */
        ElfPh pheader;
        read_from_mod(mod, &pheader, sizeof(pheader), off);

        // we're only interested in non-empty load segments
        if(pheader.p_type != PT_LOAD || pheader.p_memsz == 0)
            continue;

        int perms = 0;
        if(pheader.p_flags & PF_R)
            perms |= DTU::PTE_R;
        if(pheader.p_flags & PF_W)
            perms |= DTU::PTE_W;
        if(pheader.p_flags & PF_X)
            perms |= DTU::PTE_X;

        uintptr_t offset = Math::round_dn(pheader.p_offset, PAGE_SIZE);
        uintptr_t virt = Math::round_dn(pheader.p_vaddr, PAGE_SIZE);

        // do we need new memory for this segment?
        if((copy && (perms & DTU::PTE_W)) || pheader.p_filesz == 0) {
            // allocate memory
            size_t size = Math::round_up((pheader.p_vaddr & PAGE_BITS) + pheader.p_memsz, PAGE_SIZE);
            uintptr_t phys = DTU::build_noc_addr(
                MEMORY_CORE, MainMemory::get().map().allocate(size));

            // map it
            map_segment(vpe, phys, virt, size, perms);
            end = virt + size;

            copy_clear(vpe.core(), vpe.id(), virt, mod->addr + offset, size, pheader.p_filesz == 0);
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
        uintptr_t phys = DTU::build_noc_addr(
            MEMORY_CORE, MainMemory::get().map().allocate(INIT_HEAP_SIZE));
        map_segment(vpe, phys, Math::round_up(end, PAGE_SIZE), INIT_HEAP_SIZE, DTU::PTE_RW);
    }

    return header.e_entry;
}

static void map_idle(KVPE &vpe) {
    BootModule *idle = idles[vpe.core()];
    if(!idle) {
        bool first;
        BootModule *tmp = get_mod("idle", &first);
        idle = new BootModule;

        // copy the ELF file
        size_t size = Math::round_up(tmp->size, PAGE_SIZE);
        uintptr_t phys = DTU::build_noc_addr(
            MEMORY_CORE, MainMemory::get().map().allocate(size));
        copy_clear(MEMORY_CORE, 0, phys, tmp->addr, tmp->size, false);

        // remember the copy
        strcpy(idle->name, "idle");
        idle->addr = phys;
        idle->size = tmp->size;
        idles[vpe.core()] = idle;
    }
    if(!idle)
        PANIC("Unable to find boot module 'idle'");

    // load idle
    load_mod(vpe, idle, false, false);
}

void KVPE::init_memory(const char *name) {
    if(_bootmod) {
        bool appFirst;
        BootModule *mod = get_mod(name, &appFirst);
        if(!mod)
            PANIC("Unable to find boot module '" << name << "'");

        Serial::get() << "Loading mod '" << mod->name << "':\n";

        KDTU::get().config_pf_remote(*this, DTU::SYSC_EP);

        // map runtime space
        uintptr_t virt = RT_START;
        uintptr_t phys = DTU::build_noc_addr(MEMORY_CORE,
            MainMemory::get().map().allocate(STACK_TOP - virt));
        map_segment(*this, phys, virt, STACK_TOP - virt, DTU::PTE_RW);

        // load app
        uint64_t entry = load_mod(*this, mod, !appFirst, true);
        KDTU::get().write_mem(*this, BOOT_ENTRY, &entry, sizeof(entry));

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
        *argptr++ = (char*)(ARGV_START + off);
        for(i = 0; i < sizeof(buffer) && (c = mod->name[i]); ++i) {
            if(c == ' ') {
                args[i] = '\0';
                *argptr++ = (char*)(ARGV_START + off + i + 1);
            }
            else
                args[i] = c;
        }
        if(i == sizeof(buffer))
            PANIC("Not enough space for arguments");

        // write buffer to the target core
        size_t argssize = Math::round_up(off + i, DTU_PKG_SIZE);
        KDTU::get().write_mem(*this, ARGV_START, buffer, argssize);

        // set argc and argv
        uint64_t val = argc;
        KDTU::get().write_mem(*this, ARGC_ADDR, &val, sizeof(val));
        val = ARGV_START;
        KDTU::get().write_mem(*this, ARGV_ADDR, &val, sizeof(val));
    }

    map_idle(*this);
}

}
