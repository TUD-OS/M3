#pragma once

#define FGPA_INTERFACE_MODULE_ID                0
#define FIRST_PE_ID                             4
#define CM_CORE                                 1
#define MEMORY_CORE                             2
#define KERNEL_CORE                             FIRST_PE_ID
#define APP_CORES                               0

#define DRAM_VOFFSET                            0

#if defined(CM)
#   define PE_DMA_CONFIG                        0x6001FA00

#   define PE_DMA_REG_TARGET                    2
#   define PE_DMA_REG_REM_ADDR                  4
#   define PE_DMA_REG_LOC_ADDR                  6
#   define PE_DMA_REG_TYPE                      8
#   define PE_DMA_REG_SIZE                      10

#   define PE_IDMA_OVERALL_SLOT_STATUS_ADDRESS  (PE_DMA_CONFIG + PE_DMA_REG_SIZE * 4)
#   define DTU_READ_CMD                         0
#   define DTU_WRITE_CMD                        1
#else
#   define PE_DMA_CONFIG                        0x6001E078

#   define PE_DMA_REG_TARGET                    0
#   define PE_DMA_REG_TYPE                      2
#   define PE_DMA_REG_REM_ADDR                  4
#   define PE_DMA_REG_LOC_ADDR                  6
#   define PE_DMA_REG_SIZE                      8

#   define PE_IDMA_OVERALL_SLOT_STATUS_ADDRESS  0x6001E0A0
#   define DTU_READ_CMD                         0
#   define DTU_WRITE_CMD                        2
#endif

//############ TESTCASE SPECIFIC ADDRESSES ####################
#define TESTCASE_FINAL_RESULT_ADDRESS    0x0161FFF0
#define TESTCASE_RESULT_SUCCESS_VALUE    0x11111111
#define TESTCASE_RESULT_FAILURE_VALUE    0xAFFEAFFE

#define TESTCASE_GLOBAL_DEBUG_ADDRESS   0xAFFE0000
#define TESTCASE_GLOBAL_DEBUG_MASK      0x3F
#define TESTCASE_GLOBAL_DEBUG_CHIP_MODID FGPA_INTERFACE_MODULE_ID //send to the FPGA INTERFACE

#define TESTCASE_APP_FINAL_RESULT_ADDRESS (0xC0000000 + TESTCASE_FINAL_RESULT_ADDRESS)  //NEEDED TO WRITE THE RESULT FROM THE APP CORE TO THE FPGA INTERFACE!
#define TESTCASE_GLOBAL_APP_DEBUG_ADDRESS 0xCFFE0000      //Bits[31:28] = 0xC --> this is reserved slot for the FPGA at the App-TU (please refer to app_parameters.v)

//############ END TESTCASE SPECIFIC ADDRESSES ####################

//############ CM-CONFIGURATION SPECIFIC PARAMETERS (needed for JTAG Testcases) ####################
#define CM_IMEM_CFG_START_ADDR 0x60010000
#define CM_UMEM_CFG_START_ADDR 0x60000000
#define CM_LOCAL_ADDR_MEM_ID_POS    16       //(REF: LOCAL_ADDR_MEM_ID_POS in cm_parameters.
#define CM_TU_IMEM_ID            0x1
#define CM_TU_UMEM_ID            0x0
//############ CM-CONFIGURATION SPECIFIC PARAMETERS (needed for JTAG Testcases) ####################

//############ Duo-PE-CONFIGURATION SPECIFIC PARAMETERS (needed for JTAG Testcases) ####################
#define PE_IMEM_CFG_START_ADDR 0x60010000
#define PE_UMEM_CFG_START_ADDR 0x60008000
#define PE_LOCAL_ADDR_MEM_ID_POS    16       //(REF: LOCAL_ADDR_MEM_ID_POS in cm_parameters.
#define VDSP_IMEM            0x1
#define VDSP_UMEM            0x0
#define VDSP_MASTER          0x2
#define VDSP_CTRL            0x3
#define DUO_PE_CTRL_VDSP     0x0
#define DUO_PE_CTRL_XTENSA   0x1
//############ Duo-PE-CONFIGURATION SPECIFIC PARAMETERS (needed for JTAG Testcases) ####################

#define FIFO_8BIT_EMPTY_VALUE 255
#define FIFO_32BIT_EMPTY_VALUE 0xFFFFFFFF

#define TASK_DESCR_SLOT_ID_POS 14

