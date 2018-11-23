/*
 * Copyright (C) 2018, Sebastian Reimers <sebastian.reimers@mailbox.tu-dresden.de>
 * Copyright (C) 2017, Lukas Landgraf <llandgraf317@gmail.com>
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/col/SList.h>

#include <m3/com/MemGate.h>
#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>
#include <m3/session/ServerSession.h>

class DiskSrvSession : public m3::ServerSession {
    struct DiskSrvSGate : public m3::SListItem {
        explicit DiskSrvSGate(m3::SendGate &&_sgate) : sgate(m3::Util::move(_sgate)) {
        }
        m3::SendGate sgate;
    };

public:
    static constexpr size_t MSG_SIZE = 128;

    explicit DiskSrvSession(capsel_t srv_sel, m3::RecvGate *rgate, capsel_t _sel = m3::ObjCap::INVALID)
        : ServerSession(srv_sel, _sel), _rgate(rgate), _sgates(){};

    const m3::RecvGate &rgate() const {
        return *_rgate;
    }

    m3::Errors::Code get_sgate(m3::KIF::Service::ExchangeData &data) {
        if(data.caps != 1)
            return m3::Errors::INV_ARGS;

        label_t label       = reinterpret_cast<label_t>(this);
        DiskSrvSGate *sgate = new DiskSrvSGate(m3::SendGate::create(_rgate, label, MSG_SIZE));
        _sgates.append(sgate);

        data.caps = m3::KIF::CapRngDesc(m3::KIF::CapRngDesc::OBJ, sgate->sgate.sel()).value();
        return m3::Errors::NONE;
    }

private:
    m3::RecvGate *_rgate;
    m3::SList<DiskSrvSGate> _sgates;
};
