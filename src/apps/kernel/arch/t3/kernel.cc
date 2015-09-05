/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#include <m3/stream/Serial.h>
#include <m3/DTU.h>
#include <m3/Log.h>

#include "../../CapTable.h"
#include "../../PEManager.h"
#include "../../SyscallHandler.h"
#include "../../KWorkLoop.h"

using namespace m3;

class KernelEPSwitcher : public EPSwitcher {
public:
    virtual void switch_ep(size_t id, capsel_t, capsel_t newcap) override {
        if(newcap != Cap::INVALID) {
            MsgCapability *c = static_cast<MsgCapability*>(
                CapTable::kernel_table().get(newcap, Capability::MSG));
            // TODO use unlimited credits for the moment
            DTU::get().configure(id, c->obj->label, c->obj->core, c->obj->epid, 0xFFFF);
            LOG(IPC, "Kernel programs ep[" << id << "] to "
                << "core=" << c->obj->core << ", ep=" << c->obj->epid
                << ", lbl=" << fmt(c->obj->label, "#0x", sizeof(label_t) * 2)
                << ", credits=" << fmt(c->obj->credits, "#x"));
        }
    }
};

int main(int argc, char *argv[]) {
    Serial &ser = Serial::get();
    if(argc < 2) {
        ser << "Usage: " << argv[0] << " <program>...\n";
        return 1;
    }

    KernelEPSwitcher *epsw = new KernelEPSwitcher();
    EPMux::get().set_epswitcher(epsw);

    ser << "Initializing PEs...\n";

    PEManager::create(argc - 1, argv + 1);

    KWorkLoop::run();

    ser << "Shutting down...\n";

    PEManager::destroy();
    delete epsw;

    Machine::shutdown();
    return EXIT_SUCCESS;
}
