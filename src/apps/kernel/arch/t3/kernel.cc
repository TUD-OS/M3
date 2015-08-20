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
#include <m3/stream/IStringStream.h>
#include <m3/cap/SendGate.h>
#include <m3/ChanMng.h>
#include <m3/WorkLoop.h>
#include <m3/GateStream.h>
#include <m3/DTU.h>
#include <m3/Log.h>
#include <stdarg.h>
#include <cstring>
#include <assert.h>

#include "../../CapTable.h"
#include "../../PEManager.h"
#include "../../SyscallHandler.h"
#include "../../KWorkLoop.h"

using namespace m3;

class KernelChanSwitcher : public ChanSwitcher {
public:
    virtual void switch_chan(size_t id, capsel_t, capsel_t newcap) override {
        if(newcap != Cap::INVALID) {
            MsgCapability *c = static_cast<MsgCapability*>(
                CapTable::kernel_table().get(newcap, Capability::MSG));
            // TODO use unlimited credits for the moment
            DTU::get().config_remote(id, c->obj->core, c->obj->chanid, 0xFFFF, 0);
            DTU::get().config_label(id, c->obj->label);
            LOG(IPC, "Kernel programs chan[" << id << "] to "
                << "core=" << c->obj->core << ", chan=" << c->obj->chanid
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

    KernelChanSwitcher *chsw = new KernelChanSwitcher();
    ChanMng::get().set_chanswitcher(chsw);

    ser << "Initializing PEs...\n";

    PEManager::create(argc - 1, argv + 1);

    KWorkLoop::run();

    ser << "Shutting down...\n";

    PEManager::destroy();
    delete chsw;

    Machine::shutdown();
    return EXIT_SUCCESS;
}
