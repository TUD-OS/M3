/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/stream/Serial.h>
#include <c/div.h>
#include <cstring>

namespace m3 {

const char *Serial::_colors[] = {
    "31", "32", "33", "34", "35", "36"
};
Serial Serial::_inst INIT_PRIORITY(101) USED;

void Serial::init(const char *path, int core) {
    size_t len = strlen(path);
    const char *name = path + len - 1;
    while(name > path && *name != '/')
        name--;
    if(name != path)
        name++;

    size_t i = 0;
    strcpy(_outbuf + i, "\e[0;");
    i += 4;
    long col;
    divide(core,ARRAY_SIZE(_colors),&col);
    strcpy(_outbuf + i, _colors[col]);
    i += 2;
    _outbuf[i++] = 'm';
    _outbuf[i++] = '[';
    size_t x = 0;
    for(; x < 8 && name[x]; ++x)
        _outbuf[i++] = name[x];
    for(; x < 8; ++x)
        _outbuf[i++] = ' ';
    _outbuf[i++] = '@';
    _outbuf[i++] = core <= 9 ? '0' + core : 'A' + (core - 10);
    _outbuf[i++] = ']';
    _outbuf[i++] = ' ';
    _start = _outpos = i;
}

void Serial::write(char c) {
    if(c == '\0')
        return;

    _outbuf[_outpos++] = c;
    if(_outpos >= OUTBUF_SIZE - SUFFIX_LEN || c == '\n')
        flush();
}

char Serial::read() {
    if(_inpos >= _inlen) {
        ssize_t res = do_read(_inbuf, INBUF_SIZE);
        if(res < 0)
            return 0;
        _inlen = res;
        _inpos = 0;
    }
    return _inbuf[_inpos++];
}

bool Serial::putback(char c) {
    if(_inpos == 0)
        return false;
    _inbuf[--_inpos] = c;
    return true;
}

}
