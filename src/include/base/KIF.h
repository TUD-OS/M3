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

#include <base/Common.h>
#include <base/stream/OStream.h>
#include <base/Errors.h>

namespace m3 {

/**
 * The kernel interface
 */
struct KIF {
    KIF() = delete;

    /**
     * Represents an invalid selector
     */
    static const capsel_t INV_SEL       = 0xFFFF;

    /**
     * Represents unlimited credits
     */
    static const word_t UNLIM_CREDITS   = 0xFFFF;

    /**
     * The maximum message length that can be used
     */
    static const size_t MAX_MSG_SIZE    = 440;

    /**
     * The permissions for MemGate
     */
    struct Perm {
        static const int R = 1;
        static const int W = 2;
        static const int X = 4;
        static const int RW = R | W;
        static const int RWX = R | W | X;
    };

    enum VPEFlags {
        // whether the PE can be shared with others
        MUXABLE     = 1,
        // whether this VPE gets pinned on one PE
        PINNED      = 2,
    };

    struct CapRngDesc {
        typedef xfer_t value_type;

        enum Type {
            OBJ,
            MAP,
        };

        explicit CapRngDesc() : CapRngDesc(OBJ, 0, 0) {
        }
        explicit CapRngDesc(value_type value)
            : _value(value) {
        }
        explicit CapRngDesc(Type type, capsel_t start, capsel_t count = 1)
            : _value(static_cast<value_type>(type) |
                    (static_cast<value_type>(start) << 33) |
                    (static_cast<value_type>(count) << 1)) {
        }

        value_type value() const {
            return _value;
        }
        Type type() const {
            return static_cast<Type>(_value & 1);
        }
        capsel_t start() const {
            return _value >> 33;
        }
        capsel_t count() const {
            return (_value >> 1) & 0xFFFFFFFF;
        }

        friend OStream &operator <<(OStream &os, const CapRngDesc &crd) {
            os << "CRD[" << (crd.type() == OBJ ? "OBJ" : "MAP") << ":"
               << crd.start() << ":" << crd.count() << "]";
            return os;
        }

    private:
        value_type _value;
    };

    struct DefaultReply {
        xfer_t error;
    } PACKED;

    struct DefaultRequest {
        xfer_t opcode;
    } PACKED;

    struct ExchangeArgs {
        xfer_t count;
        union {
            xfer_t vals[8];
            struct {
                xfer_t svals[2];
                char str[48];
            } PACKED;
        } PACKED;
    } PACKED;

    /**
     * System calls
     */
    struct Syscall {
        enum Operation {
            // sent by the DTU if the PF handler is not reachable
            PAGEFAULT = 0,

            // capability creations
            CREATE_SRV,
            CREATE_SESS,
            CREATE_RGATE,
            CREATE_SGATE,
            CREATE_MGATE,
            CREATE_MAP,
            CREATE_VPEGRP,
            CREATE_VPE,

            // capability operations
            ACTIVATE,
            SRV_CTRL,
            VPE_CTRL,
            VPE_WAIT,
            DERIVE_MEM,
            OPEN_SESS,

            // capability exchange
            DELEGATE,
            OBTAIN,
            EXCHANGE,
            REVOKE,

            // forwarding
            FORWARD_MSG,
            FORWARD_MEM,
            FORWARD_REPLY,

            // misc
            NOOP,

            COUNT
        };

        enum VPEOp {
            VCTRL_INIT,
            VCTRL_START,
            VCTRL_YIELD,
            VCTRL_STOP,
        };
        enum SrvOp {
            SCTRL_SHUTDOWN,
        };

        struct Pagefault : public DefaultRequest {
            xfer_t virt;
            xfer_t access;
        } PACKED;

        struct CreateSrv : public DefaultRequest {
            xfer_t dst_sel;
            xfer_t vpe_sel;
            xfer_t rgate_sel;
            xfer_t namelen;
            char name[32];
        } PACKED;

        struct CreateSess : public DefaultRequest {
            xfer_t dst_sel;
            xfer_t srv_sel;
            xfer_t ident;
        } PACKED;

        struct CreateRGate : public DefaultRequest {
            xfer_t dst_sel;
            xfer_t order;
            xfer_t msgorder;
        } PACKED;

        struct CreateSGate : public DefaultRequest {
            xfer_t dst_sel;
            xfer_t rgate_sel;
            xfer_t label;
            xfer_t credits;
        } PACKED;

        struct CreateMGate : public DefaultRequest {
            xfer_t dst_sel;
            xfer_t addr;
            xfer_t size;
            xfer_t perms;
        } PACKED;

