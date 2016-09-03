/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Copyright (C) 2009, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

#include <base/arch/host/HWInterrupts.h>
#include <base/util/Profile.h>

#include <m3/com/GateStream.h>
#include <m3/server/Server.h>
#include <m3/server/RequestHandler.h>
#include <m3/session/arch/host/Plasma.h>
#include <m3/session/arch/host/Interrupts.h>
#include <m3/session/arch/host/VGA.h>
#include <m3/session/Session.h>
#include <m3/Syscalls.h>

#include <math.h>

#define INTRO_TIME      200      // irqs
#define SIN_LUTSIZE     (1 << 8)
#define SQRT_LUTSIZE    (1 << 16)
#define SQRT_PRESHIFT   (2)

using namespace m3;

static int8_t sinlut[SIN_LUTSIZE];
static uint8_t sqrtlut[SQRT_LUTSIZE >> SQRT_PRESHIFT];

static void gen_sinlut(void) {
    for(int i = 0; i < SIN_LUTSIZE / 2; i++) {
        float sinval = sinf(M_PI * 2 * static_cast<float>(i) / SIN_LUTSIZE);
        sinlut[i] = static_cast<int8_t>(sinval * 127);
        sinlut[i + SIN_LUTSIZE / 2] = -sinlut[i];
    }
}

static void gen_sqrtlut(void) {
    for(int i = 0; i < (SQRT_LUTSIZE >> SQRT_PRESHIFT); i++) {
        float sqrtval = sqrtf(i << SQRT_PRESHIFT);
        sqrtlut[i] = sqrtval;
    }
}

static int16_t lsin(uint8_t v) {
    return sinlut[v];
}
static int16_t lcos(uint8_t v) {
    return lsin(64 - v);
}
static uint8_t lsqrt(uint16_t v) {
    return sqrtlut[v >> SQRT_PRESHIFT];
}
static int16_t sqr(int16_t x) {
    return x * x;
}

static uint8_t distance(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    return lsqrt(sqr(x1 - x2) + sqr(y1 - y2));
}

template<unsigned ROW, unsigned COL>
class TextBuffer {
private:
    uint16_t _buffer[ROW * COL];
public:
    uint16_t &character(unsigned row, unsigned col) {
        return _buffer[row * COL + col];
    }

    const uint16_t *buffer() const {
        return _buffer;
    }
    size_t size() const {
        return sizeof(_buffer);
    }

    void blt_from(TextBuffer<ROW, COL> *target) {
        memcpy(_buffer, target->_buffer, sizeof(_buffer));
    }

    void put_text(unsigned row, unsigned col, uint8_t attr, const char *str, int len = -1) {
        while((len-- != 0) && *str != 0) {
            if(*str != ' ')
                character(row, col) = *str | (attr << 8);
            col++;
            str++;
        }
    }

    TextBuffer() {
        memset(_buffer, 0, sizeof(_buffer));
    }
};

template<unsigned ROW, unsigned COL>
class TextAnimator : public TextBuffer<ROW, COL> {
public:
    virtual void render(cycles_t now) = 0;
};

template<unsigned ROW, unsigned COL>
class PlasmaAnimator : public TextAnimator<ROW, COL> {
    size_t _color;

    void plasma_put(unsigned row, unsigned col, int8_t color) {
        static uint16_t colors[][3] = {
             {0x0000, 0x0100, 0x0900},
             {0x0000, 0x0200, 0x0A00},
             {0x0000, 0x0300, 0x0B00},
             {0x0000, 0x0400, 0x0C00},
             {0x0000, 0x0500, 0x0D00},
             {0x0000, 0x0600, 0x0E00},
             {0x0000, 0x0700, 0x0F00},
             {0x0000, 0x0800, 0x0600},
        };

        int icol = (static_cast<int>(color) + 128) >> 4;
        uint16_t *colbasetab = colors[_color];
        uint16_t coltab[8] = {
            (uint16_t)(' ' | colbasetab[0]),
            (uint16_t)(' ' | colbasetab[0]),
            (uint16_t)(':' | colbasetab[1]),
            (uint16_t)(':' | colbasetab[2]),
            (uint16_t)('o' | colbasetab[1]),
            (uint16_t)('O' | colbasetab[1]),
            (uint16_t)('O' | colbasetab[2]),
            (uint16_t)('Q' | colbasetab[2])
        };
        uint16_t attr = coltab[(icol <= 8) ? (8 - icol) : (icol - 8)];
        if(icol <= 8)
            attr = (attr & 0x8FF) | 0x0100;

        this->character(row, col) = attr;
    }

public:
    size_t color() const {
        return _color;
    }
    void set_color(size_t color) {
        _color = color % 8;
    }

