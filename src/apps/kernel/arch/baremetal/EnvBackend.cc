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

#include <base/Common.h>
#include <base/stream/Serial.h>
#include <base/Backtrace.h>
#include <base/Env.h>

#include "DTU.h"
#include "Platform.h"
#include "WorkLoop.h"

namespace kernel {

class BaremetalKEnvBackend : public m3::BaremetalEnvBackend {
public:
    explicit BaremetalKEnvBackend() {
        _workloop = new WorkLoop();
    }

    virtual void init() override {
        // don't do that on gem5 because the kernel coreid is already set by gem5
#if !defined(__gem5__)
        m3::env()->coreid = DTU::get().log_to_phys(Platform::kernel_pe());
#endif

        m3::Serial::init("kernel", m3::env()->coreid);
    }

    virtual void reinit() override {
        // not used
    }

    virtual void exit(int) override {
    }
};

EXTERN_C void init_env(m3::Env *e) {
    e->backend = new BaremetalKEnvBackend();
}

}
