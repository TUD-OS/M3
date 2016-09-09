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

#include <otf.h>

#include <iostream>
#include <queue>
#include <map>
#include <set>
#include <array>
#include <vector>
#include <algorithm>
#include <regex>

#ifndef VERBOSE
#   define VERBOSE 0
#endif

static const uint64_t GEM5_TICKS_PER_SEC    = 1000000000;
static const int GEM5_MAX_PES               = 64;

enum event_type {
    EVENT_MSG_SEND_START,
    EVENT_MSG_SEND_DONE,
    EVENT_MSG_RECV,
    EVENT_MEM_READ_START,
    EVENT_MEM_READ_DONE,
    EVENT_MEM_WRITE_START,
    EVENT_MEM_WRITE_DONE,
    EVENT_SUSPEND,
    EVENT_WAKEUP,
};

static const char *event_names[] = {
    "EVENT_MSG_SEND_START",
    "EVENT_MSG_SEND_DONE",
    "EVENT_MSG_RECV",
    "EVENT_MEM_READ_START",
    "EVENT_MEM_READ_DONE",
    "EVENT_MEM_WRITE_START",
    "EVENT_MEM_WRITE_DONE",
    "EVENT_SUSPEND",
    "EVENT_WAKEUP",
};

struct Event {
    explicit Event() : pe(), timestamp(), type(), size(), remote(), tag() {
    }
    explicit Event(int pe, uint64_t ts, int type, size_t size, int remote, uint64_t tag)
        : pe(pe), timestamp(ts / 1000), type(type), size(size), remote(remote), tag(tag) {
    }

    friend std::ostream &operator<<(std::ostream &os, const Event &ev) {
        os << ev.pe << " " << event_names[ev.type] << ": " << ev.timestamp
           << "  receiver: " << ev.remote
           << "  size: " << ev.size
           << "  tag: " << ev.tag;
        return os;
    }

    int pe;
    uint64_t timestamp;

    int type;

    size_t size;
    int remote;
    uint64_t tag;
};

struct State {
    explicit State() : in_cmd(), have_start(), start_event() {
    }

    bool in_cmd;
    bool have_start;
    Event start_event;
};

static Event build_event(event_type type, uint64_t timestamp, int pe,
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

int read_trace_file(const char *path, std::vector<Event> &buf) {
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
        "^: \e\\[1m\\[(?:sd|rp) -> (\\d+)\\]\e\\[0m with EP\\d+ of 0x[0-9a-f]+:(\\d+)"
    );
    std::regex msg_rcv_regex(
        "^: \e\\[1m\\[rv <- (\\d+)\\]\e\\[0m (\\d+) bytes on EP\\d+"
    );
    std::regex msg_rw_regex(
        "^: \e\\[1m\\[(rd|wr) -> (\\d+)\\]\e\\[0m at 0x[0-9a-f]+\\+\\d+ with EP\\d+ (?:from|into) 0x[0-9a-f]+:(\\d+)"
    );
    std::regex suswake_regex(
        "^\\.connector: (Suspending|Waking)"
    );

    State states[GEM5_MAX_PES];

    int last_pe = 0;
    int tag = 1;

    std::smatch match;

    while(fgets( readbuf, sizeof(readbuf), fd)) {
        unsigned long long timestamp;
        int pe;
        int numchars;
        if(sscanf(readbuf, "%Lu: pe%d.dtu%n", &timestamp, &pe, &numchars) != 2)
            continue;

        std::string line(readbuf + numchars);

        if(std::regex_search(line, match, msg_rcv_regex)) {
            Event ev = build_event(EVENT_MSG_RECV,
                timestamp, pe, match[1].str(), match[2].str(), 0);
            ev.tag = tag;
            buf.push_back(ev);

            last_pe = std::max(pe, std::max(last_pe, ev.remote));
        }
        else if(std::regex_search(line, match, suswake_regex)) {
            event_type type = match[1].str() == "Waking" ? EVENT_WAKEUP : EVENT_SUSPEND;
            buf.push_back(build_event(type, timestamp, pe, "", "", 0));

            last_pe = std::max(pe, last_pe);
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
                    if(states[pe].start_event.type == EVENT_MSG_SEND_START)
                        type = EVENT_MSG_SEND_DONE;
                    else if(states[pe].start_event.type == EVENT_MEM_READ_START)
                        type = EVENT_MEM_READ_DONE;
                    else
                        type = EVENT_MEM_WRITE_DONE;
                    Event ev(pe, timestamp, type,
                        states[pe].start_event.size, states[pe].start_event.remote, tag++);
                    buf.push_back(ev);

                    last_pe = std::max(pe, std::max(last_pe, ev.remote));
                }

                states[pe].in_cmd = false;
            }
            else {
                if (std::regex_search(line, match, msg_snd_regex)) {
                    states[pe].start_event = build_event(EVENT_MSG_SEND_START,
                        timestamp, pe, match[1].str(), match[2].str(), tag);
                    states[pe].have_start = true;
                    buf.push_back(states[pe].start_event);
                }
                else if(std::regex_search(line, match, msg_rw_regex)) {
                    event_type type = match[1].str() == "rd" ? EVENT_MEM_READ_START
                                                             : EVENT_MEM_WRITE_START;
                    states[pe].start_event = build_event(type,
                        timestamp, pe, match[2].str(), match[3].str(), tag);
                    states[pe].have_start = true;
                    buf.push_back(states[pe].start_event);
                }
            }
        }
    }

    fclose(fd);
    return last_pe + 1;
}

