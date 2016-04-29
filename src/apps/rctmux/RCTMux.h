/**
 * Copyright (C) 2015-2016, René Küttner <rene.kuettner@.tu-dresden.de>
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

#pragma once

#include <m3/Config.h>

#if defined(__t3__)
#   include "arch/t3/RCTMux.h"
#elif defined(__gem5__)
#   include "arch/gem5/RCTMux.h"
#else
#   error "Unsupported target"
#endif

namespace RCTMux {

extern void setup();
extern void set_idle_mode();
extern void init();             // init phase
extern void store();            // store phase
extern void reset();            // reset phase
extern void restore();          // restore phase
extern void finish();

extern bool flag_is_set(const m3::RCTMUXCtrlFlag flag);

} /* namespace RCTMux */
