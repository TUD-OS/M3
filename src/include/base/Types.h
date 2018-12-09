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

#include <stdint.h>

using uchar     = unsigned char;
using ushort    = unsigned short;
using uint      = unsigned int;
using ulong     = unsigned long;
using llong     = long long;
using ullong    = unsigned long long;

#if defined(__arm__)
using size_t    = unsigned int;
using ssize_t   = int;
#else
using size_t    = unsigned long;
using ssize_t   = long;
#endif

using word_t    = unsigned long;
using label_t   = word_t;
using capsel_t  = unsigned;
using fd_t      = int;
using cycles_t  = uint64_t;

using epid_t    = ulong;
using peid_t    = ulong;
using gaddr_t   = uint64_t;
using goff_t    = uint64_t;
using event_t   = uint64_t;
using xfer_t    = uint64_t;
