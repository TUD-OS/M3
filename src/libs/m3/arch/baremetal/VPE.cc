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

#include <m3/cap/VPE.h>
#include <m3/Syscalls.h>
#include <m3/service/Pager.h>
#include <m3/stream/FStream.h>
#include <m3/vfs/RegularFile.h>
#include <m3/vfs/Executable.h>
#include <m3/Log.h>
#include <m3/ELF.h>
#include <stdlib.h>

namespace m3 {

Errors::Code VPE::run(void *lambda) {
    copy_sections();

    /* clear core config */
    char *buffer = (char*)Heap::alloc(BUF_SIZE);
    clear_mem(buffer, sizeof(CoreConf), CONF_LOCAL);
    Heap::free(buffer);

    /* go! */
    uintptr_t entry = get_entry();
    start(entry, _caps, _eps, lambda, _mounts, _mountlen);
    return Syscalls::get().vpectrl(sel(), Syscalls::VCTRL_START, 0, nullptr);
}

Errors::Code VPE::exec(int argc, const char **argv) {
    Executable e(argc, argv);
    return exec(e);
}

Errors::Code VPE::exec(Executable &exec) {
    uintptr_t entry;
    Errors::Code err = load(exec, &entry);
    if(err != Errors::NO_ERROR)
        return err;

    /* store state to the VPE's memory */
    size_t statesize = _mountlen +
        Math::round_up(sizeof(*_caps), DTU_PKG_SIZE) +
        Math::round_up(sizeof(*_eps), DTU_PKG_SIZE);
    if(statesize > STATE_SIZE)
        PANIC("State is too large");

    size_t offset = STATE_SPACE;
    if(_mountlen > 0) {
        _mem.write_sync(_mounts, _mountlen, offset);
        offset += _mountlen;
    }

    void *caps = reinterpret_cast<void*>(offset);
    _mem.write_sync(_caps, Math::round_up(sizeof(*_caps), DTU_PKG_SIZE), offset);
    offset += Math::round_up(sizeof(*_caps), DTU_PKG_SIZE);

    void *eps = reinterpret_cast<void*>(offset);
    _mem.write_sync(_eps, Math::round_up(sizeof(*_eps), DTU_PKG_SIZE), offset);

    /* go! */
    start(entry, caps, eps, nullptr, reinterpret_cast<void*>(STATE_SPACE), _mountlen);
    return Syscalls::get().vpectrl(sel(), Syscalls::VCTRL_START, 0, nullptr);
}

void VPE::start(uintptr_t entry, void *caps, void *eps, void *lambda,
        void *mounts, size_t mountlen) {
    static_assert(BOOT_LAMBDA == BOOT_ENTRY - 8, "BOOT_LAMBDA or BOOT_ENTRY is wrong");

    /* give the PE the entry point and the address of the lambda */
    /* additionally, it needs the already allocated caps and endpoints */
    uint64_t vals[] = {
        (word_t)caps,
        (word_t)eps,
        (word_t)mounts,
        mountlen,
        _pager ? _pager->gate().sel() : -1,
        _pager ? _pager->sel() : -1,
        (word_t)lambda,
        entry,
        (word_t)get_sp(),
    };
    _mem.write_sync(vals, sizeof(vals), BOOT_CAPS);
}

void VPE::clear_mem(char *buffer, size_t count, uintptr_t dest) {
    memset(buffer, 0, BUF_SIZE);
    while(count > 0) {
        size_t amount = std::min(count, BUF_SIZE);
        _mem.write_sync(buffer, Math::round_up(amount, DTU_PKG_SIZE), dest);
        count -= amount;
        dest += amount;
    }
}

Errors::Code VPE::load_segment(Executable &exec, ElfPh &pheader, char *buffer) {
    if(_pager) {
        int prot = 0;
        if(pheader.p_flags & PF_R)
            prot |= Pager::READ;
        if(pheader.p_flags & PF_W)
            prot |= Pager::WRITE;
        if(pheader.p_flags & PF_X)
            prot |= Pager::EXEC;

        uintptr_t virt = pheader.p_vaddr;
        size_t sz = Math::round_up(pheader.p_memsz, static_cast<size_t>(PAGE_SIZE));
        if(pheader.p_memsz == pheader.p_filesz)
            return _pager->map_ds(&virt, sz, prot, 0, exec.sess(), exec.fd(), pheader.p_offset);

        assert(pheader.p_filesz == 0);
        return _pager->map_anon(&virt, sz, prot, 0);
    }

    /* seek to that offset and copy it to destination core */
    if(exec.stream().seek(pheader.p_offset, SEEK_SET) != (off_t)pheader.p_offset)
        return Errors::INVALID_ELF;

    size_t count = pheader.p_filesz;
    size_t segoff = pheader.p_vaddr;
    while(count > 0) {
        size_t amount = std::min(count, BUF_SIZE);
        if(exec.stream().read(buffer, amount) != amount)
            return Errors::last;

        _mem.write_sync(buffer, Math::round_up(amount, DTU_PKG_SIZE), segoff);
        count -= amount;
        segoff += amount;
    }

    /* zero the rest */
    clear_mem(buffer, pheader.p_memsz - pheader.p_filesz, segoff);
    return Errors::NO_ERROR;
}

Errors::Code VPE::load(Executable &exec, uintptr_t *entry) {
    Errors::Code err = Errors::NO_ERROR;
    uint64_t val;
    FStream &bin = exec.stream();
    if(!bin)
        return Errors::last;

    /* load and check ELF header */
    ElfEh header;
    if(bin.read(&header, sizeof(header)) != sizeof(header))
        return Errors::INVALID_ELF;

    if(header.e_ident[0] != '\x7F' || header.e_ident[1] != 'E' || header.e_ident[2] != 'L' ||
        header.e_ident[3] != 'F')
        return Errors::INVALID_ELF;

    char *buffer = (char*)Heap::alloc(BUF_SIZE);

    /* copy load segments to destination core */
    off_t off = header.e_phoff;
    for(uint i = 0; i < header.e_phnum; ++i, off += header.e_phentsize) {
        /* load program header */
        ElfPh pheader;
        if(bin.seek(off, SEEK_SET) != off) {
            err = Errors::INVALID_ELF;
            goto error;
        }
        if(bin.read(&pheader, sizeof(pheader)) != sizeof(pheader)) {
            err = Errors::last;
            goto error;
        }

        /* we're only interested in non-empty load segments */
        if(pheader.p_type != PT_LOAD || pheader.p_memsz == 0 || skip_section(&pheader))
            continue;

        load_segment(exec, pheader, buffer);
    }

    if(_pager) {
        // TODO temporary: just map idle where it's expected
        // note that this assumes that the first page within that file contains the code
        FStream *idle = new FStream("/bin/idle", FILE_RWX);
        uintptr_t virt = 0x3ff000;
        const RegularFile *rfile = static_cast<const RegularFile*>(&idle->file());
        err = _pager->map_ds(&virt, PAGE_SIZE, Pager::READ | Pager::WRITE | Pager::EXEC, 0,
            *rfile->fs(), rfile->fd(), 0);
        if(err != Errors::NO_ERROR)
            goto error;

        // create area for stack and runtime stuff
        virt = Math::round_dn<size_t>(RT_SPACE_END, PAGE_SIZE);
        err = _pager->map_anon(&virt, STACK_TOP - virt, Pager::READ | Pager::WRITE, 0);
        if(err != Errors::NO_ERROR)
            goto error;
    }

    {
        /* copy arguments and arg pointers to buffer */
        char **argptr = (char**)buffer;
        char *args = buffer + exec.argc() * sizeof(char*);
        for(int i = 0; i < exec.argc(); ++i) {
            size_t len = strlen(exec.argv()[i]);
            if(args + len >= buffer + BUF_SIZE) {
                err = Errors::INV_ARGS;
                goto error;
            }
            strcpy(args, exec.argv()[i]);
            *argptr++ = (char*)(ARGV_START + (args - buffer));
            args += len + 1;
        }

        /* write it to the target core */
        _mem.write_sync(buffer, Math::round_up((size_t)(args - buffer), DTU_PKG_SIZE), ARGV_START);
    }

    /* set argc and argv */
    val = exec.argc();
    _mem.write_sync(&val, sizeof(val), ARGC_ADDR);
    val = ARGV_START;
    _mem.write_sync(&val, sizeof(val), ARGV_ADDR);

    /* clear core config */
    clear_mem(buffer, sizeof(CoreConf), CONF_LOCAL);

    *entry = header.e_entry;

error:
    Heap::free(buffer);
    return err;
}

}