//0xA0000000 + 0x1A000 = 0x0001A000 (Adresse auf die der App-Core zum CM die Task-BEscrehibugn sendet)
#define APP_CM_CONFIG_BASE_ADDRESS 0xA0000000
#define CM_CM_CONFIG_BASE_ADDRESS  0x60000000

#define CM_CONFIG_TASK_DESC_BINARY_ADDRESS  0x17e00
#define CM_CONFIG_TASK_DESC_DATA            0x17e10

/* CoreManager interrupt level -> */
#define CM_INTERRUPT_NUMBER_APP_CORE_NEW_TASK           0
#define CM_INTERRUPT_NUMBER_FIFO_8                      1
#define CM_INTERRUPT_NUMBER_FIFO_7                      2
#define CM_INTERRUPT_NUMBER_FIFO_6                      3
#define CM_INTERRUPT_NUMBER_FIFO_5                      4
#define CM_INTERRUPT_NUMBER_FIFO_4                      5
#define CM_INTERRUPT_NUMBER_FIFO_3                      6
#define CM_INTERRUPT_NUMBER_FIFO_2                      7
#define CM_INTERRUPT_NUMBER_FIFO_1                      8
#define CM_INTERRUPT_NUMBER_FIFO_0                      9
#define CM_INTERRUPT_NUMBER_RESEVERED                   10
#define CM_INTERRUPT_NUMBER_PE_LAST_TRANSFER_FINISHED   11
#define CM_INTERRUPT_NUMBER_SELF                        12

/* Application processor --> Application processor Transfer Unit */
#define APP_CTRL_ID                 0xD
#define APP_CTRL_IRQ_ID             0x0
#define APP_CTRL_ADDR_MAP_ID        0x1
#define APP_APP_TU_SET_APP_IRQ          0xD0000000      //enables/disables the IRQ 0; bits[31:28] is a fixed parameter (defined by "APP_CTRL_MSB_ID"; please refer to app_parameters.v)
  /*to set the IRQ:
    addr[31:28] = APP_CTRL_ID;
    addr[27:24] = APP_CTRL_IRQ_ID;
    addr[6:3]   = ID of IRQ (0 to 15)
    data[0]     = set = 1; reset = 0;

  */

/* Application processor --> CoreManager Transfer Unit */
#define APP_CM_TU_ADDRESS_START           0xA0000000
#define APP_CM_TU_TASK_DESC_MEM_BASE    0xAA000000
#define APP_CM_TU_CONFIG_START            0xAA01F000
#define APP_CM_TU_FIFO_CM_2_APP           0xAA01F000
#define APP_CM_TU_FIFO_APP_2_CM           0xAA01F010
#define APP_CM_TU_APP_NEEDS_IRQ           0xAA01F028    //ACHTUNG: NICHT IDENTISCH ZUR HW!!!! (sonst kein printf mehr möglich beim CM!)
#define APP_CM_TU_APP_SET_IRQ               0xAA01F030
#define APP_CM_TU_DEBUG                       0xAA01F038
#define APP_CM_TU_FIFO_INT_32B_0      0xAA01F040
#define APP_CM_TU_FIFO_INT_32B_1      0xAA01F048
#define APP_CM_TU_FIFO_INT_32B_2      0xAA01F050
#define APP_CM_TU_FIFO_INT_32B_3      0xAA01F058
#define APP_CM_TU_FIFO_INT_32B_4      0xAA01F060
#define APP_CM_TU_FIFO_INT_32B_5      0xAA01F068
#define APP_CM_TU_FIFO_INT_32B_6      0xAA01F070
#define APP_CM_TU_FIFO_INT_32B_7      0xAA01F078
#define APP_CM_TU_ADDRESS_END             0xAB000000
#define APP_VIRTUAL_ADDRESS_OFFSET  0xE0000000

// Config der CM-Transferunit
#define CM_TU_START                 0xAC000000    //NEW!!!!       //neu



/* CoreManager --> CoreManager Transfer Unit */
#define CM_CM_TU_INST_ADDRESS_MAX           0x60017FFF
#define CM_CM_TU_START_ADDRESS              0x6001F000
#define CM_CM_TU_FIFO_CM_2_APP              0x6001F000
#define CM_CM_TU_FIFO_APP_2_CM              0x6001F010
#define CM_CM_TU_TASK_SYNC_APP_NEEDS_IRQ    0x6001F028          //ACHTUNG: NICHT IDENTISCH ZUR HW!!!! (sonst kein printf mehr möglich beim CM!)
#define CM_CM_TU_SET_IRQ_APP                0x6001F030
#define CM_CM_TU_SET_IRQ_CM                 0x6001F038
#define CM_CM_TU_FIFO_TASK_FINISHED         0x6001F040
        /* 2bit peTaskPos, 6bit moduleId, 6bit chipId */
