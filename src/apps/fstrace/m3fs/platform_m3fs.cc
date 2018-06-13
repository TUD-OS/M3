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

#include <base/stream/Serial.h>

#include <m3/session/LoadGen.h>

#include <stdarg.h>

#include "fsapi_m3fs.h"
#include "platform.h"

/*
 * *************************************************************************
 */

static m3::LoadGen::Channel *chan;

void Platform::init(int /*argc*/, const char * const * /*argv*/, const char *loadgen) {
    if(*loadgen) {
        // connect to load generator
        m3::LoadGen *lg = new m3::LoadGen(loadgen);
        if(lg->is_connected()) {
            chan = lg->create_channel(2 * 1024 * 1024);
            lg->start(3 * 11);
        }
    }
}


FSAPI *Platform::fsapi(bool wait, const char *root) {
    return new FSAPI_M3FS(wait, root, chan);
}


void Platform::shutdown() {

}


void Platform::checkpoint_fs() {

}


void Platform::sync_fs() {

}


void Platform::drop_caches() {

}


void Platform::log(const char *msg) {
    m3::Serial::get() << msg << "\n";
}

void Platform::logf(UNUSED const char *fmt, ...) {
#if defined(__host__)
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
#endif
}
