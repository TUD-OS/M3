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

#include <m3/session/Pager.h>
#include <m3/stream/Standard.h>
#include <m3/VPE.h>

using namespace m3;

int main() {
    uintptr_t virt = 0x20000000;

    VPE cc("child");
    cc.fds()->set(STDIN_FD, VPE::self().fds()->get(STDIN_FD));
    cc.fds()->set(STDOUT_FD, VPE::self().fds()->get(STDOUT_FD));
    cc.fds()->set(STDERR_FD, VPE::self().fds()->get(STDERR_FD));
    cc.obtain_fds();

    RecvGate rgate = RecvGate::create(nextlog2<512>::val, nextlog2<64>::val);
    SendGate sg = SendGate::create(&rgate, 0, 64);

    cc.delegate_obj(rgate.sel());
    cc.run([&rgate, virt] {
        int val;
        receive_vmsg(rgate, val);

        volatile int *nums = reinterpret_cast<volatile int*>(virt);

        while(nums[0] == 0 || nums[1] == 0)
            ;

        cout << nums[0] << ", " << nums[1] << "\n";
        return 0;
    });

    VPE::self().pager()->map_anon(&virt, 0x2000, Pager::Prot::RW, 0);

    volatile int *nums = reinterpret_cast<volatile int*>(virt);
    nums[0] = 0;

    // TODO hack: simply specify 1 page because the pager maps them together
    cc.delegate(KIF::CapRngDesc(KIF::CapRngDesc::MAP, virt >> PAGE_BITS, 1));
    send_vmsg(sg, 1);

    nums[0] = 4;
    nums[1] = 5;

    cc.wait();

    return 0;
}
