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
#include <base/KIF.h>

namespace m3 {

/**
 * The base class for all object-capabilities. Manages the selector and capability.
 *
 * Note that it does NOT have a virtual destructor, so that you can't delete an object of any
 * derived class using a pointer to a base-class! But since the capabilities are quite different,
 * this does not make sense in most cases anyway. Considering how much it decreases code-size and
 * increases performance, it's worth it, I think.
 */
class ObjCap {
public:
    static const capsel_t INVALID   = KIF::INV_SEL;

    enum {
        // whether we don't want to free the capability
        KEEP_CAP        = 1 << 0,
    };

    enum {
        MEM_GATE,
        SEND_GATE,
        RECV_GATE,
        SERVICE,
        SESSION,
        VIRTPE,
        EPMASK,
    };

    /**
     * Constructor for a new capability with given selector. Does not actually create the cap. This
     * will be done in subclasses.
     *
     * @param sel the selector
     * @param flags control whether the selector and/or the capability should be free'd
     *  during destruction
     */
    explicit ObjCap(uint type, capsel_t sel = INVALID, uint flags = 0)
        : _sel(sel), _type(type), _flags(flags) {
    }

    // object-caps are non-copyable, because I think there are very few usecases
    ObjCap(const ObjCap&) = delete;
    ObjCap& operator=(const ObjCap&) = delete;

    // but moving is allowed
    ObjCap(ObjCap &&c) : _sel(c._sel), _type(c._type), _flags(c._flags) {
        // don't destroy anything with the old cap
        c._flags = KEEP_CAP;
    }
    ObjCap& operator=(ObjCap &&c) {
        if(&c != this) {
            _sel = c._sel;
            _type = c._type;
            _flags = c._flags;
            // don't destroy anything with the old cap
            c._flags = KEEP_CAP;
        }
        return *this;
    }

    /**
     * Destructor. Depending on the flags, it frees the selector and/or the capability (revoke).
     */
    ~ObjCap() {
        release();
    }

    /**
     * @return the selector for this capability
     */
    capsel_t sel() const {
        return _sel;
    }
    uint type() const {
        return _type;
    }

protected:
    /**
     * Sets the selector
     *
     * @param sel the new selector
     */
    void sel(capsel_t sel) {
        _sel = sel;
    }
    /**
     * @return the flags
     */
    unsigned flags() const {
        return _flags;
    }
    /**
     * Sets the flags but keeps the selector
     *
     * @param flags the new flags
     */
    void flags(unsigned flags) {
        _flags = flags;
    }

    /**
     * Releases the selector and cap, depending on the set flags
     */
    void release();

private:
    uint16_t _sel;
    uint8_t _type;
    uint8_t _flags;
} PACKED;

}
