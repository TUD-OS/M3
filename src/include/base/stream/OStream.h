/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#pragma once

#include <base/Common.h>
#include <base/stream/IOSBase.h>
#include <stdarg.h>

namespace m3 {

/**
 * The output-stream is used to write formatted output to various destinations. Subclasses have
 * to implement the method to actually write a character. This class provides the higher-level
 * stuff around it.
 *
 * Note that there is no printf-like formatting, but all values are put via shift operators into
 * the stream. Because printf() is not type-safe, which means you can easily make mistakes and
 * won't notice it. I know that the compiler offers a function attribute to mark printf-like
 * functions so that he can warn us about wrong usages. The problem is that it gets really
 * cumbersome to not produce warnings (so that you actually notice a new warning) when building
 * for multiple architectures. One way to get it right is embedding the length (L, l, ...) into
 * the string via preprocessor defines that depend on the architecture (think of uint64_t). Another
 * way is to add more length-specifier for uintX_t's and similar types. But it gets worse: typedefs.
 * To pass values with typedef-types to printf() in a correct way you would have to look at the
 * "implementation" of the typedef, i.e. the type it is mapped to. This makes it really hard to
 * change typedefs afterwards. So, to avoid that you would have to introduce a length modifier for
 * each typedef in your code. Not only that you'll run out of ASCII chars, this is simply not
 * acceptable. In summary, there is no reasonable way to specify the correct type for printf().
 *
 * Therefore we go a different way here. We only use shift operators and provide a quite
 * concise and simple way to add formatting parameters (in contrast to the really verbose concept
 * of the standard C++ iostreams). This way we have type-safety (you can't accidently output a
 * string as an integer or the other way around) and it's still convenient enough to pass formatting
 * parameters. This is done via template specialization. We provide a freestanding function named
 * fmt() to make use of template parameter type inference which receives all necessary information,
 * wraps them into an object and writes this object into the stream. The method for that will create
 * the corresponding template class, depending on the type to print, which in turn will finally
 * print the value.
 * The whole formatting stuff is done by OStream::FormatParams. It receives a string with formatting
 * arguments (same syntax as printf). It does not support padding and precision and recognizes only
 * the base instead of the type (i.e. only x, o, and so on and not s, c, ...). The padding and
 * precision are always passed in a separate parameter to allow "dynamic" values and only have one
 * place to specify them.
 * There is still one problem: the difference between signed and unsigned values tends to be ignored
 * by programmers. That is, I guess most people will assume that fmt(0x1234, "x") prints it with
 * a hexadecimal base. Strictly speaking, this is wrong, because 0x1234 is an int, not unsigned int,
 * and only unsigned integers are printed in a base different than 10. Thus, one would have to use
 * fmt(0x1234U, "x") to achieve it. This is even worse for typedefs or when not passing a literal,
 * because you might not even know whether its signed or unsigned. To prevent that problem I've
 * decided to interpret some formatting parameters as hints. Since the base will only be considered
 * for unsigned values, fmt(0x1234, "x") will print 0x1234 as an unsigned integer in base 16.
 * Similarly, when passing "+" or " " it will be printed as signed, even when its unsigned. And
 * finally, "p" forces a print as a pointer (xxxx:xxxx) even when its some unsigned type or char*.
 *
 * The basic syntax is: [flags][type]
 * Where [flags] is any combination of:
 * - '-': add padding on the right side instead of on the left
 * - '+': always print the sign (+ or -); forces a signed print
 * - ' ': print a space in front of positive values; forces a signed print
 * - '#': print the base
 * - '0': use zeros for padding instead of spaces
 *
 * [type] can be:
 * - 'p':           a pointer; forces a pointer print
 * - 'b':           an unsigned integer, printed in base 2; forces an unsigned print
 * - 'o':           an unsigned integer, printed in base 8; forces an unsigned print
 * - 'x':           an unsigned integer, printed in base 16; forces an unsigned print
 * - 'X':           an unsigned integer, printed in base 16 with capital letters; forces an
 *                  unsigned print
 */
class OStream : public virtual IOSBase {
    /**
     * Collects all formatting parameters from a string
     */
    class FormatParams {
    public:
        enum Flags {
            PADRIGHT    = 1 << 0,
            FORCESIGN   = 1 << 1,
            SPACESIGN   = 1 << 2,
            PRINTBASE   = 1 << 3,
            PADZEROS    = 1 << 4,
            CAPHEX      = 1 << 5,
            POINTER     = 1 << 6,
        };

        explicit FormatParams(const char *fmt);

        uint base() const {
            return _base;
        }
        int flags() const {
            return _flags;
        }
        size_t padding() const {
            return _pad;
        }
        void padding(size_t pad) {
            _pad = pad;
        }
        size_t precision() const {
            return _prec;
        }
        void precision(size_t prec) {
            _prec = prec;
        }

    private:
        uint _base;
        int _flags;
        size_t _pad;
        size_t _prec;
    };

