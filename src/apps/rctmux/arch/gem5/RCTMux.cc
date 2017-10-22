/**
 * Copyright (C) 2016, René Küttner <rene.kuettner@.tu-dresden.de>
 * Economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <base/DTU.h>
#include <base/Env.h>

#include <base/RCTMux.h>

#include "../../RCTMux.h"

EXTERN_C void gem5_writefile(const char *str, uint64_t len, uint64_t offset, uint64_t file);

namespace RCTMux {

static m3::DTU::reg_t cmdreg;

uint64_t flags_get() {
    return *reinterpret_cast<volatile uint64_t*>(RCTMUX_FLAGS);
}

void flags_set(uint64_t flags) {
    *reinterpret_cast<volatile uint64_t*>(RCTMUX_FLAGS) = flags;
}

void sleep() {
    m3::DTU::get().sleep();
}

void save() {
    m3::DTU::get().abort(m3::DTU::ABORT_CMD | m3::DTU::ABORT_VPE, &cmdreg);
}

void resume() {
    m3::DTU::get().retry(cmdreg);
}

static size_t print_num_rec(char *buf, size_t pos, uint64_t num, uint base) {
    size_t p = pos;
    if(num > base)
        p = print_num_rec(buf, pos - 1, num / base, base);
    buf[pos] = "0123456789abcdef"[num % base];
    return p;
}

void print_num(uint64_t num, uint base) {
    char buf[16];
    size_t first = print_num_rec(buf, ARRAY_SIZE(buf) - 1, num, base);
    print(&buf[first], ARRAY_SIZE(buf) - first);
}

void print(const char *str, size_t len) {
    const char *fileAddr = "stdout";
    gem5_writefile(str, len, 0, reinterpret_cast<uint64_t>(fileAddr));
}

} /* namespace RCTMux */
