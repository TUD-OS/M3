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

#pragma once

#include <stddef.h>
#include <stdio.h>
#include <string>
#include <vector>

#include <linux/elf.h>

class Symbols {
    struct Symbol {
        explicit Symbol(uint32_t bin, unsigned long addr, const char *name)
            : bin(bin), addr(addr), name(name) {
        }

        uint32_t bin;
        unsigned long addr;
        std::string name;
    };

    static const size_t MAX_FUNC_LEN    = 255;

public:
    explicit Symbols();

    void addFile(const char *file);
    const char *resolve(unsigned long addr, uint32_t *bin);

private:
    void demangle(char *dst, size_t dstSize, const char *name);
    char *loadShSyms(FILE *f, const Elf64_Ehdr *eheader);
    Elf64_Shdr *getSecByName(FILE *f, const Elf64_Ehdr *eheader, char *syms, const char *name);
    void readat(FILE *f, off_t offset, void *buffer, size_t count);

    size_t files;
    std::vector<Symbol> syms;
};
