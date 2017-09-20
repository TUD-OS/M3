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

#include <base/CPU.h>
#include <base/ELF.h>
#include <base/Panic.h>

#include <m3/session/Pager.h>
#include <m3/stream/FStream.h>
#include <m3/vfs/MountTable.h>
#include <m3/vfs/RegularFile.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

#include <stdlib.h>

namespace m3 {

void VPE::init_state() {
    if(Heap::is_on_heap(_eps))
        delete _eps;
    if(Heap::is_on_heap(_caps))
        delete _caps;

    _caps = reinterpret_cast<BitField<CAP_TOTAL>*>(env()->caps);
    if(_caps == nullptr)
        _caps = new BitField<CAP_TOTAL>();

    _eps = reinterpret_cast<BitField<EP_COUNT>*>(env()->eps);
    if(_eps == nullptr)
        _eps = new BitField<EP_COUNT>();

    _rbufcur = env()->rbufcur;
    _rbufend = env()->rbufend;
}

void VPE::init_fs() {
    if(Heap::is_on_heap(_fds))
        delete _fds;
    if(Heap::is_on_heap(_ms))
        delete _ms;

    if(env()->pager_sess && env()->pager_sgate)
        _pager = new Pager(env()->pager_sess, env()->pager_sgate, env()->pager_rgate);

    if(env()->mounts_len)
        _ms = MountTable::unserialize(reinterpret_cast<const void*>(env()->mounts), env()->mounts_len);
    else
        _ms = reinterpret_cast<MountTable*>(env()->mounts);

    if(env()->fds_len)
        _fds = FileTable::unserialize(reinterpret_cast<const void*>(env()->fds), env()->fds_len);
    else
        _fds = reinterpret_cast<FileTable*>(env()->fds);
}

Errors::Code VPE::run(void *lambda) {
    if(_pager)
        _pager->activate_rgate();

    Errors::Code err = copy_sections();
    if(err != Errors::NONE)
        return err;

    alignas(DTU_PKG_SIZE) Env senv;
    senv.pe = 0;
    senv.argc = env()->argc;
    senv.argv = RT_SPACE_START;
    senv.sp = CPU::get_sp();
    senv.entry = get_entry();
    senv.lambda = reinterpret_cast<uintptr_t>(lambda);
    senv.exitaddr = 0;

    senv.mounts_len = 0;
    senv.mounts = reinterpret_cast<uintptr_t>(_ms);
    senv.fds_len = 0;
    senv.fds = reinterpret_cast<uintptr_t>(_fds);
    senv.rbufcur = _rbufcur;
    senv.rbufend = _rbufend;
    senv.caps = reinterpret_cast<uintptr_t>(_caps);
    senv.eps = reinterpret_cast<uintptr_t>(_eps);
    senv.pager_sgate = 0;
    senv.pager_rgate = 0;
    senv.pager_sess = 0;

    senv._backend = env()->_backend;
    senv.pedesc = _pe;

    senv.heapsize = env()->heapsize;

    /* write start env to PE */
    _mem.write(&senv, sizeof(senv), RT_START);

    /* write args */
    char *buffer = static_cast<char*>(Heap::alloc(BUF_SIZE));
    size_t size = store_arguments(buffer, static_cast<int>(env()->argc),
        reinterpret_cast<const char**>(env()->argv));
    _mem.write(buffer, size, RT_SPACE_START);
    Heap::free(buffer);

    /* go! */
    return start();
}

Errors::Code VPE::exec(int argc, const char **argv) {
    alignas(DTU_PKG_SIZE) Env senv;
    char *buffer = static_cast<char*>(Heap::alloc(BUF_SIZE));

    if(_exec)
        delete _exec;
    _exec = new FStream(argv[0], FILE_RWX);
    if(!*_exec)
        return Errors::last;

    if(_pager)
        _pager->activate_rgate();

    uintptr_t entry;
    size_t size;
    Errors::Code err = load(argc, argv, &entry, buffer, &size);
    if(err != Errors::NONE) {
        Heap::free(buffer);
        return err;
    }

    senv.argc = static_cast<uint32_t>(argc);
    senv.argv = RT_SPACE_START;
    senv.sp = STACK_TOP;
    senv.entry = entry;
    senv.lambda = 0;
    senv.exitaddr = 0;

    /* add mounts, fds, caps and eps */
    /* align it because we cannot necessarily read e.g. integers from unaligned addresses */
    size_t offset = Math::round_up(size, sizeof(word_t));

    senv.mounts = RT_SPACE_START + offset;
    senv.mounts_len = _ms->serialize(buffer + offset, RT_SPACE_SIZE - offset);
    offset = Math::round_up(offset + static_cast<size_t>(senv.mounts_len), sizeof(word_t));

    senv.fds = RT_SPACE_START + offset;
    senv.fds_len = _fds->serialize(buffer + offset, RT_SPACE_SIZE - offset);
    offset = Math::round_up(offset + static_cast<size_t>(senv.fds_len), sizeof(word_t));

    size_t eps_caps = Math::round_up(sizeof(*_caps), sizeof(word_t)) +
        Math::round_up(sizeof(*_eps), sizeof(word_t));
    if(RT_SPACE_SIZE - offset < eps_caps)
        PANIC("State is too large");

    senv.caps = RT_SPACE_START + offset;
    memcpy(buffer + offset, _caps, sizeof(*_caps));
    offset = Math::round_up(offset + sizeof(*_caps), sizeof(word_t));

    senv.eps = RT_SPACE_START + offset;
    memcpy(buffer + offset, _eps, sizeof(*_eps));
    offset = Math::round_up(offset + sizeof(*_eps), DTU_PKG_SIZE);

    /* write entire runtime stuff */
    _mem.write(buffer, offset, RT_SPACE_START);

    Heap::free(buffer);

    senv.rbufcur = _rbufcur;
    senv.rbufend = _rbufend;

    /* set pager info */
    senv.pager_sess = _pager ? _pager->sel() : 0;
    senv.pager_sgate = _pager ? _pager->gate().sel() : 0;
    senv.pager_rgate = _pager ? _pager->rgate().sel() : 0;

    senv._backend = 0;
    senv.pedesc = _pe;

    senv.heapsize = _pager ? APP_HEAP_SIZE : 0;

    /* write start env to PE */
    _mem.write(&senv, sizeof(senv), RT_START);

    /* go! */
    return start();
}

void VPE::clear_mem(char *buffer, size_t count, uintptr_t dest) {
    memset(buffer, 0, BUF_SIZE);
    while(count > 0) {
        size_t amount = std::min(count, BUF_SIZE);
        _mem.write(buffer, Math::round_up(amount, DTU_PKG_SIZE), dest);
        count -= amount;
        dest += amount;
    }
}

Errors::Code VPE::load_segment(ElfPh &pheader, char *buffer) {
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
        if(pheader.p_memsz == pheader.p_filesz) {
            const RegularFile *rfile = static_cast<const RegularFile*>(_exec->file());
            return _pager->map_ds(&virt, sz, prot, 0, *rfile->fs(), rfile->fd(), pheader.p_offset);
        }

        assert(pheader.p_filesz == 0);
        return _pager->map_anon(&virt, sz, prot, 0);
    }

