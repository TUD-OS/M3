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
    : _base(10),
      _flags(0),
      _pad(0),
      _prec(~0UL) {
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

size_t OStream::printsignedprefix(llong n, int flags) {
    size_t count = 0;
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

size_t OStream::putspad(const char *s, size_t pad, size_t prec, int flags) {
    size_t count = 0;
    if(pad > 0 && !(flags & FormatParams::PADRIGHT)) {
        size_t width = prec != static_cast<size_t>(-1) ? Math::min<size_t>(prec, strlen(s)) : strlen(s);
        if(pad > width)
            count += printpad(pad - width, flags);
    }
    count += puts(s, prec);
    if((flags & FormatParams::PADRIGHT) && pad > count)
        count += printpad(pad - static_cast<size_t>(count), flags);
    return count;
}

size_t OStream::printnpad(llong n, size_t pad, int flags) {
    size_t count = 0;
    // pad left
    if(!(flags & FormatParams::PADRIGHT) && pad > 0) {
        size_t width = Digits::count_signed(n, 10);
        if(n > 0 && (flags & (FormatParams::FORCESIGN | FormatParams::SPACESIGN)))
            width++;
        if(pad > width)
            count += printpad(pad - width, flags);
    }
    count += printsignedprefix(n, flags);
    count += printn(n);
    // pad right
    if((flags & FormatParams::PADRIGHT) && pad > count)
        count += printpad(pad - static_cast<size_t>(count), flags);
    return count;
}

size_t OStream::printupad(ullong u, uint base, size_t pad, int flags) {
    size_t count = 0;
    // pad left - spaces
    if(!(flags & FormatParams::PADRIGHT) && !(flags & FormatParams::PADZEROS) && pad > 0) {
        size_t width = Digits::count_unsigned(u, base);
        if(pad > width)
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
        if(pad > width)
            count += printpad(pad - width, flags);
    }
    // print number
    if(flags & FormatParams::CAPHEX)
        count += printu(u, base, _hexchars_big);
    else
        count += printu(u, base, _hexchars_small);
    // pad right
    if((flags & FormatParams::PADRIGHT) && pad > count)
        count += printpad(pad - static_cast<size_t>(count), flags);
    return count;
}

size_t OStream::printpad(size_t count, int flags) {
    size_t res = count;
    char c = flags & FormatParams::PADZEROS ? '0' : ' ';
    while(count-- > 0)
        write(c);
    return res;
}

USED size_t OStream::printu(ullong n, uint base, char *chars) {
    ullong rem;
    ullong quot = divide(n, base, &rem);
    size_t res = 0;
    if(n >= base)
        res += printu(quot, base, chars);
    write(chars[rem]);
    return res + 1;
}

USED size_t OStream::printn(llong n) {
    size_t res = 0;
    if(n < 0) {
        write('-');
        n = -n;
        res++;
    }

    llong rem;
    llong quot = divide(n, 10, &rem);
    if(n >= 10)
        res += printn(quot);
    write('0' + rem);
    return res + 1;
}

size_t OStream::printfloat(float d, size_t precision) {
    size_t c = 0;
    if(d < 0) {
        d = -d;
        write('-');
        c++;
    }

    if(Math::is_nan(d))
        c += puts("nan");
    else if(Math::is_inf(d))
        c += puts("inf");
    else {
        llong val = static_cast<llong>(d);
        c += printn(val);
        d -= val;
        write('.');
        c++;
        while(precision-- > 0) {
            d *= 10;
            val = static_cast<long>(d);
            write((val % 10) + '0');
            d -= val;
            c++;
        }
    }
    return c;
}

size_t OStream::printptr(uintptr_t u, int flags) {
    size_t count = 0;
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

USED size_t OStream::puts(const char *str, size_t prec) {
    const char *begin = str;
    char c;
    while((prec == ~0UL || prec-- > 0) && (c = *str)) {
        write(c);
        str++;
    }
    return static_cast<size_t>(str - begin);
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
