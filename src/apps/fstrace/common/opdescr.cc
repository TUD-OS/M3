// vim:ft=cpp
/*
 * (c) 2007-2013 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>
//#include <sys/types.h>
#include <regex.h>

#include "opdescr.h"
#include "exceptions.h"

using namespace std;

/*
 * *************************************************************************
 */

std::string OpDescr::codeLine(unsigned int lineNo) {

    stringstream stream;
    string str;

    stream << "    /* #" << lineNo << " = 0x" << std::hex << lineNo << std::dec << " */ " << codeLine();

    return stream.str();
}


void OpDescr::extractValues(const string &str, size_t numArgs,
                            ArgsVector &args, string &retVal) const {

    // validateString() checks, whether the string has a format that does
    // not lead to unexpected behaviour of the code below. unfortunately,
    // it is slow ...
    // if this function fails, it will throw a ParseException.
    //validateString(str, numArgs);

    // populate out parameter with all the extracted substrings
    args.resize(0);
    args.reserve(numArgs);

    size_t pos, newPos, len;

    // extract arguments
    pos = str.find('(') + 1;
    for (unsigned int i = 0; i < numArgs; i++) {

        len = argLen(str, pos, newPos);
        if (len == string::npos)
            break;

        args.push_back(str.substr(pos, len));
        pos = newPos;
    }

    retVal = str.substr(str.rfind('=') + 2);
    int iretVal = atoi(retVal.c_str());
    ostringstream os;
    os << iretVal;
    retVal = os.str();
}


size_t OpDescr::argLen(string const &str, size_t pos, size_t &newPos) const {

    size_t len = str.size();
    size_t i;
    bool   isEscaped = false;

    // if argument sub string is quoted, we look for the a closing single/double quote;
    // both must not be preceded by a backslash

    if (str[pos] == '"' || str[pos] == '\'') {

        char quote = str[pos];

        for (i = pos + 1; i < len; i++)  {

            if (str[i] == quote && !isEscaped)
                break;
            isEscaped = (str[i] == '\\' && !isEscaped) ? true : false;
        }

        if (i >= len)
           return string::npos;

        // skip to the character after th closing quote
        i++;

        // skip '...' after abbreviated quoted argument, if necessary
        newPos = (str.find("...", i) == i) ? i + 5 : i + 2;
        return i - pos;
    }

    // look for a comma or a closing bracket
    i = str.find(',', pos);
    if (i == string::npos)
        i = str.find(')', pos);

    // found a comma or bracket?
    if (i != string::npos) {
       newPos = i + 2;
       return i - pos;
    }

    // whoops!
    return string::npos;
}


void OpDescr::validateString(const string &str, size_t numArgs) const {

    regex_t re;
    bool    reCompiled = false;
    int     ret        = 0;

    try {
        unsigned int i;
        string pattern;

        // construct validation pattern for regex
        pattern.append("^.*(");
        pattern.append(".*");
        for (i = 1; i < numArgs; i++)
            pattern.append(", .*");
        pattern.append(") *= [0123456789]*$");

        // compile regex
        ret = regcomp(&re, pattern.c_str(), REG_NOSUB);
        if (ret != 0)
            throw ParseException("Regex compilation failed");
        reCompiled = true;

        // do pattern matching
        ret = regexec(&re, str.c_str(), 0, NULL, 0);
        if (ret != 0)
            throw ParseException("Regex evaluation failed");

        regfree(&re);
    }

    catch (ParseException &e) {
        char error[1024];

        if (reCompiled)
            regfree(&re);

        regerror(ret, &re, error, sizeof(error));
        throw ParseException(e.msg() + ": " + error + ": " + str);
    }
}


void OpDescr::buildCodeLine(string const &opcode, string const &argsName,
                              string const &args) {

    codeStr  = "{ .opcode = " + opcode;
    codeStr += ", .args." + argsName + " = { " + args + " } },";
}


string OpDescr::codeLine() {

    return codeStr;
}

