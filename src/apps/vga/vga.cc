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

#include <m3/server/Server.h>
#include <m3/service/arch/host/VGA.h>
#include <m3/arch/host/SharedMemory.h>
#include <m3/cap/VPE.h>
#include <m3/WorkLoop.h>
#include <m3/Log.h>

using namespace m3;

class VGAHandler : public Handler<> {
public:
    explicit VGAHandler(MemGate *vgamem) : _vgamem(vgamem) {
    }

    virtual void handle_obtain(SessionData *, RecvBuf *, GateIStream &args, uint capcount) override {
        if(capcount != 1) {
            reply_vmsg_on(args, Errors::INV_ARGS);
            return;
        }

        reply_vmsg_on(args, Errors::NO_ERROR, CapRngDesc(CapRngDesc::OBJ, _vgamem->sel()));
    }

private:
    MemGate *_vgamem;
};

int main() {
    SharedMemory vgamem("vga", VGA::SIZE, SharedMemory::JOIN);
    MemGate memgate = VPE::self().mem().derive(
        reinterpret_cast<uintptr_t>(vgamem.addr()), VGA::SIZE, MemGate::RW);

    Server<VGAHandler> srv("vga", new VGAHandler(&memgate));
    if(Errors::occurred())
        PANIC("Unable to register service 'vga'");

    env()->backend->workloop->run();
    return 0;
}
