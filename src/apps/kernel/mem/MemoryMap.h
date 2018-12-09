/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

namespace kernel {

class MemoryMap {
    struct Area {
        goff_t addr;
        size_t size;
        Area *next;

        static void *operator new(size_t);
        static void operator delete(void *ptr);
    };

    struct Init {
        Init();
        static Init inst;
    };

    static const size_t MAX_AREAS   = 4096;

public:
    /**
     * Creates a memory-map of <size> bytes.
     *
     * @param addr the base address
     * @param size the mem size
     */
    explicit MemoryMap(goff_t addr, size_t size);

    /**
     * Destroys this map
     */
    ~MemoryMap();

    /**
     * Allocates an area in the given map, that is <size> bytes large.
     *
     * @param map the map
     * @param size the size of the area
     * @param align the desired alignment
     * @return the address of -1 if failed
     */
    goff_t allocate(size_t size, size_t align);

    /**
     * Frees the area at <addr> with <size> bytes.
     *
     * @param map the map
     * @param addr the address of the area
     * @param size the size of the area
     */
    void free(goff_t addr, size_t size);

    /**
     * Just for debugging/testing: Determines the total number of free bytes in the map
     *
     * @param map the map
     * @param areas will be set to the number of areas in the map
     * @return the free bytes
     */
    size_t get_size(size_t *areas = nullptr) const;

    friend m3::OStream &operator<<(m3::OStream &os, const MemoryMap &map) {
        size_t areas;
        os << "Total: " << (map.get_size(&areas) / 1024) << " KiB:\n";
        for(Area *a = map.list; a != nullptr; a = a->next)
            os << "\t@ " << m3::fmt(a->addr, "p") << ", " << (a->size / 1024) << " KiB\n";
        return os;
    }

private:
    Area *list;
    static Area *freelist;
    static Area areas[MAX_AREAS];
};

}
