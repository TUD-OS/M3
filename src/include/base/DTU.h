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

#pragma once

#if defined(__host__)
#   include <base/arch/host/DTU.h>
#elif defined(__t2__)
#   include <base/arch/t2/DTU.h>
#elif defined(__t3__)
#   include <base/arch/t3/DTU.h>
#elif defined(__gem5__)
#   include <base/arch/gem5/DTU.h>
#else
#   error "Unsupported target"
#endif
