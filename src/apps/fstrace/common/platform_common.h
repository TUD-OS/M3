// vim:ft=cpp
/*
 * (c) 2007-2013 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING-GPL-2 file for details.
 */

#ifndef __TRACE_BENCH_PLATFORM_COMMON_H
#define __TRACE_BENCH_PLATFORM_COMMON_H

#include <string>

#include "fsapi.h"

/*
 * *************************************************************************
 */

class Platform {

  public:
    /*
     * @brief Initializes the platform.
     */
    static void init(int argc, const char * const * argv);

    /**
     * @brief return the FSAPI object
     */
    static FSAPI *fsapi(const char *root);

    /*
     * @brief Shutdown platform subsystems (if any).
     */
    static void shutdown();

    /*
     * @brief Sync all data to stable storage.
     */
    static void sync_fs();

    /*
     * @brief Sync all data to stable storage.
     */
    static void checkpoint_fs();

    /*
     * @brief Drop all clean data from trusted and untrusted caches.
     */
    static void drop_caches();

    /*
     * @brief Prints a log message in as necessary for the used platform.
     */
    static void log(const std::string &msg);

    /*
     * @brief Prints a formatted log message in as necessary for the used platform.
     */
    static void logf(const char *fmt, ...);
};

#endif /* __TRACE_BENCH_PLATFORM_COMMON_H */