int main(int argc,char **argv) {
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        fprintf(stderr, "  <file>: the gem5 log file with flags >= Dtu,DtuCmd,DtuConnector\n");
        return EXIT_FAILURE;
    }

    std::vector<Event> trace_buf;

    int pe_count = read_trace_file(argv[1], trace_buf);

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

    // Processes.
    int stream = 1;
    for(int i = 0; i < pe_count; ++i) {
        char peName[8];
        snprintf(peName, sizeof(peName), "Pe%d", i);
        OTF_Writer_writeDefProcess(writer, 0, i, peName, 0);
        OTF_Writer_assignProcess(writer, i, stream);
    }

    // Process groups
    uint32_t allPEs[pe_count];
    for(int i = 0; i < pe_count; ++i)
        allPEs[i] = i;

    unsigned grp_mem = (1 << 20) + 1;
    OTF_Writer_writeDefProcessGroup(writer, 0, grp_mem, "Memory Read/Write", pe_count, allPEs);
    unsigned grp_msg = (1 << 20) + 2;
    OTF_Writer_writeDefProcessGroup(writer, 0, grp_msg, "Message Send/Receive", pe_count, allPEs);

    // Function groups
    unsigned grp_func_count = 0;
    unsigned grp_func_gen = grp_func_count++;
    OTF_Writer_writeDefFunctionGroup(writer, 0, grp_func_gen, "General");
    unsigned grp_func_mem = grp_func_count++;
    OTF_Writer_writeDefFunctionGroup(writer, 0, grp_func_mem, "Memory");
    unsigned grp_func_msg = grp_func_count++;
    OTF_Writer_writeDefFunctionGroup(writer, 0, grp_func_msg, "Messaging");
    unsigned grp_func_user = grp_func_count++;
    OTF_Writer_writeDefFunctionGroup(writer, 0, grp_func_user, "User");

    // General functions
    unsigned fn_gen_awake = (2 << 20) + 1;
    OTF_Writer_writeDefFunction(writer, 0, fn_gen_awake, "Running", grp_func_gen, 0);
    unsigned fn_gen_sleep = (2 << 20) + 2;
    OTF_Writer_writeDefFunction(writer, 0, fn_gen_sleep, "Sleeping", grp_func_gen, 0);

    // Message functions
    unsigned fn_msg_send = (2 << 20) + 3;
    OTF_Writer_writeDefFunction(writer, 0, fn_msg_send, "msg_send", grp_func_msg, 0);

    // Memory Functions
    unsigned fn_mem_read = (2 << 20) + 4;
    OTF_Writer_writeDefFunction(writer, 0, fn_mem_read, "mem_read", grp_func_mem, 0);
    unsigned fn_mem_write = (2 << 20) + 5;
    OTF_Writer_writeDefFunction(writer, 0, fn_mem_write, "mem_write", grp_func_mem, 0);

    unsigned processed_events = 0;
    unsigned num_send = 0, num_recv = 0, num_read = 0, num_write = 0, num_finish = 0;
    unsigned num_ufunc_enter = 0, num_ufunc_exit = 0, num_func_enter = 0, num_func_exit = 0;
    unsigned warnings = 0;

    printf("writing OTF events\n");

    uint64_t timestamp = 0;

    bool awake[pe_count];

    for(int i = 0; i < pe_count; ++i) {
        awake[i] = true;
        OTF_Writer_writeEnter(writer, timestamp, fn_gen_awake, i, 0);
    }

    // finally loop over events and write OTF
    for(auto event = trace_buf.begin(); event != trace_buf.end(); ++event) {
        // don't use the same timestamp twice
        if(event->timestamp == timestamp)
            event->timestamp++;

        timestamp = event->timestamp;

        if(VERBOSE)
            std::cout << *event << "\n";

        switch(event->type) {
            case EVENT_MSG_SEND_START:
                OTF_Writer_writeEnter(writer, timestamp, fn_msg_send, event->pe, 0);
                OTF_Writer_writeSendMsg(writer, timestamp,
                    event->pe, event->remote, grp_msg, event->tag, event->size, 0);
                ++num_send;
                break;

            case EVENT_MSG_RECV:
                OTF_Writer_writeRecvMsg(writer, timestamp,
                    event->pe, event->remote, grp_msg, event->tag, event->size, 0);
                ++num_recv;
                break;

            case EVENT_MSG_SEND_DONE:
                OTF_Writer_writeLeave(writer, timestamp, fn_msg_send, event->pe, 0);
                break;

            case EVENT_MEM_READ_START:
                OTF_Writer_writeEnter(writer, timestamp, fn_mem_read, event->pe, 0);
                OTF_Writer_writeSendMsg(writer, timestamp,
                    event->pe, event->remote, grp_mem, event->tag, event->size, 0);
                ++num_read;
                break;

            case EVENT_MEM_READ_DONE:
                OTF_Writer_writeLeave(writer, timestamp, fn_mem_read, event->pe, 0);
                OTF_Writer_writeRecvMsg(writer, timestamp,
                    event->remote, event->pe, grp_mem, event->tag, event->size, 0);
                ++num_finish;
                break;

            case EVENT_MEM_WRITE_START:
                OTF_Writer_writeEnter(writer, timestamp, fn_mem_write, event->pe, 0);
                OTF_Writer_writeSendMsg(writer, timestamp,
                    event->pe, event->remote, grp_mem, event->tag, event->size, 0);
                ++num_write;
                break;

            case EVENT_MEM_WRITE_DONE:
                OTF_Writer_writeLeave(writer, timestamp, fn_mem_write, event->pe, 0);
                OTF_Writer_writeRecvMsg(writer, timestamp,
                    event->remote, event->pe, grp_mem, event->tag, event->size, 0);
                ++num_finish;
                break;

            case EVENT_WAKEUP:
                if(!awake[event->pe]) {
                    OTF_Writer_writeLeave(writer, timestamp - 1, fn_gen_sleep, event->pe, 0);
                    OTF_Writer_writeEnter(writer, timestamp, fn_gen_awake, event->pe, 0);
                    awake[event->pe] = true;
                }
                break;

            case EVENT_SUSPEND:
                if(awake[event->pe]) {
                    OTF_Writer_writeLeave(writer, timestamp - 1, fn_gen_awake, event->pe, 0);
                    OTF_Writer_writeEnter(writer, timestamp, fn_gen_sleep, event->pe, 0);
                    awake[event->pe] = false;
                }
                break;
        }

        ++processed_events;
    }

    for(int i = 0; i < pe_count; ++i) {
        if(awake[i])
            OTF_Writer_writeLeave(writer, timestamp, fn_gen_awake, i, 0);
        else
            OTF_Writer_writeLeave(writer, timestamp, fn_gen_sleep, i, 0);
    }

    if(num_send != num_recv) {
        printf("WARNING: num_send != num_recv\n");
        ++warnings;
    }
    if(num_read + num_write != num_finish) {
        printf("WARNING: num_read+num_write != num_finish\n");
        ++warnings;
    }
    if(num_func_enter != num_func_exit) {
        printf("WARNING: num_func_enter != num_func_exit\n");
        ++warnings;
    }
    if(num_ufunc_enter != num_ufunc_exit) {
        printf("WARNING: num_ufunc_enter != num_ufunc_exit\n");
        ++warnings;
    }

    printf("processed events: %u\n", processed_events);
    printf("warnings: %u\n", warnings);
    printf("num_send: %u\n", num_send);
    printf("num_recv: %u\n", num_recv);
    printf("num_read: %u\n", num_read);
    printf("num_write: %u\n", num_write);
    printf("num_finish: %u\n", num_finish);
    printf("num_func_enter: %u\n", num_func_enter);
    printf("num_func_exit: %u\n", num_func_exit);
    printf("num_ufunc_enter: %u\n", num_ufunc_enter);
    printf("num_ufunc_exit: %u\n", num_ufunc_exit);

    // Clean up before exiting the program.
    OTF_Writer_close(writer);
    OTF_FileManager_close(manager);

    return 0;
}