    /**
     * We use template specialization to do different formatting operations depending on the
     * type given to fmt().
     */
    template<typename T>
    class FormatImpl {
    };
    template<typename T>
    class FormatImplPtr {
    public:
        void write(OStream &os, const FormatParams &p, T value) {
            os.printptr(reinterpret_cast<uintptr_t>(value), p.flags());
        }
    };
    template<typename T>
    class FormatImplUint {
    public:
        void write(OStream &os, const FormatParams &p, const T &value) {
            // let the user print an integer as a pointer if a wants to. this saves a cast to void*
            if(p.flags() & FormatParams::POINTER)
                os.printptr(static_cast<uintptr_t>(value), p.flags());
            // although we rely on the type in most cases, we let the user select between signed
            // and unsigned by specifying certain flags that are only used at one place.
            // this free's the user from having to be really careful whether a value is signed or
            // unsigned, which is especially a problem when using typedefs.
            else if(p.flags() & (FormatParams::FORCESIGN | FormatParams::SPACESIGN))
                os.printnpad(static_cast<llong>(value), p.padding(), p.flags());
            else
                os.printupad(static_cast<ullong>(value), p.base(), p.padding(), p.flags());
        }
    };
    template<typename T>
    class FormatImplInt {
    public:
        void write(OStream &os, const FormatParams &p, const T &value) {
            // like above; the base is only used in unsigned print, so do that if the user specified
            // a base (10 is default)
            if(p.base() != 10)
                os.printupad(static_cast<ullong>(value), p.base(), p.padding(), p.flags());
            else
                os.printnpad(static_cast<llong>(value), p.padding(), p.flags());
        }
    };
    template<typename T>
    class FormatImplFloat {
    public:
        void write(OStream &os, const FormatParams &p, const T &value) {
            os.printfloat(value, p.precision());
        }
    };
    template<typename T>
    class FormatImplStr {
    public:
        void write(OStream &os, const FormatParams &p, const T &value) {
            if(p.flags() & FormatParams::POINTER)
                os.printptr(reinterpret_cast<uintptr_t>(value), p.flags());
            else
                os.putspad(value, p.padding(), p.precision(), p.flags());
        }
    };

public:
    /**
     * This class can be written into an OStream to apply formatting while using the stream operators
     * It will be used by the freestanding fmt() function, which makes it shorter because of template
     * parameter type inference.
     */
    template<typename T>
    class Format {
    public:
        explicit Format(const char *fmt, const T &value, uint pad, uint prec)
            : _fmt(fmt),
              _value(value),
              _pad(pad),
              _prec(prec) {
        }

        const char *fmt() const {
            return _fmt;
        }
        const T &value() const {
            return _value;
        }
        uint padding() const {
            return _pad;
        }
        uint precision() const {
            return _prec;
        }

    private:
        const char *_fmt;
        const T &_value;
        uint _pad;
        uint _prec;
    };

    explicit OStream() : IOSBase() {
    }
    virtual ~OStream() {
    }

    OStream(const OStream&) = delete;
    OStream &operator=(const OStream&) = delete;

    /**
     * Writes a value into the stream with formatting applied. This operator should be used in
     * combination with fmt():
     * Serial::get() << fmt(0x123, "x") << "\n";
     */
    template<typename T>
    OStream & operator<<(const Format<T>& fmt) {
        FormatParams p(fmt.fmt());
        p.padding(fmt.padding());
        p.precision(fmt.precision());
        FormatImpl<T>().write(*this, p, fmt.value());
        return *this;
    }

    /**
     * Writes the given character/integer into the stream, without formatting
     */
    OStream & operator<<(char c) {
        write(c);
        return *this;
    }
    OStream & operator<<(uchar u) {
        return operator<<(static_cast<ullong>(u));
    }
    OStream & operator<<(short n) {
        return operator<<(static_cast<llong>(n));
    }
    OStream & operator<<(ushort u) {
        return operator<<(static_cast<ullong>(u));
    }
    OStream & operator<<(int n) {
        return operator<<(static_cast<llong>(n));
    }
    OStream & operator<<(uint u) {
        return operator<<(static_cast<ullong>(u));
    }
    OStream & operator<<(long n) {
        printn(n);
        return *this;
    }
    OStream & operator<<(llong n) {
        printn(n);
        return *this;
    }
    OStream & operator<<(ulong u) {
        printu(u, 10, _hexchars_small);
        return *this;
    }
    OStream & operator<<(ullong u) {
        printu(u,10, _hexchars_small);
        return *this;
    }
    OStream & operator<<(float f) {
        printfloat(f, 3);
        return *this;
    }

    /**
     * Writes the given string into the stream
     */
    OStream & operator<<(const char *str) {
        puts(str);
        return *this;
    }
    /**
     * Writes the given pointer into the stream (xxxx:xxxx)
     */
    OStream & operator<<(const void *p) {
        printptr(reinterpret_cast<uintptr_t>(p), 0);
        return *this;
    }

    /**
     * Produces a hexdump of the given data.
     *
     * @param data the data
     * @param size the number of bytes
     */
    void dump(const void *data, size_t size);

