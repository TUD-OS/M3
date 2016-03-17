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

#include <m3/cap/RecvGate.h>
#include <m3/Env.h>
#include <m3/Syscalls.h>
#include <m3/GateStream.h>
#include <m3/DTU.h>
#include <m3/Log.h>

#include <fstream>
#include <unistd.h>
#include <fcntl.h>

#ifndef NDEBUG
volatile int wait_for_debugger = 1;
#endif

namespace m3 {

Env *Env::_inst = nullptr;
Env::Init Env::_init INIT_PRIORITY(107);
Env::PostInit Env::_postInit INIT_PRIORITY(109);
char Env::_exec[128];
char Env::_exec_short[128];
const char *Env::_exec_short_ptr = nullptr;

static void stop_dtu() {
    DTU::get().stop();
    pthread_join(DTU::get().tid(), nullptr);
}

static void on_exit_func(int status, void *) {
    Syscalls::get().exit(status);
    stop_dtu();
}

static void load_params(Env *e) {
    char path[64];
    snprintf(path, sizeof(path), "/tmp/m3/%d", getpid());
    std::ifstream in(path);
    if(!in.good())
        PANIC("Unable to read " << path);

    int coreid;
    size_t epid;
    word_t credits;
    label_t lbl;
    std::string shm_prefix;
    in >> shm_prefix >> coreid >> lbl >> epid >> credits;

    e->set_params(coreid, shm_prefix, lbl, epid, credits);
}

EXTERN_C WEAK void init_env() {
    int logfd = open("run/log.txt", O_WRONLY | O_APPEND);

    new Env(new HostEnvBackend(), logfd);
    load_params(env());

    // use on_exit to get the return-value of main and pass it to the m3 kernel
    on_exit(on_exit_func, nullptr);
}

HostEnvBackend::HostEnvBackend() {
    _workloop = new WorkLoop();
}

HostEnvBackend::~HostEnvBackend() {
    delete _def_recvgate;
}

Env::Init::Init() {
#ifndef NDEBUG
    char *val = getenv("M3_WAIT");
    if(val && strstr(Env::executable(), val) != nullptr) {
        while(wait_for_debugger)
            usleep(20000);
    }
#endif

    init_env();

    Serial::init(executable(), env()->coreid);
    _inst->init_dtu();
}

Env::Init::~Init() {
    delete _inst;
}

Env::PostInit::PostInit() {
    if(!env()->is_kernel())
        env()->init_syscall(DTU::get().ep_regs());
}

void Env::init_syscall(void *sepregs) {
    LOG(SYSC, "init(addr=" << sepregs << ")");
    send_receive_vmsg(Syscalls::get()._gate, Syscalls::COUNT, sepregs);
}

void Env::reset() {
    load_params(this);

    Serial::init(executable(), env()->coreid);

    DTU::get().reset();
    EPMux::get().reset();

    init_dtu();

    // we have to call init for this VPE in case we hadn't done that yet
    init_syscall(DTU::get().ep_regs());
}

Env::Env(EnvBackend *backend, int logfd)
        : coreid(set_inst(this)), backend(backend), _logfd(logfd), _shm_prefix(),
          _sysc_label(), _sysc_epid(), _sysc_credits(),
          _log_mutex(PTHREAD_MUTEX_INITIALIZER),
          // the memory receive buffer is required to let others access our memory via DTU
          _mem_recvbuf(RecvBuf::bindto(DTU::MEM_EP, 0, sizeof(word_t) * 8 - 1,
                           RecvBuf::NO_HEADER | RecvBuf::NO_RINGBUF)),
          _def_recvbuf(RecvBuf::create(DTU::DEF_RECVEP, nextlog2<256>::val, nextlog2<128>::val, 0)),
          _mem_recvgate(new RecvGate(RecvGate::create(&_mem_recvbuf))) {
    backend->_def_recvgate = new RecvGate(RecvGate::create(&_def_recvbuf));
}

void Env::init_executable() {
    int fd = open("/proc/self/cmdline", O_RDONLY);
    if(fd == -1)
        PANIC("open");
    if(read(fd, _exec, sizeof(_exec)) == -1)
        PANIC("read");
    close(fd);
    strncpy(_exec_short, _exec, sizeof(_exec_short));
    _exec_short[sizeof(_exec_short) - 1] = '\0';
    _exec_short_ptr = basename(_exec_short);
}

void Env::init_dtu() {
    // we have to init that here, too, because the kernel doesn't know where it is
    DTU::get().configure_recv(DTU::MEM_EP, reinterpret_cast<uintptr_t>(_mem_recvbuf.addr()),
        _mem_recvbuf.order(), _mem_recvbuf.msgorder(), _mem_recvbuf.flags());
    DTU::get().configure_recv(DTU::DEF_RECVEP, reinterpret_cast<uintptr_t>(_def_recvbuf.addr()),
        _def_recvbuf.order(), _def_recvbuf.msgorder(), _def_recvbuf.flags());

    DTU::get().configure(DTU::SYSC_EP, _sysc_label, 0, _sysc_epid, _sysc_credits);

    DTU::get().start();
}

void Env::print() const {
    char **env = environ;
    while(*env) {
        if(strstr(*env, "M3_") != nullptr || strstr(*env, "LD_") != nullptr) {
            char *dup = strdup(*env);
            char *name = strtok(dup, "=");
            printf("%s = %s\n", name, getenv(name));
            free(dup);
        }
        env++;
    }
}

Env::~Env() {
    delete _mem_recvgate;
    delete backend;
    if(is_kernel())
        stop_dtu();
}

}