    PlasmaAnimator() : _color(0) {
    }

    void render(cycles_t now) {
        unsigned t = now >> 25;

        // Double ROW to correct aspect ratio.
        for(unsigned rc = 0; rc < ROW * 2; rc += 2) {
            for(unsigned cc = 0; cc < COL; cc++) {
                int8_t v1 = lsin(distance(rc, cc, ROW * 2 / 2, COL / 2) * 2 + 2 * t);
                int8_t v2 = lsin(distance(rc, cc, (lsin(t >> 5) / 2 + 60), (lcos(t >> 5) / 2 + 60)));

                int8_t v3 = lsin(distance(rc, cc, (lsin(-t * 3) / 2 + 64), (lcos(-t * 3) / 2 + 64)));

                plasma_put(rc / 2, cc, (v1 + v2 + v3) / 3);
            }
        }

        this->character(ROW - 1, 0) = (this->character(ROW - 1, 0) & 0xFF00) | 'J';
        this->character(ROW - 1, 1) = (this->character(ROW - 1, 0) & 0xFF00) | 'S';
    }
};

template<unsigned ROW, unsigned COL>
class QuoteAnimator : public TextAnimator<ROW, COL> {
    TextAnimator<ROW, COL> *_background;
    bool _start_init;
    cycles_t _start;
    ssize_t _quote;
    ssize_t _next_quote;

public:
    QuoteAnimator(TextAnimator<ROW, COL> *background)
        : _background(background), _start_init(false), _start(), _quote(-1), _next_quote(0) {
    }

    size_t get_quote() const {
        return _next_quote != -1 ? _next_quote : _quote;
    }
    void to_quote(cycles_t now, size_t i) {
        if(_next_quote == -1)
            _start = now;
        _next_quote = i;
    }

    void render(cycles_t now) {
        const uint16_t text_bg_attr = 0x0800;

        _background->render(now);
        this->blt_from(_background);

        if(!_start_init) {
            _start = now - (400 << 22);
            _start_init = true;
        }

        unsigned t = static_cast<unsigned>((now - _start) >> 22);
        if(_next_quote != -1) {
            size_t off = _quote != -1 ? 800 : 0;
            if(t < 800 && off)
                bar_out(text_bg_attr, t, 0);
            else if(t < off + 800)
                bar_in(text_bg_attr, t, off);
            else {
                _quote = _next_quote;
                _next_quote = -1;
            }
        }

        if(_next_quote == -1) {
            bar_empty(text_bg_attr);
            bar_text(text_bg_attr, t, 1700);
        }
    }

private:
    void bar_empty(uint16_t text_bg_attr) {
        for(unsigned rc = 0; rc < ROW; rc++) {
            for(unsigned cc = 0; cc < COL; cc++) {
                if((rc > (ROW / 2 - 2)) && (rc < (ROW / 2 + 2))) {
                    uint16_t &ch = this->character(rc, cc);
                    ch = (ch & 0xFF) | text_bg_attr;
                }
            }
        }
    }

    void bar_text(uint16_t, unsigned t, unsigned start) {
        const char *msg[] = {
            // A collection of nice Perlisms.
            "Simplicity does not precede complexity, but follows it.",
            "If your computer speaks English, it was probably made in Japan.",
            "To understand a program you must become both the machine and the program.",
            "If a listener nods his head when you're explaining your program, wake him up.",
            // Alan Kay quotes
            "Perspective is worth 80 IQ points.",
            "A successful technology creates problems that only it can solve.",
            "Simple things should be simple. Complex things should be possible.",
            "If you don't fail >= 90% of the time, you're not aiming high enough."
            // Kent Pitman
        };
        unsigned msg_no = sizeof(msg) / sizeof(*msg);
        const char *cur_msg = msg[_quote % msg_no];
        unsigned msg_len = strlen(cur_msg);

        if(t >= start)
            this->put_text(ROW / 2, COL / 2 - msg_len / 2 - 1, 0x0F, cur_msg, (t - start) / 10);
    }

    void bar_in(uint16_t text_bg_attr, unsigned t, unsigned start) {
        for(unsigned rc = 0; rc < ROW; rc++) {
            for(unsigned cc = 0; cc < COL; cc++) {
                if((cc <= (t - start) / 10) && (rc > (ROW / 2 - 2)) && (rc < (ROW / 2 + 2))) {
                    uint16_t &ch = this->character(rc, cc);
                    ch = (ch & 0xFF) | text_bg_attr;
                }
            }
        }
    }

