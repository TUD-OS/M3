/*
 * Copyright (C) 2015, Matthias Lieber <matthias.lieber@tu-dresden.de>
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

// enforce TRACE_HUMAN_READABLE
#include <m3/tracing/Config.h>
#ifndef TRACE_HUMAN_READABLE
#   define TRACE_HUMAN_READABLE
#endif
#define TRACE_FUNCS_TO_STRING

#include <m3/tracing/Event.h>

#include <iostream>
#include <queue>
#include <map>
#include <set>
#include <array>
#include <vector>
#include <algorithm>

#ifndef VERBOSE
#define VERBOSE 0
#endif

#define M3_TRACE_FILE_NAME "./trace.txt"
#define TH_CLOCK_MHZ 400
#define TH_NUM_PES 8

#define MEM_TAG 0

using namespace m3;

struct Event_Pe
{
    // we store the whole 64bit Event, but we only use the least 32bits that store the event payload, the rest is type and timestamp
    Event event;
    unsigned int pe;
    //unsigned int type;
    uint64_t timestamp;
};


int read_m3_trace_file( char *path, std::vector<Event_Pe> &buf )
{
    char filename[256];
    char readbuf[32];
    if( path )
        strcpy( filename, path );
    else
        strcpy( filename, M3_TRACE_FILE_NAME );

    printf( "reading trace file: %s\n", filename );

    FILE *fd = fopen( filename, "r" );
    if( !fd )
    {
        perror( "cannot open trace file" );
        return 1;
    }

    unsigned int pe = 0;
    Event_Pe tmp;
    uint64_t timestamp = 0;

    // read the trace file into the buffer
    // and extrace pe and timestamp into type Event_Pe
    while( fgets( readbuf, 32, fd ) )
    {
        if( readbuf[0] == 'p' )
        {
            sscanf( readbuf + 2, "%u", &pe );
            timestamp = 0;
        }
        else
        {
            if( !pe )
            {
                puts( "no pe defined\n" );
                return 2;
            }
            long long unsigned int record;
            sscanf( readbuf, "%Lx", &record );
            tmp.event.record = ( rec_t )record;
            if( tmp.event.type() == EVENT_TIMESTAMP )
            {
                timestamp = tmp.event.init_timestamp();
            }
            else
            {
                timestamp += ( tmp.event.timestamp() << TIMESTAMP_SHIFT );
                tmp.timestamp = timestamp;
                tmp.pe = pe;
                buf.push_back( tmp );
            }
        }
    }

    fclose( fd );
    return 0;
}


int main( int argc, char **argv )
{
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    std::vector<Event_Pe> trace_buf;

    read_m3_trace_file( argv[1], trace_buf );

    unsigned int num_events = ( unsigned int )trace_buf.size();

    // now sort the trace buffer according to timestamps
    printf( "sorting %u events\n", num_events );
    struct
    {
        bool operator()( const Event_Pe &a, const Event_Pe &b )
        {
            return a.timestamp < b.timestamp;
        }
    } eventCmpOp;
    std::sort( trace_buf.begin(), trace_buf.end(), eventCmpOp );

    // Declare a file manager and a writer.
    OTF_FileManager *manager;
    OTF_Writer *writer;

    // Initialize the file manager. Open at most 100 OS files.
    manager = OTF_FileManager_open( 100 );
    assert( manager );

    // Initialize the writer.
    writer = OTF_Writer_open( "trace", 1, manager );
    assert( writer );

    // Write some important Definition Records.
    // Timer res. in ticks per second
    OTF_Writer_writeDefTimerResolution( writer, 0, TH_CLOCK_MHZ * 1000 * 1000 );

    // Processes.
    int stream = 1;
    int pe1 = 4;
    for( int i = 0; i < TH_NUM_PES; ++i )
    {
        char peName[8];
        snprintf( peName, 5, "Pe%d", i + 1 );
        OTF_Writer_writeDefProcess( writer, 0, pe1 + i, peName, 0 );
        OTF_Writer_assignProcess( writer, pe1 + i, stream );
    }
    int mem = 2;
    OTF_Writer_writeDefProcess( writer, 0, mem, "Mem", 0 );
    OTF_Writer_assignProcess( writer, mem, stream );

    // Process groups
    uint32_t allPEs[TH_NUM_PES + 1];
    for( int i = 0; i < TH_NUM_PES; ++i ) allPEs[i] = pe1 + i;
    allPEs[TH_NUM_PES] = mem;
    unsigned grp_mem = ( 1 << 20 ) + 1;
    OTF_Writer_writeDefProcessGroup( writer, 0, grp_mem, "Remote Memory Read/Write", TH_NUM_PES + 1, allPEs );
    unsigned grp_msg = ( 1 << 20 ) + 2;
    OTF_Writer_writeDefProcessGroup( writer, 0, grp_msg, "Remote Message Send/Receive", TH_NUM_PES, allPEs );


    // Function groups
    // defined in Event.h
    unsigned grp_func_start = ( 5 << 20 );
    for( unsigned int i = 0; i < event_func_groups_size; ++i )
    {
        OTF_Writer_writeDefFunctionGroup( writer, 0, grp_func_start + i, event_func_groups[i] );
    }
    // other groups
    unsigned grp_func_count = grp_func_start + event_func_groups_size;
    unsigned grp_func_mem = grp_func_count++;
    OTF_Writer_writeDefFunctionGroup( writer, 0, grp_func_mem, "Memory" );
    unsigned grp_func_user = grp_func_count++;
    OTF_Writer_writeDefFunctionGroup( writer, 0, grp_func_user, "User" );


    // Memory Functions
    unsigned fn_mem_read = ( 2 << 20 ) + 1;
    OTF_Writer_writeDefFunction( writer, 0, fn_mem_read, "mem_read", grp_func_mem, 0 );
    unsigned fn_mem_write = ( 2 << 20 ) + 2;
    OTF_Writer_writeDefFunction( writer, 0, fn_mem_write, "mem_write", grp_func_mem, 0 );
    // other functions will be defined on the fly


    unsigned processed_events = 0;
    unsigned num_send = 0, num_recv = 0, num_read = 0, num_write = 0, num_finish = 0;
    unsigned num_ufunc_enter = 0, num_ufunc_exit = 0, num_func_enter = 0, num_func_exit = 0;
    unsigned warnings = 0;
    uint32_t max_timestamp = 0;

    std::queue<Event_Pe> mem_event[TH_NUM_PES];

    uint32_t ufunc_max_id = ( 3 << 20 );
    std::map<uint32_t, uint32_t> ufunc_map;

    uint32_t func_start_id = ( 4 << 20 );
    std::set<uint32_t> func_set;

    // function call stack per PE
    std::array<uint, TH_NUM_PES> func_stack;
    func_stack.fill( 0 );
    std::array<uint, TH_NUM_PES> ufunc_stack;
    ufunc_stack.fill( 0 );

    printf( "writing OTF events\n" );

    // finally loop over events and write OTF
    for( unsigned i = 0; i < num_events; ++i )
    {
        Event event = trace_buf[i].event;
        uint64_t timestamp = trace_buf[i].timestamp;
        unsigned int pe = trace_buf[i].pe;

        switch( trace_buf[i].event.type() )
        {
            case EVENT_TIMESTAMP:
                if( VERBOSE ) std::cout << pe << " EVENT_TIMESTAMP: " << timestamp << "\n";
                // ignored, we already have absolute timestamps
                break;
            case EVENT_MSG_SEND:
                if( VERBOSE ) std::cout << pe << " EVENT_MSG_SEND: " << timestamp << "  receiver: " << event.msg_remote() << "  size: " << event.msg_size() << "  tag: " << event.msg_tag() << "\n";
                OTF_Writer_writeSendMsg( writer, timestamp, pe, event.msg_remote(), grp_msg, event.msg_tag(), event.msg_size(), 0 );
                ++num_send;
                break;
            case EVENT_MSG_RECV:
                if( VERBOSE ) std::cout << pe << " EVENT_MSG_RECV: " << timestamp << "  sender: " << event.msg_remote() << "  size: " << event.msg_size() << "  tag: " << event.msg_tag() << "\n";
                OTF_Writer_writeRecvMsg( writer, timestamp, pe, event.msg_remote(), grp_msg, event.msg_tag(), event.msg_size(), 0 );
                ++num_recv;
                break;
            case EVENT_MEM_READ:
                if( VERBOSE ) std::cout << pe << " EVENT_MEM_READ: " << timestamp << "  core: " << event.mem_remote() << "  size: " << event.mem_size() << "\n";
                OTF_Writer_writeEnter( writer, timestamp, fn_mem_read, pe, 0 );
                OTF_Writer_writeSendMsg( writer, timestamp, event.msg_remote(), pe, grp_mem, MEM_TAG, event.msg_size(), 0 );
                mem_event[pe - pe1].push( trace_buf[i] );
                ++num_read;
                break;
            case EVENT_MEM_WRITE:
                if( VERBOSE ) std::cout << pe << " EVENT_MEM_WRITE: " << timestamp << "  core: " << event.mem_remote() << "  size: " << event.mem_size() << "\n";
                OTF_Writer_writeEnter( writer, timestamp, fn_mem_write, pe, 0 );
                OTF_Writer_writeSendMsg( writer, timestamp, pe, event.msg_remote(), grp_mem, MEM_TAG, event.msg_size(), 0 );
                mem_event[pe - pe1].push( trace_buf[i] );
                ++num_write;
                break;
            case EVENT_MEM_FINISH:
            {
                Event_Pe me;
                me.event.record = 0;
                if( !mem_event[pe - pe1].empty() )
                {
                    me = mem_event[pe - pe1].front();
                    mem_event[pe - pe1].pop();
                }
                if( me.event.type() == EVENT_MEM_READ )
                {
                    if( VERBOSE ) std::cout << pe << " EVENT_MEM_FINISH: " << timestamp << " (read) \n";
                    OTF_Writer_writeLeave( writer, timestamp, fn_mem_read, pe, 0 );
                    OTF_Writer_writeRecvMsg( writer, timestamp, pe, me.event.msg_remote(), grp_mem, MEM_TAG, me.event.msg_size(), 0 );
                }
                else if( me.event.type() == EVENT_MEM_WRITE )
                {
                    if( VERBOSE ) std::cout << pe << " EVENT_MEM_FINISH: " << timestamp << " (write) \n";
                    OTF_Writer_writeLeave( writer, timestamp, fn_mem_write, pe, 0 );
                    OTF_Writer_writeRecvMsg( writer, timestamp, me.event.msg_remote(), pe, grp_mem, MEM_TAG, me.event.msg_size(), 0 );
                }
                else
                {
                    std::cout << pe << " EVENT_MEM_FINISH: " << timestamp << " WARNING: No match. Dropping event.\n";
                    ++warnings;
                }
                ++num_finish;
            }
            break;
            case EVENT_UFUNC_ENTER:
            {
                if( VERBOSE ) std::cout << pe << " EVENT_UFUNC_ENTER: " << timestamp << "  name: " << event.func_id() << "  " << event.ufunc_name_str() << "\n";
                std::map<uint32_t, uint32_t>::iterator ufunc_map_iter = ufunc_map.find( event.func_id() );
                uint32_t id = 0;
                if( ufunc_map_iter == ufunc_map.end() )
                {
                    id = ( ++ufunc_max_id );
                    ufunc_map.insert( std::pair<uint32_t, uint32_t>( event.func_id(), id ) );
                    OTF_Writer_writeDefFunction( writer, 0, id, event.ufunc_name_str(), grp_func_user, 0 );
                }
                else
                {
                    id = ufunc_map_iter->second;
                }
                ++( ufunc_stack[pe - pe1] );
                OTF_Writer_writeEnter( writer, timestamp, id, pe, 0 );
                ++num_ufunc_enter;
            }
            break;
            case EVENT_UFUNC_EXIT:
            {
                if( VERBOSE ) std::cout << pe << " EVENT_UFUNC_EXIT: " << timestamp << "\n";
                if( ufunc_stack[pe - pe1] < 1 )
                {
                    std::cout << pe << " WARNING: exit at ufunc stack level " << ufunc_stack[pe - pe1] << " dropped.\n";
                    ++warnings;
                }
                else
                {
                    --( ufunc_stack[pe - pe1] );
                    OTF_Writer_writeLeave( writer, timestamp, 0, pe, 0 );
                }
                ++num_ufunc_exit;
            }
            break;
            case EVENT_FUNC_ENTER:
            {
                if( VERBOSE ) std::cout << pe << " EVENT_FUNC_ENTER: " << timestamp << "  name: " << event.func_id() << "  " << event_funcs[event.func_id()].name << "\n";
                uint32_t id = event.func_id();
                if( func_set.find( id ) == func_set.end() )
                {
                    func_set.insert( id );
                    unsigned group = grp_func_start + event_funcs[id].group;
                    OTF_Writer_writeDefFunction( writer, 0, func_start_id + id, event_funcs[id].name, group , 0 );
                }
                ++( func_stack[pe - pe1] );
                OTF_Writer_writeEnter( writer, timestamp, func_start_id + id, pe, 0 );
                ++num_func_enter;
            }
            break;
            case EVENT_FUNC_EXIT:
            {
                if( VERBOSE ) std::cout << pe << " EVENT_FUNC_EXIT: " << timestamp << "\n";
                if( func_stack[pe - pe1] < 1 )
                {
                    std::cout << pe << " WARNING: exit at func stack level " << func_stack[pe - pe1] << " dropped.\n";
                    ++warnings;
                }
                else
                {
                    --( func_stack[pe - pe1] );
                    OTF_Writer_writeLeave( writer, timestamp, 0, pe, 0 );
                }
                ++num_func_exit;
            }
            break;
            default:
                printf( "WARNING: UNKOWN EVENT TYPE %lu at %d\n", ( long unsigned int )( trace_buf[i].event.type() ), i );
                ++warnings;
        }

        if( trace_buf[i].event.type() != EVENT_TIMESTAMP )
        {
            if( max_timestamp < event.timestamp() ) max_timestamp = event.timestamp();
        }

        ++processed_events;
    }

    if( num_send != num_recv )
    {
        printf( "WARNING: num_send != num_recv\n" );
        ++warnings;
    }
    if( num_read + num_write != num_finish )
    {
        printf( "WARNING: num_read+num_write != num_finish\n" );
        ++warnings;
    }
    if( num_func_enter != num_func_exit )
    {
        printf( "WARNING: num_func_enter != num_func_exit\n" );
        ++warnings;
    }
    if( num_ufunc_enter != num_ufunc_exit )
    {
        printf( "WARNING: num_ufunc_enter != num_ufunc_exit\n" );
        ++warnings;
    }

    printf( "processed events: %u\n", processed_events );
    printf( "warnings: %u\n", warnings );
    printf( "num_send: %u\n", num_send );
    printf( "num_recv: %u\n", num_recv );
    printf( "num_read: %u\n", num_read );
    printf( "num_write: %u\n", num_write );
    printf( "num_finish: %u\n", num_finish );
    printf( "num_func_enter: %u\n", num_func_enter );
    printf( "num_func_exit: %u\n", num_func_exit );
    printf( "num_ufunc_enter: %u\n", num_ufunc_enter );
    printf( "num_ufunc_exit: %u\n", num_ufunc_exit );
    printf( "max_timestamp delta: %u / %lu  %.1fx\n", max_timestamp, REC_MASK_TIMESTAMP, ( float )REC_MASK_TIMESTAMP / ( float )max_timestamp );


    // Clean up before exiting the program.
    OTF_Writer_close( writer );
    OTF_FileManager_close( manager );

    return 0;
}
