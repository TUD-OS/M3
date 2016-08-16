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

#include <base/stream/OStream.h>
#include <base/util/Digits.h>
#include <base/util/Math.h>
#include <c/div.h>
#include <cstring>

namespace m3 {

USED char OStream::_hexchars_big[]     = "0123456789ABCDEF";
USED char OStream::_hexchars_small[]   = "0123456789abcdef";

OStream::FormatParams::FormatParams(const char *fmt)
        : _base(10), _flags(0), _pad(0), _prec(-1) {
    // read flags
    bool read_flags = true;
    while(read_flags) {
        switch(*fmt) {
            case '-':
                _flags |= PADRIGHT;
                fmt++;
                break;
            case '+':
                _flags |= FORCESIGN;
                fmt++;
                break;
            case ' ':
                _flags |= SPACESIGN;
                fmt++;
                break;
            case '#':
                _flags |= PRINTBASE;
                fmt++;
                break;
            case '0':
                _flags |= PADZEROS;
                fmt++;
                break;
            default:
                read_flags = false;
                break;
        }
    }

    // read base
    switch(*fmt) {
        case 'X':
        case 'x':
            if(*fmt == 'X')
                _flags |= CAPHEX;
            _base = 16;
            break;
        case 'o':
            _base = 8;
            break;
        case 'b':
            _base = 2;
            break;
        case 'p':
            _flags |= POINTER;
            break;
    }
}

int OStream::printsignedprefix(long n, uint flags) {
    int count = 0;
    if(n > 0) {
        if(flags & FormatParams::FORCESIGN) {
            write('+');
            count++;
        }
        else if(flags & FormatParams::SPACESIGN) {
            write(' ');
            count++;
        }
    }
    return count;
}

int OStream::putspad(const char *s, uint pad, uint prec, uint flags) {
    int count = 0;
    if(pad > 0 && !(flags & FormatParams::PADRIGHT)) {
        ulong width = prec != static_cast<uint>(-1) ? Math::min<size_t>(prec, strlen(s)) : strlen(s);
        count += printpad(pad - width, flags);
    }
    count += puts(s, prec);
    if(pad > 0 && (flags & FormatParams::PADRIGHT))
        count += printpad(pad - count, flags);
    return count;
}

int OStream::printnpad(long n, uint pad, uint flags) {
    int count = 0;
    // pad left
    if(!(flags & FormatParams::PADRIGHT) && pad > 0) {
        size_t width = Digits::count_signed(n, 10);
        if(n > 0 && (flags & (FormatParams::FORCESIGN | FormatParams::SPACESIGN)))
            width++;
        count += printpad(pad - width, flags);
    }
    count += printsignedprefix(n, flags);
    count += printn(n);
    // pad right
    if((flags & FormatParams::PADRIGHT) && pad > 0)
        count += printpad(pad - count, flags);
    return count;
}

int OStream::printupad(ulong u, uint base, uint pad, uint flags) {
    int count = 0;
    // pad left - spaces
    if(!(flags & FormatParams::PADRIGHT) && !(flags & FormatParams::PADZEROS) && pad > 0) {
        size_t width = Digits::count_unsigned(u, base);
        count += printpad(pad - width, flags);
    }
    // print base-prefix
    if((flags & FormatParams::PRINTBASE)) {
        if(base == 16 || base == 8) {
            write('0');
            count++;
        }
        if(base == 16) {
            char c = (flags & FormatParams::CAPHEX) ? 'X' : 'x';
            write(c);
            count++;
        }
    }
    // pad left - zeros
    if(!(flags & FormatParams::PADRIGHT) && (flags & FormatParams::PADZEROS) && pad > 0) {
        size_t width = Digits::count_unsigned(u, base);
        count += printpad(pad - width, flags);
    }
    // print number
    if(flags & FormatParams::CAPHEX)
        count += printu(u, base, _hexchars_big);
    else
        count += printu(u, base, _hexchars_small);
    // pad right
    if((flags & FormatParams::PADRIGHT) && pad > 0)
        count += printpad(pad - count, flags);
    return count;
}

int OStream::printpad(int count, uint flags) {
    int res = count;
    char c = flags & FormatParams::PADZEROS ? '0' : ' ';
    while(count-- > 0)
        write(c);
    return res;
}

USED int OStream::printu(ulong n, uint base, char *chars) {
    long rem;
    long quot = divide(n, base, &rem);
    int res = 0;
    if(n >= base)
        res += printu(quot, base, chars);
    write(chars[rem]);
    return res + 1;
}

int OStream::printllu(ullong n, uint base, char *chars) {
    llong rem;
    llong quot = divide(n, base, &rem);
    int res = 0;
    if(n >= base)
        res += printllu(quot, base, chars);
    write(chars[rem]);
    return res + 1;
}

int OStream::printlln(llong n) {
    llong rem;
    llong quot = divide(n, 10, &rem);
    int res = 0;
    if(n >= 10)
        res += printlln(quot);
    write(_hexchars_big[rem]);
    return res + 1;
}

USED int OStream::printn(long n) {
    int res = 0;
    if(n < 0) {
        write('-');
        n = -n;
        res++;
    }

    long rem;
    long quot = divide(n, 10, &rem);
    if(n >= 10)
        res += printn(quot);
    write('0' + rem);
    return res + 1;
}

int OStream::printfloat(float d, uint precision) {
    int c = 0;
    if(d < 0) {
        d = -d;
        write('-');
        c++;
    }

    if(Math::is_nan(d))
        c += puts("nan", -1);
    else if(Math::is_inf(d))
        c += puts("inf", -1);
    else {
        llong val = (llong)d;
        c += printlln(val);
        d -= val;
        write('.');
        c++;
        while(precision-- > 0) {
            d *= 10;
            val = (long)d;
            write((val % 10) + '0');
            d -= val;
            c++;
        }
    }
    return c;
}

int OStream::printptr(uintptr_t u, uint flags) {
    int count = 0;
    size_t size = sizeof(uintptr_t);
    flags |= FormatParams::PADZEROS;
    // 2 hex-digits per byte and a ':' every 2 bytes
    while(size > 0) {
        count += printupad((u >> (size * 8 - 16)) & 0xFFFF, 16, 4, flags);
        size -= 2;
        if(size > 0) {
            write(':');
            // don't print the base again
            flags &= ~FormatParams::PRINTBASE;
            count++;
        }
    }
    return count;
}

USED int OStream::puts(const char *str, ulong prec) {
    const char *begin = str;
    char c;
    while((prec == static_cast<ulong>(-1) || prec-- > 0) && (c = *str)) {
        write(c);
        str++;
    }
    return str - begin;
}

void OStream::dump(const void *data, size_t size) {
    const uint8_t *bytes = reinterpret_cast<const uint8_t*>(data);
    for(size_t i = 0; i < size; ++i) {
        if((i % 16) == 0) {
            if(i > 0)
                write('\n');
            printupad(i, 16, 4, FormatParams::PADZEROS);
            write(':');
            write(' ');
        }
        printupad(bytes[i], 16, 2, FormatParams::PADZEROS);
        if(i + 1 < size)
            write(' ');
    }
    write('\n');
}

}
