// vim:ft=cpp
/*
 * (c) 2007-2013 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING-GPL-2 file for details.
 */

#ifndef __TRACE_BENCH_TRACE_RECORDER_H
#define __TRACE_BENCH_TRACE_RECORDER_H

#include <list>
#include <set>

#include "opdescr.h"

/*
 * *************************************************************************
 */

class TraceRecorder {

  public:
    typedef std::list<OpDescr *>                  TraceList;
    typedef std::list<OpDescr *>::const_iterator  TraceListIterator;
    typedef std::set<std::string>                 SysCallSet;
    typedef std::set<std::string>::const_iterator SysCallSetIterator;

    /*
     * @brief Import a strace log and create a trace of Op objects.
     */
    void import();

    /*
     * @brief Print the C code that describes the complete trace.
     */
    void print(const char *name);

  protected:
    /*
     * @brief Print some C code that prepares the trace description.
     */
    void printPrologue(const char *name);

    void printFuncStart(unsigned int funcNum);
    void printFuncEnd();

    void memorizeUnkownSysCall(const std::string &sysCallName);
    void reportUnknownSysCalls();

    /*
     * @brief Print some C code that finalizes the trace description.
     */
    void printEpilogue();

    TraceList  ops;
    SysCallSet sysCalls;
};

#endif // __TRACE_BENCH_TRACE_RECORDER_H
