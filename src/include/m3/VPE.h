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

#include <base/util/BitField.h>
#include <base/util/Math.h>
#include <base/util/String.h>
#include <base/ELF.h>
#include <base/Errors.h>
#include <base/KIF.h>
#include <base/PEDesc.h>

#include <m3/com/MemGate.h>
#include <m3/ObjCap.h>

#include <functional>

namespace m3 {

class VFS;
class FileTable;
class MountTable;
class Pager;
class FStream;
class EnvUserBackend;
class RecvGate;
class Session;

/**
 * Represents a virtual processing element which has been assigned to a PE. It will be under your
 * control in the sense that you can run arbitrary programs on it, exchange capabilities, wait until
 * a program on it finished and so on. You can also execute multiple programs in a row on it.
 *
 * Note that you have an object for your own VPE, but you can't use it to exchange capabilities or
 * execute programs in it. You can access the memory to derive sub areas from it, though.
 */
class VPE : public ObjCap {
    friend class EnvUserBackend;
    friend class CapRngDesc;
    friend class RecvGate;
    friend class Session;
    friend class VFS;

    static const size_t BUF_SIZE;

public:
    /**
     * The first available selector
     */
    static constexpr uint SEL_START     = 2 + (EP_COUNT - DTU::FIRST_FREE_EP);

    explicit VPE();

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
     * @param pe the desired PE type (default: same as the current PE)
     * @param pager the pager (optional)
     * @param tmuxable whether this VPE can share a PE with others
     */
    explicit VPE(const String &name, const PEDesc &pe = VPE::self().pe(), const char *pager = nullptr, bool tmuxable = false);
    virtual ~VPE();

    /**
     * @return the PE description this VPE has been assigned to
     */
    const PEDesc &pe() const {
        return _pe;
    }

    capsel_t ep_sel(epid_t ep) {
        return sel() + 2 + ep - DTU::FIRST_FREE_EP;
    }

    /**
     * @return the pager of this VPE (or nullptr)
     */
    Pager *pager() {
        return _pager;
    }

    /**
     * @return the mount table
     */
    MountTable *mounts() {
        return _ms;
    }

    /**
     * Clones the given mount table into this VPE.
     *
     * @param ms the mount table
     */
    void mounts(const MountTable &ms);

    /**
     * Lets this VPE obtain all mount points in its mount table, i.e., the required capability
     * exchanges are performed.
     *
     * @return the error, if any
     */
    Errors::Code obtain_mounts();

    /**
     * @return the file descriptors
     */
    FileTable *fds() {
        return _fds;
    }

    /**
     * Clones the given file descriptors into this VPE. Note that the file descriptors depend
     * on the mount table, so that you should always prepare the mount table first.
     *
     * @param fds the file descriptors
     */
    void fds(const FileTable &fds);

    /**
     * Lets this VPE obtain all files in its file table, i.e., the required capability exchanges
     * are performed.
     *
     * @return the error, if any
     */
    Errors::Code obtain_fds();

    /**
     * Allocates capability selectors.
     *
     * @param count the number of selectors
     * @return the first one
     */
    capsel_t alloc_sels(uint count) {
        _next_sel += count;
        return _next_sel - count;
    }
    capsel_t alloc_sel() {
        return _next_sel++;
    }

    /**
     * Allocates an endpoint.
     *
     * @return the endpoint id or 0 if there is no free EP
     */
    epid_t alloc_ep();

    /**
     * @param id the endpoint id
     * @return true if the endpoint is free
     */
    bool is_ep_free(epid_t id) {
        return id >= DTU::FIRST_FREE_EP && (_eps & (static_cast<uint64_t>(1) << id)) == 0;
    }

    /**
     * Frees the given endpoint
     *
     * @param id the endpoint id
     */
    void free_ep(epid_t id) {
        _eps &= ~(static_cast<uint64_t>(1) << id);
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
     * Delegates the given object capability to this VPE.
     *
     * @param sel the selector
     * @return the error code
     */
    Errors::Code delegate_obj(capsel_t sel) {
        return delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sel));
    }

    /**
     * Delegates the given range of capabilities to this VPE. They are put at the same selectors.
     *
     * @param crd the capabilities of your to VPE to delegate to this VPE
     * @return the error code
     */
    Errors::Code delegate(const KIF::CapRngDesc &crd) {
        return delegate(crd, crd.start());
    }

    /**
     * Delegates the given range of capabilities to this VPE at position <dest>.
     *
     * @param crd the capabilities of your to VPE to delegate to this VPE
     * @param dest the destination in this VPE
     * @return the error code
     */
    Errors::Code delegate(const KIF::CapRngDesc &crd, capsel_t dest);

    /**
     * Obtains the given range of capabilities from this VPE to your VPE. The selectors are
     * automatically chosen.
     *
     * @param crd the capabilities of this VPE to delegate to your VPE
     * @return the error code
     */
    Errors::Code obtain(const KIF::CapRngDesc &crd);

    /**
     * Obtains the given range of capabilities from this VPE to your VPE at position <dest>.
     *
     * @param crd the capabilities of this VPE to delegate to your VPE
     * @param dest the destination in your VPE
     * @return the error code
     */
    Errors::Code obtain(const KIF::CapRngDesc &crd, capsel_t dest);

    /**
     * Revokes the given range of capabilities from this VPE.
     *
     * @param crd the capabilities to revoke
     * @param delonly whether to revoke delegations only
     * @return the error code
     */
    Errors::Code revoke(const KIF::CapRngDesc &crd, bool delonly = false);

    /**
     * Starts the VPE, i.e., prepares the PE for execution and wakes it up.
     *
     * @return the error if any occurred
     */
    Errors::Code start();

    /**
     * Stops the VPE, i.e., if it is running, the execution is stopped.
     *
     * @return the error if any occurred
     */
    Errors::Code stop();

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
     * @return the error if any occurred
     */
    Errors::Code exec(int argc, const char **argv);

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
    void mark_caps_allocated(capsel_t sel, uint count) {
        _next_sel = Math::max(_next_sel, sel + count);
    }

    void init_state();
    void init_fs();
    Errors::Code run(void *lambda);
    Errors::Code load_segment(ElfPh &pheader, char *buffer);
    Errors::Code load(int argc, const char **argv, uintptr_t *entry, char *buffer, size_t *size);
    void clear_mem(char *buffer, size_t count, uintptr_t dest);
    size_t store_arguments(char *buffer, int argc, const char **argv);

    uintptr_t get_entry();
    static bool skip_section(ElfPh *ph);
    Errors::Code copy_sections();

    PEDesc _pe;
    MemGate _mem;
    capsel_t _next_sel;
    uint64_t _eps;
    Pager *_pager;
    uint64_t _rbufcur;
    uint64_t _rbufend;
    MountTable *_ms;
    FileTable *_fds;
    FStream *_exec;
    bool _tmuxable;
    static VPE _self;
};

}
