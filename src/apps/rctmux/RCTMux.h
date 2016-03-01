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

void setup();
void init_switch();      // init phase
void store();            // store phase
void reset();            // reset phase
void restore();          // restore phase
void finish_switch();

void flag_set(const m3::RCTMUXCtrlFlag flag);
void flag_unset(const m3::RCTMUXCtrlFlag flag);
void flags_reset();
bool flag_is_set(const m3::RCTMUXCtrlFlag flag);

void notify_kernel();
void set_idle_mode();

} /* namespace RCTMux */
