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

#include <base/tracing/Tracing.h>
#include <base/KIF.h>

#include <m3/com/Gate.h>

namespace m3 {

class VPE;

/**
 * A memory gate is used to access PE-external memory via the DTU. You can either create a MemGate
 * by requesting PE-external memory from the kernel or bind a MemGate to an existing capability.
 */
class MemGate : public Gate {
    explicit MemGate(uint flags, capsel_t cap)
        : Gate(MEM_GATE, cap, flags),
          _cmdflags() {
    }

public:
    static const int R = KIF::Perm::R;
    static const int W = KIF::Perm::W;
    static const int X = KIF::Perm::X;
    static const int RW = R | W;
    static const int RWX = R | W | X;
    static const int PERM_BITS = 3;

    enum CmdFlags {
        /**
         * Pagefaults result in an abort
         */
        CMD_NOPF = DTU::CmdFlags::NOPF,
    };

    /**
     * Creates a new memory gate for global memory. That is, it requests <size> bytes of global
     * memory with given permissions.
     *
     * @param size the memory size
     * @param perms the permissions (see MemGate::RWX)
     * @param sel the selector to use (if != INVALID, the selector is NOT freed on destruction)
     * @return the memory gate
     */
    static MemGate create_global(size_t size, int perms, capsel_t sel = INVALID) {
        return create_global_for(static_cast<goff_t>(-1), size, perms, sel);
    }

    /**
     * Creates a new memory-gate for the global memory [addr..addr+size) with given permissions.
     *
     * @param addr the address
     * @param size the memory size
     * @param perms the permissions (see MemGate::RWX)
     * @param sel the selector to use (if != INVALID, the selector is NOT freed on destruction)
     * @return the memory gate
     */
    static MemGate create_global_for(goff_t addr, size_t size, int perms, capsel_t sel = INVALID);

    /**
     * Binds this gate for read/write/cmpxchg to the given memory capability. That is, the
     * capability should be a memory capability you've received from somebody else.
     *
     * @param sel the capability selector
     * @param flags the flags to control whether cap/selector are kept (default: both)
     */
    static MemGate bind(capsel_t sel, uint flags = ObjCap::KEEP_CAP) {
        return MemGate(flags, sel);
    }

    /**
     * Activates this gate for <vpe> at EP <ep> with the given offset. That is, the EP <ep> will be
     * configured to start at this memory region plus <offset>.
     *
     * @param vpe the VPE to activate it for
     * @param ep the ep id
     * @param offset the offset within this memory region
     * @return the result
     */
    Errors::Code activate_for(VPE &vpe, epid_t ep, goff_t offset = 0);

    /**
     * @return the command flags
     */
    uint cmdflags() const {
        return _cmdflags;
    }
    /**
     * Sets the given command flags
     */
    void cmdflags(uint flags) {
        _cmdflags = flags;
    }

    /**
     * Derives memory from this memory gate. That is, it creates a new memory capability that is
     * bound to a subset of this memory (in space or permissions).
     *
     * @param offset the offset inside this memory capability
     * @param size the size of the memory area
     * @param perms the permissions (you can only downgrade)
     * @return the new memory gate
     */
    MemGate derive(goff_t offset, size_t size, int perms = RWX) const;

    /**
     * Derives memory from this memory gate and uses <sel> for it. That is, it creates a new memory
     * capability that is bound to a subset of this memory (in space or permissions).
     *
     * @param sel the capability selector to use
     * @param offset the offset inside this memory capability
     * @param size the size of the memory area
     * @param perms the permissions (you can only downgrade)
     * @return the new memory gate
     */
    MemGate derive(capsel_t sel, goff_t offset, size_t size, int perms = RWX) const;

    /**
     * Writes the <len> bytes at <data> to <offset>.
     *
     * @param data the data to write
     * @param len the number of bytes to write
     * @param offset the start-offset
     * @return the error code or Errors::NONE
     */
    Errors::Code write(const void *data, size_t len, goff_t offset);

    /**
     * Reads <len> bytes from <offset> into <data>.
     *
     * @param data the buffer to write into
     * @param len the number of bytes to read
     * @param offset the start-offset
     * @return the error code or Errors::NONE
     */
    Errors::Code read(void *data, size_t len, goff_t offset);

private:
    Errors::Code forward(void *&data, size_t &len, goff_t &offset, uint flags);

    uint _cmdflags;
};

}
