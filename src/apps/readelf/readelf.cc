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

#include <base/Common.h>
#include <base/stream/Serial.h>
#include <base/ELF.h>
#include <base/Log.h>

#include <m3/stream/FStream.h>

using namespace m3;

alignas(DTU_PKG_SIZE) static char buffer[1024];

static const char *phtypes[] = {
    "NULL   ",
    "LOAD   ",
    "DYNAMIC",
    "INTERP ",
    "NOTE   ",
    "SHLIB  ",
    "PHDR   ",
    "TLS    "
};

template<typename ELF_EH,typename ELF_PH>
static void parse(FStream &bin) {
    bin.seek(0, SEEK_SET);

    ELF_EH header;
    if(bin.read(&header, sizeof(header)) != sizeof(header))
        PANIC("Invalid ELF-file");

    Serial::get() << "Program Headers:\n";
    Serial::get() << "  Type    Offset     VirtAddr     PhysAddr     FileSiz   MemSiz    Flg Align\n";

    /* copy load segments to destination core */
    off_t off = header.e_phoff;
    for(uint i = 0; i < header.e_phnum; ++i, off += header.e_phentsize) {
        /* load program header */
        ELF_PH pheader;
        if(bin.seek(off, SEEK_SET) != off)
            PANIC("Invalid ELF-file");
        if(bin.read(&pheader, sizeof(pheader)) != sizeof(pheader))
            PANIC("Invalid ELF-file: " << Errors::to_string(Errors::last));

        Serial::get() << "  " << (pheader.p_type < ARRAY_SIZE(phtypes) ? phtypes[pheader.p_type] : "???????")
            << " " << fmt(pheader.p_offset, "#0x", 8) << " "
            << fmt(pheader.p_vaddr, "#0x", 10) << " "
            << fmt(pheader.p_paddr, "#0x", 10) << " "
            << fmt(pheader.p_filesz, "#0x", 7) << " "
            << fmt(pheader.p_memsz, "#0x", 7) << " "
            << ((pheader.p_flags & PF_R) ? "R" : " ")
            << ((pheader.p_flags & PF_W) ? "W" : " ")
            << ((pheader.p_flags & PF_X) ? "E" : " ") << " "
            << fmt(pheader.p_align, "#0x") << "\n";

        /* seek to that offset and copy it to destination core */
        if(bin.seek(pheader.p_offset, SEEK_SET) != (off_t)pheader.p_offset)
            PANIC("Invalid ELF-file");

        size_t count = pheader.p_filesz;
        size_t segoff = pheader.p_vaddr;
        while(count > 0) {
            size_t amount = std::min(count, sizeof(buffer));
            if(bin.read(buffer, amount) != amount)
                PANIC("Reading failed: " << Errors::to_string(Errors::last));

            count -= amount;
            segoff += amount;
        }
    }
}

int main(int argc, char **argv) {
    if(argc < 2)
        PANIC("Usage: " << argv[0] << " <bin>");

    if(VFS::mount("/", new M3FS("m3fs")) != Errors::NO_ERROR) {
        if(Errors::last != Errors::EXISTS)
            PANIC("Mounting root-fs failed");
    }

    FStream bin(argv[1], FILE_R);
    if(Errors::occurred())
        PANIC("open(" << argv[1] << ") failed: " << Errors::to_string(Errors::last));

    /* load and check ELF header */
    ElfEh header;
    if(bin.read(&header, sizeof(header)) != sizeof(header))
        PANIC("Invalid ELF-file");

    if(header.e_ident[0] != '\x7F' || header.e_ident[1] != 'E' || header.e_ident[2] != 'L' ||
        header.e_ident[3] != 'F')
        PANIC("Invalid ELF-file");

    if(header.e_ident[EI_CLASS] == ELFCLASS32)
        parse<Elf32_Ehdr,Elf32_Phdr>(bin);
    else
        parse<Elf64_Ehdr,Elf64_Phdr>(bin);
    return 0;
}
