/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <ostream>
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <vector>

#include <linux/elf.h>

class Symbols {
    struct Symbol {
        explicit Symbol(uint32_t bin, unsigned long addr, const char *name)
            : bin(bin),
              addr(addr),
              name(name) {
        }

        uint32_t bin;
        unsigned long addr;
        std::string name;
    };

public:
    static const size_t MAX_FUNC_LEN    = 255;

    typedef std::vector<Symbols::Symbol>::const_iterator symbol_t;

    explicit Symbols();

    void addFile(const char *file);
    symbol_t resolve(unsigned long addr);
    bool valid(symbol_t sym) const {
        return sym != syms.end();
    }
    void demangle(char *dst, size_t dstSize, const char *name);
    void print(std::ostream &os);

private:
    char *loadShSyms(FILE *f, const Elf64_Ehdr *eheader);
    Elf64_Shdr *getSecByName(FILE *f, const Elf64_Ehdr *eheader, char *syms, const char *name);
    void readat(FILE *f, off_t offset, void *buffer, size_t count);

    size_t files;
    symbol_t last;
    std::vector<Symbol> syms;
};
