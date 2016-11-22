/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cxxabi.h>

#include <algorithm>
#include <string>

#include <linux/elf.h>

#include "Symbols.h"

Symbols::Symbols() : files(), syms() {
}

void Symbols::addFile(const char *file) {
    FILE *f = fopen(file,"r");
    if(!f)
        perror("fopen");

    Elf64_Ehdr eheader;
    readat(f, 0, &eheader, sizeof(Elf64_Ehdr));
    char *shsyms = loadShSyms(f, &eheader);

    Elf64_Shdr *sheader = getSecByName(f, &eheader, shsyms, ".symtab");
    if(sheader) {
        size_t count = sheader->sh_size / sizeof(Elf64_Sym);
        Elf64_Sym *list = new Elf64_Sym[count];
        readat(f, static_cast<off_t>(sheader->sh_offset), list, sheader->sh_size);

        sheader = getSecByName(f, &eheader, shsyms, ".strtab");
        if(sheader) {
            char *names = new char[sheader->sh_size];
            readat(f, static_cast<off_t>(sheader->sh_offset), names, sheader->sh_size);

            for(size_t i = 0; i < count; ++i) {
                if(ELF32_ST_TYPE(list[i].st_info) == STT_FUNC)
                    syms.push_back(Symbol(files, list[i].st_value, names + list[i].st_name));
            }

            delete[] names;
        }

        delete[] list;
    }

    std::sort(syms.begin(), syms.end(), [](const Symbol &a, const Symbol &b) {
        return a.addr < b.addr;
    });

    fclose(f);

    files++;
}

const char *Symbols::resolve(unsigned long addr, uint32_t *bin) {
    char *symbolName = (char*)malloc(MAX_FUNC_LEN);
    for(auto it = syms.begin(); it != syms.end(); ++it) {
        auto next = it + 1;
        if(next != syms.end() && next->addr > addr) {
            demangle(symbolName, MAX_FUNC_LEN, it->name.c_str());
            *bin = it->bin;
            return symbolName;
        }
    }
    *bin = static_cast<uint32_t>(-1);
    snprintf(symbolName, MAX_FUNC_LEN, "%lu", addr);
    return symbolName;
}

void Symbols::demangle(char *dst, size_t dstSize, const char *name) {
    int status;
    size_t len = dstSize;
    if(!abi::__cxa_demangle(name, dst, &len, &status)) {
        strncpy(dst, name, dstSize);
        dst[dstSize - 1] = '\0';
    }
}

char *Symbols::loadShSyms(FILE *f, const Elf64_Ehdr *eheader) {
    Elf64_Shdr sheader;
    unsigned char *datPtr;
    char *shsymbols;
    datPtr = reinterpret_cast<unsigned char*>(
        eheader->e_shoff + static_cast<size_t>(eheader->e_shstrndx) * eheader->e_shentsize);
    readat(f, (off_t)datPtr, &sheader, sizeof(Elf64_Shdr));
    shsymbols = (char*)malloc(sheader.sh_size);
    if(shsymbols == NULL)
        perror("malloc");
    readat(f, static_cast<off_t>(sheader.sh_offset), shsymbols, sheader.sh_size);
    return shsymbols;
}

Elf64_Shdr *Symbols::getSecByName(FILE *f, const Elf64_Ehdr *eheader, char *syms, const char *name) {
    static Elf64_Shdr section[1];
    int i;
    unsigned char *datPtr = reinterpret_cast<unsigned char*>(eheader->e_shoff);
    for(i = 0; i < eheader->e_shnum; datPtr += eheader->e_shentsize,  i++) {
        readat(f, (off_t)datPtr, section, sizeof(Elf64_Shdr));
        if(strcmp(syms + section->sh_name, name) == 0)
            return section;
    }
    return NULL;
}

void Symbols::readat(FILE *f, off_t offset, void *buffer, size_t count) {
    if(fseek(f, offset, SEEK_SET) < 0)
        perror("fseek");
    if(fread(buffer, 1, count, f) != count)
        perror("fread");
}
