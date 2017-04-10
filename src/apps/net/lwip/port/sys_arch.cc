/*
 * Copyright (C) 2017, Georg Kotheimer <georg.kotheimer@mailbox.tu-dresden.de>
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

#include <base/Config.h>
#include <base/DTU.h>
#include <thread/ThreadManager.h>

#include "lwip/def.h"
#include "arch/sys_arch.h"
#include "lwip/sys.h"

u32_t sys_now(void) {
    return (u32_t)(m3::DTU::get().tsc() * 1000 / m3::DTU::get().clock());
}