        struct CreateMap : public DefaultRequest {
            xfer_t dst_sel;
            xfer_t vpe_sel;
            xfer_t mgate_sel;
            xfer_t first;
            xfer_t pages;
            xfer_t perms;
        } PACKED;

        struct CreateVPEGrp : public DefaultRequest {
            xfer_t dst_sel;
        } PACKED;

        struct CreateVPE : public DefaultRequest {
            xfer_t dst_crd;
            xfer_t sgate_sel;
            xfer_t pe;
            xfer_t sep;
            xfer_t rep;
            xfer_t flags;
            xfer_t group_sel;
            xfer_t namelen;
            char name[32];
        } PACKED;

        struct CreateVPEReply : public DefaultReply {
            xfer_t pe;
        } PACKED;

        struct Activate : public DefaultRequest {
            xfer_t ep_sel;
            xfer_t gate_sel;
            xfer_t addr;
        } PACKED;

        struct SrvCtrl : public DefaultRequest {
            xfer_t srv_sel;
            xfer_t op;
        } PACKED;

        struct VPECtrl : public DefaultRequest {
            xfer_t vpe_sel;
            xfer_t op;
            xfer_t arg;
        } PACKED;

        struct VPEWait : public DefaultRequest {
            xfer_t vpe_count;
            xfer_t sels[16];
        } PACKED;

        struct VPEWaitReply : public DefaultReply {
            xfer_t vpe_sel;
            xfer_t exitcode;
        } PACKED;

        struct DeriveMem : public DefaultRequest {
            xfer_t dst_sel;
            xfer_t src_sel;
            xfer_t offset;
            xfer_t size;
            xfer_t perms;
        } PACKED;

        struct OpenSess : public DefaultRequest {
            xfer_t dst_sel;
            xfer_t arg;
            xfer_t namelen;
            char name[32];
        } PACKED;

        struct Exchange : public DefaultRequest {
            xfer_t vpe_sel;
            xfer_t own_crd;
            xfer_t other_sel;
            xfer_t obtain;
        } PACKED;

        struct ExchangeSess : public DefaultRequest {
            xfer_t vpe_sel;
            xfer_t sess_sel;
            xfer_t crd;
            ExchangeArgs args;
        } PACKED;

        struct ExchangeSessReply : public DefaultReply {
            ExchangeArgs args;
        } PACKED;

        struct Revoke : public DefaultRequest {
            xfer_t vpe_sel;
            xfer_t crd;
            xfer_t own;
        } PACKED;

        struct ForwardMsg : public DefaultRequest {
            xfer_t sgate_sel;
            xfer_t rgate_sel;
            xfer_t len;
            xfer_t rlabel;
            xfer_t event;
            char msg[MAX_MSG_SIZE];
        };

        struct ForwardMem : public DefaultRequest {
            enum Flags {
                NOPF    = 1,
                WRITE   = 2,
            };

            xfer_t mgate_sel;
            xfer_t len;
            xfer_t offset;
            xfer_t flags;
            xfer_t event;
            char data[MAX_MSG_SIZE];
        };

        struct ForwardMemReply : public DefaultReply {
            char data[MAX_MSG_SIZE];
        };

        struct ForwardReply : public DefaultRequest {
            xfer_t rgate_sel;
            xfer_t msgaddr;
            xfer_t len;
            xfer_t event;
            char msg[MAX_MSG_SIZE];
        };

        struct Noop : public DefaultRequest {
        } PACKED;
    };

    /**
     * Service calls
     */
    struct Service {
        enum Operation {
            OPEN,
            OBTAIN,
            DELEGATE,
            CLOSE,
            SHUTDOWN
        };

        struct Open : public DefaultRequest {
            xfer_t arg;
        } PACKED;

        struct OpenReply : public DefaultReply {
            xfer_t sess;
        } PACKED;

        struct ExchangeData {
            xfer_t caps;
            ExchangeArgs args;
        } PACKED;

        struct Exchange : public DefaultRequest {
            xfer_t sess;
            ExchangeData data;
        } PACKED;

        struct ExchangeReply : public DefaultReply {
            ExchangeData data;
        } PACKED;

        struct Close : public DefaultRequest {
            xfer_t sess;
        } PACKED;

        struct Shutdown : public DefaultRequest {
        } PACKED;
    };

    /**
     * Upcalls
     */
    struct Upcall {
        enum Operation {
            NOTIFY,
        };

        struct Notify : public DefaultRequest {
            xfer_t error;
            xfer_t event;
        } PACKED;
    };
};

}
