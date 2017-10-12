/*
 * Copyright (C) 2015, Matthias Lieber <matthias.lieber@tu-dresden.de>
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#define TRACE_FUNCS_TO_STRING
#include <base/tracing/Event.h>

#include <vampirtrace/open-trace-format/otf.h>

#include <iostream>
#include <queue>
#include <map>
#include <set>
#include <array>
#include <vector>
#include <algorithm>
#include <regex>

#include "Symbols.h"

static bool verbose = 0;
static const uint64_t GEM5_TICKS_PER_SEC    = 1000000000;
static const int GEM5_MAX_PES               = 64;
static const int GEM5_MAX_VPES              = 1024 + 1;
static const unsigned INVALID_VPEID         = 0xFFFF;

enum event_type {
    EVENT_FUNC_ENTER    = m3::EVENT_FUNC_ENTER,
    EVENT_FUNC_EXIT     = m3::EVENT_FUNC_EXIT,
    EVENT_UFUNC_ENTER   = m3::EVENT_UFUNC_ENTER,
    EVENT_UFUNC_EXIT    = m3::EVENT_UFUNC_EXIT,
    EVENT_MSG_SEND_START,
    EVENT_MSG_SEND_DONE,
    EVENT_MSG_RECV,
    EVENT_MEM_READ_START,
    EVENT_MEM_READ_DONE,
    EVENT_MEM_WRITE_START,
    EVENT_MEM_WRITE_DONE,
    EVENT_SUSPEND,
    EVENT_WAKEUP,
    EVENT_SET_VPEID,
};

static const char *event_names[] = {
    "",
    "EVENT_FUNC_ENTER",
    "EVENT_FUNC_EXIT",
    "EVENT_UFUNC_ENTER",
    "EVENT_UFUNC_EXIT",
    "EVENT_MSG_SEND_START",
    "EVENT_MSG_SEND_DONE",
    "EVENT_MSG_RECV",
    "EVENT_MEM_READ_START",
    "EVENT_MEM_READ_DONE",
    "EVENT_MEM_WRITE_START",
    "EVENT_MEM_WRITE_DONE",
    "EVENT_SUSPEND",
    "EVENT_WAKEUP",
    "EVENT_SET_VPEID",
};

struct Event {
    explicit Event() : pe(), timestamp(), type(), size(), remote(), tag(), bin(static_cast<uint32_t>(-1)), name() {
    }
    explicit Event(uint32_t pe, uint64_t ts, int type, size_t size, uint32_t remote, uint64_t tag)
        : pe(pe), timestamp(ts / 1000), type(type), size(size), remote(remote), tag(tag), bin(static_cast<uint32_t>(-1)), name() {
    }
    explicit Event(uint32_t pe, uint64_t ts, int type, uint32_t bin, const char *name)
        : pe(pe), timestamp(ts / 1000), type(type), size(), remote(), tag(), bin(bin), name(name) {
    }

    const char *tag_to_string() const {
        static char buf[5];
        buf[0] = static_cast<char>((tag >> 24) & 0xFF);
        buf[1] = static_cast<char>((tag >> 16) & 0xFF);
        buf[2] = static_cast<char>((tag >>  8) & 0xFF);
        buf[3] = static_cast<char>((tag >>  0) & 0xFF);
        buf[4] = '\0';
        return buf;
    }

    friend std::ostream &operator<<(std::ostream &os, const Event &ev) {
        os << ev.pe << " " << event_names[ev.type] << ": " << ev.timestamp;
        switch(ev.type) {
            case EVENT_FUNC_ENTER:
            case EVENT_FUNC_EXIT:
                if(ev.tag >= sizeof(m3::event_funcs) / sizeof(m3::event_funcs[0]))
                    os << " function: unknown (" << ev.tag << ")";
                else
                    os << " function: " << m3::event_funcs[ev.tag].name;
                break;

            case EVENT_UFUNC_ENTER:
            case EVENT_UFUNC_EXIT:
                os << " function: " << ev.name;
                break;

            default:
                os << "  receiver: " << ev.remote
                   << "  size: " << ev.size
                   << "  tag: " << ev.tag;
                break;
        }
        return os;
    }

    uint32_t pe;
    uint64_t timestamp;

    int type;

    size_t size;
    uint32_t remote;
    uint64_t tag;

    uint32_t bin;
    const char *name;
};

struct State {
    static const size_t INVALID_IDX = static_cast<size_t>(-1);

    explicit State() : tag(), addr(), sym(), in_cmd(), have_start(), start_idx(INVALID_IDX) {
    }

    uint64_t tag;
    unsigned long addr;
    Symbols::symbol_t sym;
    bool in_cmd;
    bool have_start;
    size_t start_idx;
};

struct Stats {
    unsigned total = 0;
    unsigned send = 0;
    unsigned recv = 0;
    unsigned read = 0;
    unsigned write = 0;
    unsigned finish = 0;
    unsigned ufunc_enter = 0;
    unsigned ufunc_exit = 0;
    unsigned func_enter = 0;
    unsigned func_exit = 0;
    unsigned warnings = 0;
};

enum Mode {
    MODE_PES,
    MODE_VPES,
};

static Symbols syms;

static Event build_event(event_type type, uint64_t timestamp, uint32_t pe,
                         const std::string &remote, const std::string &size, uint64_t tag) {
    Event ev(
        pe,
        timestamp,
        type,
        strtoull(size.c_str(), nullptr, 10),
        strtoull(remote.c_str(), nullptr, 10),
        tag
    );
    return ev;
}

uint32_t read_trace_file(const char *path, Mode mode, std::vector<Event> &buf) {
    char filename[256];
    char readbuf[256];
    if(path) {
        strncpy(filename, path, sizeof(filename));
        filename[sizeof(filename) - 1] = '\0';
    }

    printf("reading trace file: %s\n", filename);

    FILE *fd = fopen(filename, "r");
    if(!fd) {
        perror("cannot open trace file");
        return 0;
    }

    std::regex msg_snd_regex(
        "^: \e\\[1m\\[(?:sd|rp) -> (\\d+)\\]\e\\[0m with EP\\d+ of (?:0x)?[0-9a-f]+:(\\d+)"
    );
    std::regex msg_rcv_regex(
        "^: \e\\[1m\\[rv <- (\\d+)\\]\e\\[0m (\\d+) bytes on EP\\d+"
    );
    std::regex msg_rw_regex(
        "^: \e\\[1m\\[(rd|wr) -> (\\d+)\\]\e\\[0m at (?:0x)?[0-9a-f]+\\+(?:0x)?[0-9a-f]+"
        " with EP\\d+ (?:from|into) (?:0x)?[0-9a-f]+:(\\d+)"
    );
    std::regex suswake_regex(
        "^: (Suspending|Waking)"
    );
    std::regex setvpe_regex(
        "^\\.regFile: NOC-> DTU\\[VPE_ID      \\]: 0x([0-9a-f]+)"
    );
    std::regex debug_regex(
        "^: DEBUG (?:0x)([0-9a-f]+)"
    );
    std::regex exec_regex(
        "^(?:0x)([0-9a-f]+) @ .*  :"
    );
    std::regex call_regex(
        "^(?:0x)([0-9a-f]+) @ .*  :   CALL_NEAR"
    );
    std::regex ret_regex(
        "^(?:0x)([0-9a-f]+) @ .*\\.0  :   RET_NEAR"
    );

    State states[GEM5_MAX_PES];

    uint32_t last_pe = 0;
    uint64_t tag = 1;

    std::smatch match;

    unsigned long long timestamp;
    while(fgets(readbuf, sizeof(readbuf), fd)) {
        unsigned long addr;
        uint32_t pe;
        int numchars;
        int tid;

        if(mode == MODE_VPES &&
                sscanf(readbuf, "%Lu: pe%u.cpu T%d : %lx @", &timestamp, &pe, &tid, &addr) == 4) {

            if(states[pe].addr == addr)
                continue;

            unsigned long oldaddr = states[pe].addr;
            states[pe].addr = addr;

            Symbols::symbol_t sym = syms.resolve(addr);
            if(states[pe].sym == sym)
                continue;

            if(oldaddr)
                buf.push_back(Event(pe, timestamp, EVENT_UFUNC_EXIT, 0, ""));

            uint32_t bin;
            char *namebuf = (char*)malloc(Symbols::MAX_FUNC_LEN + 1);
            if(!syms.valid(sym)) {
                bin = static_cast<uint32_t>(-1);
                snprintf(namebuf, Symbols::MAX_FUNC_LEN, "%#lx", addr);
            }
            else {
                bin = sym->bin;
                syms.demangle(namebuf, Symbols::MAX_FUNC_LEN, sym->name.c_str());
            }

            buf.push_back(Event(pe, timestamp, EVENT_UFUNC_ENTER, bin, namebuf));

            states[pe].sym = sym;
            last_pe = std::max(pe, last_pe);
            continue;
        }

        if(sscanf(readbuf, "%Lu: pe%d.dtu%n", &timestamp, &pe, &numchars) != 2)
            continue;

        std::string line(readbuf + numchars);

        if(strstr(line.c_str(), "rv") && std::regex_search(line, match, msg_rcv_regex)) {
            Event ev = build_event(EVENT_MSG_RECV,
                timestamp, pe, match[1].str(), match[2].str(), tag);
            buf.push_back(ev);

            last_pe = std::max(pe, std::max(last_pe, ev.remote));
            states[pe].tag = tag++;
        }
        else if(strstr(line.c_str(), "ing") && std::regex_search(line, match, suswake_regex)) {
            event_type type = match[1].str() == "Waking" ? EVENT_WAKEUP : EVENT_SUSPEND;
            buf.push_back(build_event(type, timestamp, pe, "", "", tag));

            last_pe = std::max(pe, last_pe);
            states[pe].tag = tag++;
        }
        else if(strstr(line.c_str(), "VPE_ID") && std::regex_search(line, match, setvpe_regex)) {
            uint32_t vpetag = strtoul(match[1].str().c_str(), NULL, 16);
            buf.push_back(build_event(EVENT_SET_VPEID, timestamp, pe, "", "", vpetag));

            last_pe = std::max(pe, last_pe);
        }
        else if(mode == MODE_VPES && std::regex_search(line, match, debug_regex)) {
            uint64_t value = strtoul(match[1].str().c_str(), NULL, 16);
            if(value >> 48 != 0) {
                event_type type = static_cast<event_type>(value >> 48);
                uint64_t vpetag = value & 0xFFFFFFFFFFFF;
                buf.push_back(build_event(type, timestamp, pe, "", "", vpetag));
            }
        }
        else if (!states[pe].in_cmd) {
            if(strncmp(line.c_str(), ": Starting command ", 19) == 0) {
                states[pe].in_cmd = true;
                states[pe].have_start = false;
            }
        }
        else {
            if(strncmp(line.c_str(), ": Finished command ", 19) == 0) {
                if(states[pe].have_start) {
                    int type;
                    assert(states[pe].start_idx != State::INVALID_IDX);
                    const Event &start_ev = buf[states[pe].start_idx];
                    if(start_ev.type == EVENT_MSG_SEND_START)
                        type = EVENT_MSG_SEND_DONE;
                    else if(start_ev.type == EVENT_MEM_READ_START)
                        type = EVENT_MEM_READ_DONE;
                    else
                        type = EVENT_MEM_WRITE_DONE;
                    uint32_t remote = start_ev.remote;
                    Event ev(pe, timestamp, type, start_ev.size, remote, states[pe].tag);
                    buf.push_back(ev);

                    last_pe = std::max(pe, std::max(last_pe, remote));
                    states[pe].start_idx = State::INVALID_IDX;
                }

                states[pe].in_cmd = false;
            }
            else {
                if ((strstr(line.c_str(), "sd") || strstr(line.c_str(), "rp")) &&
                        std::regex_search(line, match, msg_snd_regex)) {
                    Event ev = build_event(EVENT_MSG_SEND_START,
                        timestamp, pe, match[1].str(), match[2].str(), tag);
                    states[pe].have_start = true;
                    buf.push_back(ev);
                    states[pe].start_idx = buf.size() - 1;
                    states[pe].tag = tag++;
                }
                else if((strstr(line.c_str(), "rd") || strstr(line.c_str(), "wr")) &&
                        std::regex_search(line, match, msg_rw_regex)) {
                    event_type type = match[1].str() == "rd" ? EVENT_MEM_READ_START
                                                             : EVENT_MEM_WRITE_START;
                    if(states[pe].start_idx != State::INVALID_IDX)
                        buf[states[pe].start_idx].size += strtoull(match[3].str().c_str(), nullptr, 10);
                    else {
                        Event ev = build_event(type,
                            timestamp, pe, match[2].str(), match[3].str(), tag);
                        states[pe].have_start = true;
                        buf.push_back(ev);
                        states[pe].start_idx = buf.size() - 1;
                        states[pe].tag = tag++;
                    }
                }
            }
        }
    }

    for(size_t i = 0; i <= last_pe; ++i) {
        if(states[i].addr)
            buf.push_back(Event(i, ++timestamp, EVENT_UFUNC_EXIT, 0, ""));
    }

    fclose(fd);
    return last_pe + 1;
}

static void gen_pe_events(OTF_Writer *writer, Stats &stats, std::vector<Event> &trace_buf, uint32_t pe_count) {
    // Processes.
    uint32_t stream = 1;
    for(uint32_t i = 0; i < pe_count; ++i) {
        char peName[8];
        snprintf(peName, sizeof(peName), "PE%d", i);
        OTF_Writer_writeDefProcess(writer, 0, i, peName, 0);
        OTF_Writer_assignProcess(writer, i, stream);
    }

    // Process groups
    uint32_t allPEs[pe_count];
    for(uint32_t i = 0; i < pe_count; ++i)
        allPEs[i] = i;

    unsigned grp_mem = (1 << 20) + 1;
    OTF_Writer_writeDefProcessGroup(writer, 0, grp_mem, "Memory Read/Write", pe_count, allPEs);
    unsigned grp_msg = (1 << 20) + 2;
    OTF_Writer_writeDefProcessGroup(writer, 0, grp_msg, "Message Send/Receive", pe_count, allPEs);

    // Function groups
    unsigned grp_func_count = 0;
    unsigned grp_func_exec = grp_func_count++;
    OTF_Writer_writeDefFunctionGroup(writer, 0, grp_func_exec, "Execution");

    // Execution functions
    unsigned fn_exec_last = (2 << 20) + 0;
    std::map<unsigned, unsigned> vpefuncs;

    unsigned fn_exec_sleep = ++fn_exec_last;
    OTF_Writer_writeDefFunction(writer, 0, fn_exec_sleep, "Sleeping", grp_func_exec, 0);

    unsigned fn_vpe_invalid = ++fn_exec_last;
    vpefuncs[INVALID_VPEID] = fn_vpe_invalid;
    OTF_Writer_writeDefFunction(writer, 0, fn_vpe_invalid, "No VPE", grp_func_exec, 0);

    printf("writing OTF events\n");

    uint64_t timestamp = 0;

    bool awake[pe_count];
    unsigned cur_vpe[pe_count];

    for(uint32_t i = 0; i < pe_count; ++i) {
        awake[i] = true;
        cur_vpe[i] = fn_vpe_invalid;
        OTF_Writer_writeEnter(writer, timestamp, fn_vpe_invalid, i, 0);
    }

    // finally loop over events and write OTF
    for(auto event = trace_buf.begin(); event != trace_buf.end(); ++event) {
        // don't use the same timestamp twice
        if(event->timestamp <= timestamp)
            event->timestamp = timestamp + 1;

        timestamp = event->timestamp;

        if(verbose)
            std::cout << *event << "\n";

        switch(event->type) {
            case EVENT_MSG_SEND_START:
                OTF_Writer_writeSendMsg(writer, timestamp,
                    event->pe, event->remote, grp_msg, event->tag, event->size, 0);
                ++stats.send;
                break;

            case EVENT_MSG_RECV:
                OTF_Writer_writeRecvMsg(writer, timestamp,
                    event->pe, event->remote, grp_msg, event->tag, event->size, 0);
                ++stats.recv;
                break;

            case EVENT_MSG_SEND_DONE:
                break;

            case EVENT_MEM_READ_START:
                OTF_Writer_writeSendMsg(writer, timestamp,
                    event->pe, event->remote, grp_mem, event->tag, event->size, 0);
                ++stats.read;
                break;

            case EVENT_MEM_READ_DONE:
                OTF_Writer_writeRecvMsg(writer, timestamp,
                    event->remote, event->pe, grp_mem, event->tag, event->size, 0);
                ++stats.finish;
                break;

            case EVENT_MEM_WRITE_START:
                OTF_Writer_writeSendMsg(writer, timestamp,
                    event->pe, event->remote, grp_mem, event->tag, event->size, 0);
                ++stats.write;
                break;

            case EVENT_MEM_WRITE_DONE:
                OTF_Writer_writeRecvMsg(writer, timestamp,
                    event->remote, event->pe, grp_mem, event->tag, event->size, 0);
                ++stats.finish;
                break;

            case EVENT_WAKEUP:
                if(!awake[event->pe]) {
                    OTF_Writer_writeLeave(writer, timestamp - 1, fn_exec_sleep, event->pe, 0);
                    OTF_Writer_writeEnter(writer, timestamp, cur_vpe[event->pe], event->pe, 0);
                    awake[event->pe] = true;
                }
                break;

            case EVENT_SUSPEND:
                if(awake[event->pe]) {
                    OTF_Writer_writeLeave(writer, timestamp - 1, cur_vpe[event->pe], event->pe, 0);
                    OTF_Writer_writeEnter(writer, timestamp, fn_exec_sleep, event->pe, 0);
                    awake[event->pe] = false;
                }
                break;

            case EVENT_SET_VPEID: {
                auto fn = vpefuncs.find(event->tag);
                if(fn == vpefuncs.end()) {
                    char name[16];
                    snprintf(name, sizeof(name), "VPE%u", (unsigned)event->tag);
                    vpefuncs[event->tag] = ++fn_exec_last;
                    OTF_Writer_writeDefFunction(writer, 0, vpefuncs[event->tag], name, grp_func_exec, 0);
                    fn = vpefuncs.find(event->tag);
                }

                if(awake[event->pe] && cur_vpe[event->pe] != fn->second) {
                    OTF_Writer_writeLeave(writer, timestamp - 1, cur_vpe[event->pe], event->pe, 0);
                    OTF_Writer_writeEnter(writer, timestamp, fn->second, event->pe, 0);
                }

                cur_vpe[event->pe] = fn->second;
                break;
            }
        }

        ++stats.total;
    }

    for(uint32_t i = 0; i < pe_count; ++i) {
        if(awake[i])
            OTF_Writer_writeLeave(writer, timestamp, cur_vpe[i], i, 0);
        else
            OTF_Writer_writeLeave(writer, timestamp, fn_exec_sleep, i, 0);
    }
}

static void gen_vpe_events(OTF_Writer *writer, Stats &stats, std::vector<Event> &trace_buf,
        uint32_t pe_count, uint32_t binary_count, char **binaries) {
    // Processes
    std::set<unsigned> vpeIds;

    OTF_Writer_writeDefProcess(writer, 0, INVALID_VPEID, "No VPE", 0);
    OTF_Writer_assignProcess(writer, INVALID_VPEID, 1);
    vpeIds.insert(INVALID_VPEID);

    for(auto ev = trace_buf.begin(); ev != trace_buf.end(); ++ev) {
        if(ev->type == EVENT_SET_VPEID && vpeIds.find(ev->tag) == vpeIds.end()) {
            char vpeName[8];
            snprintf(vpeName, sizeof(vpeName), "VPE%u", (unsigned)ev->tag);
            OTF_Writer_writeDefProcess(writer, 0, ev->tag, vpeName, 0);
            OTF_Writer_assignProcess(writer, ev->tag, 1);
            vpeIds.insert(ev->tag);
        }
    }

    // Process groups
    size_t i = 0;
    uint32_t allVPEs[vpeIds.size()];
    for(auto it = vpeIds.begin(); it != vpeIds.end(); ++it, ++i)
        allVPEs[i] = *it;

    unsigned grp_mem = (1 << 20) + 1;
    OTF_Writer_writeDefProcessGroup(writer, 0, grp_mem, "Memory Read/Write", pe_count, allVPEs);
    unsigned grp_msg = (1 << 20) + 2;
    OTF_Writer_writeDefProcessGroup(writer, 0, grp_msg, "Message Send/Receive", pe_count, allVPEs);

    // Function groups
    unsigned grp_func_count = 0;
    unsigned grp_func_exec = grp_func_count++;
    OTF_Writer_writeDefFunctionGroup(writer, 0, grp_func_exec, "Execution");
    unsigned grp_func_mem = grp_func_count++;
    OTF_Writer_writeDefFunctionGroup(writer, 0, grp_func_mem, "Memory");
    unsigned grp_func_msg = grp_func_count++;
    OTF_Writer_writeDefFunctionGroup(writer, 0, grp_func_msg, "Messaging");
    unsigned grp_func_user = grp_func_count++;
    OTF_Writer_writeDefFunctionGroup(writer, 0, grp_func_user, "User");

    for(uint32_t i = 0; i < binary_count; ++i)
        OTF_Writer_writeDefFunctionGroup(writer, 0, grp_func_count + i, binaries[i]);

    // Execution functions
    unsigned fn_exec_last = (2 << 20) + 0;
    std::map<unsigned, unsigned> vpefuncs;

    unsigned fn_exec_sleep = ++fn_exec_last;
    OTF_Writer_writeDefFunction(writer, 0, fn_exec_sleep, "Sleeping", grp_func_exec, 0);
    unsigned fn_exec_running = ++fn_exec_last;
    OTF_Writer_writeDefFunction(writer, 0, fn_exec_running, "Running", grp_func_exec, 0);

    // Message functions
    unsigned fn_msg_send = (3 << 20) + 1;
    OTF_Writer_writeDefFunction(writer, 0, fn_msg_send, "msg_send", grp_func_msg, 0);

    // Memory Functions
    unsigned fn_mem_read = (3 << 20) + 2;
    OTF_Writer_writeDefFunction(writer, 0, fn_mem_read, "mem_read", grp_func_mem, 0);
    unsigned fn_mem_write = (3 << 20) + 3;
    OTF_Writer_writeDefFunction(writer, 0, fn_mem_write, "mem_write", grp_func_mem, 0);

    // Function groups, defined in Event.h
    unsigned grp_func_start = (4 << 20);
    for(unsigned int i = 0; i < m3::event_func_groups_size; ++i)
        OTF_Writer_writeDefFunctionGroup(writer, 0, grp_func_start + i, m3::event_func_groups[i]);

    printf("writing OTF events\n");

    unsigned cur_vpe[pe_count];

    for(uint32_t i = 0; i < pe_count; ++i)
        cur_vpe[i] = INVALID_VPEID;

    uint32_t ufunc_max_id = ( 3 << 20 );
    std::map<std::pair<int, std::string>, uint32_t> ufunc_map;

    uint32_t func_start_id = ( 4 << 20 );
    std::set<uint32_t> func_set;

    // function call stack per VPE
    std::array<uint, GEM5_MAX_VPES> func_stack;
    func_stack.fill( 0 );
    std::array<uint, GEM5_MAX_VPES> ufunc_stack;
    ufunc_stack.fill( 0 );

    uint64_t timestamp = 0;

    std::map<unsigned, bool> awake;
    for(auto it = vpeIds.begin(); it != vpeIds.end(); ++it) {
        awake[*it] = false;
        OTF_Writer_writeEnter(writer, timestamp, fn_exec_sleep, *it, 0);
    }

    // finally loop over events and write OTF
    for(auto event = trace_buf.begin(); event != trace_buf.end(); ++event) {
        // don't use the same timestamp twice
        if(event->timestamp <= timestamp)
            event->timestamp = timestamp + 1;

        timestamp = event->timestamp;
        unsigned vpe = cur_vpe[event->pe];
        unsigned remote_vpe = cur_vpe[event->remote];

        if(verbose) {
            unsigned pe = event->pe;
            unsigned remote = event->remote;
            event->pe = vpe;
            event->remote = remote_vpe;
            std::cout << pe << ": " << *event << "\n";
            event->pe = pe;
            event->remote = remote;
        }

        if(vpe == INVALID_VPEID && event->type != EVENT_SET_VPEID)
            continue;

        switch(event->type) {
            case EVENT_MSG_SEND_START:
                // TODO currently, we don't display that as functions, because it interferes with
                // the UFUNCs.
                // OTF_Writer_writeEnter(writer, timestamp, fn_msg_send, vpe, 0);
                OTF_Writer_writeSendMsg(writer, timestamp,
                    vpe, remote_vpe, grp_msg, event->tag, event->size, 0);
                ++stats.send;
                break;

            case EVENT_MSG_RECV:
                OTF_Writer_writeRecvMsg(writer, timestamp,
                    vpe, remote_vpe, grp_msg, event->tag, event->size, 0);
                ++stats.recv;
                break;

            case EVENT_MSG_SEND_DONE:
                // OTF_Writer_writeLeave(writer, timestamp, fn_msg_send, vpe, 0);
                break;

            case EVENT_MEM_READ_START:
                // OTF_Writer_writeEnter(writer, timestamp, fn_mem_read, vpe, 0);
                OTF_Writer_writeSendMsg(writer, timestamp,
                    vpe, remote_vpe, grp_mem, event->tag, event->size, 0);
                ++stats.read;
                break;

            case EVENT_MEM_READ_DONE:
                // OTF_Writer_writeLeave(writer, timestamp, fn_mem_read, vpe, 0);
                OTF_Writer_writeRecvMsg(writer, timestamp,
                    remote_vpe, vpe, grp_mem, event->tag, event->size, 0);
                ++stats.finish;
                break;

            case EVENT_MEM_WRITE_START:
                // OTF_Writer_writeEnter(writer, timestamp, fn_mem_write, vpe, 0);
                OTF_Writer_writeSendMsg(writer, timestamp,
                    vpe, remote_vpe, grp_mem, event->tag, event->size, 0);
                ++stats.write;
                break;

            case EVENT_MEM_WRITE_DONE:
                if(stats.read || stats.write) {
                    // OTF_Writer_writeLeave(writer, timestamp, fn_mem_write, vpe, 0);
                    OTF_Writer_writeRecvMsg(writer, timestamp,
                        remote_vpe, vpe, grp_mem, event->tag, event->size, 0);
                    ++stats.finish;
                }
                break;

            case EVENT_WAKEUP:
                if(!awake[vpe]) {
                    OTF_Writer_writeLeave(writer, timestamp - 1, fn_exec_sleep, vpe, 0);
                    OTF_Writer_writeEnter(writer, timestamp, fn_exec_running, vpe, 0);
                    awake[vpe] = true;
                }
                break;

            case EVENT_SUSPEND:
                if(awake[vpe]) {
                    OTF_Writer_writeLeave(writer, timestamp - 1, fn_exec_running, vpe, 0);
                    OTF_Writer_writeEnter(writer, timestamp, fn_exec_sleep, vpe, 0);
                    awake[vpe] = false;
                }
                break;

            case EVENT_SET_VPEID: {
                if(awake[vpe]) {
                    OTF_Writer_writeLeave(writer, timestamp - 1, fn_exec_running, vpe, 0);
                    OTF_Writer_writeEnter(writer, timestamp, fn_exec_sleep, vpe, 0);
                    awake[vpe] = false;
                }

                cur_vpe[event->pe] = event->tag;
                break;
            }

            case EVENT_UFUNC_ENTER: {
                auto ufunc_map_iter = ufunc_map.find(std::make_pair(event->bin, event->name));
                uint32_t id = 0;
                if(ufunc_map_iter == ufunc_map.end()) {
                    id = (++ufunc_max_id);
                    ufunc_map.insert(std::make_pair(std::make_pair(event->bin, event->name), id));
                    unsigned group = grp_func_user;
                    if(event->bin != static_cast<uint32_t>(-1))
                        group = grp_func_count + event->bin;
                    OTF_Writer_writeDefFunction(writer, 0, id, event->name, group, 0);
                }
                else
                    id = ufunc_map_iter->second;
                ++(ufunc_stack[vpe]);
                OTF_Writer_writeEnter(writer, timestamp, id, vpe, 0);
                ++stats.ufunc_enter;
            }
            break;

            case EVENT_UFUNC_EXIT: {
                if(ufunc_stack[vpe] < 1) {
                    std::cout << vpe << " WARNING: exit at ufunc stack level "
                              << ufunc_stack[vpe] << " dropped.\n";
                    ++stats.warnings;
                }
                else {
                    --(ufunc_stack[vpe]);
                    OTF_Writer_writeLeave(writer, timestamp, 0, vpe, 0);
                }
                ++stats.ufunc_exit;
            }
            break;

            case EVENT_FUNC_ENTER: {
                uint32_t id = event->tag;
                if(func_set.find(id) == func_set.end()) {
                    func_set.insert(id);
                    unsigned group = grp_func_start + m3::event_funcs[id].group;
                    OTF_Writer_writeDefFunction(writer, 0, func_start_id + id, m3::event_funcs[id].name, group, 0);
                }
                ++(func_stack[vpe]);
                OTF_Writer_writeEnter(writer, timestamp, func_start_id + id, vpe, 0);
                ++stats.func_enter;
            }
            break;

            case EVENT_FUNC_EXIT: {
                if(func_stack[vpe] < 1) {
                    std::cout << vpe << " WARNING: exit at func stack level "
                              << func_stack[vpe] << " dropped.\n";
                    ++stats.warnings;
                }
                else {
                    --(func_stack[vpe]);
                    OTF_Writer_writeLeave(writer, timestamp, 0, vpe, 0);
                }
                ++stats.func_exit;
            }
            break;
        }

        ++stats.total;
    }

    for(auto it = vpeIds.begin(); it != vpeIds.end(); ++it) {
        if(awake[*it])
            OTF_Writer_writeLeave(writer, timestamp, fn_exec_running, *it, 0);
        else
            OTF_Writer_writeLeave(writer, timestamp, fn_exec_sleep, *it, 0);
    }
}

static void usage(const char *name) {
    fprintf(stderr, "Usage: %s [-v] (pes|vpes) <file> [<binary>...]\n", name);
    fprintf(stderr, "  -v:            be verbose\n");
    fprintf(stderr, "  (pes|vpes):    the mode\n");
    fprintf(stderr, "  <file>:        the gem5 log file\n");
    fprintf(stderr, "  [<binary>...]: optionally a list of binaries for profiling\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "The 'pes' mode generates a PE-centric trace, i.e., the PEs are the processes");
    fprintf(stderr, " and it is shown at which points in time which VPE was running on which PE.\n");
    fprintf(stderr, "The 'vpes' mode generates a VPE-centric trace, i.e., the VPEs are the processes");
    fprintf(stderr, " and it is shown what they do.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "The following gem5 flags (M3_GEM5_DBG) are used:\n");
    fprintf(stderr, " - Dtu,DtuCmd    for messages and memory reads/writes\n");
    fprintf(stderr, " - DtuConnector  for suspend/wakeup\n");
    fprintf(stderr, " - DtuRegWrite   for the running VPE\n");
    fprintf(stderr, " - Exec,ExecPC   for profiling (only in 'vpes' mode)\n");
    exit(EXIT_FAILURE);
}

int main(int argc,char **argv) {
    if(argc < 3)
        usage(argv[0]);

    int argstart = 1;
    Mode mode = MODE_PES;
    if(strcmp(argv[1], "-v") == 0) {
        verbose = 1;
        argstart++;
    }

    if(strcmp(argv[argstart], "pes") == 0)
        mode = MODE_PES;
    else if(strcmp(argv[argstart], "vpes") == 0)
        mode = MODE_VPES;
    else
        usage(argv[0]);

    if(mode == MODE_VPES) {
        for(int i = argstart + 2; i < argc; ++i)
            syms.addFile(argv[i]);
    }

    std::vector<Event> trace_buf;

    uint32_t pe_count = read_trace_file(argv[argstart + 1], mode, trace_buf);

    // now sort the trace buffer according to timestamps
    printf( "sorting %zu events\n", trace_buf.size());

    std::sort(trace_buf.begin(), trace_buf.end(), [](const Event &a, const Event &b) {
        return a.timestamp < b.timestamp;
    });

    // Declare a file manager and a writer.
    OTF_FileManager *manager;
    OTF_Writer *writer;

    // Initialize the file manager. Open at most 100 OS files.
    manager = OTF_FileManager_open(100);
    assert(manager);

    // Initialize the writer.
    writer = OTF_Writer_open("trace", 1, manager);
    assert(writer);

    // Write some important Definition Records.
    // Timer res. in ticks per second
    OTF_Writer_writeDefTimerResolution(writer, 0, GEM5_TICKS_PER_SEC);

    Stats stats;

    if(mode == MODE_PES)
        gen_pe_events(writer, stats, trace_buf, pe_count);
    else {
        gen_vpe_events(writer, stats, trace_buf, pe_count,
            static_cast<uint32_t>(argc - (argstart + 2)), argv + argstart + 2);
    }

    if(stats.send != stats.recv) {
        printf("WARNING: #send != #recv\n");
        ++stats.warnings;
    }
    if(stats.read + stats.write != stats.finish) {
        printf("WARNING: #read+#write != #finish\n");
        ++stats.warnings;
    }
    if(stats.func_enter != stats.func_exit) {
        printf("WARNING: #func_enter != #func_exit\n");
        ++stats.warnings;
    }
    if(stats.ufunc_enter != stats.ufunc_exit) {
        printf("WARNING: #ufunc_enter != #ufunc_exit\n");
        ++stats.warnings;
    }

    printf("total events: %u\n", stats.total);
    printf("warnings: %u\n", stats.warnings);
    printf("send: %u\n", stats.send);
    printf("recv: %u\n", stats.recv);
    printf("read: %u\n", stats.read);
    printf("write: %u\n", stats.write);
    printf("finish: %u\n", stats.finish);
    printf("func_enter: %u\n", stats.func_enter);
    printf("func_exit: %u\n", stats.func_exit);
    printf("ufunc_enter: %u\n", stats.ufunc_enter);
    printf("ufunc_exit: %u\n", stats.ufunc_exit);

    // Clean up before exiting the program.
    OTF_Writer_close(writer);
    OTF_FileManager_close(manager);

    return 0;
}
