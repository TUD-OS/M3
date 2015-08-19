// vim:ft=cpp
/*
 * (c) 2007-2013 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING-GPL-2 file for details.
 */

#ifndef __TRACE_BENCH_EXCEPTIONS_H
#define __TRACE_BENCH_EXCEPTIONS_H

#include <string>
#include <sstream>

#include "platform_common.h"

#if defined(__LINUX__)
#   define THROW(ex)                    throw ex()
#   define THROW1(ex, arg1, ...)        throw ex(arg1, ## __VA_ARGS__)
#else
#   include <m3/Log.h>
#   define THROW(...)                   PANIC("Exception raised at " << __FILE__ << ":" << __LINE__)
#   define THROW1(ex, err, exp, line)   PANIC("Exception raised for " << line \
                                            << " at " << __FILE__ << ":" << __LINE__ \
                                            << "; got " << err << ", expected " << exp)
#endif

/*
 * *************************************************************************
 */

class Exception {

  public:
    virtual ~Exception() { };

    virtual void complain() = 0;
};


class LogableException: public Exception {

  public:
    virtual void complain() {

        Platform::log(text);
    }

    virtual std::string &msg() {

        return text;
    };

  protected:
    std::string text;
};


class IllegalArgumentException: public LogableException {

  public:
    IllegalArgumentException() {

        text = "Illegal argument";
    };

    IllegalArgumentException(const std::string &txt) {

        text = txt;
    };
};


class OutOfMemoryException: public LogableException {

  public:
    OutOfMemoryException() {

        text = "Out of memory";
    };
};


class ReturnValueException: public LogableException {

  public:
    ReturnValueException(int got, int expected, int lineNo = -1) {

        std::stringstream s;
        s << "Unexpected return value " << got << " instead of " << expected;
        if (lineNo >= 0)
            s << " in line #" << lineNo;
        text = s.str();
    };
};


class ParseException: public LogableException {

  public:
    ParseException(const std::string &line, int lineNo = -1, int colNo = -1) {

        std::stringstream s;
        s << "Parse error";
        if (lineNo >= 0)
            s << " in line " << lineNo;
        if (colNo >= 0)
            s << " at col " << colNo;
        s << ": " << line;
        text = s.str();
    }
};


class IoException: public LogableException {

  public:
    IoException(const std::string &msg, const std::string &name = "", int errorNo = 0) {

        std::stringstream s;
        s << "I/O error ";
        if (errorNo != 0)
           s << errorNo;
        if ( !name.empty())
            s << " for file " << name << ": " << msg;
        text = s.str();
    }
};

#endif /* __TRACE_BENCH_EXCEPTIONS_H */

