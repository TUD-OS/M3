// vim:ft=cpp
/*
 * (c) 2007-2013 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING-GPL-2 file for details.
 */

#ifndef __TRACE_BENCH_CLOCK_H
#define __TRACE_BENCH_CLOCK_H

#include <m3/Common.h>

#include <sys/time.h>
#include <stdint.h>
#include "exceptions.h"

/*
 * *************************************************************************
 */

class Clock {

  public:
    typedef enum { MicroSeconds, MilliSeconds, Seconds } Unit;

    Clock() : t0(0), t1(0) { }

    /*
     * @brief Returns the current time in CPU cycles.
     */
    uint64_t timeStamp() {

        struct timeval tv;
        gettimeofday(&tv, nullptr);
        //printf("ts=%llu\n", tv.tv_sec * 1000000ULL + tv.tv_usec);
        return tv.tv_sec * 1000000ULL + tv.tv_usec;
    };

    /*
     * @brief Start timer.
     */
    void start() {

        t0 = timeStamp();
    };

    /*
     * @brief Stop timer.
     */
    void stop() {

        t1 = timeStamp();
    };

    /*
     * @brief Returns the time that passed between the last start()/stop()
     *        cycle.
     *
     * @param unit  The unit in which the result should be reported.
     *
     * @return Duration in the specified time unit.
     */
    unsigned long long duration(Unit unit) {

        uint64_t d = t1 - t0;

        switch (unit) {
            case MicroSeconds:
                return d;
            case MilliSeconds:
                return d / 1000;
            case Seconds:
                return d / 1000000;
            default:
                UNREACHED;
        }
    };

  private:
    uint64_t t0, t1;
};

#endif /* __TRACE_BENCH_CLOCK_H */

