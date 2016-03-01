/**
 * Copyright (C) 2015, René Küttner <rene.kuettner@.tu-dresden.de>
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

#include <m3/Config.h>

namespace m3 {

/**
 * These flags implement the flags register for remote controlled
 * time-multiplexing since the corresponding register is not yet
 * available in the DTU.
 *
 * Flags marked with [*] do not exist in the proposed design. They
 * allow for the polling part of the protocol or without occupying
 * additional memory.
 *
 * The ERROR flag is a workaround for the missing hardware-reset
 * feature.
 */
enum RCTMUXCtrlFlag {
    NONE                = 0,
    // general flags
    ERROR               = 1 << 0,       // an error occured
    // rctmux flags
    INITIALIZED         = 1 << 1,       // rctmux has been initialized [*]
    // kernel flags
    SWITCHREQ           = 1 << 2,       // context switch requested
    STORE               = 1 << 3,       // save operation required
    RESTORE             = 1 << 4,       // restore operation required
    STORAGE_ATTACHED    = 1 << 5,       // attached save/restore storage [*]
    //
    SIGNAL              = 1 << 7,       // used for event polling
};

/**
 * The app layout structure holds information on the segments of the
 * currently running app. This information is used for time multiplexing
 * and VPE management. It is initialized by the app itself.
 *
 * Note: We do not store the stack pointer here, since it can easily
 * be collected whenever necessary. The data size will be maintained
 * by the heap manager.
 */
struct AppLayout {
    word_t reset_start;
    word_t reset_size;
    word_t text_start;
    word_t text_size;
    word_t data_start;
    word_t data_size;
    word_t stack_top;
    word_t _unused;
} PACKED;

static inline AppLayout *applayout() {
    return reinterpret_cast<AppLayout*>(APP_LAYOUT_LOCAL);
}

}
