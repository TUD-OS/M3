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
    static const word_t UNLIM_CREDITS   = -1;

    /**
     * The maximum message length that can be used
     */
    static const word_t MAX_MSG_SIZE    = 440;

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

    struct CapRngDesc {
        typedef uint64_t value_type;

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
            : _value(type |
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
        word_t error;
    } PACKED;

    struct DefaultRequest {
        word_t opcode;
    } PACKED;

    /**
     * System calls
     */
    struct Syscall {
        enum Operation {
            PAGEFAULT = 0,  // sent by the DTU if the PF handler is not reachable
            CREATESRV,
            CREATESESS,
            CREATESESSAT,
            CREATEGATE,
            CREATEVPE,
            CREATEMAP,
            ATTACHRB,
            DETACHRB,
            EXCHANGE,
            VPECTRL,
            DELEGATE,
            OBTAIN,
            ACTIVATE,
            FORWARDMSG,
            FORWARDMEM,
            FORWARDREPLY,
            REQMEM,
            DERIVEMEM,
            REVOKE,
            EXIT,
            NOOP,
            IDLE,
            INIT,
            COUNT
        };

        enum VPEOp {
            VCTRL_START,
            VCTRL_STOP,
            VCTRL_WAIT,
        };

        struct Pagefault : public DefaultRequest {
            word_t virt;
            word_t access;
        } PACKED;

        struct Activate : public DefaultRequest {
            word_t ep;
            word_t cap;
        } PACKED;

        struct ForwardMsg : public DefaultRequest {
            word_t cap;
            word_t len;
            word_t repid;
            word_t rlabel;
            word_t event;
            char msg[MAX_MSG_SIZE];
        };

        struct ForwardMem : public DefaultRequest {
            enum Flags {
                NOPF    = 1,
                WRITE   = 2,
            };

            word_t cap;
            word_t len;
            word_t offset;
            word_t flags;
            word_t event;
            char data[MAX_MSG_SIZE];
        };

        struct ForwardMemReply : public DefaultReply {
            char data[MAX_MSG_SIZE];
        };

        struct ForwardReply : public DefaultRequest {
            word_t epid;
            word_t msgaddr;
            word_t len;
            word_t event;
            char msg[MAX_MSG_SIZE];
        };

        struct CreateSrv : public DefaultRequest {
            word_t srv;
            word_t label;
            word_t namelen;
            char name[32];
        } PACKED;

        struct CreateSess : public DefaultRequest {
            word_t vpe;
            word_t sess;
            word_t arg;
            word_t namelen;
            char name[32];
        } PACKED;

        struct CreateSessAt : public DefaultRequest {
            word_t srv;
            word_t sess;
            word_t ident;
        } PACKED;

        struct CreateGate : public DefaultRequest {
            word_t vpe;
            word_t gate;
            word_t label;
            word_t ep;
            word_t credits;
        } PACKED;

        struct CreateVPE : public DefaultRequest {
            word_t vpe;
            word_t mem;
            word_t gate;
            word_t pe;
            word_t ep;
            word_t muxable;
            word_t namelen;
            char name[32];
        } PACKED;

        struct CreateVPEReply : public DefaultReply {
            word_t pe;
        } PACKED;

        struct CreateMap : public DefaultRequest {
            word_t vpe;
            word_t mem;
            word_t first;
            word_t pages;
            word_t dest;
            word_t perms;
        } PACKED;

        struct AttachRB : public DefaultRequest {
            word_t vpe;
            word_t addr;
            word_t ep;
            word_t order;
            word_t msgorder;
        } PACKED;

        struct DetachRB : public DefaultRequest {
            word_t vpe;
            word_t ep;
        } PACKED;

        struct VPECtrl : public DefaultRequest {
            word_t vpe;
            word_t op;
            word_t pid;
        } PACKED;

        struct VPECtrlReply : public DefaultReply {
            word_t exitcode;
        } PACKED;

        struct Exchange : public DefaultRequest {
            word_t vpe;
            word_t own;
            word_t other;
            word_t obtain;
        } PACKED;

        struct ExchangeSess : public DefaultRequest {
            word_t vpe;
            word_t sess;
            word_t caps;
            word_t argcount;
            word_t args[8];
        } PACKED;

        struct ExchangeSessReply : public DefaultReply {
            word_t argcount;
            word_t args[8];
        } PACKED;

        struct Revoke : public DefaultRequest {
            word_t vpe;
            word_t crd;
            word_t own;
        } PACKED;

        struct ReqMem : public DefaultRequest {
            word_t mem;
            word_t addr;
            word_t size;
            word_t perms;
        } PACKED;

        struct DeriveMem : public DefaultRequest {
            word_t src;
            word_t dst;
            word_t offset;
            word_t size;
            word_t perms;
        } PACKED;

        struct Exit : public DefaultRequest {
            word_t exitcode;
        } PACKED;

        struct Noop : public DefaultRequest {
        } PACKED;

        struct Init : public DefaultRequest {
            word_t eps;
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
            word_t arg;
        } PACKED;

        struct OpenReply : public DefaultReply {
            word_t sess;
        } PACKED;

        struct ExchangeData {
            word_t caps;
            word_t argcount;
            word_t args[8];
        } PACKED;

        struct Exchange : public DefaultRequest {
            word_t sess;
            ExchangeData data;
        } PACKED;

        struct ExchangeReply : public DefaultReply {
            ExchangeData data;
        } PACKED;

        struct Close : public DefaultRequest {
            word_t sess;
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
            word_t error;
            word_t event;
        } PACKED;
    };
};

}