#define CM_CM_TU_FIFO_TASK_OUT_FINISHED     0x6001F050
#define CM_CM_TU_FIFO_TASK_REASY_POS        0x6001F060
#define CM_CM_TU_DEBUG_MESSAGE              0x6001F070
#define CM_CM_TU_DEBUG_CHIPID_MODULEID      0x6001F078
#define CM_CM_TU_DEBUG_START_ADDRESS        0x6001F080
#define CM_CM_TU_DEBUG_ADDRESS_MASK         0x6001F088
#define CM_CM_TU_TASK_BINARY_ADDRESS        0x6001F090
//#define CM_CM_TU_FIFO_TASK_READY_LX4      0x6001F088
//#define CM_CM_TU_FIFO_TASK_READY_VDSP     0x6001F098

#define CM_CM_TU_FIFO_INT_32B_0             0x6001F100
#define CM_CM_TU_FIFO_INT_32B_1             0x6001F108
#define CM_CM_TU_FIFO_INT_32B_2             0x6001F110
#define CM_CM_TU_FIFO_INT_32B_3             0x6001F118

#define CM_CM_TU_FIFO_INT_32B_4             0x6001F120
#define CM_CM_TU_FIFO_INT_32B_5             0x6001F128
#define CM_CM_TU_FIFO_INT_32B_6             0x6001F130
#define CM_CM_TU_FIFO_INT_32B_7             0x6001F138

#define CM_CM_TU_FIFO_INT_INC_4             0x6001F140
#define CM_CM_TU_FIFO_INT_INC_5             0x6001F148
#define CM_CM_TU_FIFO_INT_INC_6             0x6001F150
#define CM_CM_TU_FIFO_INT_INC_7             0x6001F158


#define CM_CM_TU_FIFO_IRQ_REG               0x6001F1A0


/* CoreManager --> Application processor Transfer Unit */
#define CM_APP_TU_SET_APP_IRQ               APP_APP_TU_SET_APP_IRQ


/* zum schreiben in die 31..23: imem: 1, dmem: 0 -> danach Adresse (bit 22..0) */


// #define CM_CM_TU_TASK_DESC_CM_IS_EMPTY       0x20
// #define CM_CM_TU_APP_NEEDS_IRQ               0x28
// #define CM_CM_TU_APP_SET_IRQ             0x30
// #define CM_CM_TU_FIFO_TASK_FINISHED      0x40
// #define CM_CM_TU_FIFO_TASK_OUT_FINISHED      0x50
// #define CM_CM_TU_FIFO_TASK_READY_POS     0x60
// #define CM_CM_TU_IRQ_UNIT                    0x6001FF10
// #define CM_CM_TU_APP_IRQ                 0x6001FF20


#define CM_TU_SET_REG0_CHIP_ID                  0x6001FA00
#define CM_TU_SET_REG0_MODULE_ID                0x6001FA08
#define CM_TU_SET_REG0_DEST_ADDRESS             0x6001FA10      //SYS ADDR (specifies the ADDRESS which will be used in the FLIT
#define CM_TU_SET_REG0_SRC_ADDRESS              0x6001FA18      //LOCAL ADDR (specifies the ADDRESS of the CM)
#define CM_TU_SET_REG0_READ_WRITE               0x6001FA20
#define CM_TU_SET_REG0_SIZE                     0x6001FA28

#define CM_TU_SET_REG1_CHIP_ID                  0x6001FA00
#define CM_TU_SET_REG1_MODULE_ID                0x6001FA08
#define CM_TU_SET_REG1_DEST_ADDRESS             0x6001FA10
#define CM_TU_SET_REG1_SRC_ADDRESS              0x6001FA18
#define CM_TU_SET_REG1_READ_WRITE               0x6001FA20
#define CM_TU_SET_REG1_SIZE                     0x6001FA28

#define CM_TU_DMA_TRF_TYPE_IN                   0
#define CM_TU_DMA_TRF_TYPE_OUT                  1
#define CM_TU_DMA_TRF_TYPE_TASK_OUT             2
#define CM_TU_DMA_TRF_TYPE_PMU_CFG_IN           3
#define CM_TU_DMA_TRF_TYPE_PMU_CFG_OUT          4

