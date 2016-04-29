/*
 * Copyright (C) 2016, René Küttner <rene.kuettner@tu-dresden.de>
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

#if defined(__t3__)
#   include <m3/arch/t3/RCTMux.h>
#elif defined(__gem5__)
    // gem5 needs no additional declarations
#else
#   error "RCTMux has not yet been implemented for your target"
#endif

#define RCTMUX_FLAG_ERROR   (1 << 0)
#define RCTMUX_FLAG_SIGNAL  (1 << 1)
#define RCTMUX_FLAG_STORE   (1 << 2)
#define RCTMUX_FLAG_RESTORE (1 << 3)

#if defined(__cplusplus)
namespace m3 {

/**
 * These flags implement the flags register for remote controlled
 * time-multiplexing since the corresponding register is not yet
 * available in the DTU of t3.
 *
 * The SIGNAL flag does not exist in the proposed design. It allows
 * for the polling part of the protocol without occupying additional
 * memory.
 *
 * The ERROR flag is a workaround for the missing hardware-reset
 * feature on t3.
 */
enum RCTMUXCtrlFlag {
    NONE                = 0,
    ERROR               = RCTMUX_FLAG_ERROR,    // an error occured
    STORE               = RCTMUX_FLAG_STORE,    // save operation required
    RESTORE             = RCTMUX_FLAG_RESTORE,  // restore operation required
    SIGNAL              = RCTMUX_FLAG_SIGNAL,   // used for event polling
};

} /* namespace m3 */
#endif /* defined(__cplusplus) */