/*
 * *************************************************************************
 */

string FoldableOpDescr::codeLine() {

    stringstream s;
    string repeatArg;

    s << repeatCount;
    s >> repeatArg;

    string dummy = "@COUNT@";
    string finalizedCodeStr = codeStr;
    size_t dummy_pos  = finalizedCodeStr.find(dummy);
    size_t dummy_size = dummy.size();

    return finalizedCodeStr.replace(dummy_pos, dummy_size, repeatArg);
}


bool FoldableOpDescr::merge(const FoldableOpDescr &fod) {

    if (fod.codeStr != codeStr)
        return false;

    repeatCount++;
    return true;
}

/*
 * *************************************************************************
 */

class WaitUntilOpDescr: public FoldableOpDescr {
public:
    WaitUntilOpDescr(const std::string &str);
    string codeLine() {
        // revert back to basic OpDescr, we do not need the pattern magic
        // that is implemented in the direct base class 'FoldableOpDescr'
        return OpDescr::codeLine();
    }
    bool merge(const FoldableOpDescr &fod) {
        // we can merge, if the previous op is also 'WAITUNTIL_OP'
        WaitUntilOpDescr const *wuod = dynamic_cast<WaitUntilOpDescr const *>(&fod);
        if (wuod == 0)
            return false;
        // our own codestring includes an outdated timestamp, copy new one
        // from the descriptor we merge into 'this'
        codeStr = wuod->codeStr;
        return true;
    }
};


class OpenOpDescr: public OpDescr {
public:
    OpenOpDescr(const std::string &str);
};


class CloseOpDescr: public OpDescr {
public:
    CloseOpDescr(const std::string &str);
};


class FsyncOpDescr: public OpDescr {
public:
    FsyncOpDescr(const std::string &str);
};


class ReadOpDescr: public FoldableOpDescr {
public:
    ReadOpDescr(const std::string &str);
};


class WriteOpDescr: public FoldableOpDescr {
public:
    WriteOpDescr(const std::string &str);
};


class PReadOpDescr: public OpDescr {
public:
    PReadOpDescr(const std::string &str);
};


class PWriteOpDescr: public OpDescr {
public:
    PWriteOpDescr(const std::string &str);
};


class LSeekOpDescr: public OpDescr {
public:
    LSeekOpDescr(const std::string &str);
};


class _LlSeekOpDescr: public OpDescr {
public:
    _LlSeekOpDescr(const std::string &str);
};


class FTruncateOpDescr: public OpDescr {
public:
    FTruncateOpDescr(const std::string &str);
};


class FStatOpDescr: public OpDescr {
public:
    FStatOpDescr(const std::string &str);
};


class StatOpDescr: public OpDescr {
public:
    StatOpDescr(const std::string &str);
};


class RenameOpDescr: public OpDescr {
public:
    RenameOpDescr(const std::string &str);
};


class UnlinkOpDescr: public OpDescr {
public:
    UnlinkOpDescr(const std::string &str);
};


class RmdirOpDescr: public OpDescr {
public:
    RmdirOpDescr(const std::string &str);
};


class MkdirOpDescr: public OpDescr {
public:
    MkdirOpDescr(const std::string &str);
};


class SendfileOpDescr: public OpDescr {
public:
    SendfileOpDescr(const std::string &str);
};


class GetDEntsOpDescr: public OpDescr {
public:
    GetDEntsOpDescr(const std::string &str);
};


class CreateFileOpDescr: public OpDescr {
public:
    CreateFileOpDescr(const std::string &str);
};

/*
 * *************************************************************************
 */

WaitUntilOpDescr::WaitUntilOpDescr(const string &in) {

    string retVal;
    ArgsVector args(1);

    extractValues(in, 1, args, retVal);
    buildCodeLine("WAITUNTIL_OP", "waituntil", retVal + ", " + args[0]);
}

/*
 * *************************************************************************
 */

