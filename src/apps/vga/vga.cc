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

#include <base/arch/host/SharedMemory.h>

#include <m3/server/Server.h>
#include <m3/session/arch/host/VGA.h>
#include <m3/stream/Standard.h>
#include <m3/VPE.h>

using namespace m3;

class VGAHandler : public Handler<void> {
public:
    explicit VGAHandler(MemGate *vgamem) : _vgamem(vgamem) {
    }

    virtual Errors::Code open(void **sess, word_t) override {
        *sess = nullptr;
        return Errors::NONE;
    }
    virtual Errors::Code obtain(void *, KIF::Service::ExchangeData &data) override {
        if(data.caps != 1 || data.args.count != 0)
            return Errors::INV_ARGS;

        KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, _vgamem->sel());
        data.caps = crd.value();
        return Errors::NONE;
    }
    virtual Errors::Code close(void *) override {
        return Errors::NONE;
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
        exitmsg("Unable to register service 'vga'");

    env()->workloop()->run();
    return 0;
}