    /* seek to that offset and copy it to destination PE */
    size_t off = pheader.p_offset;
    if(_exec->seek(off, M3FS_SEEK_SET) != off)
        return Errors::INVALID_ELF;

    size_t count = pheader.p_filesz;
    size_t segoff = pheader.p_vaddr;
    while(count > 0) {
        size_t amount = std::min(count, BUF_SIZE);
        if(_exec->read(buffer, amount) != amount)
            return Errors::last;

        _mem.write(buffer, Math::round_up(amount, DTU_PKG_SIZE), segoff);
        count -= amount;
        segoff += amount;
    }

    /* zero the rest */
    clear_mem(buffer, pheader.p_memsz - pheader.p_filesz, segoff);
    return Errors::NONE;
}

Errors::Code VPE::load(int argc, const char **argv, uintptr_t *entry, char *buffer, size_t *size) {
    /* load and check ELF header */
    ElfEh header;
    if(_exec->read(&header, sizeof(header)) != sizeof(header))
        return Errors::INVALID_ELF;

    if(header.e_ident[0] != '\x7F' || header.e_ident[1] != 'E' || header.e_ident[2] != 'L' ||
        header.e_ident[3] != 'F')
        return Errors::INVALID_ELF;

    /* copy load segments to destination PE */
    uintptr_t end = 0;
    size_t off = header.e_phoff;
    for(uint i = 0; i < header.e_phnum; ++i, off += header.e_phentsize) {
        /* load program header */
        ElfPh pheader;
        if(_exec->seek(off, M3FS_SEEK_SET) != off)
            return Errors::INVALID_ELF;
        if(_exec->read(&pheader, sizeof(pheader)) != sizeof(pheader))
            return Errors::last;

        /* we're only interested in non-empty load segments */
        if(pheader.p_type != PT_LOAD || pheader.p_memsz == 0 || skip_section(&pheader))
            continue;

        load_segment(pheader, buffer);
        end = pheader.p_vaddr + pheader.p_memsz;
    }

    if(_pager) {
        // create area for stack and boot/runtime stuff
        uintptr_t virt = RT_START;
        Errors::Code err = _pager->map_anon(&virt, STACK_TOP - virt, Pager::READ | Pager::WRITE, 0);
        if(err != Errors::NONE)
            return err;

        // create heap
        virt = Math::round_up(end, static_cast<uintptr_t>(PAGE_SIZE));
        err = _pager->map_anon(&virt, APP_HEAP_SIZE, Pager::READ | Pager::WRITE, 0);
        if(err != Errors::NONE)
            return err;
    }

    *size = store_arguments(buffer, argc, argv);

    *entry = header.e_entry;
    return Errors::NONE;
}

size_t VPE::store_arguments(char *buffer, int argc, const char **argv) {
    /* copy arguments and arg pointers to buffer */
    uint64_t *argptr = reinterpret_cast<uint64_t*>(buffer);
    char *args = buffer + static_cast<size_t>(argc) * sizeof(uint64_t);
    for(int i = 0; i < argc; ++i) {
        size_t len = strlen(argv[i]);
        if(args + len >= buffer + BUF_SIZE)
            return Errors::INV_ARGS;
        strcpy(args, argv[i]);
        *argptr++ = RT_SPACE_START + static_cast<size_t>(args - buffer);
        args += len + 1;
    }
    return static_cast<size_t>(args - buffer);
}

}
