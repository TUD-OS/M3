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

#define FGPA_INTERFACE_MODULE_ID            0
#define FIRST_PE_ID                         1
#define MEMORY_CORE                         0
#define KERNEL_CORE                         FIRST_PE_ID
#define APP_CORES                           (FIRST_PE_ID + 1)

#define DRAM_VOFFSET                        0x60008000

/* IDMA STUFF */
#define IDMA_TRF_FINISHED                   0xAFFE0000
#define IDMA_OVERALL_SLOT_STATUS            0x60000000
#define IDMA_PORT_STATUS                    0x60000008
#define IDMA_CONFIG_ADDR                    0xF0000000
#define IDMA_DATA_PORT_ADDR                 0xF1000000
#define PE_IDMA_CONFIG_ADDRESS_MASK         0x0001FC00  //defines the bits, necessary to identify action
#define PE_IDMA_CONFIG_ADDRESS              0x60019000 // should be blocked! (not in use!)
#define PE_IDMA_DATA_PORT_ADDRESS           0x6001B000 //reserved! not accessible for the core! OLD: 0x6001E000
#define PE_IDMA_CREDIT_RESET_ADDRESS        PE_IDMA_DATA_PORT_ADDRESS
#define PE_IDMA_SLOT_FIFO_ELEM_CNT          0x6001B000
#define PE_IDMA_SLOT_FIFO_GET_ELEM          0x6001C800
#define PE_IDMA_SLOT_FIFO_RELEASE_ELEM      0x6001D000
//#define PE_IDMA_DATA_FIFO_PNTR_ADDRESS      0x6001C000
#define PE_IDMA_PORT_CREDIT_STATUS          0x6001C000  //TODO: change address
#define PE_IDMA_SLOT_SIZE_STATUS_ADDRESS    0x6001C400  //TODO: change address

#define PE_IDMA_SLOT_POS                    6
#define PE_IDMA_SLOT_TRG_ID_POS             3
#define IDMA_SLOT_POS                       12 //null wird als position mitgezählt!
#define IDMA_SLOT_TRG_ID_POS                3 //null wird als position mitgezählt!
//#define IDMA_SLOT_MASK                    0xF   //number of slots from idma_defines!
//#define IDMA_SLOT_TRG_ID_MASK             0x7   //number of targets from idma_defines!

#define PE_IDMA_OVERALL_SLOT_STATUS_ADDRESS 0x6001E200
#define PE_IDMA_PORT_STATUS_ADDRESS         0x6001F208
