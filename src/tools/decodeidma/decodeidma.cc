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

#include <m3/Common.h>
#include <m3/Config.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__t3__)
struct {
    unsigned long id;
    const char *name;
} const cmds[] = {
    {OVERALL_SLOT_CFG,                              "OverallSlotCfg"},
    {LOCAL_CFG_ADDRESS_FIFO_CMD,                    "LocalCfgAddrFifo"},
    {TRANSFER_CFG_SIZE_STRIDE_REPEAT_CMD,           "TransferSize"},
    {FILTER_CFG_MASK_CMD,                           "FilterMask"},
    {FILTER_CFG_OPERAND1_CMD,                       "FilterOperand1"},
    {FILTER_CFG_OPERATION_SIZE_CMD,                 "FilterOperationSize"},
    {FILTER_CFG_TRANSFER_MASK0_CMD,                 "FilterTransferMask0"},
    {FILTER_CFG_TRANSFER_MASK1_CMD,                 "FilterTransferMask1"},
    {FILTER_CFG_TRANSFER_MASK2_CMD,                 "FilterTransferMask2"},
    {FILTER_CFG_DISTRMASK1_CMD,                     "FilterDistrMask1"},
    {FILTER_CFG_TRANSFER_LABEL1_CMD,                "FilterTransferLabel1"},
    {EXTERN_CFG_ADDRESS_MODULE_CHIP_CTA_INC_CMD,    "ExternAddrModChip"},
    {EXTERN_CFG_SIZE_CREDITS_CMD,                   "ExternSizeCredits"},
    {HEADER_CFG_REPLY_LABEL_SLOT_ENABLE_ADDR,       "ReplyLblSlotEnable"},
    {FIRE_CMD,                                      "Fire"},
    {DEBUG_CMD,                                     "Debug"},
    {REPLY_CAP_RESP_CMD,                            "Reply"},
    {IDMA_CREDIT_RESPONSE_CMD,                      "CreditResponse"},
    {ALLOWED_MEMORY_MODULE_LOCATION_CMD,            "AllowedModLoc"},
    {ALLOWED_MEMORY_MODULE_LOCK,                    "AllowedModLock"},
    {IDMA_FIRST_STATUS_CMD,                         "FirstStatus"},
    {IDMA_CM_SPINOFF_REG_STATUS,                    "CMSpinOffRegStatus"},
    {IDMA_OVERALL_SLOT_STATUS,                      "OverallSlotStatus"},
    {IDMA_CREDIT_STATUS,                            "CreditStatus"},
    {IDMA_SLOT_FIFO_ELEM_CNT,                       "FifoElemCount"},
    {IDMA_SLOT_FIFO_GET_ELEM,                       "FifoGetElem"},
    {IDMA_SLOT_FIFO_RELEASE_ELEM,                   "FifoRelElem"},
    {IDMA_PORT_CREDIT_STATUS,                       "PortCreditStatus"},
    {IDMA_SLOT_SIZE_STATUS,                         "SlotSizeStatus"},
    {IDMA_CREDIT_COMPARE_EXCHANGE,                  "CreditCmpxchg"},
};
#endif

int main(int argc, char **argv) {
    if(argc < 2) {
        fprintf(stderr, "Usage: %s <addr>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

#if defined(__t3__)
    unsigned long val = strtoul(argv[1], nullptr, 0);
    unsigned long cmd = (val >> IDMA_CMD_POS) & IDMA_CMD_MASK;
    unsigned long ep = (val >> IDMA_SLOT_POS) & IDMA_SLOT_MASK;

    size_t cmdidx = 0;
    for(; cmdidx < ARRAY_SIZE(cmds); ++cmdidx) {
        if(cmds[cmdidx].id == cmd) {
            printf("iDMA:%lu:%s\n", ep, cmds[cmdidx].name);
            return 0;
        }
    }

    printf("iDMA:%lu:%#lx\n", ep, cmd);
#endif
    return 0;
}
