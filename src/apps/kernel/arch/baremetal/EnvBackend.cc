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
#include "WorkLoop.h"

namespace kernel {

class BaremetalKEnvBackend : public m3::BaremetalEnvBackend {
public:
    explicit BaremetalKEnvBackend() {
        _workloop = new WorkLoop();
    }

    virtual void init() override {
        m3::env()->coreid = DTU::get().log_to_phys(KERNEL_CORE);

        m3::Serial::init("kernel", DTU::get().log_to_phys(KERNEL_CORE));
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
