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

#include <m3/Config.h>
#include <m3/RecvBuf.h>
#include <m3/Syscalls.h>
#include <m3/Log.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>

extern const char *__progname;

namespace m3 {

struct Config {
    Config();
    ~Config();
};

static Config conf INIT_PRIORITY(101);
static RecvBuf *def_rbuf;
RecvGate *def_rgate;

Config::Config() {
    void *res = mmap((void*)CONF_LOCAL, sizeof(CoreConf), PROT_READ | PROT_WRITE,
        MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if(res == MAP_FAILED)
        PANIC("Unable to map local config");

    // if we are not the kernel, wait until the kernel initialized us
    volatile CoreConf *conf = coreconf();
    if(!strstr(__progname, "kernel")) {
        while(conf->coreid == 0)
            ;
    }

    def_rbuf = new RecvBuf(RecvBuf::create(
        ChanMng::DEF_RECVCHAN, nextlog2<256>::val, nextlog2<128>::val, 0));
    def_rgate = new RecvGate(RecvGate::create(def_rbuf));

    Serial::get().init(__progname, conf->coreid);
}

Config::~Config() {
    if(!strstr(__progname, "kernel")) {
        Syscalls::get().exit(0);
        // stay here
        while(1)
            asm volatile ("hlt");
    }
    else
        Machine::shutdown();
}

}
