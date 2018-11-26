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

#pragma once

#include <base/Config.h>
#include <base/DTU.h>

#include <sys/un.h>
#include <poll.h>

namespace m3 {

class DTUBackend {
public:
    enum class Dest {
        DTU = 0,
        CU  = 1,
    };

    explicit DTUBackend();
    ~DTUBackend();

    void create() {
    }
    void destroy() {
    }

    bool has_command();
    epid_t has_msg();

    void notify(Dest dst);
    void wait(Dest dst);
    void send(peid_t pe, epid_t ep, const DTU::Buffer *buf);
    ssize_t recv(epid_t ep, DTU::Buffer *buf);

private:
    void poll();

    int _sock;
    int _pending;
    // the last two are used for DTU-CU notifications
    int _localsocks[EP_COUNT + 2];
    struct pollfd _fds[EP_COUNT + 1];
    sockaddr_un _endpoints[PE_COUNT * (EP_COUNT + 2)];
};

}
