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

#include <base/Common.h>
#include <base/stream/Serial.h>
#include <base/Backtrace.h>

#include "Exceptions.h"

// Our ISRs
EXTERN_C void isr0();
EXTERN_C void isr1();
EXTERN_C void isr2();
EXTERN_C void isr3();
EXTERN_C void isr4();
EXTERN_C void isr5();
EXTERN_C void isr6();
EXTERN_C void isr7();
EXTERN_C void isr8();
EXTERN_C void isr9();
EXTERN_C void isr10();
EXTERN_C void isr11();
EXTERN_C void isr12();
EXTERN_C void isr13();
EXTERN_C void isr14();
EXTERN_C void isr15();
EXTERN_C void isr16();
// for the DTU
EXTERN_C void isr64();
EXTERN_C void isr65();
// the handler for a other interrupts
EXTERN_C void isrNull();

m3::Exceptions::isr_func Exceptions::isrs[IDT_COUNT];
Exceptions::Desc Exceptions::gdt[GDT_ENTRY_COUNT];
Exceptions::Desc64 Exceptions::idt[IDT_COUNT];
Exceptions::TSS Exceptions::tss ALIGNED(PAGE_SIZE);

void Exceptions::handler(m3::Exceptions::State *state) {
    // TODO move that to Entry.S
    isrs[state->intrptNo](state);
}

void Exceptions::null_handler(m3::Exceptions::State *) {
}

void Exceptions::init() {
    // setup GDT
    DescTable gdtTable;
    gdtTable.offset = reinterpret_cast<uintptr_t>(gdt);
    gdtTable.size = GDT_ENTRY_COUNT * sizeof(Desc) - 1;

    // code+data
    setDesc(gdt + SEG_CODE, 0, ~0UL >> PAGE_BITS, Desc::GRANU_PAGES, Desc::CODE_XR, Desc::DPL_KERNEL);
    setDesc(gdt + SEG_DATA, 0, ~0UL >> PAGE_BITS, Desc::GRANU_PAGES, Desc::DATA_RW, Desc::DPL_KERNEL);
    // tss (we don't need a valid stack pointer because we don't switch the ring)
    setTSS(gdt, &tss, 0);

    // now load GDT and TSS
    loadGDT(&gdtTable);
    loadTSS(SEG_TSS * sizeof(Desc));

    // setup the idt-pointer
    DescTable tbl;
    tbl.offset = reinterpret_cast<uintptr_t>(idt);
    tbl.size = sizeof(idt) - 1;

    // setup the idt
    setIDT(0, isr0, Desc::DPL_KERNEL);
    setIDT(1, isr1, Desc::DPL_KERNEL);
    setIDT(2, isr2, Desc::DPL_KERNEL);
    setIDT(3, isr3, Desc::DPL_KERNEL);
    setIDT(4, isr4, Desc::DPL_KERNEL);
    setIDT(5, isr5, Desc::DPL_KERNEL);
    setIDT(6, isr6, Desc::DPL_KERNEL);
    setIDT(7, isr7, Desc::DPL_KERNEL);
    setIDT(8, isr8, Desc::DPL_KERNEL);
    setIDT(9, isr9, Desc::DPL_KERNEL);
    setIDT(10, isr10, Desc::DPL_KERNEL);
    setIDT(11, isr11, Desc::DPL_KERNEL);
    setIDT(12, isr12, Desc::DPL_KERNEL);
    setIDT(13, isr13, Desc::DPL_KERNEL);
    setIDT(14, isr14, Desc::DPL_KERNEL);
    setIDT(15, isr15, Desc::DPL_KERNEL);
    setIDT(16, isr16, Desc::DPL_KERNEL);

    // all other interrupts
    for(size_t i = 17; i < 63; i++)
        setIDT(i, isrNull, Desc::DPL_KERNEL);

    // DTU interrupts
    setIDT(64, isr64, Desc::DPL_KERNEL);
    setIDT(65, isr65, Desc::DPL_KERNEL);

    for(size_t i = 0; i < IDT_COUNT; ++i)
        isrs[i] = null_handler;

    // now we can use our idt
    loadIDT(&tbl);
}

void Exceptions::setDesc(Desc *d, uintptr_t address, size_t limit, uint8_t granu,
        uint8_t type, uint8_t dpl) {
    d->addrLow = address & 0xFFFF;
    d->addrMiddle = (address >> 16) & 0xFF;
    d->limitLow = limit & 0xFFFF;
    d->addrHigh = ((address & 0xFF000000) >> 16) | ((limit >> 16) & 0xF) |
        Desc::BITS_64 | Desc::SIZE_16 | granu;
    d->present = 1;
    d->dpl = dpl;
    d->type = type;
}

void Exceptions::setDesc64(Desc *d, uintptr_t address, size_t limit, uint8_t granu,
        uint8_t type, uint8_t dpl) {
    Desc64 *d64 = reinterpret_cast<Desc64*>(d);
    setDesc(d64,address,limit,granu,type,dpl);
    d64->addrUpper = address >> 32;
}

void Exceptions::setIDT(size_t number, entry_func handler, uint8_t dpl) {
    Desc64 *e = idt + number;
    e->type = Desc::SYS_INTR_GATE;
    e->dpl = dpl;
    e->present = number != INTEL_RES1 && number != INTEL_RES2;
    e->addrLow = SEG_CODE << 3;
    e->addrHigh = (reinterpret_cast<uintptr_t>(handler) >> 16) & 0xFFFF;
    e->limitLow = reinterpret_cast<uintptr_t>(handler) & 0xFFFF;
    e->addrUpper = reinterpret_cast<uintptr_t>(handler) >> 32;
}

void Exceptions::setTSS(Desc *gdt, TSS *tss, uintptr_t kstack) {
    tss->setSP(kstack);
    setDesc64(gdt + SEG_TSS, reinterpret_cast<uintptr_t>(tss), sizeof(TSS) - 1,
        Desc::GRANU_BYTES, Desc::SYS_TSS, Desc::DPL_KERNEL);
}