    void bar_out(uint16_t text_bg_attr, unsigned t, unsigned start) {
        for(unsigned rc = 0; rc < ROW; rc++) {
            for(unsigned cc = 0; cc < COL; cc++) {
                if((cc >= (t - start) / 10) && (rc > (ROW / 2 - 2)) && (rc < (ROW / 2 + 2))) {
                    uint16_t &ch = this->character(rc, cc);
                    ch = (ch & 0xFF) | text_bg_attr;
                }
            }
        }
    }
};

static const char *intro_text[] = {
#include "intro-text.inc"
};

template<unsigned ROW, unsigned COL>
class IntroAnimator : public TextAnimator<ROW, COL> {
    bool _start_init;
    cycles_t _start;
    bool _done;
public:
    bool done() const {
        return _done;
    }

    void render(cycles_t now) {
        if(!_start_init) {
            _start = now;
            _start_init = true;

            for(unsigned rc = 0; rc < ROW; rc++) {
                for(unsigned cc = 0; cc < COL; cc++) {
                    uint32_t r = random() % (127 - 32) + 32;
                    this->character(rc, cc) = r | 0x0800;
                }
            }
        }
        else {
            for(unsigned rc = 0; rc < ROW; rc++) {
                for(unsigned cc = 0; cc < COL; cc++) {
                    unsigned target;
                    uint16_t &chara = this->character(rc, cc);

                    unsigned start_row = (ROW - sizeof(intro_text) / sizeof(*intro_text)) / 2;
                    if((rc >= start_row)
                       && (rc - start_row < sizeof(intro_text) / sizeof(*intro_text))
                       && (cc < strlen(intro_text[rc - start_row]))) {
                        target = intro_text[rc - start_row][cc];
                    }
                    else {
                        target = ' ';
                    }

                    if((chara & 0xFF) == target) {
                        chara = (chara & 0xFF) | 0x0F00;
                        continue;
                    }

                    chara++;
                    if((chara & 0xFF) > 127)
                        chara = 0x0700 | 32;
                }
            }
        }
    }

    IntroAnimator() : _start_init(false), _start(), _done(false) {
    }
};

static inline cycles_t tsc() {
    return Profile::start();
}

static IntroAnimator<VGA::ROWS, VGA::COLS> ia;
static PlasmaAnimator<VGA::ROWS, VGA::COLS> pa;
static QuoteAnimator<VGA::ROWS, VGA::COLS> qa(&pa);
static unsigned irqs = 0;

class PlasmaRequestHandler;
using plasma_reqh_base_t = RequestHandler<PlasmaRequestHandler, Plasma::Operation, Plasma::COUNT>;

class PlasmaRequestHandler : public plasma_reqh_base_t {
public:
    explicit PlasmaRequestHandler() : plasma_reqh_base_t() {
        add_operation(Plasma::LEFT, &PlasmaRequestHandler::left);
        add_operation(Plasma::RIGHT, &PlasmaRequestHandler::right);
        add_operation(Plasma::COLUP, &PlasmaRequestHandler::colup);
        add_operation(Plasma::COLDOWN, &PlasmaRequestHandler::coldown);
    }

    virtual size_t credits() override {
        return Server<PlasmaRequestHandler>::DEF_MSGSIZE;
    }

    void left(GateIStream &is) {
        qa.to_quote(tsc(), qa.get_quote() - 1);
        reply_vmsg(is, 0);
    }
    void right(GateIStream &is) {
        qa.to_quote(tsc(), qa.get_quote() + 1);
        reply_vmsg(is, 0);
    }

    void colup(GateIStream &is) {
        pa.set_color(pa.color() + 1);
        reply_vmsg(is, 0);
    }
    void coldown(GateIStream &is) {
        pa.set_color(pa.color() - 1);
        reply_vmsg(is, 0);
    }
};

static void refresh_screen(VGA *vga, GateIStream &, Subscriber<GateIStream&> *) {
    if(irqs++ < INTRO_TIME) {
        ia.render(tsc());
        vga->gate().write_sync(ia.buffer(), ia.size(), 0);
    }
    else {
        qa.render(tsc());
        vga->gate().write_sync(qa.buffer(), qa.size(), 0);
    }
}

int main() {
    gen_sinlut();
    gen_sqrtlut();

    VGA vga("vga");

    Interrupts timerirqs("interrupts", HWInterrupts::TIMER);
    timerirqs.gate().subscribe(std::bind(refresh_screen, &vga, std::placeholders::_1, std::placeholders::_2));

    Server<PlasmaRequestHandler> srv("plasma", new PlasmaRequestHandler());

    env()->workloop()->run();
    return 0;
}