#define CM_TU_SET_FIFO                      0x6001FE00
#define CM_CM_TU_END_ADDRESS                0x60020000



/* Processing Element --> CoreManager Spin-off, immer alternierend */
#define CM_SO_PE_INST_POINTER   0x6001F000
#define CM_SO_PE_DATA_COUNT     0x6001F008
#define CM_SO_PE_ARG_0          0x6001F010
#define CM_SO_PE_ARG_1          0x6001F018
#define CM_SO_PE_ARG_2          0x6001F020
#define CM_SO_PE_ARG_3          0x6001F028
#define CM_SO_PE_ARG_4          0x6001F030
#define CM_SO_PE_ARG_5          0x6001F038
#define CM_SO_PE_ARG_6          0x6001F040
#define CM_SO_PE_ARG_7          0x6001f048
#define CM_SO_PE_ARG_8          0x6001F050
#define CM_SO_PE_ARG_9          0x6001F058
#define CM_SO_PE_ARG_10         0x6001F060
#define CM_SO_PE_ARG_11         0x6001F068

#define CM_SO_PE_TASK_INCREMENT 0x80 //0x88

#define CM_SO_PE_TASK_FINISHED  CM_SO_PE_INST_POINTER
#define CM_SO_PE_CLEAR_IRQ      0x6001E070
#define CM_SO_PE_LOCAL_MEM      0x60000000
#define CM_SO_PE_DMA_CONFIG   0x6001E078
#define CM_SO_PE_DMA_LOCK_REQ 0x6001E0A0

#define TASK_TRANSFER_SKIP      0x0
#define TASK_TRANSFER_IN        0x1
#define TASK_TRANSFER_OUT       0x2
#define TASK_TRANSFER_INOUT     0x3
#define TASK_TRANSFER_2D_IN     0x4
#define TASK_TRANSFER_2D_OUT    0x8
#define TASK_TRANSFER_2D_INOUT  0xc

/* HLR ADDITIONAL TRANSFERS */
#define TASK_TRANSFER_LAZY_OUT   0xE      //Out-Transfers sollen als LAZY write back behandelt werden
#define TASK_TRANSFER_LAZY_INOUT 0xF      //siehe LAZY-OUT
#define TASK_TRANSFER_OUT_SKIP   0xA      //NUR FÜR DMA-TRANSFERS!: OUT-Transfer wird nicht ausgeführt; z.B. wenn IN-Transfer überschrieben wird (findDataToMove!)

/* HLR additional DEFINES */
#define TASK_DMA_FCTN_PNTR 0x1      //gibt an, dass es sich nur um einen DMA Transfer handelt!

#define TASK_TRF_FINISHED_ADDR   0xAFFEDEAD
#define TASK_EXEC_FINISHED_ADDR  0xAFFEDEA0



/* CoreManager Spin-off --> CoreManager Transfer Unit */
#define CM_TU_PE_START_ADDRESS          0x0B000000
#define CM_TU_PE_END_ADDRESS            0x0B100000


/* IDMA STUFF */
#define IDMA_RAW_SLOT_ID0                   7
#define IDMA_DEBUG_SLOT_ID0                 0
#define IDMA_TRF_FINISHED                   0xAFFE0000
//#define IDMA_OVERALL_SLOT_STATUS            0x60000000  //
#define IDMA_PORT_STATUS                    0x60000008  //DEPRECATED: USE CMD instead!
#define IDMA_CONFIG_ADDR                    0xF0000000
#define IDMA_DATA_PORT_ADDR                 0xF1000000
#define PE_IDMA_CONFIG_ADDRESS_MASK         0x0001FC00  //defines the bits, necessary to identify action
#define PE_IDMA_CONFIG_ADDRESS              0x60018000 // should be blocked! (not in use!)
#define PE_IDMA_DATA_PORT_ADDRESS           0xA001B000 //reserved! not accessible for the core! OLD: 0x6001E000
#define PE_IDMA_CREDIT_RESET_ADDRESS        PE_IDMA_DATA_PORT_ADDRESS
#define PE_IDMA_SLOT_FIFO_ELEM_CNT          0xA001B000    //DEPRECATED
#define PE_IDMA_SLOT_FIFO_GET_ELEM          0xA001C800    //DEPRECATED
#define PE_IDMA_SLOT_FIFO_RELEASE_ELEM      0xA001D000    //DEPRECATED
//#define PE_IDMA_DATA_FIFO_PNTR_ADDRESS      0x6001C000
#define PE_IDMA_PORT_CREDIT_STATUS          0xA001C000  //DEPRECATED //TODO: change address
#define PE_IDMA_SLOT_SIZE_STATUS_ADDRESS    0xA001C400  //DEPRECATED//TODO: change address

