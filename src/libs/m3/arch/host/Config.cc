/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/arch/host/Backtrace.h>
#include <m3/cap/RecvGate.h>
#include <m3/Config.h>
#include <m3/Syscalls.h>
#include <m3/ChanMng.h>
#include <m3/DTU.h>
#include <m3/Log.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <cstdlib>
#include <cstdio>
#include <pthread.h>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef NDEBUG
volatile int wait_for_debugger = 1;
#endif

int __default_conf WEAK = 1;

namespace m3 {

Config *Config::_inst = nullptr;
Config::Init Config::_init INIT_PRIORITY(107);
char Config::_exec[128];
char Config::_exec_short[128];
const char *Config::_exec_short_ptr = nullptr;

static const char *gen_prefix() {
    static char prefix[32];
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    srand(tv.tv_usec);
    snprintf(prefix, sizeof(prefix), "/asyncipc-%d-", rand());
    return prefix;
}

static void stop_dtu() {
    DTU::get().stop();
    pthread_join(DTU::get().tid(), nullptr);
}

static void on_exit_func(int status, void *) {
    Syscalls::get().exit(status);
    stop_dtu();
}

Config::Init::Init() {
#ifndef NDEBUG
    char *val = getenv("M3_WAIT");
    if(val && strstr(Config::executable(), val) != nullptr) {
        while(wait_for_debugger)
            usleep(20000);
    }
#endif

    if(__default_conf)
        new Config();
    else
        new Config(0, gen_prefix());

    // use on_exit to get the return-value of main and pass it to the m3 kernel
    if(!_inst->is_kernel())
        on_exit(on_exit_func, nullptr);
}

Config::Init::~Init() {
    delete _inst;
}

void Config::reset() {
    set_params(this, nullptr, false);
    Serial::get().init(executable(), coreid());

    DTU::get().reset();
    ChanMng::get().reset();

    init_dtu();

    // we have to call init for this VPE in case we hadn't done that yet
    Syscalls::get().init(DTU::get().ep_regs());
}

static void sighandler(int sig, siginfo_t *info, void *secret) {
    ucontext_t *uc = static_cast<ucontext_t*>(secret);
    if(sig == SIGSEGV) {
        LOG(DEF, "Got signal " << sig << ", faulty address is " << info->si_addr
#ifdef __i386__
                << ", from " << reinterpret_cast<void*>(uc->uc_mcontext.gregs[REG_EIP])
#elif defined(__x86_64__)
                << ", from " << reinterpret_cast<void*>(uc->uc_mcontext.gregs[REG_RIP])
#endif
        );
    }
    else
        LOG(DEF, "Got signal " << sig);
    std::string str;
    std::ifstream in("/proc/self/maps");
    while(!in.eof()) {
        std::getline(in,str);
        LOG(DEF, str.c_str());
    }
    LOG(DEF, Backtrace());
    exit(EXIT_FAILURE);
}

Config::Config()
        : _core(), _logfd(-1), _shm_prefix(), _is_kernel(set_params(this, nullptr, false)),
          _log_mutex(PTHREAD_MUTEX_INITIALIZER),
          // the memory receive buffer is required to let others access our memory via DTU
          _mem_recvbuf(RecvBuf::bindto(ChanMng::MEM_CHAN, 0, sizeof(word_t) * 8 - 1,
                           RecvBuf::NO_HEADER | RecvBuf::NO_RINGBUF)),
          _def_recvbuf(RecvBuf::create(ChanMng::DEF_RECVCHAN, nextlog2<256>::val, nextlog2<128>::val, 0)),
          _mem_recvgate(new RecvGate(RecvGate::create(&_mem_recvbuf))),
          _def_recvgate(new RecvGate(RecvGate::create(&_def_recvbuf))) {
    init();
}

Config::Config(int core, const char *shmprefix)
        : _core(core), _logfd(-1), _shm_prefix(), _is_kernel(set_params(this, shmprefix, true)),
          _log_mutex(PTHREAD_MUTEX_INITIALIZER),
          _mem_recvbuf(RecvBuf::bindto(ChanMng::MEM_CHAN, 0, sizeof(word_t) * 8 - 1,
                           RecvBuf::NO_HEADER | RecvBuf::NO_RINGBUF)),
          _def_recvbuf(RecvBuf::create(ChanMng::DEF_RECVCHAN, nextlog2<256>::val, nextlog2<128>::val, 0)),
          _mem_recvgate(new RecvGate(RecvGate::create(&_mem_recvbuf))),
          _def_recvgate(new RecvGate(RecvGate::create(&_def_recvbuf))) {
    init();
}

void Config::init_executable() {
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

void Config::init() {
    struct sigaction sa;
    sa.sa_sigaction = sighandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGUSR1, &sa, nullptr);

    Serial::get().init(executable(), coreid());

    init_dtu();
}

void Config::init_dtu() {
    // we have to init that here, too, because the kernel doesn't know where it is
    DTU::get().configure_recv(ChanMng::MEM_CHAN, reinterpret_cast<uintptr_t>(_mem_recvbuf.addr()),
        _mem_recvbuf.order(), _mem_recvbuf.msgorder(), _mem_recvbuf.flags());
    DTU::get().configure_recv(ChanMng::DEF_RECVCHAN, reinterpret_cast<uintptr_t>(_def_recvbuf.addr()),
        _def_recvbuf.order(), _def_recvbuf.msgorder(), _def_recvbuf.flags());

    DTU::get().configure(ChanMng::SYSC_CHAN, _sysc_label, 0, _sysc_cid, _sysc_credits);

    DTU::get().start();
}

bool Config::set_params(Config *env, const char *shm_prefix, bool is_kernel) {
    if(!is_kernel) {
        char path[64];
        snprintf(path, sizeof(path), "/tmp/m3/%d", getpid());
        std::ifstream in(path);
        if(!in.good())
            PANIC("Unable to read " << path);
        label_t lbl;
        std::string shm_prefix;
        in >> shm_prefix >> env->_core >> lbl >> env->_sysc_cid;
        in >> env->_sysc_credits;
        env->_shm_prefix = String(shm_prefix.c_str());
        env->_sysc_label = lbl;
        env->_logfd = open("run/log.txt", O_WRONLY | O_APPEND);
    }
    else {
        assert(shm_prefix != nullptr);
        env->_shm_prefix = String(shm_prefix);
        env->_logfd = open("run/log.txt", O_CREAT | O_TRUNC | O_WRONLY | O_APPEND, 0644);
    }
    _inst = env;
    return is_kernel;
}

void Config::print() const {
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

Config::~Config() {
    delete _mem_recvgate;
    delete _def_recvgate;
    if(is_kernel())
        stop_dtu();
}

}