    /**
     * Writes the given character into the stream.
     *
     * @param c the character
     */
    virtual void write(char c) = 0;

private:
    size_t printsignedprefix(llong n, int flags);
    size_t putspad(const char *s, size_t pad, size_t prec, int flags);
    size_t printnpad(llong n, size_t pad, int flags);
    size_t printupad(ullong u, uint base, size_t pad, int flags);
    size_t printpad(size_t count, int flags);
    size_t printu(ullong n, uint base, char *chars);
    size_t printn(llong n);
    size_t printfloat(float d, size_t precision);
    size_t printptr(uintptr_t u, int flags);
    size_t puts(const char *str, size_t prec = ~0UL);

    static char _hexchars_big[];
    static char _hexchars_small[];
};

template<>
class OStream::FormatImpl<void*> : public OStream::FormatImplPtr<const void*> {
};
template<>
class OStream::FormatImpl<const void*> : public OStream::FormatImplPtr<const void*> {
};
template<>
class OStream::FormatImpl<uchar> : public OStream::FormatImplUint<uchar> {
};
template<>
class OStream::FormatImpl<ushort> : public OStream::FormatImplUint<ushort> {
};
template<>
class OStream::FormatImpl<uint> : public OStream::FormatImplUint<uint> {
};
template<>
class OStream::FormatImpl<ulong> : public OStream::FormatImplUint<ulong> {
};
template<>
class OStream::FormatImpl<ullong> : public OStream::FormatImplUint<ullong> {
};
template<>
class OStream::FormatImpl<char> : public OStream::FormatImplInt<char> {
};
template<>
class OStream::FormatImpl<short> : public OStream::FormatImplInt<short> {
};
template<>
class OStream::FormatImpl<int> : public OStream::FormatImplInt<int> {
};
template<>
class OStream::FormatImpl<long> : public OStream::FormatImplInt<long> {
};
template<>
class OStream::FormatImpl<llong> : public OStream::FormatImplInt<llong> {
};
template<>
class OStream::FormatImpl<float> : public OStream::FormatImplFloat<float> {
};
template<>
class OStream::FormatImpl<const char*> : public OStream::FormatImplStr<const char*> {
};
template<>
class OStream::FormatImpl<char*> : public OStream::FormatImplStr<char*> {
};
// this is necessary to be able to pass a string literal to fmt()
template<int X>
class OStream::FormatImpl<char [X]> : public OStream::FormatImplStr<char [X]> {
};

// unfortunatly, we have to add special templates for volatile :(
template<>
class OStream::FormatImpl<volatile void*> : public OStream::FormatImplPtr<volatile void*> {
};
template<>
class OStream::FormatImpl<volatile uchar> : public OStream::FormatImplUint<volatile uchar> {
};
template<>
class OStream::FormatImpl<volatile ushort> : public OStream::FormatImplUint<volatile ushort> {
};
template<>
class OStream::FormatImpl<volatile uint> : public OStream::FormatImplUint<volatile uint> {
};
template<>
class OStream::FormatImpl<volatile ulong> : public OStream::FormatImplUint<volatile ulong> {
};
template<>
class OStream::FormatImpl<volatile char> : public OStream::FormatImplInt<volatile char> {
};
template<>
class OStream::FormatImpl<volatile short> : public OStream::FormatImplInt<volatile short> {
};
template<>
class OStream::FormatImpl<volatile int> : public OStream::FormatImplInt<volatile int> {
};
template<>
class OStream::FormatImpl<volatile long> : public OStream::FormatImplInt<volatile long> {
};
template<>
class OStream::FormatImpl<volatile float> : public OStream::FormatImplFloat<volatile float> {
};
template<>
class OStream::FormatImpl<volatile const char*> : public OStream::FormatImplStr<volatile const char*> {
};
template<>
class OStream::FormatImpl<volatile char*> : public OStream::FormatImplStr<volatile char*> {
};
// this is necessary to be able to pass a string literal to fmt()
template<int X>
class OStream::FormatImpl<volatile char [X]> : public OStream::FormatImplStr<volatile char [X]> {
};

/**
 * Creates a Format-object that can be written into OStream to write the given value into the
 * stream with specified formatting parameters. This function exists to allow template parameter
 * type inference.
 *
 * Example usage:
 * Serial::get() << "Hello " << fmt(0x1234, "#0x", 8) << "\n";
 *
 * @param value the value to format
 * @param fmt the format parameters
 * @param pad the number of padding characters (default 0)
 * @param precision the precision (default -1 = none)
 * @return the Format object
 */
template<typename T>
static inline OStream::Format<T> fmt(const T &value, const char *fmt, size_t pad = 0, size_t prec = ~0UL) {
    return OStream::Format<T>(fmt, value, pad, prec);
}
template<typename T>
static inline OStream::Format<T> fmt(const T &value, size_t pad = 0, size_t prec = ~0UL) {
    return OStream::Format<T>("", value, pad, prec);
}

}