#define PE_IDMA_CMD_POS                   3   //OLD: 10
#define PE_IDMA_SLOT_POS                  11  //OLD: 6
#define PE_IDMA_SLOT_TRG_ID_POS           8   //OLD: 3
#define IDMA_CMD_POS                      3// 10
#define IDMA_SLOT_POS                     11// 6 //12 //null wird als position mitgezählt!
#define IDMA_SLOT_TRG_ID_POS              8 //3 //null wird als position mitgezählt!

#define IDMA_SLOT_MASK                    0x7   //number of slots from idma_defines!
#define IDMA_SLOT_WIDTH                   0x3
#define IDMA_SLOT_TRG_ID_MASK             0x7   //number of targets from idma_defines!
#define IDMA_CMD_MASK                     0x1F
#define IDMA_HEADER_REPLY_CREDITS_MASK    0x1FFF    //mask of credits stored in header!
#define HEADER_SIZE_IN_FLITS              2

/*
parameter IDMA_SLOT_ID_WIDTH        = 'd3;
parameter IDMA_SLOT_MODE_WIDTH      = 'd2;
parameter IDMA_SLOT_SIZE_WIDTH      = 'd32;
parameter IDMA_SLOT_REPEAT_WIDTH    = 'd16;
parameter IDMA_SLOT_STRIDE_WIDTH    = 'd16;
parameter IDMA_SLOT_ADDR_WIDTH      = 'd32;
parameter IDMA_SLOT_ADDR_INC_WIDTH  = 'd16;*/
#define IDMA_SLOT_CREDIT_WIDTH            16

#define IDMA_TRF_TYPE_READ                0x1
#define IDMA_TRF_TYPE_WRITE               0x2
#define IDMA_TRF_TYPE_STREAMREAD          0x3
#define IDMA_TRF_TYPE_STREAM_WRITE        0x4

#define OVERALL_SLOT_CFG                              0x0
#define LOCAL_CFG_ADDRESS_FIFO_CMD                    0x1
#define TRANSFER_CFG_SIZE_STRIDE_REPEAT_CMD           0x2
#define FILTER_CFG_MASK1_CMD                          0x3
#define FILTER_CFG_MASK2_CMD                          0x4
#define FILTER_CFG_OPERAND1_CMD                       0x5
#define FILTER_CFG_OPERAND2_CMD                       0x6
#define FILTER_CFG_OPERATION_SIZE_CMD                 0x7
#define FILTER_CFG_TRANSFER_MASK1_CMD                 0x8
#define FILTER_CFG_TRANSFER_MASK2_CMD                 0x9
#define FILTER_CFG_TRANSFER_LABEL1_CMD                0xA
#define FILTER_CFG_TRANSFER_LABEL2_CMD                0xB
#define FILTER_CFG_DISTRMASK1_CMD                     0xC
#define FILTER_CFG_DISTRMASK2_CMD                     0xD
#define EXTERN_CFG_ADDRESS_MODULE_CHIP_CTA_INC_CMD   0xE
#define EXTERN_CFG_SIZE_CREDITS_CMD                  0xF
#define HEADER_CFG_REPLY_LABEL_SLOT_ENABLE_ADDR       0x10
#define FIRE_CMD                                      0x11
#define DEBUG_CMD                                     0x12
#define IDMA_CREDIT_RESPONSE_CMD                      0x14
#define ALLOWED_MEMORY_LOCATION_CMD                   0x15
#define ALLOWED_MODULE_LOCATION_CMD                   0x16
#define IDMA_FIRST_STATUS_CMD                         0x17

#define IDMA_CM_SPINOFF_REG_STATUS                    0x17
#define IDMA_OVERALL_SLOT_STATUS                      0x18
#define IDMA_CREDIT_STATUS                            0x19
#define IDMA_SLOT_FIFO_ELEM_CNT                       0x1A
#define IDMA_SLOT_FIFO_GET_ELEM                       0x1B
#define IDMA_SLOT_FIFO_RELEASE_ELEM                   0x1C
#define IDMA_PORT_CREDIT_STATUS                       0x1D
#define IDMA_SLOT_SIZE_STATUS                         0x1E
/*TODO NEW: (replace old: HEADER_CFG_REPLY_LABEL_SLOT_ENABLE_ADDR)
 #define HEADER_CFG_REPLY_LABEL_SLOT_ENABLE_ADDR       0xD
 #define HEADER_CFG_REPLY_LABEL_SLOT_ENABLE_ADDR       0xD

*/
#define IDMA_CREDIT_COMPARE_EXCHANGE                  0x1B
//#define IDMA_SLOT_TARGET_CFG_COUNT                    0x12