OpenOpDescr::OpenOpDescr(const string &in) {

    string retVal;
    ArgsVector args(3);

    extractValues(in, 3, args, retVal);
    buildCodeLine("OPEN_OP", "open",
                  retVal + ", " + args[0] + ", " + args[1] +
                  ", " + ((args.size() > 2) ? args[2] : "0"));
}

/*
 * *************************************************************************
 */

CloseOpDescr::CloseOpDescr(const string &in) {

    string retVal;
    ArgsVector args(1);

    extractValues(in, 1, args, retVal);
    buildCodeLine("CLOSE_OP", "close", retVal + ", " + args[0]);
}

/*
 * *************************************************************************
 */

FsyncOpDescr::FsyncOpDescr(const string &in) {

    string retVal;
    ArgsVector args(1);

    extractValues(in, 1, args, retVal);
    buildCodeLine("FSYNC_OP", "fsync", retVal + ", " + args[0]);
}

/*
 * *************************************************************************
 */

ReadOpDescr::ReadOpDescr(const string &in) {

    string retVal;
    ArgsVector args(3);

    extractValues(in, 3, args, retVal);
    string argStr = retVal + ", " + args[0] + ", " + args[2] + ", @COUNT@";
    buildCodeLine("READ_OP", "read", argStr);
}

/*
 * *************************************************************************
 */

WriteOpDescr::WriteOpDescr(const string &in) {

    string retVal;
    ArgsVector args(3);

    extractValues(in, 3, args, retVal);
    string argStr = retVal + ", " + args[0] + ", " + args[2] + ", @COUNT@";
    buildCodeLine("WRITE_OP", "write", argStr);
}

/*
 * *************************************************************************
 */

PReadOpDescr::PReadOpDescr(const string &in) {

    string retVal;
    ArgsVector args(4);

    extractValues(in, 4, args, retVal);
    string argStr = retVal + ", " + args[0] + ", " + args[2] + ", " + args[3];
    buildCodeLine("PREAD_OP", "pread", argStr);
}

/*
 * *************************************************************************
 */

PWriteOpDescr::PWriteOpDescr(const string &in) {

    string retVal;
    ArgsVector args(4);

    extractValues(in, 4, args, retVal);
    string argStr = retVal + ", " + args[0] + ", " + args[2] + ", " + args[3];
    buildCodeLine("PWRITE_OP", "pwrite", argStr);
}

/*
 * *************************************************************************
 */

LSeekOpDescr::LSeekOpDescr(const string &in) {

    string retVal;
    ArgsVector args(3);

    extractValues(in, 3, args, retVal);
    buildCodeLine("LSEEK_OP", "lseek",
                  retVal + ", " + args[0] + ", " + args[1] + ", " + args[2]);
}

/*
 * *************************************************************************
 */

_LlSeekOpDescr::_LlSeekOpDescr(const string &in) {

    string retVal;
    ArgsVector args(4);

    extractValues(in, 4, args, retVal);
    string new_pos = args[2].substr(1, args[2].size() - 2);
    buildCodeLine("LSEEK_OP", "lseek",
                  ((retVal == "0") ? new_pos : retVal) + ", " +
                  args[0] + ", " + args[1] + ", " + args[3]);
}

/*
 * *************************************************************************
 */

FTruncateOpDescr::FTruncateOpDescr(const string &in) {

    string retVal;
    ArgsVector args(2);

    extractValues(in, 2, args, retVal);
    buildCodeLine("FTRUNCATE_OP", "ftruncate", retVal + ", " + args[0] + ", " + args[1]);
}

/*
 * *************************************************************************
 */

FStatOpDescr::FStatOpDescr(const string &in) {

    string retVal;
    ArgsVector args(1);

    extractValues(in, 1, args, retVal);
    buildCodeLine("FSTAT_OP", "fstat", retVal + ", " + args[0]);
}

/*
 * *************************************************************************
 */

