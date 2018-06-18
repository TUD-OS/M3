// vim:ft=cpp
/*
 * (c) 2007-2013 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING-GPL-2 file for details.
 */

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "fsapi_posix.h"
#include "platform.h"

/*
 * *************************************************************************
 */

void Platform::init(int /*argc*/, const char * const * /*argv*/, const char *) {

}

FSAPI *Platform::fsapi(bool, bool, bool, const char *root) {
    return new FSAPI_POSIX(root);
}


void Platform::shutdown() {

}


void Platform::checkpoint_fs() {

    sync_fs();
}


void Platform::sync_fs() {

    ::sync();
    printf("Synced all buffers\n");
}


void Platform::drop_caches() {

    printf("Sync before dropping caches\n");
    ::sync();

    int fd = ::open("/proc/sys/vm/drop_caches", O_RDWR);
    if (fd < 0) {
        printf("cannot drop caches\n");
        return;
    }

    char const *cmd = "3";
    ssize_t written = write(fd, cmd, strlen(cmd));
    if (written != (ssize_t)strlen(cmd)) {
        printf("cannot drop caches\n");
        return;
    }

    ::close(fd);
    printf("Dropped everything from page, inode, and dentry caches\n");
}


void Platform::log(const char *msg) {

    std::cerr << msg << std::endl;
}

void Platform::logf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
