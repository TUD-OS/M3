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

#ifndef RCTMUX_H
#define RCTMUX_H

#if defined(__t3__)
#   include "arch/t3/rctmux.h"
#elif defined(__gem5__)
#   include "arch/gem5/rctmux.h"
#else
#   error "Unsupported target"
#endif

#endif /* RCTMUX_H */
