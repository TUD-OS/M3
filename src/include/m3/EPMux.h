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

#include <m3/Common.h>
#include <m3/Config.h>
#include <assert.h>

namespace m3 {

class Gate;

/**
 * The endpoint multiplexer allows us to have more gates than endpoints by multiplexing
 * the endpoints among the gates.
 */
class EPMux {
    explicit EPMux();

public:
    /**
     * @return the EPMux instance
     */
    static EPMux &get() {
        return _inst;
    }

    /**
     * Reserves the given endpoint in the sense that it is not used for multiplexing. This is
     * necessary for receive gates, that need to stay on one endpoint all the time.
     *
     * @param ep the endpoint id
     */
    void reserve(size_t ep);

    /**
     * Configures an endpoint for the given gate. If necessary, a victim will be picked and removed
     * from an endpoint.
     *
     * @param gate the gate
     */
    void switch_to(Gate *gate);

    /**
     * If <gate> is already configured on some endpoint, it exchanges the configuration to use the
     * one from the capability <newcap>. If it is not configured somewhere, nothing happens.
     *
     * @param gate the gate
     * @param newcap the capability to use
     */
    void switch_cap(Gate *gate, capsel_t newcap);

    /**
     * Removes <gate> from the endpoint it is configured on, if any. If <invalidate> is true, the
     * kernel will invalidate the endpoint as well.
     *
     * @param gate the gate
     * @param invalidate whether to invalidate it, too
     */
    void remove(Gate *gate, bool invalidate);

    /**
     * Resets the state of the EP switcher.
     */
    void reset();

private:
    size_t select_victim();

    size_t _next_victim;
    Gate *_gates[EP_COUNT];
    static EPMux _inst;
};

}
