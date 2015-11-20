/*
 * Copyright (C) 2015, Matthias Lieber <matthias.lieber@tu-dresden.de>
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

#include <m3/stream/OStream.h>
#include <m3/stream/Serial.h>
#include <m3/util/Profile.h>
#include <m3/tracing/Tracing.h>
#include <m3/Log.h>

#if defined(TRACE_ENABLED)

#define GET_CYCLES      Profile::start()

namespace m3 {

Tracing Tracing::_inst INIT_PRIORITY(1000);

OStream & operator<<(OStream &os, const Event &event) {
#if defined(TRACE_HUMAN_READABLE)
    switch(event.type()) {
        case EVENT_TIMESTAMP:
            os << "EVENT_TIMESTAMP: " << event.init_timestamp();
            break;
        case EVENT_MSG_SEND:
            os << "EVENT_MSG_SEND: " << event.timestamp()
               << "  receiver: " << event.msg_remote()
               << "  size: " << event.msg_size()
               << "  tag: " << event.msg_tag();
            break;
        case EVENT_MSG_RECV:
            os << "EVENT_MSG_RECV: " << event.timestamp()
               << "  sender: " << event.msg_remote()
               << "  size: " << event.msg_size()
               << "  tag: " << event.msg_tag();
            break;
        case EVENT_MEM_READ:
            os << "EVENT_MEM_READ: " << event.timestamp()
               << "  core: " << event.mem_remote()
               << "  size: " << event.mem_size();
            break;
        case EVENT_MEM_WRITE:
            os << "EVENT_MEM_WRITE: " << event.timestamp()
               << "  core: " << event.mem_remote()
               << "  size: " << event.mem_size();
            break;
        case EVENT_MEM_FINISH:
            os << "EVENT_MEM_FINISH: " << event.timestamp();
            break;
        case EVENT_UFUNC_ENTER:
            os << "EVENT_UFUNC_ENTER: " << event.timestamp()
               << "  name: " << event.func_id()
               << "  " << event.ufunc_name_str();
            break;
        case EVENT_UFUNC_EXIT:
            os << "EVENT_UFUNC_EXIT: " << event.timestamp()
               << "  name: " << event.func_id()
               << "  " << event.ufunc_name_str();
            break;
        default:
            os << "UNKOWN EVENT TYPE:" << event.type()
               << " [" << fmt(event.record >> 32, "X")
               << ":" << fmt(event.record & 0xFFFFFFFF, "8X")
               << "]";
            break;
    }
#else
    // format: [32bit]:[32bit]
    os << "EVENT " << fmt(event.record >> 32, "X") << ":" << fmt(event.record & 0xFFFFFFFF, "X");
#endif
    return os;
}

Tracing::Tracing() {
    trace_enabled = false;

#if !defined(__t2__)
    for(int i = 0; i < ARRAY_SIZE(trace_core); ++i)
        trace_core[i] = !!((1 << i) & PE_MASK);
#endif

    if(coreid() != KERNEL_CORE)
        reinit();
    else
        reset();

    trace_enabled = true;
}

void Tracing::flush_light() {
    // is the current buffer half >80% full? if not, return
    if(next_event < eventbuf_half) {
        // we are in first half
        if(next_event - eventbuf <= 4 * (eventbuf_half - next_event))
            return;
    }
    else {
        // we are in second half
        if(next_event - eventbuf_half <= 4 * (eventbuf_end - next_event))
            return;
    }

    trace_enabled = false;

    LOG(TRACE, "Tracing::flush_light, event count: " << event_counter);
    send_event_buffer_to_mem();

    trace_enabled = true;
}

void Tracing::flush() {
    trace_enabled = false;

    LOG(TRACE, "Tracing::flush, event count: " << event_counter);
    between_flush_and_reinit = true;

    send_event_buffer_to_mem();

    // write event counter at beginning of core's trace buffer
    uint64_t c = event_counter;
    mem_write(membuf_start, &c, sizeof(c));

    next_event = eventbuf;

    trace_enabled = true;
}

void Tracing::reinit() {
    assert(coreid() == KERNEL_CORE);
    trace_enabled = false;
    reset();

    uint64_t c = 0;
    mem_read(membuf_start, &c, sizeof(c));
    event_counter = c;
    membuf_cur = membuf_start + (1 + event_counter) * sizeof(Event);

    LOG(TRACE, "Tracing::reinit"
        << ", event_counter: " << event_counter
        << ", membuf_cur: " << fmt(membuf_cur, "p")
        << ", buffer ok: " << (membuf_cur < membuf_start + TRACE_MEMBUF_SIZE));

    // create init_timestamp event
    record_event_timestamp();

    trace_enabled = true;
}

void Tracing::init_kernel() {
    assert(coreid() != KERNEL_CORE);
    trace_enabled = false;
    reset();

    LOG(TRACE, "Tracing::init_kernel");

    // set up buffers in Mem: write number of events (i.e. 0) to start addresses of each core's buffer
    for(int icore = 0; icore < MAX_CORES; ++icore) {
        uint64_t c = 0;
        mem_write(membuf_of(icore), &c, sizeof(c));
    }

    // create init_timestamp event
    record_event_timestamp();

    trace_enabled = true;
}

void Tracing::trace_dump() {
    assert(coreid() != KERNEL_CORE);
    trace_enabled = false;

    for(uint icore = 0; icore < MAX_CORES; ++icore) {
        cycles_t timestamp = 0;
        uint64_t c = 0;
        mem_read(membuf_of(icore), &c, sizeof(c));
        LOG(TRACE, "Dumping trace events of " << icore + FIRST_PE_ID << "  " << c);
        if(c > TRACE_MEMBUF_SIZE / sizeof(Event) - 1)
            // TODO BUG?
            c = c > TRACE_MEMBUF_SIZE / sizeof(Event) - 1;

        // read the records using the eventbuf
        for(uint ir = 0; ir < c; ir += TRACE_EVENTBUF_SIZE) {
            uint nevents = TRACE_EVENTBUF_SIZE;
            if(ir + TRACE_EVENTBUF_SIZE > c)
                nevents = c - ir;

            mem_read(membuf_of(icore) + (1 + ir) * sizeof(Event), eventbuf, nevents * sizeof(Event));
            for(uint ievent = 0; ievent < nevents; ++ievent) {
                Event event = eventbuf[ievent];
                if(event.type() == EVENT_TIMESTAMP)
                    timestamp = event.init_timestamp();
                else
                    timestamp += event.timestamp() << TIMESTAMP_SHIFT;
                LOG(TRACE, "Trace event " << timestamp
                    << ":" << event.type()
                    << ":" << fmt(event.record >> 32,"X")
                    << ":" << fmt(event.record & 0xFFFFFFFF,"X"));
            }
        }
    }
}

void Tracing::record_event_msg(uint8_t type, uchar remotecore, size_t length, uint16_t tag) {
    cycles_t now = GET_CYCLES;
    handle_zero_delta(&now);
    handle_large_delta(now);
    next_event->record  = (((rec_t)type) & REC_MASK_TYPE) << REC_SHIFT_TYPE;
    next_event->record |= (((rec_t)((now - last_timestamp) >> TIMESTAMP_SHIFT)) & REC_MASK_TIMESTAMP) << REC_SHIFT_TIMESTAMP;
    next_event->record |= (((rec_t)length) & REC_MASK_MSG_SIZE) << REC_SHIFT_MSG_SIZE;
    next_event->record |= (((rec_t)remotecore) & REC_MASK_MSG_REMOTE) << REC_SHIFT_MSG_REMOTE;
    next_event->record |= (((rec_t)tag) & REC_MASK_MSG_TAG) << REC_SHIFT_MSG_TAG;
    last_timestamp = now;
    advance_event_buffer();
}

void Tracing::record_event_mem(uint8_t type, uchar remotecore, size_t length) {
    cycles_t now = GET_CYCLES;
    handle_zero_delta(&now);
    handle_large_delta(now);
    next_event->record  = (((rec_t)type) & REC_MASK_TYPE) << REC_SHIFT_TYPE;
    next_event->record |= (((rec_t)((now - last_timestamp) >> TIMESTAMP_SHIFT)) & REC_MASK_TIMESTAMP) << REC_SHIFT_TIMESTAMP;
    next_event->record |= (((rec_t)length) & REC_MASK_MEM_SIZE) << REC_SHIFT_MEM_SIZE;
    next_event->record |= (((rec_t)remotecore) & REC_MASK_MEM_REMOTE) << REC_SHIFT_MEM_REMOTE;
    //next_event->record |= (((rec_t) 0 ) & REC_MASK_MEM_DURATION ) << REC_SHIFT_MEM_DURATION;
    last_timestamp = now;
    last_mem_event = next_event; // save the last memory event
    advance_event_buffer();
}

void Tracing::record_event_mem_finish() {
    cycles_t now = GET_CYCLES;
    handle_zero_delta(&now);
    handle_large_delta(now);
    next_event->record  = (((rec_t)EVENT_MEM_FINISH) & REC_MASK_TYPE) << REC_SHIFT_TYPE;
    next_event->record |= (((rec_t)((now - last_timestamp) >> TIMESTAMP_SHIFT)) & REC_MASK_TIMESTAMP) << REC_SHIFT_TIMESTAMP;
    last_timestamp = now;
    last_mem_event = 0;
    advance_event_buffer();
}

void Tracing::record_event_timestamp() {
    cycles_t now = GET_CYCLES;
    next_event->record  = (((rec_t)EVENT_TIMESTAMP) & REC_MASK_TYPE) << REC_SHIFT_TYPE;
    next_event->record |= (((rec_t)now) & REC_MASK_INIT_TIMESTAMP) << REC_SHIFT_INIT_TIMESTAMP;
    last_timestamp = now;
    advance_event_buffer();
}

void Tracing::record_event_timestamp(cycles_t now) {
    next_event->record  = (((rec_t)EVENT_TIMESTAMP) & REC_MASK_TYPE) << REC_SHIFT_TYPE;
    next_event->record |= (((rec_t)now ) & REC_MASK_INIT_TIMESTAMP) << REC_SHIFT_INIT_TIMESTAMP;
    last_timestamp = now;
    advance_event_buffer();
}

void Tracing::record_event_func(uint8_t type, uint32_t id) {
    cycles_t now = GET_CYCLES;
    handle_zero_delta(&now);
    handle_large_delta(now);
    next_event->record  = (((rec_t)type) & REC_MASK_TYPE) << REC_SHIFT_TYPE;
    next_event->record |= (((rec_t)((now - last_timestamp) >> TIMESTAMP_SHIFT)) & REC_MASK_TIMESTAMP) << REC_SHIFT_TIMESTAMP;
    next_event->record |= (((rec_t)id) & REC_MASK_FUNC_ID) << REC_SHIFT_FUNC_ID;
    last_timestamp = now;
    advance_event_buffer();
}

void Tracing::record_event_func_exit(uint8_t type) {
    cycles_t now = GET_CYCLES;
    handle_zero_delta(&now);
    handle_large_delta(now);
    next_event->record  = (((rec_t)type) & REC_MASK_TYPE) << REC_SHIFT_TYPE;
    next_event->record |= (((rec_t)((now - last_timestamp) >> TIMESTAMP_SHIFT)) & REC_MASK_TIMESTAMP) << REC_SHIFT_TIMESTAMP;
    //next_event->record |= (((rec_t) last_function ) & REC_MASK_FUNC_ID ) << REC_SHIFT_FUNC_ID;
    last_timestamp = now;
    advance_event_buffer();
}

inline void Tracing::advance_event_buffer() {
    event_counter++;
    next_event++;

    // send the full buffer half to Mem
    if(next_event == eventbuf_half || next_event == eventbuf_end)
        send_event_buffer_to_mem();

    // if there is an event after flushing and before reinit, we flush after each event
    // this is slow, but consistent
    if(between_flush_and_reinit)
        flush();
}

void Tracing::send_event_buffer_to_mem() {
    // return if Mem buffer is full
    if(membuf_cur >= membuf_start + TRACE_MEMBUF_SIZE)
        return;

    Event* ptr = (next_event > eventbuf_half) ? eventbuf_half : eventbuf;
    size_t size = ((unsigned char*)next_event) - ((unsigned char*)ptr);
    if(size == 0)
        return;

    // switch from 2nd buffer half to 1st: next event goes to the beginning of the buffer
    if(next_event > eventbuf_half)
        next_event = eventbuf;
    // switch from 1st buffer half to 2nd
    else if(next_event < eventbuf_half && next_event > eventbuf)
        next_event = eventbuf_half;

    if(!between_flush_and_reinit && trace_core[coreid()])
        record_event_func(EVENT_FUNC_ENTER, EVENT_FUNC_buffer_to_mem);

    LOG(TRACE, "Tracing::send_event_buffer_to_mem: "
        << size / sizeof(Event)
        << " Events to " << fmt((void*)membuf_cur, "p"));

    if(membuf_cur + size > membuf_start + TRACE_MEMBUF_SIZE) {
        // Mem trace buffer overflow! truncate...
        size = membuf_start + TRACE_MEMBUF_SIZE - membuf_cur;
        LOG(TRACE, "Tracing::send_event_buffer_to_mem: Mem buffer overflow, truncate to "
            << size / sizeof(Event) << " Events");
    }

    // write buffer to Mem
    mem_write(membuf_cur, ptr, size);
    membuf_cur += size;
    if(!between_flush_and_reinit && trace_core[coreid()])
        record_event_func(EVENT_FUNC_EXIT, EVENT_FUNC_buffer_to_mem);
}

void Tracing::reset() {
    last_mem_event = 0;
    next_event = eventbuf;
    membuf_start = membuf_of(coreid() - FIRST_PE_ID);
    membuf_cur = membuf_start + 8; // this will be overwritten in reinit()
    event_counter = 0;
    eventbuf_half = eventbuf + TRACE_EVENTBUF_SIZE / 2;
    eventbuf_end = eventbuf + TRACE_EVENTBUF_SIZE;
    last_timestamp = 0;
    between_flush_and_reinit = false;
}

void Tracing::mem_write(size_t addr, const void * buf, size_t size) {
    LOG(TRACE, "Tracing::mem_write:"
        << fmt((void*)addr, "p") << " " << fmt(buf, "p") << " " << fmt((void*)size, "p"));

    DTU::get().wait_until_ready(SLOT_NO);
    DTU::get().set_target(SLOT_NO, MEMORY_CORE, addr);
    Sync::memory_barrier();
    DTU::get().fire(SLOT_NO, DTU::WRITE, buf, size);

    // wait until the size-register has been decremented to 0
    size_t rem;
    while((rem = DTU::get().get_remaining(0)) > 0);
    //DTU::get().wait_until_ready(SLOT_NO);
}

void Tracing::mem_read(size_t addr, void * buf, size_t size)
{
    LOG(TRACE, "Tracing::mem_read:"
        << fmt((void*)addr, "p") << " " << fmt(buf, "p") << " " << fmt((void*)size, "p"));

    DTU::get().wait_until_ready(SLOT_NO);
    DTU::get().set_target(SLOT_NO, MEMORY_CORE, addr);
    Sync::memory_barrier();
    reinterpret_cast<unsigned char*>(buf)[size - 1] = 0xFF;
    DTU::get().fire(SLOT_NO, DTU::READ, buf, size);

    // wait until the size-register has been decremented to 0
    size_t rem;
    while((rem = DTU::get().get_remaining(0)) > 0);

    // stop if the end has been overwritten
    for(volatile size_t i = 0; i < 1000; ++i) {
        if(reinterpret_cast<unsigned char*>(buf)[size - 1] != 0xFF)
            break;
    }
}

}
#endif