/*
#define OVERALL_SLOT_CFG                              0x0
#define LOCAL_CFG_ADDRESS_FIFO_CMD                    0x1
#define TRANSFER_CFG_SIZE_STRIDE_REPEAT_CMD           0x2
#define FILTER_CFG_MASK1_ADDR                         0x3
#define FILTER_CFG_MASK2_ADDR                         0x4
#define FILTER_CFG_OPERAND1_ADDR                      0x5
#define FILTER_CFG_OPERAND2_ADDR                      0x6
#define FILTER_CFG_OPERATION_SIZE_ADDR                0x7
#define FILTER_CFG_TRANSFER_MASK1_ADDR                0x8
#define FILTER_CFG_TRANSFER_MASK2_ADDR                0x9
#define FILTER_CFG_TRANSFER_LABEL_ADDR                0xA
#define EXTERN_CFG_ADDRESS_MODULE_CHIP_CTA_INC_CMD   0xB
#define EXTERN_CFG_SIZE_CREDITS_CMD                   0xC
#define HEADER_CFG_REPLY_LABEL_SLOT_ENABLE_ADDR       0xD
*/



/*#define ALLOWED_MEMORY_LOCATION_CMD                   0xE
#define ALLOWED_MODULE_LOCATION_CMD                   0xF
#define FIRE_CMD                                      0x10
#define DEBUG_CMD                                     0x11

#define IDMA_CREDIT_RESPONSE_CMD                      0x12
#define IDMA_FIRST_STATUS_CMD                         0x13

#define IDMA_CM_SPINOFF_REG_STATUS                    0x13
#define IDMA_OVERALL_SLOT_STATUS                      0x14
#define IDMA_CREDIT_STATUS                            0x15
#define IDMA_SLOT_FIFO_ELEM_CNT                       0x16
#define IDMA_SLOT_FIFO_GET_ELEM                       0x17
#define IDMA_SLOT_FIFO_RELEASE_ELEM                   0x18
#define IDMA_PORT_CREDIT_STATUS                       0x19
#define IDMA_SLOT_SIZE_STATUS                         0x1A


#define IDMA_CREDIT_COMPARE_EXCHANGE                  0x1B
//#define IDMA_SLOT_TARGET_CFG_COUNT                    0x12
*/







//#define PE_IDMA_OVERALL_SLOT_STATUS_ADDRESS 0x6001E200
#define PE_IDMA_PORT_STATUS_ADDRESS         0x6001F208










#define MAX_TASK_DATA_COUNT_PE 12
#define MAX_TASK_DATA_COUNT_CM 8



 /* transferType:
    IN          0x1
    OUT         0x2
    INOUT       0x3
    IN_2D       0x4
    OUT_2D      0x8
    INOUT_2D    0xC
 */


struct appTransfer
{
    unsigned pointer;
    unsigned size_moduleId_type;            // 16 8 8
    unsigned stride_lineCount;  // 16 16
    unsigned dummy;
}; // 16 bytes


struct appTaskDesc
{
    unsigned taskName_peTypes;                      // 7! 16
    unsigned depCheckValue_depCheckMode_dataCount;  // 16 8 8
    unsigned taskId;
    unsigned dummy2;
    struct appTransfer transfers[MAX_TASK_DATA_COUNT_CM];
}; // size: 4*4 + 12*16 = 208 bytes


struct startUpCodeTransfer
{
    unsigned isPrefetch_transferType_localPtr;  // 4 4 24
    unsigned globalChipId_globalModuleId_size;  // 8 8 16
    unsigned globalPtr;                         // 32
    unsigned stride_lineCount;                  // 16 16
}; // size: 16 bytes


struct startUpCodeTask
{
    struct startUpCodeTransfer transfers[MAX_TASK_DATA_COUNT_PE];
    unsigned soDataCount_peData_Count; // 16 16
    unsigned localInstPtr;
}; // size: 12*16 + 8 = 200 bytes = C8
