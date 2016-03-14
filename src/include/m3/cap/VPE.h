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

#include <m3/cap/ObjCap.h>
#include <m3/cap/MemGate.h>
#include <m3/util/String.h>
#include <m3/BitField.h>
#include <m3/CapRngDesc.h>
#include <m3/ELF.h>
#include <m3/Errors.h>
#include <functional>

namespace m3 {

class VFS;
class Pager;
class FStream;
class Executable;

/**
 * Represents a virtual processing element which has been assigned to a PE. It will be under your
 * control in the sense that you can run arbitrary programs on it, exchange capabilities, wait until
 * a program on it finished and so on. You can also execute multiple programs in a row on it.
 *
 * Note that you have an object for your own VPE, but you can't use it to exchange capabilities or
 * execute programs in it. You can access the memory to derive sub areas from it, though.
 */
class VPE : public ObjCap {
    friend struct Env;
    friend class CapRngDesc;
    friend class VFS;

    static constexpr uint SEL_START     = 2;
    static constexpr uint SEL_COUNT     = CAP_TOTAL - SEL_START;
    static const size_t BUF_SIZE;

public:
    /**
     * The total number of selectors, i.e. starting at 0
     */
    static constexpr uint SEL_TOTAL     = CAP_TOTAL;

    // don't revoke these. they kernel does so on exit
    explicit VPE()
        : ObjCap(VIRTPE, 0, KEEP_SEL | KEEP_CAP), _mem(MemGate::bind(1)),
          _caps(), _eps(), _pager(), _mounts(), _mountlen() {
        init_state();
        init();
    }

public:
    /**
     * @return your own VPE
     */
    static VPE &self() {
        return _self;
    }

    /**
     * Creates a new VPE and assigns the given name to it.
     *
     * @param name the VPE name
     * @param core the core type that is required (empty = default core)
     * @param pager the pager (optional)
     */
    explicit VPE(const String &name, const String &core = String(), const char *pager = nullptr);
    ~VPE();

    /**
     * @return the pager of this VPE (or nullptr)
     */
    Pager *pager() {
        return _pager;
    }

    /**
     * Allocates capability selectors.
     *
     * @param count the number of caps
     * @return the first one
     */
    capsel_t alloc_caps(uint count);
    capsel_t alloc_cap() {
        return alloc_caps(1);
    }

    /**
     * @param sel the selector
     * @return true if the selector/capability is free
     */
    bool is_cap_free(capsel_t sel) {
        return !_caps->is_set(sel);
    }

    /**
     * Frees the given capability selectors
     *
     * @param start the start selector
     * @param count the number of selectors
     */
    void free_caps(capsel_t start, uint count) {
        while(count-- > 0)
            _caps->clear(start + count);
    }
    void free_cap(capsel_t cap) {
        free_caps(cap, 1);
    }

    /**
     * Allocates an endpoint.
     *
     * @return the endpoint id
     */
    size_t alloc_ep();

    /**
     * @param id the endpoint id
     * @return true if the endpoint is free
     */
    bool is_ep_free(size_t id) {
        return !_eps->is_set(id);
    }

    /**
     * Frees the given endpoint
     *
     * @param id the endpoint id
     */
    void free_ep(size_t id) {
        _eps->clear(id);
    }

    /**
     * @return the local memory of the PE this VPE is attached to
     */
    MemGate &mem() {
        return _mem;
    }
    const MemGate &mem() const {
        return _mem;
    }

    /**
     * Delegates the current mounts to this VPE. Note that this can only be done once.
     */
    void delegate_mounts();

    /**
     * Delegates the given object capability to this VPE.
     *
     * @param sel the selector
     */
    void delegate_obj(capsel_t sel) {
        delegate(CapRngDesc(CapRngDesc::OBJ, sel));
    }

    /**
     * Delegates the given range of capabilities to this VPE. They are put at the same selectors.
     *
     * @param crd the capabilities of your to VPE to delegate to this VPE
     */
    void delegate(const CapRngDesc &crd);

    /**
     * Obtains the given range of capabilities from this VPE to your VPE. The selectors are
     * automatically chosen.
     *
     * @param crd the capabilities of this VPE to delegate to your VPE
     */
    void obtain(const CapRngDesc &crd);

    /**
     * Obtains the given range of capabilities from this VPE to your VPE at position <dest>.
     *
     * @param crd the capabilities of this VPE to delegate to your VPE
     * @param dest the destination in your VPE
     */
    void obtain(const CapRngDesc &crd, capsel_t dest);

    /**
     * Waits until the currently executing program on this VPE is finished
     *
     * @return the exitcode
     */
    int wait();

    /**
     * Executes the given program on this VPE.
     *
     * @param argc the number of arguments to pass to the program
     * @param argv the arguments to pass (argv[0] is the executable)
     */
    Errors::Code exec(int argc, const char **argv);

    /**
     * Executes the given executable on this VPE.
     *
     * @param exec the executable to run
     */
    Errors::Code exec(Executable &exec);

    /**
     * Clones this program onto this VPE and executes the given function.
     *
     * @param f the function to execute
     * @return the error if any occurred
     */
    Errors::Code run(std::function<int()> f) {
        std::function<int()> *copy = new std::function<int()>(f);
        Errors::Code res = run(copy);
        delete copy;
        return res;
    }

private:
    void init();
    void init_state();
    Errors::Code run(void *lambda);
    Errors::Code load_segment(Executable &exec, ElfPh &pheader, char *buffer);
    Errors::Code load(Executable &exec, uintptr_t *entry, char *buffer, size_t *size);
    void clear_mem(char *buffer, size_t count, uintptr_t dest);

    uintptr_t get_entry();
    static word_t get_sp();
    static bool skip_section(ElfPh *ph);
    void copy_sections();

    MemGate _mem;
    BitField<SEL_TOTAL> *_caps;
    BitField<EP_COUNT> *_eps;
    Pager *_pager;
    void *_mounts;
    size_t _mountlen;
    static VPE _self;
};

}
