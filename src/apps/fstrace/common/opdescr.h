// vim:ft=cpp
/*
 * (c) 2007-2013 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING-GPL-2 file for details.
 */

#ifndef __TRACE_BENCH_OP_DESCR_H
#define __TRACE_BENCH_OP_DESCR_H

#include <iostream>
#include <vector>
#include <string>

/*
 * *************************************************************************
 */

class OpDescr {

  public:
    typedef std::vector<std::string> ArgsVector;

    virtual ~OpDescr() { };

    /*
     * @brief Returns a line of C code that describes the operation to be
     *        performed.
     *
     * @params lineNo  Monotonically increasing number to identify a specific
     *                 operation (like a line number).
     */
    virtual std::string codeLine(unsigned int lineNo);

protected:
    /*
     * @brief Extract a number of substrings from another string that represents
     *        a system call logged by strace.
     *
     * @param str         String to extract the arguments and return value from.
     * @param numArgs     Number arguments.
     * @param args        This vector will be filled with strings containing the
     *                    extracted arguments.
     * @param retVal      Extracted return value.
     */
    virtual void extractValues(const std::string &str, size_t numArgs,
                               ArgsVector &args, std::string &retVal) const;

    virtual size_t argLen(std::string const &str, size_t pos, size_t &newPos, bool &foundEnd) const;
    virtual void validateString(const std::string &str, size_t numArgs) const;
    virtual void buildCodeLine(std::string const &opcode, std::string const &argsName,
                               std::string const &args);
    virtual std::string codeLine();

    /* internal state is kept as a string of C code */
    std::string codeStr;
};


class FoldableOpDescr: public OpDescr {

  public:
    FoldableOpDescr(): repeatCount(1) { };

    /*
     * @brief Merge another FoldableOpDescr if the operations matches described
     *        by both of them match exactly.
     */
    virtual bool merge(const FoldableOpDescr &fod);

  protected:
    virtual std::string codeLine();

    unsigned int repeatCount;
};

/*
 * *************************************************************************
 */

class OpDescrFactory {

  public:
    /*
     * @brief Create a descendent of OpDescr representing the I/O operation
     *        that is described by the a line of strace log output.
     */
    static OpDescr *create(const std::string &line);

    /*
     * @brief Extract the function name of the syscall.
     */
    static std::string sysCallName(const std::string &line);

  protected:
    static bool isStringHead(const std::string &str, const std::string &head);
};

#endif // __TRACE_BENCH_OP_DESCR_H