StatOpDescr::StatOpDescr(const string &in) {

    string retVal;
    ArgsVector args(1);

    extractValues(in, 1, args, retVal);
    buildCodeLine("STAT_OP", "stat", retVal + ", " + args[0]);
}

/*
 * *************************************************************************
 */

RenameOpDescr::RenameOpDescr(const string &in) {

    string retVal;
    ArgsVector args(2);

    extractValues(in, 2, args, retVal);
    buildCodeLine("RENAME_OP", "rename", retVal + ", " + args[0] + ", " + args[1]);
}

/*
 * *************************************************************************
 */

UnlinkOpDescr::UnlinkOpDescr(const string &in) {

    string retVal;
    ArgsVector args(1);

    extractValues(in, 1, args, retVal);
    buildCodeLine("UNLINK_OP", "unlink", retVal + ", " + args[0]);
}

/*
 * *************************************************************************
 */

RmdirOpDescr::RmdirOpDescr(const string &in) {

    string retVal;
    ArgsVector args(1);

    extractValues(in, 1, args, retVal);
    buildCodeLine("RMDIR_OP", "rmdir", retVal + ", " + args[0]);
}

/*
 * *************************************************************************
 */

MkdirOpDescr::MkdirOpDescr(const string &in) {

    string retVal;
    ArgsVector args(2);

    extractValues(in, 2, args, retVal);
    buildCodeLine("MKDIR_OP", "mkdir", retVal + ", " + args[0] + ", " + args[1]);
}

/*
 * *************************************************************************
 */

SendfileOpDescr::SendfileOpDescr(const string &in) {

    string retVal;
    ArgsVector args(4);

    extractValues(in, 4, args, retVal);
    buildCodeLine("SENDFILE_OP", "sendfile",
        retVal + ", " + args[0] + ", " + args[1] + ", " + args[2] + ", " + args[3]);
}

/*
 * *************************************************************************
 */

GetDEntsOpDescr::GetDEntsOpDescr(const string &in) {

    string retVal;
    ArgsVector args(3);

    extractValues(in, 3, args, retVal);
    buildCodeLine("GETDENTS_OP", "getdents",
        retVal + ", " + args[0] + ", " + args[1] + ", " + args[2]);
}

/*
 * *************************************************************************
 */

CreateFileOpDescr::CreateFileOpDescr(const string &in) {

    string retVal;
    ArgsVector args(3);

    extractValues(in, 3, args, retVal);
    buildCodeLine("CREATEFILE_OP", "createfile",
                  retVal + ", " + args[0] + ", " + args[1] + ", " + args[2]);
}

/*
 * *************************************************************************
 */

OpDescr *OpDescrFactory::create(const string &line) {

    if (OpDescrFactory::isStringHead(line, "_waituntil("))
        return new WaitUntilOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "open("))
        return new OpenOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "close("))
        return new CloseOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "fsync("))
        return new FsyncOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "read("))
        return new ReadOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "write("))
        return new WriteOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "pread("))
        return new PReadOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "pwrite("))
        return new PWriteOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "lseek("))
        return new LSeekOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "_llseek("))
        return new _LlSeekOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "ftruncate("))
        return new FTruncateOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "fstat64("))
        return new FStatOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "stat64("))
        return new StatOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "lstat64("))
        return new StatOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "rename("))
        return new RenameOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "unlink("))
        return new UnlinkOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "rmdir("))
        return new RmdirOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "mkdir("))
        return new MkdirOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "sendfile64("))
        return new SendfileOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "getdents64("))
        return new GetDEntsOpDescr(line);
    if (OpDescrFactory::isStringHead(line, "_createfile("))
        return new CreateFileOpDescr(line);

    return 0;
}


string OpDescrFactory::sysCallName(const string &line) {

    size_t nameLen = line.find('(');

    return (nameLen > 0) ? line.substr(0, nameLen) : "";
}


inline bool OpDescrFactory::isStringHead(const string &str, const string &head) {

    return str.find(head) == 0;
}

