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

    alignas(DTU_PKG_SIZE) Env senv;
    senv.coreid = 0;
    senv.argc = 0;
    senv.argv = 0;
    senv.sp = get_sp();
    senv.entry = get_entry();
    senv.lambda = reinterpret_cast<uintptr_t>(lambda);
    senv.exit = 0;

    senv.mount_len = _mountlen;
    senv.mounts = reinterpret_cast<uintptr_t>(_mounts);
    senv.caps = reinterpret_cast<uintptr_t>(_caps);
    senv.eps = reinterpret_cast<uintptr_t>(_eps);
    senv.pager_gate = 0;
    senv.pager_sess = 0;

    senv.backend = env()->backend;

    /* write start env to PE */
    _mem.write_sync(&senv, sizeof(senv), RT_START);

    /* go! */
    return Syscalls::get().vpectrl(sel(), Syscalls::VCTRL_START, 0, nullptr);
}

Errors::Code VPE::exec(int argc, const char **argv) {
    Executable e(argc, argv);
    return exec(e);
}

Errors::Code VPE::exec(Executable &exec) {
    alignas(DTU_PKG_SIZE) Env senv;
    char *buffer = (char*)Heap::alloc(BUF_SIZE);

    uintptr_t entry;
    size_t size;
    Errors::Code err = load(exec, &entry, buffer, &size);
    if(err != Errors::NO_ERROR) {
        Heap::free(buffer);
        return err;
    }

    senv.argc = exec.argc();
    senv.argv = reinterpret_cast<char**>(RT_SPACE_START);
    senv.sp = get_sp();
    senv.entry = entry;
    senv.lambda = 0;
    senv.exit = 0;

    /* check state size */
    if(size + _mountlen + sizeof(*_caps) + sizeof(*_eps) > RT_SPACE_SIZE)
        PANIC("State is too large");

    /* add mounts, caps and eps */
    /* align it because we cannot necessarily read e.g. integers from unaligned addresses */
    size_t offset = Math::round_up(size, sizeof(word_t));
    senv.mount_len = _mountlen;
    senv.mounts = RT_SPACE_START + offset;
    if(_mountlen > 0) {
        memcpy(buffer + offset, _mounts, _mountlen);
        offset = Math::round_up(offset + _mountlen, sizeof(word_t));
    }

    senv.caps = RT_SPACE_START + offset;
    memcpy(buffer + offset, _caps, sizeof(*_caps));
    offset = Math::round_up(offset + sizeof(*_caps), sizeof(word_t));

    senv.eps = RT_SPACE_START + offset;
    memcpy(buffer + offset, _eps, sizeof(*_eps));
    offset = Math::round_up(offset + sizeof(*_eps), DTU_PKG_SIZE);

    /* write entire runtime stuff */
    _mem.write_sync(buffer, offset, RT_SPACE_START);

    Heap::free(buffer);

    /* set pager info */
    senv.pager_sess = _pager ? _pager->sel() : 0;
    senv.pager_gate = _pager ? _pager->gate().sel() : 0;

    senv.backend = nullptr;

    /* write start env to PE */
    _mem.write_sync(&senv, sizeof(senv), RT_START);

    /* go! */
    return Syscalls::get().vpectrl(sel(), Syscalls::VCTRL_START, 0, nullptr);
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

Errors::Code VPE::load(Executable &exec, uintptr_t *entry, char *buffer, size_t *size) {
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

    /* copy load segments to destination core */
    uintptr_t end = 0;
    off_t off = header.e_phoff;
    for(uint i = 0; i < header.e_phnum; ++i, off += header.e_phentsize) {
        /* load program header */
        ElfPh pheader;
        if(bin.seek(off, SEEK_SET) != off)
            return Errors::INVALID_ELF;
        if(bin.read(&pheader, sizeof(pheader)) != sizeof(pheader))
            return Errors::last;

        /* we're only interested in non-empty load segments */
        if(pheader.p_type != PT_LOAD || pheader.p_memsz == 0 || skip_section(&pheader))
            continue;

        load_segment(exec, pheader, buffer);
        end = pheader.p_vaddr + pheader.p_memsz;
    }

    if(_pager) {
        // create area for stack and boot/runtime stuff
        uintptr_t virt = RT_START;
        Errors::Code err = _pager->map_anon(&virt, STACK_TOP - virt, Pager::READ | Pager::WRITE, 0);
        if(err != Errors::NO_ERROR)
            return err;

        // create heap
        virt = Math::round_up(end, static_cast<uintptr_t>(PAGE_SIZE));
        err = _pager->map_anon(&virt, INIT_HEAP_SIZE, Pager::READ | Pager::WRITE, 0);
        if(err != Errors::NO_ERROR)
            return err;
    }

    {
        /* copy arguments and arg pointers to buffer */
        char **argptr = (char**)buffer;
        char *args = buffer + exec.argc() * sizeof(char*);
        for(int i = 0; i < exec.argc(); ++i) {
            size_t len = strlen(exec.argv()[i]);
            if(args + len >= buffer + BUF_SIZE)
                return Errors::INV_ARGS;
            strcpy(args, exec.argv()[i]);
            *argptr++ = (char*)(RT_SPACE_START + (args - buffer));
            args += len + 1;
        }

        *size = args - buffer;
    }

    *entry = header.e_entry;
    return Errors::NO_ERROR;
}

}
