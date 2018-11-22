#pragma once

#include <m3/session/ServerSession.h>
#include <m3/com/MemGate.h>
#include <m3/com/SendGate.h>
#include <m3/com/RecvGate.h>
#include <base/col/SList.h>

using namespace m3;

class DiskSrvSession : public ServerSession {
        struct DiskSrvSGate : public m3::SListItem {
            explicit DiskSrvSGate(m3::SendGate &&_sgate) : sgate(m3::Util::move(_sgate)) {
            }
            m3::SendGate sgate;
        };

public:
        static constexpr size_t MSG_SIZE = 128;

        enum Operation {
                COUNT
        };

        explicit DiskSrvSession(capsel_t srv_sel, RecvGate *rgate, capsel_t _sel = ObjCap::INVALID)
                : ServerSession(srv_sel, _sel),
                  _rgate(rgate),
                  _sgates() {};

        const RecvGate &rgate() const {
                return *_rgate;
        }

        Errors::Code get_sgate(KIF::Service::ExchangeData &data) {
            if(data.caps != 1)
                return Errors::INV_ARGS;

            label_t label = reinterpret_cast<label_t>(this);
            DiskSrvSGate *sgate = new DiskSrvSGate(SendGate::create(_rgate, label, MSG_SIZE));
            _sgates.append(sgate);

            data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sgate->sgate.sel()).value();
            return Errors::NONE;
        }

private:
        RecvGate *_rgate;
        SList<DiskSrvSGate> _sgates;
};
