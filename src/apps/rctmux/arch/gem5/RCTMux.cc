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
#include "../../Print.h"

namespace RCTMux {

namespace Arch {

static m3::DTU::reg_t cmdreg;

void abort() {
    m3::DTU::get().abort(m3::DTU::ABORT_CMD | m3::DTU::ABORT_VPE, &cmdreg);
}

void resume() {
    if(cmdreg) {
        m3::DTU::get().retry(cmdreg);
        cmdreg = 0;
    }
}

void sleep() {
    m3::DTU::get().sleep();
}

}

EXTERN_C void gem5_writefile(const char *str, uint64_t len, uint64_t offset, uint64_t file);

void print(const char *str, size_t len) {
    const char *fileAddr = "stdout";
    gem5_writefile(str, len, 0, reinterpret_cast<uint64_t>(fileAddr));
}

}
