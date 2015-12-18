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

#pragma once

#include <m3/Common.h>
#include <m3/Config.h>

namespace m3 {

class Exceptions {
    Exceptions() = delete;

    /* the descriptor table */
    struct DescTable {
        uint16_t size;      /* the size of the table -1 (size=0 is not allowed) */
        ulong offset;
    } PACKED;

    /* segments numbers */
    enum {
        SEG_CODE           = 1,
        SEG_DATA           = 2,
        SEG_TSS            = 3,
    };

    /* a descriptor */
    struct Desc {
        enum {
            SYS_TASK_GATE   = 0x05,
            SYS_TSS         = 0x09,
            SYS_INTR_GATE   = 0x0E,
            DATA_RO         = 0x10,
            DATA_RW         = 0x12,
            CODE_X          = 0x18,
            CODE_XR         = 0x1A,
        };

        enum {
            DPL_KERNEL      = 0x0,
            DPL_USER        = 0x3,
        };

        enum {
            BITS_32         = 0 << 5,
            BITS_64         = 1 << 5,
        };

        /**
         * size:        If 0 the selector defines 16 bit protected mode. If 1 it defines 32 bit
         *              protected mode. You can have both 16 bit and 32 bit selectors at once.
         */
        enum {
            SIZE_16         = 0 << 6,
            SIZE_32         = 1 << 6,
        };

        /**
         * granularity: If 0 the limit is in 1 B blocks (byte granularity), if 1 the limit is in
         *              4 KiB blocks (page granularity).
         */
        enum {
            GRANU_BYTES     = 0 << 7,
            GRANU_PAGES     = 1 << 7,
        };

        /* limit[0..15] */
        uint16_t limitLow;

        /* address[0..15] */
        uint16_t addrLow;

        /* address[16..23] */
        uint8_t addrMiddle;

        /*
         * present:     This must be 1 for all valid selectors.
         * dpl:         Contains the ring level, 0 = highest (kernel), 3 = lowest (user applications).
         * type:        segment type
         */
        uint8_t type : 5,
                dpl : 2,
                present : 1;

        /* address[24..31] and other fields, depending on the type of descriptor */
        uint16_t addrHigh;
    } PACKED;

    /* only on x86_64 */
    struct Desc64 : public Desc {
        uint32_t addrUpper;
        uint32_t : 32;
    };

    /* the Task State Segment */
    struct TSS {
        /* the size of the io-map (in bits) */
        static const size_t IO_MAP_SIZE                 = 0xFFFF;

        static const uint16_t IO_MAP_OFFSET             = 104;
        /* an invalid offset for the io-bitmap => not loaded yet */
        static const uint16_t IO_MAP_OFFSET_INVALID     = 104 + 16;

        TSS() {
            ioMapOffset = IO_MAP_OFFSET_INVALID;
            ioMapEnd = 0xFF;
        }

        void setSP(uintptr_t sp) {
            rsp0 = sp;
        }

        uint32_t : 32; /* reserved */
        /* stack pointer for privilege levels 0-2 */
        uint64_t rsp0;
        uint64_t rsp1;
        uint64_t rsp2;
        uint32_t : 32; /* reserved */
        uint32_t : 32; /* reserved */
        /* interrupt stack table pointer */
        uint64_t ist1;
        uint64_t ist2;
        uint64_t ist3;
        uint64_t ist4;
        uint64_t ist5;
        uint64_t ist6;
        uint64_t ist7;
        uint32_t : 32; /* reserved */
        uint32_t : 32; /* reserved */
        uint16_t : 16; /* reserved */
        /* Contains a 16-bit offset from the base of the TSS to the I/O permission bit map
         * and interrupt redirection bitmap. When present, these maps are stored in the
         * TSS at higher addresses. The I/O map base address points to the beginning of the
         * I/O permission bit map and the end of the interrupt redirection bit map. */
        uint16_t ioMapOffset;
        uint8_t ioMap[IO_MAP_SIZE / 8];
        uint8_t ioMapEnd;
    } PACKED;

    /* isr prototype */
    typedef void (*isr_func)();

    /* reserved by intel */
    enum {
        INTEL_RES1          = 2,
        INTEL_RES2          = 15
    };

    /* the stack frame for the interrupt-handler */
    struct State {
        /* general purpose registers */
        ulong r15;
        ulong r14;
        ulong r13;
        ulong r12;
        ulong r11;
        ulong r10;
        ulong r9;
        ulong r8;
        ulong rbp;
        ulong rsi;
        ulong rdi;
        ulong rdx;
        ulong rcx;
        ulong rbx;
        ulong rax;
        /* interrupt-number */
        ulong intrptNo;
        /* error-code (for exceptions); default = 0 */
        ulong errorCode;
        /* pushed by the CPU */
        ulong rip;
        ulong cs;
        ulong rflags;
        ulong rsp;
        ulong ss;
    } PACKED;

    static const size_t IDT_COUNT       = 256;

    /* we need 5 entries: null-entry, code for kernel, data for kernel, 2 for TSS (on x86_64) */
    static const size_t GDT_ENTRY_COUNT = 5;

public:
    static void init();

private:
    static void handler(State *state) asm("intrpt_handler");

    static void loadIDT(DescTable *tbl) {
        asm volatile ("lidt %0" : : "m"(*tbl));
    }
    static void loadTSS(size_t gdtOffset) {
        asm volatile ("ltr %0" : : "m"(gdtOffset));
    }
    static void loadGDT(DescTable *gdt) {
        asm volatile ("lgdt (%0)" : : "r"(gdt));
    }

    static void setDesc(Desc *d,uintptr_t address,size_t limit,uint8_t granu,uint8_t type,uint8_t dpl);
    static void setDesc64(Desc *d,uintptr_t address,size_t limit,uint8_t granu,uint8_t type,uint8_t dpl);
    static void setIDT(size_t number,isr_func handler,uint8_t dpl);
    static void setTSS(Desc *gdt,TSS *tss,uintptr_t kstack);

    static Desc gdt[GDT_ENTRY_COUNT];
    static Desc64 idt[IDT_COUNT];
    static TSS tss;
};

}
