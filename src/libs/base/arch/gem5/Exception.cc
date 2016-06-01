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
#include <base/arch/gem5/Exception.h>
#include <base/stream/Serial.h>
#include <base/Backtrace.h>

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

namespace m3 {

static const char *exNames[] = {
    /* 0x00 */ "Divide by zero",
    /* 0x01 */ "Single step",
    /* 0x02 */ "Non maskable",
    /* 0x03 */ "Breakpoint",
    /* 0x04 */ "Overflow",
    /* 0x05 */ "Bounds check",
    /* 0x06 */ "Invalid opcode",
    /* 0x07 */ "Co-proc. n/a",
    /* 0x08 */ "Double fault",
    /* 0x09 */ "Co-proc seg. overrun",
    /* 0x0A */ "Invalid TSS",
    /* 0x0B */ "Segment not present",
    /* 0x0C */ "Stack exception",
    /* 0x0D */ "Gen. prot. fault",
    /* 0x0E */ "Page fault",
    /* 0x0F */ "<unknown>",
    /* 0x10 */ "Co-processor error",
};

Exceptions::Desc Exceptions::gdt[GDT_ENTRY_COUNT];
Exceptions::Desc64 Exceptions::idt[IDT_COUNT];
Exceptions::TSS Exceptions::tss ALIGNED(PAGE_SIZE);

void Exceptions::handler(State *state) {
    auto &ser = Serial::get();
    ser << "Interruption @ " << fmt(state->rip, "p");
    if(state->intrptNo == 0xe)
        ser << " for address " << fmt(getCR2(), "p");
    else if(state->intrptNo == 65)
        ser << " for address " << fmt(DTU::get().get_last_pf(), "p");
    ser << "\n  irq: ";
    if(state->intrptNo < ARRAY_SIZE(exNames))
        ser << exNames[state->intrptNo];
    else if(state->intrptNo == 64)
        ser << "DTU (" << state->intrptNo << ")";
    else if(state->intrptNo == 65)
        ser << "DTUPF (" << state->intrptNo << ")";
    else
        ser << "<unknown> (" << state->intrptNo << ")";
    ser << "\n";

    Backtrace::print(ser);

    ser << "  err: " << state->errorCode << "\n";
    ser << "  rax: " << fmt(state->rax,    "#0x", 16) << "\n";
    ser << "  rbx: " << fmt(state->rbx,    "#0x", 16) << "\n";
    ser << "  rcx: " << fmt(state->rcx,    "#0x", 16) << "\n";
    ser << "  rdx: " << fmt(state->rdx,    "#0x", 16) << "\n";
    ser << "  rsi: " << fmt(state->rsi,    "#0x", 16) << "\n";
    ser << "  rdi: " << fmt(state->rdi,    "#0x", 16) << "\n";
    ser << "  rsp: " << fmt(state->rsp,    "#0x", 16) << "\n";
    ser << "  rbp: " << fmt(state->rbp,    "#0x", 16) << "\n";
    ser << "  r8 : " << fmt(state->r8,     "#0x", 16) << "\n";
    ser << "  r9 : " << fmt(state->r9,     "#0x", 16) << "\n";
    ser << "  r10: " << fmt(state->r10,    "#0x", 16) << "\n";
    ser << "  r11: " << fmt(state->r11,    "#0x", 16) << "\n";
    ser << "  r12: " << fmt(state->r12,    "#0x", 16) << "\n";
    ser << "  r13: " << fmt(state->r13,    "#0x", 16) << "\n";
    ser << "  r14: " << fmt(state->r14,    "#0x", 16) << "\n";
    ser << "  r15: " << fmt(state->r15,    "#0x", 16) << "\n";
    ser << "  flg: " << fmt(state->rflags, "#0x", 16) << "\n";
    env()->exit(1);
}

void Exceptions::init() {
    // setup GDT
    DescTable gdtTable;
    gdtTable.offset = (ulong)gdt;
    gdtTable.size = GDT_ENTRY_COUNT * sizeof(Desc) - 1;

    // code+data
    setDesc(gdt + SEG_CODE,0,~0UL >> PAGE_BITS,Desc::GRANU_PAGES,Desc::CODE_XR,Desc::DPL_KERNEL);
    setDesc(gdt + SEG_DATA,0,~0UL >> PAGE_BITS,Desc::GRANU_PAGES,Desc::DATA_RW,Desc::DPL_KERNEL);
    // tss (we don't need a valid stack pointer because we don't switch the ring)
    setTSS(gdt,&tss,0);

    // now load GDT and TSS
    loadGDT(&gdtTable);
    loadTSS(SEG_TSS * sizeof(Desc));

    // setup the idt-pointer
    DescTable tbl;
    tbl.offset = (ulong)idt;
    tbl.size = sizeof(idt) - 1;

    // setup the idt
    setIDT(0,isr0,Desc::DPL_KERNEL);
    setIDT(1,isr1,Desc::DPL_KERNEL);
    setIDT(2,isr2,Desc::DPL_KERNEL);
    setIDT(3,isr3,Desc::DPL_KERNEL);
    setIDT(4,isr4,Desc::DPL_KERNEL);
    setIDT(5,isr5,Desc::DPL_KERNEL);
    setIDT(6,isr6,Desc::DPL_KERNEL);
    setIDT(7,isr7,Desc::DPL_KERNEL);
    setIDT(8,isr8,Desc::DPL_KERNEL);
    setIDT(9,isr9,Desc::DPL_KERNEL);
    setIDT(10,isr10,Desc::DPL_KERNEL);
    setIDT(11,isr11,Desc::DPL_KERNEL);
    setIDT(12,isr12,Desc::DPL_KERNEL);
    setIDT(13,isr13,Desc::DPL_KERNEL);
    setIDT(14,isr14,Desc::DPL_KERNEL);
    setIDT(15,isr15,Desc::DPL_KERNEL);
    setIDT(16,isr16,Desc::DPL_KERNEL);

    // all other interrupts
    for(size_t i = 17; i < 63; i++)
        setIDT(i,isrNull,Desc::DPL_KERNEL);

    // DTU interrupts
    setIDT(64,isr64,Desc::DPL_KERNEL);
    setIDT(65,isr65,Desc::DPL_KERNEL);

    // now we can use our idt
    loadIDT(&tbl);
    enableIRQs();
}

void Exceptions::setDesc(Desc *d,uintptr_t address,size_t limit,uint8_t granu,uint8_t type,uint8_t dpl) {
    d->addrLow = address & 0xFFFF;
    d->addrMiddle = (address >> 16) & 0xFF;
    d->limitLow = limit & 0xFFFF;
    d->addrHigh = ((address & 0xFF000000) >> 16) | ((limit >> 16) & 0xF) |
        Desc::BITS_64 | Desc::SIZE_16 | granu;
    d->present = 1;
    d->dpl = dpl;
    d->type = type;
}

void Exceptions::setDesc64(Desc *d,uintptr_t address,size_t limit,uint8_t granu,uint8_t type,uint8_t dpl) {
    Desc64 *d64 = (Desc64*)d;
    setDesc(d64,address,limit,granu,type,dpl);
    d64->addrUpper = address >> 32;
}

void Exceptions::setIDT(size_t number,isr_func handler,uint8_t dpl) {
    Desc64 *e = idt + number;
    e->type = Desc::SYS_INTR_GATE;
    e->dpl = dpl;
    e->present = number != INTEL_RES1 && number != INTEL_RES2;
    e->addrLow = SEG_CODE << 3;
    e->addrHigh = ((uintptr_t)handler >> 16) & 0xFFFF;
    e->limitLow = (uintptr_t)handler & 0xFFFF;
    e->addrUpper = (uintptr_t)handler >> 32;
}

void Exceptions::setTSS(Desc *gdt,TSS *tss,uintptr_t kstack) {
    tss->setSP(kstack + PAGE_SIZE - 1 * sizeof(ulong));
    setDesc64(gdt + SEG_TSS,(uintptr_t)tss,sizeof(TSS) - 1,
        Desc::GRANU_BYTES,Desc::SYS_TSS,Desc::DPL_KERNEL);
}

}
