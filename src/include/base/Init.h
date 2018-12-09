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

#include <base/Compiler.h>

#define INIT_PRIO_EPMUX         INIT_PRIO(101)
#define INIT_PRIO_SENDQUEUE     INIT_PRIO(101)
#define INIT_PRIO_DTU           INIT_PRIO(102)

#define INIT_PRIO_RECVBUF       INIT_PRIO(104)
#define INIT_PRIO_RECVGATE      INIT_PRIO(105)
#define INIT_PRIO_SYSC          INIT_PRIO(106)

#if defined(__host__)
#   define INIT_PRIO_ENV        INIT_PRIO(103)
// this needs to run as soon as syscalls work
#   define INIT_PRIO_ENV_POST   INIT_PRIO(107)
#endif

#define INIT_PRIO_VPE           INIT_PRIO(108)
#define INIT_PRIO_VFS           INIT_PRIO(109)
#define INIT_PRIO_STREAM        INIT_PRIO(110)

#define INIT_PRIO_USER(X)       INIT_PRIO(200 + (X))
