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

#include <base/Common.h>
#include <base/stream/IStringStream.h>
#include <base/Env.h>

#include <m3/com/SendGate.h>
#include <m3/com/GateStream.h>
#include <m3/session/arch/host/Interrupts.h>
#include <m3/session/arch/host/Keyboard.h>
#include <m3/session/arch/host/VGA.h>
#include <m3/stream/FStream.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>

using namespace m3;

enum Status {
    PLAYING,
    PAUSED
};

static constexpr int SPEED = 4;
static constexpr int ROWS = 13;
static constexpr int COLS = 70;

static Status status = PLAYING;
static const char *statustext[] = {"Playing", "Paused "};
static const char *moviefile;
static FStream *movie;
static VGA vga("vga");
static int frame = 0;
static int ticks = 0;
static int next_tick = 0;
static char linebuf[80];
static char vgabuf[VGA::SIZE];
static int startrow = VGA::ROWS / 2 - ROWS / 2;
static int startcol = VGA::COLS / 2 - COLS / 2;

static void copy_to_vga(int row, int col, const char *str, size_t len, size_t total) {
    char *start = vgabuf + row * VGA::COLS * 2 + col * 2;
    for(size_t i = 0; i < total; ++i) {
        start[i * 2] = i < len ? str[i] : ' ';
        start[i * 2 + 1] = 0x07;
    }
}

static void timer_event(GateIStream &) {
    static Status laststatus = PLAYING;
    if(status != laststatus || ticks == next_tick) {
        if(status == PLAYING) {
            movie->getline(linebuf, sizeof(linebuf));
            next_tick = ticks + IStringStream::read_from<int>(linebuf) * SPEED;
            for(int i = 0; i < ROWS; ++i) {
                size_t res = movie->getline(linebuf, sizeof(linebuf));
                if(movie->error())
                    errmsg("Unable to read from movie file");
                else
                    copy_to_vga(startrow + i, startcol, linebuf, res, COLS);
            }
            frame++;
        }

        size_t len = strlen(moviefile);
        copy_to_vga(0, VGA::COLS / 2 - static_cast<int>(len) / 2, moviefile, len, len);

        len = strlen(statustext[status]);
        copy_to_vga(VGA::ROWS - 1, 0, statustext[status], len, len);

        snprintf(linebuf, sizeof(linebuf), "Frame %d", frame);
        len = strlen(linebuf);
        copy_to_vga(VGA::ROWS - 1, VGA::COLS - static_cast<int>(len), linebuf, len, len);

        vga.gate().write(vgabuf, sizeof(vgabuf), 0);
        laststatus = status;
    }
    if(status != PAUSED)
        ticks++;
}

static void kb_event(GateIStream &is) {
    Keyboard::Event ev;
    is >> ev;
    if(!ev.isbreak)
        return;
    switch(ev.keycode) {
        case Keyboard::VK_SPACE:
        case Keyboard::VK_P:
            status = status == PLAYING ? PAUSED : PLAYING;
            break;
    }
}

int main(int argc, char **argv) {
    if(argc < 2)
        exitmsg("Usage: " << argv[0] << " <movie-file>");

    moviefile = argv[1];

    if(VFS::mount("/", "m3fs") != Errors::NONE)
        exitmsg("Mounting root-fs failed");

    movie = new FStream(moviefile, FILE_R);
    if(!*movie)
        exitmsg("Opening " << moviefile << " failed");

    Interrupts timerirqs("interrupts", HWInterrupts::TIMER);
    timerirqs.rgate().start(timer_event);

    Keyboard kb("keyb");
    kb.rgate().start(kb_event);

    env()->workloop()->run();
    return 0;
}
