/*
 * Copyright (c) 1999-2001, Intel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Intel Corporation nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _E100_INC_
#define _E100_INC_

#ifdef IANS
#include "ans_driv.h"
#endif

/* /proc definitions */
#define ADAPTERS_PROC_DIR         "PRO_LAN_Adapters"

#define DESCRIPTION_TAG           "Description"
#define DRVR_NAME_TAG             "Driver_Name"
#define DRVR_VERSION_TAG          "Driver_Version"
#define PCI_VENDOR_TAG            "PCI_Vendor"
#define PCI_DEVICE_ID_TAG         "PCI_Device_ID"
#define PCI_SUBSYSTEM_VENDOR_TAG  "PCI_Subsystem_Vendor"
#define PCI_SUBSYSTEM_ID_TAG      "PCI_Subsystem_ID"
#define PCI_REVISION_ID_TAG       "PCI_Revision_ID"
#define PCI_BUS_TAG               "PCI_Bus"
#define PCI_SLOT_TAG              "PCI_Slot"
#define IRQ_TAG                   "IRQ"
#define SYSTEM_DEVICE_NAME_TAG    "System_Device_Name"
#define CURRENT_HWADDR_TAG        "Current_HWaddr"
#define PERMANENT_HWADDR_TAG      "Permanent_HWaddr"
#define PART_NUMBER_TAG           "Part_Number"

#define LINK_TAG                  "Link"
#define SPEED_TAG                 "Speed"
#define DUPLEX_TAG                "Duplex"
#define STATE_TAG                 "State"

#define RX_PACKETS_TAG            "Rx_Packets"
#define TX_PACKETS_TAG            "Tx_Packets"
#define RX_BYTES_TAG              "Rx_Bytes"
#define TX_BYTES_TAG              "Tx_Bytes"
#define RX_ERRORS_TAG             "Rx_Errors"
#define TX_ERRORS_TAG             "Tx_Errors"
#define RX_DROPPED_TAG            "Rx_Dropped"
#define TX_DROPPED_TAG            "Tx_Dropped"
#define MULTICAST_TAG             "Multicast"
#define COLLISIONS_TAG            "Collisions"
#define RX_LENGTH_ERRORS_TAG      "Rx_Length_Errors"
#define RX_OVER_ERRORS_TAG        "Rx_Over_Errors"
#define RX_CRC_ERRORS_TAG         "Rx_CRC_Errors"
#define RX_FRAME_ERRORS_TAG       "Rx_Frame_Errors"
#define RX_FIFO_ERRORS_TAG        "Rx_FIFO_Errors"
#define RX_MISSED_ERRORS_TAG      "Rx_Missed_Errors"
#define TX_ABORTED_ERRORS_TAG     "Tx_Aborted_Errors"
#define TX_CARRIER_ERRORS_TAG     "Tx_Carrier_Errors"
#define TX_FIFO_ERRORS_TAG        "Tx_FIFO_Errors"
#define TX_HEARTBEAT_ERRORS_TAG   "Tx_Heartbeat_Errors"
#define TX_WINDOW_ERRORS_TAG      "Tx_Window_Errors"

#define RX_TCP_CHECKSUM_GOOD_TAG  "Rx_TCP_Checksum_Good"
#define RX_TCP_CHECKSUM_BAD_TAG   "Rx_TCP_Checksum_Bad"
#define TX_TCP_CHECKSUM_GOOD_TAG  "Tx_TCP_Checksum_Good"
#define TX_TCP_CHECKSUM_BAD_TAG   "Tx_TCP_Checksum_Bad"

#define TX_ABORT_LATE_COLL_TAG    "Tx_Abort_Late_Coll"
#define TX_DEFERRED_OK_TAG        "Tx_Deferred_Ok"
#define TX_SINGLE_COLL_OK_TAG     "Tx_Single_Coll_Ok"
#define TX_MULTI_COLL_OK_TAG      "Tx_Multi_Coll_Ok"
#define RX_ALIGN_ERRORS_TAG       "Rx_Align_Errors"
#define RX_LONG_LENGTH_ERRORS_TAG "Rx_Long_Length_Errors"


/* E100 compile time configurable parameters */

#define IFNAME   "e100"

/*
 *  Configure parameters for buffers per controller.
 *  If the machine this is being used on is a faster machine (i.e. > 150MHz)
 *  and running on a 10MBS network then more queueing of data occurs. This
 *  may indicate the some of the numbers below should be adjusted.  Here are
 *  some typical numbers:
 *                             MAX_TCB 64
 *                             MAX_RFD 64
 *  The default numbers give work well on most systems tests so no real
 *  adjustments really need to take place.  Also, if the machine is connected
 *  to a 100MBS network the numbers described above can be lowered from the
 *  defaults as considerably less data will be queued.
 */

#define MAX_TCB        64       /* number of transmit control blocks */
#define MAX_TBD        MAX_TCB
#define TX_FRAME_CNT   7        /* consecutive transmit frames per interrupt */

/* TX_FRAME_CNT must be less than MAX_TCB
 */
#define MAX_RFD      64

#define E100_DEFAULT_TCB   MAX_TCB
#define E100_MIN_TCB       (2*TX_FRAME_CNT+3) /* make room for at least 2 interrupts */
#define E100_MAX_TCB       1024
#define E100_DEFAULT_RFD   MAX_RFD
#define E100_MIN_RFD       8
#define E100_MAX_RFD       1024
#define E100_DEFAULT_XSUM  TRUE

#define TX_CONG_DFLT   0        /* congestion enable flag for National TX Phy */
#define TX_FIFO_LMT    0
#define RX_FIFO_LMT    8
#define TX_DMA_CNT     0
#define RX_DMA_CNT     0
#define TX_UR_RETRY    3
#define TX_THRSHLD     8

/* These are special #defs and are NOT TO BE TOUCHED!
 */
#define DEFAULT_TX_FIFO_LIMIT      0x00
#define DEFAULT_RX_FIFO_LIMIT      0x08
#define CB_557_CFIG_DEFAULT_PARM4  0x00
#define CB_557_CFIG_DEFAULT_PARM5  0x00
#define DEFAULT_TRANSMIT_THRESHOLD 12 /* 12 -> 96 bytes */
#define ADAPTIVE_IFS               0

/* Note: The 2 parameters below might affect NFS performance. If there are
 * problems with NFS, set CPU_CYCLE_SAVER to 0x0 and CFG_BYTE_PARM6 to 0x32.
 */

/* CPU_CYCLE_SAVER: Used for receive interrupt bundling microcode. 
 * Valid values are from 0 to 0xc000. Setting it to 0 disables the feature.
 */
#define CPU_CYCLE_SAVER  0x0600

/* CFG_BYTE_PARM6: Determines whether the card generates CNA interrupts
 * or not. 
 * 0x32 configures the card to generate CNA interrupts
 * 0x3a configures the card not to generate CNA interrupts
 */
#define CFG_BYTE_PARM6  0x32

/* CPUSAVER_BUNDLE_MAX: Sets the maximum number of frames that will be bundled.
 * In some situations, such as the TCP windowing algorithm, it may be
 * better to limit the growth of the bundle size than let it go as
 * high as it can, because that could cause too much added latency.
 * The default is six, because this is the number of packets in the
 * default TCP window size.  A value of 1 would make CPUSaver indicate
 * an interrupt for every frame received.  If you do not want to put
 * a limit on the bundle size, set this value to xFFFF.
 */
#define CPUSAVER_BUNDLE_MAX  0x0006

/* end of configurables */

/*****************************************************************************
 **
 **  PCI bus changes
 **
 *****************************************************************************/

/* Dynamic DMA maping
 * Instead of using virt_to_bus, bus mastering PCI drivers should use the DMA 
 * mapping API to get bus addresses.  This lets some platforms use dynamic 
 * mapping to use PCI devices that do not support DAC in a 64-bit address space
 */
#define pci_dma_supported(dev, addr_mask)                    (1)
#define pci_free_consistent(dev, size, cpu_addr, dma_handle) kfree(cpu_addr)
#define pci_map_single(dev, addr, size, direction)           virt_to_bus(addr)
#define pci_unmap_single(dev, dma_handle, size, direction)   do{} while(0)


/*****************************************************************************
 **
 **  Network Device API changes
 **
 *****************************************************************************/

typedef struct device device_t;

/* 'Softnet' network stack changes merged in 2.3.43 
 * these are 2.2 compatible defines for the new network interface API
 * 2.3.47 added some more inline functions for softnet to remove explicit 
 * bit tests in drivers
 */
#define netif_start_queue(dev)   ( clear_bit(0, &(dev)->tx_busy) )
#define netif_stop_queue(dev)    ( set_bit(0, &(dev)->tx_busy) )
#define netif_wake_queue(dev)    { clear_bit(0, &(dev)->tx_busy); \
                                   mark_bh(NET_BH); }
#define netif_running(dev)       ( test_bit(0, &(dev)->start) )
#define netif_queue_stopped(dev) ( test_bit(0, &(dev)->tx_busy) )

/* ====================================================================== */
/*                                hw                                      */
/* ====================================================================== */

/* watchdog invocation intervals */
#define E100_SRV_BUSY   40
#define E100_SRV_LAZY   1000
#define RX_SRV_COUNT    RxDescriptors

/* timeout for command completion */
#define E100_CMD_WAIT   100     /* iterations */

/* Typedefs */
typedef unsigned long paddr_t;
typedef unsigned char boolean_t;

/* this is where we store the results of the DUMP & RESET statitics cmd */
typedef struct _err_stats_t {
  DWORD gd_xmits;
  DWORD gd_recvs;
  DWORD tx_abrt_xs_col;
  DWORD tx_late_col;
  DWORD tx_dma_urun;
  DWORD tx_lost_csrs;
  DWORD tx_ok_defrd;
  DWORD tx_one_retry;
  DWORD tx_mt_one_retry;
  DWORD tx_tot_retries;
  DWORD rcv_crc_err;
  DWORD rcv_align_err;
  DWORD rcv_rsrc_err;
  DWORD rcv_dma_orun;
  DWORD rcv_cdt_frames;
  DWORD rcv_runts;
  DWORD num_gd_xmts;            /* # of last good tx frames since the last */
} err_stats_t, *perr_stats_t;   /* adjustment of the TX THRESHOLD */

#define STATIC static

#define B_TRUE          1
#define B_FALSE         0
#define TRUE            1
#define FALSE           0

typedef struct pci_dev pci_dev_t;
typedef struct sk_buff sk_buff_t;
typedef struct net_device_stats net_device_stats_t;

/* Flags for hardware identification and status
 */
#define USE_IPCB              0x20 /* set if using ipcb for transmits */
#define IS_BACHELOR           0x40 /* set if 82558 or newer board */
#define CU_ACTIVE_TOOLONG     0x80
#define INVALID_SPEED_DPLX    0x100
#define PRINT_SPEED_DPLX      0x200
#define BOARD_IN_SERVICE      0x400
#define IS_ICH                0x800
#define IS_82562EH            0x1000

/* Changed for 82558 and 82559 enhancements
 * defines for 82558/9 flow control configure paramters
 */
#define DFLT_FC_DELAY_LSB  0x1f /* Delay for outgoing Pause frames */
#define DFLT_FC_DELAY_MSB  0x01 /* Delay for outgoing Pause frames */

/* defines for 82558/9 flow control CSR values
 */
#define DFLT_FC_THLD       0x00 /* Rx FIFO threshold of 0.5KB free  */
#define DFLT_FC_CMD        0x00 /* FC Command in CSR */

/* ====================================================================== */
/*                              equates                                   */
/* ====================================================================== */

/*
 * These are general purpose defines 
 *
 * Bit Mask definitions
 */
#define BIT_0       0x0001
#define BIT_1       0x0002
#define BIT_2       0x0004
#define BIT_3       0x0008
#define BIT_4       0x0010
#define BIT_5       0x0020
#define BIT_6       0x0040
#define BIT_7       0x0080
#define BIT_8       0x0100
#define BIT_9       0x0200
#define BIT_10      0x0400
#define BIT_11      0x0800
#define BIT_12      0x1000
#define BIT_13      0x2000
#define BIT_14      0x4000
#define BIT_15      0x8000
#define BIT_28      0x10000000

#define BIT_0_2     0x0007
#define BIT_0_3     0x000F
#define BIT_0_4     0x001F
#define BIT_0_5     0x003F
#define BIT_0_6     0x007F
#define BIT_0_7     0x00FF
#define BIT_0_8     0x01FF
#define BIT_0_13    0x3FFF
#define BIT_0_15    0xFFFF
#define BIT_1_2     0x0006
#define BIT_1_3     0x000E
#define BIT_2_5     0x003C
#define BIT_3_4     0x0018
#define BIT_4_5     0x0030
#define BIT_4_6     0x0070
#define BIT_4_7     0x00F0
#define BIT_5_7     0x00E0
#define BIT_5_12    0x1FE0
#define BIT_5_15    0xFFE0
#define BIT_6_7     0x00c0
#define BIT_7_11    0x0F80
#define BIT_8_10    0x0700
#define BIT_9_13    0x3E00
#define BIT_12_15   0xF000
#define BIT_8_15    0xFF00

#define BIT_16_20   0x001F0000
#define BIT_21_25   0x03E00000
#define BIT_26_27   0x0C000000

/*- Miscellaneous Equates */
#define CR      0x0D            /* Carriage Return */
#define LF      0x0A            /* Line Feed */

#define TX_OK          1
#define E100_NULL ((DWORD)0xffffffff)

/* OEM Message Tags */
#define stringTag   0xFEFA      /* Length Byte After String */
#define lStringTag  0xFEFB      /* Length Byte Before String */
#define zStringTag  0xFEFC      /* Zero-Terminated String Tag */
#define nStringTag  0xFEFD      /* No Length Byte Or 0-Term */

/* Phy related constants */
#define PHY_503                 0
#define PHY_100_A               0x000003E0
#define PHY_100_C               0x035002A8
#define PHY_NSC_TX              0x5c002000
#define PHY_82562ET             0x033002A8
#define PHY_82562EM             0x032002A8
#define PHY_82562EH             0x017002A8
#define PHY_82555_TX            0x015002a8 /* added this for 82555 */
#define PHY_OTHER               0xFFFF
#define MAX_PHY_ADDR            31

#define PHY_MODEL_REV_ID_MASK   0xFFF0FFFF
#define PARALLEL_DETECT         0
#define N_WAY                   1

/* Transmit Threshold related constants */
#define DEFAULT_TX_PER_UNDERRUN         20000

/* Ethernet Frame Sizes */
#define ETHERNET_ADDRESS_LENGTH         6
#define ETHERNET_HEADER_SIZE            14
#define MINIMUM_ETHERNET_PACKET_SIZE    60
#define MAXIMUM_ETHERNET_PACKET_SIZE    1514
#define SIZEOF_COALESCE_BUFF            1536

#define MAX_MULTICAST_ADDRS             64
#define TCB_BUFFER_SIZE                 64

#define RCB_BUFFER_SIZE                 MAXIMUM_ETHERNET_PACKET_SIZE

/*- Area reserved for all Non Transmit command blocks */
#define MAX_NON_TX_CB_AREA              512

/* driver constants */
#define MAX_PHYS_DESC                   16

#define DUMP_STATS_TIMEOUT              500
#define FULL_DUPLEX      2
#define HALF_DUPLEX      1
#define NO_DUPLEX        0

/*
 * These defines are specific to the 82557 
 */

/* E100 PORT functions -- lower 4 bits */
#define PORT_SOFTWARE_RESET         0
#define PORT_SELFTEST               1
#define PORT_SELECTIVE_RESET        2
#define PORT_DUMP                   3 

/* CSR field definitions -- Offsets from CSR base */
#define SCB_STATUS_LOW_BYTE         0x0
#define SCB_STATUS_HIGH_BYTE        0x1
#define SCB_COMMAND_LOW_BYTE        0x2
#define SCB_COMMAND_HIGH_BYTE       0x3
#define SCB_GENERAL_POINTER         0x4
#define CSR_PORT_LOW_WORD           0x8
#define CSR_PORT_HIGH_WORD          0x0a
#define CSR_FLASH_CONTROL_REG       0x0c
#define CSR_EEPROM_CONTROL_REG      0x0e
#define CSR_MDI_CONTROL_LOW_WORD    0x10
#define CSR_MDI_CONTROL_HIGH_WORD   0x12

/* Changed for 82558 enhancements */
/* define 82558/9 fields */
#define CSR_RX_DMA_BYTE_COUNT       0x14
#define CSR_EARLY_RX_INT            0x18
#define CSR_FC_THRESHOLD            0x19
#define CSR_FC_XON_XOFF             0x1a
#define CSR_POWER_MGMT_REG          0x1b

/* SCB Status Word bit definitions */
/* Interrupt status/ack fields */
/* ER and FCP interrupts for 82558 masks  */
#define SCB_STATUS_ACK_MASK        BIT_8_15 /* Status Mask */
#define SCB_STATUS_ACK_CX          BIT_15 /* CU Completed Action Cmd */
#define SCB_STATUS_ACK_FR          BIT_14 /* RU Received A Frame */
#define SCB_STATUS_ACK_CNA         BIT_13 /* CU Became Inactive (IDLE) */
#define SCB_STATUS_ACK_RNR         BIT_12 /* RU Became Not Ready */
#define SCB_STATUS_ACK_MDI         BIT_11 /* MDI read or write done */
#define SCB_STATUS_ACK_SWI         BIT_10 /* S/W generated interrupt */
#define SCB_STATUS_ACK_ER          BIT_9 /* Early Receive */
#define SCB_STATUS_ACK_FCP         BIT_8 /* Flow Control Pause */

/*- CUS Fields */
#define SCB_CUS_MASK            (BIT_6 | BIT_7) /* CUS 2-bit Mask */
#define SCB_CUS_IDLE            0 /* CU Idle */
#define SCB_CUS_SUSPEND         BIT_6 /* CU Suspended */
#define SCB_CUS_ACTIVE          BIT_7 /* CU Active */

/*- RUS Fields */
#define SCB_RUS_IDLE            0 /* RU Idle */
#define SCB_RUS_MASK            BIT_2_5 /* RUS 3-bit Mask */
#define SCB_RUS_SUSPEND         BIT_2 /* RU Suspended */
#define SCB_RUS_NO_RESOURCES    BIT_3 /* RU Out Of Resources */
#define SCB_RUS_READY           BIT_4 /* RU Ready */
#define SCB_RUS_SUSP_NO_RBDS    (BIT_2 | BIT_5) /* RU No More RBDs */
#define SCB_RUS_NO_RBDS         (BIT_3 | BIT_5) /* RU No More RBDs */
#define SCB_RUS_READY_NO_RBDS   (BIT_4 | BIT_5) /* RU Ready, No RBDs */


/* SCB Command Word bit definitions */
/*- CUC fields */
/* Changing mask to 4 bits */
#define SCB_CUC_MASK            BIT_4_7 /* CUC 4-bit Mask */
#define SCB_CUC_NOOP            0
#define SCB_CUC_START           BIT_4 /* CU Start */
#define SCB_CUC_RESUME          BIT_5 /* CU Resume */

/* Changed for 82558 enhancements */
#define SCB_CUC_STATIC_RESUME   (BIT_5 | BIT_7) /* 82558/9 Static Resume */
#define SCB_CUC_DUMP_ADDR       BIT_6 /* CU Dump Counters Address */
#define SCB_CUC_DUMP_STAT       (BIT_4 | BIT_6) /* CU Dump stat. counters */
#define SCB_CUC_LOAD_BASE       (BIT_5 | BIT_6) /* Load the CU base */
#define SCB_CUC_DUMP_RST_STAT   BIT_4_6 /* CU Dump & reset statistics cntrs */

/*- RUC fields */
#define SCB_RUC_MASK            BIT_0_2 /* RUC 3-bit Mask */
#define SCB_RUC_START           BIT_0 /* RU Start */
#define SCB_RUC_RESUME          BIT_1 /* RU Resume */
#define SCB_RUC_ABORT           BIT_2 /* RU Abort */
#define SCB_RUC_LOAD_HDS        (BIT_0 | BIT_2) /* Load RFD Header Data Size */
#define SCB_RUC_LOAD_BASE       (BIT_1 | BIT_2) /* Load the RU base */
#define SCB_RUC_RBD_RESUME      BIT_0_2 /* RBD resume */

/* Interrupt fields (assuming byte addressing) */
#define SCB_INT_MASK            BIT_0 /* Mask interrupts */
#define SCB_SOFT_INT            BIT_1 /* Generate a S/W interrupt */

/*  Specific Interrupt Mask Bits (upper byte of SCB Command word) */
#define SCB_FCP_INT_MASK        BIT_2 /* Flow Control Pause */
#define SCB_ER_INT_MASK         BIT_3 /* Early Receive */
#define SCB_RNR_INT_MASK        BIT_4 /* RU Not Ready */
#define SCB_CNA_INT_MASK        BIT_5 /* CU Not Active */
#define SCB_FR_INT_MASK         BIT_6 /* Frame Received */
#define SCB_CX_INT_MASK         BIT_7 /* CU eXecution w/ I-bit done */
#define SCB_BACHELOR_INT_MASK   BIT_2_7 /* 82558 interrupt mask bits */

#define SCB_GCR2_EEPROM_ACCESS_SEMAPHORE BIT_7

/* EEPROM bit definitions */
/*- EEPROM control register bits */
#define EN_TRNF          0x10   /* Enable turnoff */
#define EEDO             0x08   /* EEPROM data out */
#define EEDI             0x04   /* EEPROM data in (set for writing data) */
#define EECS             0x02   /* EEPROM chip select (1=hi, 0=lo) */
#define EESK             0x01   /* EEPROM shift clock (1=hi, 0=lo) */

/*- EEPROM opcodes */
#define EEPROM_READ_OPCODE          06
#define EEPROM_WRITE_OPCODE         05
#define EEPROM_ERASE_OPCODE         07
#define EEPROM_EWEN_OPCODE          19 /* Erase/write enable */
#define EEPROM_EWDS_OPCODE          16 /* Erase/write disable */

/*- EEPROM data locations */
#define EEPROM_NODE_ADDRESS_BYTE_0      0
#define EEPROM_COMPATIBILITY_WORD       3
#define EEPROM_PWA_NO                   8
#define EEPROM_SUBSYSTEM_ID_WORD        0x0b
#define EEPROM_SUBSYSTEM_VENDOR_WORD    0x0c

#define E100_WRITE_REG(reg, value) {bddp->scbp->reg = value; }
#define E100_READ_REG(reg) bddp->scbp->reg

#define EEPROM_CTRL scb_eprm_cntrl
#define EEPROM_CHECKSUM_REG             0x3f
#define EEPROM_SUM                      0xbaba


/* MDI Control register bit definitions */
#define MDI_DATA_MASK       BIT_0_15 /* MDI Data port */
#define MDI_REG_ADDR        BIT_16_20 /* which MDI register to read/write */
#define MDI_PHY_ADDR        BIT_21_25 /* which PHY to read/write */
#define MDI_PHY_OPCODE      BIT_26_27 /* which PHY to read/write */
#define MDI_PHY_READY       BIT_28 /* PHY is ready for next MDI cycle */
#define MDI_PHY_INT_ENABLE  BIT_29 /* Assert INT at MDI cycle compltion */


/* MDI Control register opcode definitions */
#define MDI_WRITE       1       /* Phy Writ */
#define MDI_READ        2       /* Phy read */

// Zero Locking Algorithm definitions:
#define ZLOCK_ZERO_MASK		0x00F0
#define ZLOCK_MAX_READS		50
#define ZLOCK_SET_ZERO		0x2010
#define ZLOCK_MAX_SLEEP		300 * HZ
#define ZLOCK_MAX_ERRORS	300

/* E100 Action Commands */
#define CB_NOP                  0
#define CB_IA_ADDRESS           1
#define CB_CONFIGURE            2
#define CB_MULTICAST            3
#define CB_TRANSMIT             4
#define CB_LOAD_MICROCODE       5
#define CB_DUMP                 6
#define CB_DIAGNOSE             7

/* the following are dummy commands to maintain the CU state */
#define CB_NULL                 8
#define CB_TRANSMIT_FIRST       9
#define CB_DUMP_RST_STAT        10

#define CB_IPCB_TRANSMIT                9


/* Command Block (CB) Field Definitions */
/*- CB Command Word */
#define CB_EL_BIT           BIT_15 /* CB EL Bit */
#define CB_S_BIT            BIT_14 /* CB Suspend Bit */
#define CB_I_BIT            BIT_13 /* CB Interrupt Bit */
#define CB_TX_SF_BIT        BIT_3 /* TX CB Flexible Mode */
#define CB_CMD_MASK         BIT_0_2 /* CB 3-bit CMD Mask */

/*- CB Status Word */
#define CB_STATUS_MASK          BIT_12_15 /* CB Status Mask (4-bits) */
#define CB_STATUS_COMPLETE      BIT_15 /* CB Complete Bit */
#define CB_STATUS_OK            BIT_13 /* CB OK Bit */
#define CB_STATUS_UNDERRUN      BIT_12 /* CB A Bit */
#define CB_STATUS_FAIL          BIT_11 /* CB Fail (F) Bit */

/*misc command bits */
#define CB_TX_EOF_BIT           BIT_15 /* TX CB/TBD EOF Bit */


/* Config CB Parameter Fields */
#define CB_CFIG_BYTE_COUNT          22 /* 22 config bytes */
#define CB_CFIG_D102_BYTE_COUNT    10

/* byte 0 bit definitions */
#define CB_CFIG_BYTE_COUNT_MASK     BIT_0_5 /* Byte count occupies bit 5-0 */

/* byte 1 bit definitions */
#define CB_CFIG_RXFIFO_LIMIT_MASK   BIT_0_4 /* RxFifo limit mask */
#define CB_CFIG_TXFIFO_LIMIT_MASK   BIT_4_7 /* TxFifo limit mask */

/* byte 2 bit definitions -- ADAPTIVE_IFS */

/* word 3 bit definitions -- RESERVED */
/* Changed for 82558 enhancements */
/* byte 3 bit definitions */
#define CB_CFIG_MWI_EN      BIT_0 /* Enable MWI on PCI bus */
#define CB_CFIG_TYPE_EN     BIT_1 /* Type Enable */
#define CB_CFIG_READAL_EN   BIT_2 /* Enable Read Align */
#define CB_CFIG_TERMCL_EN   BIT_3 /* Cache line write  */

/* byte 4 bit definitions */
#define CB_CFIG_RX_MIN_DMA_MASK     BIT_0_6 /* Rx minimum DMA count mask */

/* byte 5 bit definitions */
#define CB_CFIG_TX_MIN_DMA_MASK BIT_0_6 /* Tx minimum DMA count mask */
#define CB_CFIG_DMBC_EN         BIT_7 /* Enable Tx/Rx min. DMA counts */

/* Changed for 82558 enhancements */
/* byte 6 bit definitions */
#define CB_CFIG_LATE_SCB           BIT_0 /* Update SCB After New Tx Start */
#define CB_CFIG_DIRECT_DMA_DIS     BIT_1 /* Direct DMA mode */
#define CB_CFIG_TNO_INT            BIT_2 /* Tx Not OK Interrupt */
#define CB_CFIG_CI_INT             BIT_3 /* Command Complete Interrupt */
#define CB_CFIG_EXT_TCB_DIS        BIT_4 /* Extended TCB */
#define CB_CFIG_EXT_STAT_DIS       BIT_5 /* Extended Stats */
#define CB_CFIG_SAVE_BAD_FRAMES    BIT_7 /* Save Bad Frames Enabled */

/* byte 7 bit definitions */
#define CB_CFIG_DISC_SHORT_FRAMES   BIT_0 /* Discard Short Frames */
#define CB_CFIG_URUN_RETRY          BIT_1_2 /* Underrun Retry Count */
#define CB_CFIG_DYNTBD_EN           BIT_7 /* Enable dynamic TBD */
/* Enable extended RFD's on D102 */
#define CB_CFIG_EXTENDED_RFD        BIT_5

/* byte 8 bit definitions */
#define CB_CFIG_503_MII             BIT_0 /* 503 vs. MII mode */

/* byte 9 bit definitions -- pre-defined all zeros */

/* byte 10 bit definitions */
#define CB_CFIG_NO_SRCADR       BIT_3 /* No Source Address Insertion */
#define CB_CFIG_PREAMBLE_LEN    BIT_4_5 /* Preamble Length */
#define CB_CFIG_LOOPBACK_MODE   BIT_6_7 /* Loopback Mode */

/* byte 11 bit definitions */
#define CB_CFIG_LINEAR_PRIORITY     BIT_0_2 /* Linear Priority */

/* byte 12 bit definitions */
#define CB_CFIG_LINEAR_PRI_MODE     BIT_0 /* Linear Priority mode */
#define CB_CFIG_IFS_MASK            BIT_4_7 /* Interframe Spacing mask */

/* byte 13 bit definitions -- pre-defined all zeros */

/* byte 14 bit definitions -- pre-defined 0xf2 */

/* byte 15 bit definitions */
#define CB_CFIG_PROMISCUOUS         BIT_0 /* Promiscuous Mode Enable */
#define CB_CFIG_BROADCAST_DIS       BIT_1 /* Broadcast Mode Disable */
#define CB_CFIG_CRS_OR_CDT          BIT_7 /* CRS Or CDT */

/* byte 16 bit definitions -- pre-defined all zeros */

/* byte 17 bit definitions -- pre-defined 0x40 */

/* byte 18 bit definitions */
#define CB_CFIG_STRIPPING           BIT_0 /* Padding Disabled */
#define CB_CFIG_PADDING             BIT_1 /* Padding Disabled */
#define CB_CFIG_CRC_IN_MEM          BIT_2 /* Transfer CRC To Memory */

/* byte 19 bit definitions */
#define CB_CFIG_TX_ADDR_WAKE        BIT_0 /* Address Wakeup */
#define CB_CFIG_TX_MAGPAK_WAKE      BIT_1 /* Magic Packet Wakeup */

/* Changed TX_FC_EN to TX_FC_DIS because 0 enables, 1 disables. Jul 8, 1999 */
#define CB_CFIG_TX_FC_DIS           BIT_2 /* Tx Flow Control Disable */
#define CB_CFIG_FC_RESTOP           BIT_3 /* Rx Flow Control Restop */
#define CB_CFIG_FC_RESTART          BIT_4 /* Rx Flow Control Restart */
#define CB_CFIG_REJECT_FC           BIT_5 /* Rx Flow Control Restart */
/* end 82558/9 specifics */

#define CB_CFIG_FORCE_FDX           BIT_6 /* Force Full Duplex */
#define CB_CFIG_FDX_ENABLE          BIT_7 /* Full Duplex Enabled */

/* byte 20 bit definitions */
#define CB_CFIG_MULTI_IA            BIT_6 /* Multiple IA Addr */

/* byte 21 bit definitions */
#define CB_CFIG_MULTICAST_ALL       BIT_3 /* Multicast All */

/* byte 22 bit defines */
#define CB_CFIG_RECEIVE_GAMLA_MODE  BIT_0 /* D102 receive mode */
#define CB_CFIG_VLAN_DROP_ENABLE    BIT_1 /* vlan stripping */
 

/* Receive Frame Descriptor Fields */

/*- RFD Status Bits */
#define RFD_RECEIVE_COLLISION   BIT_0 /* Collision detected on Receive */
#define RFD_IA_MATCH            BIT_1 /* Indv Address Match Bit */
#define RFD_RX_ERR              BIT_4 /* RX_ERR pin on Phy was set */
#define RFD_FRAME_TOO_SHORT     BIT_7 /* Receive Frame Short */
#define RFD_DMA_OVERRUN         BIT_8 /* Receive DMA Overrun */
#define RFD_NO_RESOURCES        BIT_9 /* No Buffer Space */
#define RFD_ALIGNMENT_ERROR     BIT_10 /* Alignment Error */
#define RFD_CRC_ERROR           BIT_11 /* CRC Error */
#define RFD_STATUS_OK           BIT_13 /* RFD OK Bit */
#define RFD_STATUS_COMPLETE     BIT_15 /* RFD Complete Bit */

/*- RFD Command Bits*/
#define RFD_EL_BIT      BIT_15  /* RFD EL Bit */
#define RFD_S_BIT       BIT_14  /* RFD Suspend Bit */
#define RFD_H_BIT       BIT_4   /* Header RFD Bit */
#define RFD_SF_BIT      BIT_3   /* RFD Flexible Mode */

/*- RFD misc bits*/
#define RFD_EOF_BIT         BIT_15 /* RFD End-Of-Frame Bit */
#define RFD_F_BIT           BIT_14 /* RFD Buffer Fetch Bit */
#define RFD_ACT_COUNT_MASK  BIT_0_13 /* RFD Actual Count Mask */


/* Receive Buffer Descriptor Fields */
#define RBD_EOF_BIT             BIT_15 /* RBD End-Of-Frame Bit */
#define RBD_F_BIT               BIT_14 /* RBD Buffer Fetch Bit */
#define RBD_ACT_COUNT_MASK      BIT_0_13 /* RBD Actual Count Mask */

#define SIZE_FIELD_MASK     BIT_0_13 /* Size of the associated buffer */
#define RBD_EL_BIT          BIT_15 /* RBD EL Bit */


#define DUMP_BUFFER_SIZE            600 /* size of the dump buffer */

 
/* Self Test Results */
#define CB_SELFTEST_FAIL_BIT        BIT_12
#define CB_SELFTEST_DIAG_BIT        BIT_5
#define CB_SELFTEST_REGISTER_BIT    BIT_3
#define CB_SELFTEST_ROM_BIT         BIT_2

#define CB_SELFTEST_ERROR_MASK  \
         ( CB_SELFTEST_FAIL_BIT | CB_SELFTEST_DIAG_BIT | \
           CB_SELFTEST_REGISTER_BIT | CB_SELFTEST_ROM_BIT )


/*-------------------------------------------------------------------------
 * Driver Configuration Default Parameters for the 557
 *  Note: If the driver uses any defaults that are different from the chip's
 *        defaults, it will be noted below
 *-------------------------------------------------------------------------
 */
/* Byte 0 (byte count) default */
#define CB_557_CFIG_DEFAULT_PARM0   CB_CFIG_BYTE_COUNT

/* Byte 1 (fifo limits) default ( bit 7 is always set ) */
#define CB_557_CFIG_DEFAULT_PARM1   0x88

/* Byte 2 (adaptive IFS) default */
#define CB_557_CFIG_DEFAULT_PARM2   0x00

/* Byte 3 (reserved) default */
#define CB_557_CFIG_DEFAULT_PARM3   0x00

/* Byte 4 (Rx DMA min count) default */
#define CB_557_CFIG_DEFAULT_PARM4   0x00

/* Byte 5 (Tx DMA min count, DMA min count enable) default */
#define CB_557_CFIG_DEFAULT_PARM5   0x00

/* Byte 6 (No Late SCB, CNA int) default */
#define CB_557_CFIG_DEFAULT_PARM6   0x32

/* Byte 7 (Discard short frames, underrun retry) default */
/*          note: disc short frames will be enabled */
#define DEFAULT_UNDERRUN_RETRY      0x01
#define CB_557_CFIG_DEFAULT_PARM7   0x03

/* Byte 8 (MII or 503) default */
/*          note: MII will be the default */
#define CB_557_CFIG_DEFAULT_PARM8   0x01

/* Byte 9 (reserved) default */
#define CB_557_CFIG_DEFAULT_PARM9   0x00

/* Byte 10 (scr addr insertion, preamble, loopback) default */
#define CB_557_CFIG_DEFAULT_PARM10  0x2e

/* Byte 11 (linear priority) default */
#define CB_557_CFIG_DEFAULT_PARM11  0x00

/* Byte 12 (IFS,linear priority mode) default */
#define CB_557_CFIG_DEFAULT_PARM12  0x60

/* Byte 13 (reserved) default */
#define CB_557_CFIG_DEFAULT_PARM13  0x00

/* Byte 14 (reserved) default */
#define CB_557_CFIG_DEFAULT_PARM14  0xf2

/* Byte 15 (broadcast, CRS/CDT) default */
#define CB_557_CFIG_DEFAULT_PARM15  0xc8

/* Byte 16 (reserved) default */
#define CB_557_CFIG_DEFAULT_PARM16  0x00

/* Byte 17 (reserved) default */
#define CB_557_CFIG_DEFAULT_PARM17  0x40

/* Byte 18 (Stripping, padding, Rcv CRC in mem) default */
/*          note: padding is enabled with 0xf2, disabled if 0xf0 */
#define CB_557_CFIG_DEFAULT_PARM18  0xf2

/* Byte 19 (reserved) default */
/*          note: full duplex is enabled if FDX# pin is 0 */
#define CB_557_CFIG_DEFAULT_PARM19  0x80

/* Byte 20 (multi-IA) default */
#define CB_557_CFIG_DEFAULT_PARM20  0x3f

/* Byte 21 (multicast all) default */
#define CB_557_CFIG_DEFAULT_PARM21  0x05


/* 82557 PCI Register Definitions */
/* Refer To The PCI Specification For Detailed Explanations */

/*- Register Offsets*/
#define PCI_VENDOR_ID_REGISTER      0x00 /* PCI Vendor ID Register */
#define PCI_DEVICE_ID_REGISTER      0x02 /* PCI Device ID Register */
#define PCI_CONFIG_ID_REGISTER      0x00 /* PCI Configuration ID Register */
#define PCI_COMMAND_REGISTER        0x04 /* PCI Command Register */
#define PCI_STATUS_REGISTER         0x06 /* PCI Status Register */
#define PCI_REV_ID_REGISTER         0x08 /* PCI Revision ID Register */
#define PCI_CLASS_CODE_REGISTER     0x09 /* PCI Class Code Register */
#define PCI_CACHE_LINE_REGISTER     0x0C /* PCI Cache Line Register */
#define PCI_BIST_REGISTER           0x0F /* PCI Built-In SelfTest Register */
#define PCI_BAR_0_REGISTER          0x10 /* PCI Base Address Register 0 */
#define PCI_BAR_1_REGISTER          0x14 /* PCI Base Address Register 1 */
#define PCI_BAR_2_REGISTER          0x18 /* PCI Base Address Register 2 */
#define PCI_BAR_3_REGISTER          0x1C /* PCI Base Address Register 3 */
#define PCI_BAR_4_REGISTER          0x20 /* PCI Base Address Register 4 */
#define PCI_BAR_5_REGISTER          0x24 /* PCI Base Address Register 5 */
#define PCI_EXPANSION_ROM           0x30 /* PCI Expansion ROM Base Register */
#define PCI_MIN_GNT_REGISTER        0x3E /* PCI Min-Gnt Register */
#define PCI_MAX_LAT_REGISTER        0x3F /* PCI Max_Lat Register */
#define PCI_NODE_ADDR_REGISTER      0x40 /* PCI Node Address Register */

/* PCI access methods */
#define P_CONF_T1    1
#define P_CONF_T2    2

/* max number of pci buses */
#define MAX_PCI_BUSES   0xFF

/* number of PCI config bytes to access */
#define PCI_BYTE    1
#define PCI_WORD    2
#define PCI_DWORD   4

/* adapter vendor & device ids */
#define PCI_OHIO_BOARD   0x10f0 /* subdevice ID, Ohio dual port nic */

/* PCI related constants */
#define CMD_IO_ENBL     BIT_0
#define CMD_MEM_ENBL    BIT_1
#define CMD_BUS_MASTER  BIT_2
#define P_TEST_PATN     0xCDEF
#define PO_DEV_NO       11
#define PO_BUS_NO       16
#define PO_FUN_NO       8
#define P_CSPACE        0x80000000

/* PCI addresses */
#define PCI_SPACE_ENABLE            0xCF8
#define CF1_CONFIG_ADDR_REGISTER    0x0CF8
#define CF1_CONFIG_DATA_REGISTER    0x0CFC
#define CF2_FORWARD_REGISTER        0x0CFA
#define CF2_BASE_ADDRESS            0xC000

/* PCI Device ID - for internal use */
#define PCI_DEV_NO              0x00FF
#define PCI_BUS_NO              0xFF00

/* PCI configuration space definitions */
#define PCI_CMD_OFFSET  0x4     /* command register offset */
#define PCI_IO_ENABLE   0x00000001 /* I/O space access  */
#define PCI_MEM_ENABLE  0x00000002 /* memory space access  */
#define PCI_MASTER      0x00000004 /* bus master enabled */
#define PCI_IO_ADDR      0x00000001 /* BAR is an I/O space */

/* Values for PCI_REV_ID_REGISTER values */
#define D101A4_REV_ID      4    /* 82558 A4 stepping */
#define D101B0_REV_ID      5    /* 82558 B0 stepping */
#define D101MA_REV_ID      8    /* 82559 A0 stepping */

/* Added 82559S rev ID */
#define D101S_REV_ID      9     /* 82559S A-step */
#define D102_REV_ID      12
#define D102C_REV_ID     13     /* 82550 step C */

/* PHY 100 MDI Register/Bit Definitions */

/* MDI register set */
#define MDI_CONTROL_REG             0x00 /* MDI control register */
#define MDI_STATUS_REG              0x01 /* MDI Status regiser */
#define PHY_ID_REG_1                0x02 /* Phy indentification reg (word 1) */
#define PHY_ID_REG_2                0x03 /* Phy indentification reg (word 2) */
#define AUTO_NEG_ADVERTISE_REG      0x04 /* Auto-negotiation advertisement */
#define AUTO_NEG_LINK_PARTNER_REG   0x05 /* Auto-negotiation link partner 
                                          * ability */
#define AUTO_NEG_EXPANSION_REG      0x06 /* Auto-negotiation expansion */
#define AUTO_NEG_NEXT_PAGE_REG      0x07 /* Auto-negotiation next page xmit */
#define EXTENDED_REG_0              0x10 /* Extended reg 0 (Phy 100 modes) */
#define EXTENDED_REG_1              0x14 /* Extended reg 1 (Phy 100 error 
                                          * indications) */
#define NSC_CONG_CONTROL_REG        0x17 /* National (TX) congestion control */
#define NSC_SPEED_IND_REG           0x19 /* National (TX) speed indication */

/* ############Start of 82555 specific defines################## */

/* Intel 82555 specific registers */
#define PHY_82555_CSR            		0x10 /* 82555 CSR */
#define PHY_82555_SPECIAL_CONTROL    	0x11 /* 82555 special control register */

#define PHY_82555_RCV_ERR				0x15 /* 82555 100BaseTx Receive Error 
                                   * * Frame Counter */
#define PHY_82555_SYMBOL_ERR			0x16 /* 82555 RCV Symbol Error Counter */
#define PHY_82555_PREM_EOF_ERR			0x17 /* 82555 100BaseTx RCV Premature End
                                       * * of Frame Error Counter */
#define PHY_82555_EOF_COUNTER    		0x18 /* 82555 end of frame error counter */
#define PHY_82555_MDI_EQUALIZER_CSR     0x1a /* 82555 specific equalizer reg. */

/* 82555 CSR bits */
#define PHY_82555_SPEED_BIT       BIT_1
#define PHY_82555_POLARITY_BIT    BIT_8

/* 82555 equalizer reg. opcodes */
#define ENABLE_ZERO_FORCING     0x2010 /* write to ASD conf. reg. 0 */
#define DISABLE_ZERO_FORCING    0x2000 /* write to ASD conf. reg. 0 */

/* 82555 special control reg. opcodes */
#define DISABLE_AUTO_POLARITY       0x0010
#define EXTENDED_SQUELCH_BIT         BIT_2

/* ############End of 82555 specific defines##################### */

/* MDI Control register bit definitions */
#define MDI_CR_COLL_TEST_ENABLE     BIT_7 /* Collision test enable */
#define MDI_CR_FULL_HALF            BIT_8 /* FDX =1, half duplex =0 */
#define MDI_CR_RESTART_AUTO_NEG     BIT_9 /* Restart auto negotiation */
#define MDI_CR_ISOLATE              BIT_10 /* Isolate PHY from MII */
#define MDI_CR_POWER_DOWN           BIT_11 /* Power down */
#define MDI_CR_AUTO_SELECT          BIT_12 /* Auto speed select enable */
#define MDI_CR_10_100               BIT_13 /* 0 = 10Mbs, 1 = 100Mbs */
#define MDI_CR_LOOPBACK             BIT_14 /* 0 = normal, 1 = loopback */
#define MDI_CR_RESET                BIT_15 /* 0 = normal, 1 = PHY reset */

/* MDI Status register bit definitions */
#define MDI_SR_EXT_REG_CAPABLE      BIT_0 /* Extended register capabilities */
#define MDI_SR_JABBER_DETECT        BIT_1 /* Jabber detected */
#define MDI_SR_LINK_STATUS          BIT_2 /* Link Status -- 1 = link */
#define MDI_SR_AUTO_SELECT_CAPABLE  BIT_3 /* Auto speed select capable */
#define MDI_SR_REMOTE_FAULT_DETECT  BIT_4 /* Remote fault detect */
#define MDI_SR_AUTO_NEG_COMPLETE    BIT_5 /* Auto negotiation complete */
#define MDI_SR_10T_HALF_DPX         BIT_11 /* 10BaseT Half Duplex capable */
#define MDI_SR_10T_FULL_DPX         BIT_12 /* 10BaseT full duplex capable */
#define MDI_SR_TX_HALF_DPX          BIT_13 /* TX Half Duplex capable */
#define MDI_SR_TX_FULL_DPX          BIT_14 /* TX full duplex capable */
#define MDI_SR_T4_CAPABLE           BIT_15 /* T4 capable */

/* Auto-Negotiation advertisement register bit definitions */
#define NWAY_AD_SELCTOR_FIELD   BIT_0_4 /* identifies supported protocol */
#define NWAY_AD_ABILITY         BIT_5_12 /* technologies supported */
#define NWAY_AD_10T_HALF_DPX    BIT_5 /* 10BaseT Half Duplex capable */
#define NWAY_AD_10T_FULL_DPX    BIT_6 /* 10BaseT full duplex capable */
#define NWAY_AD_TX_HALF_DPX     BIT_7 /* TX Half Duplex capable */
#define NWAY_AD_TX_FULL_DPX     BIT_8 /* TX full duplex capable */
#define NWAY_AD_T4_CAPABLE      BIT_9 /* T4 capable */
#define NWAY_AD_REMOTE_FAULT    BIT_13 /* indicates local remote fault */
#define NWAY_AD_RESERVED        BIT_14 /* reserved */
#define NWAY_AD_NEXT_PAGE       BIT_15 /* Next page (not supported) */

/* Auto-Negotiation link partner ability register bit definitions */
#define NWAY_LP_SELCTOR_FIELD   BIT_0_4 /* identifies supported protocol */
#define NWAY_LP_ABILITY         BIT_5_12 /* technologies supported */
#define NWAY_LP_REMOTE_FAULT    BIT_13 /* indic8 partner remote fault */
#define NWAY_LP_ACKNOWLEDGE     BIT_14 /* acknowledge */
#define NWAY_LP_NEXT_PAGE       BIT_15 /* Next page (not supported) */

/* Auto-Negotiation expansion register bit definitions */
#define NWAY_EX_LP_NWAY         BIT_0 /* link partner is NWAY */
#define NWAY_EX_PAGE_RECEIVED   BIT_1 /* link code word received */
#define NWAY_EX_NEXT_PAGE_ABLE      BIT_2 /* local is next page able */
#define NWAY_EX_LP_NEXT_PAGE_ABLE   BIT_3 /* partner is next page able */
#define NWAY_EX_PARALLEL_DET_FLT    BIT_4 /* parallel detection fault */
#define NWAY_EX_RESERVED    BIT_5_15 /* reserved */


/* PHY 100 Extended Register 0 bit definitions */
#define PHY_100_ER0_FDX_INDIC       BIT_0 /* 1 = FDX, 0 = half duplex */
#define PHY_100_ER0_SPEED_INDIC     BIT_1 /* 1 = 100mbs, 0= 10mbs */
#define PHY_100_ER0_WAKE_UP         BIT_2 /* Wake up DAC */
#define PHY_100_ER0_RESERVED        BIT_3_4 /* Reserved */
#define PHY_100_ER0_REV_CNTRL       BIT_5_7 /* Revsion control (A step = 000) */
#define PHY_100_ER0_FORCE_FAIL      BIT_8 /* Force Fail is enabled */
#define PHY_100_ER0_TEST            BIT_9_13 /* Revsion control (A step = 000) */
#define PHY_100_ER0_LINKDIS         BIT_14 /* Link integrity test disabled */
#define PHY_100_ER0_JABDIS          BIT_15 /* Jabber function is disabled */


/* PHY 100 Extended Register 1 bit definitions */
#define PHY_100_ER1_RESERVED        BIT_0_8 /* Reserved */
#define PHY_100_ER1_CH2_DET_ERR     BIT_9 /* Channel 2 EOF detection error */
#define PHY_100_ER1_MANCH_CODE_ERR  BIT_10 /* Manchester code error */
#define PHY_100_ER1_EOP_ERR         BIT_11 /* EOP error */
#define PHY_100_ER1_BAD_CODE_ERR    BIT_12 /* bad code error */
#define PHY_100_ER1_INV_CODE_ERR    BIT_13 /* invalid code error */
#define PHY_100_ER1_DC_BAL_ERR      BIT_14 /* DC balance error */
#define PHY_100_ER1_PAIR_SKEW_ERR   BIT_15 /* Pair skew error */

/* National Semiconductor TX phy congestion control register bit definitions */
#define NSC_TX_CONG_TXREADY         BIT_10 /* Makes TxReady an input */
#define NSC_TX_CONG_ENABLE          BIT_8 /* Enables congestion control */

/* National Semiconductor TX phy speed indication register bit definitions */
#define NSC_TX_SPD_INDC_SPEED       BIT_6 /* 0 = 100mb, 1=10mb */

/* Return status for xmit bufs */
#define E_NOBUFS  0x10          /* no more buffers */
#define E_NOCBUFS 0x20          /* no more coalesce buffers */

#define XMITS_PER_INTR 4

/*  Size of loadable micro code image for each supported chip.  */
#ifndef D100_NUM_MICROCODE_DWORDS
#define     D100_NUM_MICROCODE_DWORDS    66
#endif

#ifndef D101_NUM_MICROCODE_DWORDS
#define     D101_NUM_MICROCODE_DWORDS    102
#endif

#ifndef D101M_NUM_MICROCODE_DWORDS
#define     D101M_NUM_MICROCODE_DWORDS   134
#endif

#ifndef D101S_NUM_MICROCODE_DWORDS
#define     D101S_NUM_MICROCODE_DWORDS   134
#endif

#ifndef D102_NUM_MICROCODE_DWORDS
#define     D102_NUM_MICROCODE_DWORDS   134
#endif

#ifndef D102C_NUM_MICROCODE_DWORDS
#define     D102C_NUM_MICROCODE_DWORDS   134
#endif

/* ====================================================================== */
/*                              checksumming                              */
/* ====================================================================== */

/* Note: we support TCP and UDP, not IPv4 checksums. IPv4 checksums must be 
 * done in software 
 */
#define HDR_LEN_MSK             0x0F
#define PROTOCOL_TYPE_MASK      0x00FF
#define PROTOCOL_VERSION_MASK   0x00FF
#define TCP_PROTOCOL            6
#define UDP_PROTOCOL            17
#define NUMBER_OF_BOARDS        4
#define IP_8022_DSAPSSAP        0x5E5E
#define IP_8022_DOD_DSAPSSAP    0x0606
#define IPv4_NIBBLE             4
#define MAX_PKT_LEN_FIELD       1500
#define MIN_IP_PKT_SIZE         50      /* 14 for the MAC Header   */
#define PROTOCOL_ID_LEN         5
#define ADDRESS_LEN             6
#define FRAG_OFFSET_MASK        0xFF1F
#define HARDWARE_CHECKSUM_START_OFFSET 14
#define CRC_LENGTH              4
#define IP_PROTOCOL             0x0008 /* Network Byte order */
#define CHKSUM_SIZE             2

#define IP_HEADER_NO_OPTIONS_SIZE 20

#define RFD_PARSE_BIT             BIT_3
#define RFD_TCP_PACKET            0x00
#define RFD_UDP_PACKET            0x01
#define TCPUDP_CHECKSUM_BIT_VALID BIT_4
#define TCPUDP_CHECKSUM_VALID     BIT_5
#define CHECKSUM_PROTOCOL_MASK    0x03

/* Macro to adjust for carry in our adjust value */
#define CHECK_FOR_CARRY(_XSUM)  (((_XSUM << 16) | (_XSUM >> 16)) + _XSUM) >> 16

/* byte swap a 2 byte value */
#ifndef BYTE_SWAP_WORD
#define BYTE_SWAP_WORD(value)   ( (((WORD)(value) & 0x00ff) << 8) | \
                                  (((WORD)(value) & 0xff00) >> 8) )
#endif

/* Word swap a 4 byte value */
#define WORD_SWAP_DWORD(value)    ( (((DWORD)(value) & 0x0000FFFF) << 16) | \
                                    (((DWORD)(value) & 0xFFFF0000) >> 16) )

typedef struct _ethernet_ii_header_ {
  BYTE DestAddr[ADDRESS_LEN];
  BYTE SourceAddr[ADDRESS_LEN];
  WORD TypeLength;
} ethernet_ii_header, *pethernet_ii_header;

/********************************************************************** */
/* Ethernet SNAP Header Definition */
/********************************************************************** */

typedef struct _ethernet_snap_header_ {
  BYTE DestAddr[ADDRESS_LEN];
  BYTE SourceAddr[ADDRESS_LEN];
  WORD TypeLength;
  BYTE DSAP;
  BYTE SSAP;
  BYTE Ctrl;
  BYTE ProtocolId[PROTOCOL_ID_LEN];   
} ethernet_snap_header, *pethernet_snap_header;

/*
 * Internet Protocol Version 4 (IPV4) Header Definition
 */
typedef struct _ip_v4_header_ {
  DWORD HdrLength:4,            /* Network Byte order */
        HdrVersion:4,           /* Network Byte order */
        TOS:8,                  /* Network Byte order */
        Length:16;              /* Network Byte order */

  union {
    DWORD IdThruOffset;
    struct {
      DWORD DatagramId:16, FragOffset1:5, /* Network Byte order */
            MoreFragments:1, NoFragment:1,
            Reserved2:1, FragOffset:8;
    } Bits;

    struct {
      DWORD DatagramId:16, FragsNFlags:16; /* Network Byte order */
    } Fields;
  } FragmentArea;

  BYTE TimeToLive;
  BYTE ProtocolCarried;
  WORD Checksum;

  union {
    DWORD SourceIPAddress;
    struct {
      DWORD SrcAddrLo:16,       /* Network Byte order */
            SrcAddrHi:16;
    } Fields;

    struct {
      DWORD OctA:8, OctB:8, OctC:8, OctD:8;
    } SFields;
  } SrcAddr;

  union {
    DWORD TargetIPAddress;
    struct {
      DWORD DestAddrLo:16,      /* Network Byte order */
            DestAddrHi:16;
    } Fields;

    struct {
      DWORD OctA:8, OctB:8, OctC:8, OctD:8;
    } DFields;
  } DestAddr;

  WORD IpOptions[20];   /* Maximum of 40 bytes (20 words) of IP options  */
} ip_v4_header;


/*
 * Ethernet II IP Packet Definition
 */
typedef struct _ethernet_ii_ipv4_packet_ {
  ethernet_ii_header Ethernet;  /* Ethernet II Header */
  ip_v4_header IpHeader;        /* IP Header (No Options) */
} ethernet_ii_ipv4_packet, *pethernet_ii_ipv4_packet;

/*
 * Ethernet SNAP IP Packet Definition
 */
typedef struct _ethernet__snap_ipv4_packet_ {
  ethernet_snap_header Ethernet;  /* Ethernet SNAP Header */
  ip_v4_header         IpHeader;  /* IP Header (No Options) */
} ethernet_snap_ipv4_packet, *pethernet_snap_ipv4_packet;

/*
 * TCP Header Definition
 */
typedef struct _tcp_header_ {
  WORD SourcePort;
  WORD TargetPort;
  DWORD SourceSeqNum;
  DWORD AckSeqNum;
  BYTE HeaderLen_Rsvd;
  BYTE SessionBits;
  WORD WindowSize;
  WORD Checksum;
  WORD UrgentDataPointer;   
} tcp_header, *ptcp_header;

/*
 * UDP Header Definition
 */
typedef struct _udp_header_ {
  WORD SourcePort;
  WORD TargetPort;
  WORD Length;
  WORD Checksum;
} udp_header, *pudp_header;

ip_v4_header *e100_check_for_ip (int *HeaderOffset, struct sk_buff *skb);

#define   E100_NAME         "e100"

/* Bits for bdp->flags */
#define BOARD_PRESENT      0x0001
#define BOARD_DISABLED     0x0002
#define DF_OPENED          0x0004
#define DF_UCODE_LOADED    0x0008
#define DF_PHY_82555       0x2000
#define DF_SPEED_FORCED    0x4000 /* set if speed is forced */
#define DF_LINK_UP         0x8000 /* set if link is up */

typedef struct net_device_stats net_dev_stats_t;

/* These macros use the bdp pointer. If you use them it better be defined
 */
#define PREV_TCB_USED(X)  ((X)->tail ? (X)->tail-1 : TxDescriptors[bdp->bd_number]-1)
#define NEXT_TCB_TOUSE(X) ((((X)+1) >= TxDescriptors[bdp->bd_number]) ? 0 : (X)+1)
#define TCB_TO_USE(X)     ((X)->tail)
#define TCBS_AVAIL(X)     (NEXT_TCB_TOUSE( NEXT_TCB_TOUSE((X)->tail)) != (X)->head)

/* leave a gap of 2 TCB's in e100_tx_srv */
#define IS_IT_GAP(X)      (NEXT_TCB_TOUSE(NEXT_TCB_TOUSE((X)->head))==(X)->tail)

/* Macro to see if address is a Multicast address. Jun 22, 1999 */
#define IS_VALID_MULTICAST(x)  ((x)->bytes[0] & 0x1 )

/* Device ID macros */
#define get_pci_dev(X)         ((X)&PCI_DEV_NO)
#define get_pci_bus(X)         ((X)>>8)
#define mk_pci_dev_id(X,Y)     (((X)<<8)|((Y)&PCI_DEV_NO))

#define RFD_POINTER(X,Y)       ((rfd_t *) (((BYTE*)((X)->data))-((Y)->rfd_size)))

/* ====================================================================== */
/*                              82557                                     */
/* ====================================================================== */

#define D102_RFD_EXTRA_SIZE      16

typedef union {
  BYTE bytes[ETHERNET_ADDRESS_LENGTH];
  WORD words[ETHERNET_ADDRESS_LENGTH / 2];
} e100_eaddr_t;

typedef struct {
  DWORD etherAlignErrors;       /* Frame alignment errors */
  DWORD etherCRCerrors;         /* CRC erros */
  DWORD etherMissedPkts;        /* Packet overflow or missed inter */
  DWORD etherOverrunErrors;     /* Overrun errors */
  DWORD etherUnderrunErrors;    /* Underrun errors */
  DWORD etherCollisions;        /* Total collisions */
  DWORD etherAbortErrors;       /* Transmits aborted at interface */
  DWORD etherCarrierLost;       /* Carrier sense signal lost */
  DWORD etherReadqFull;         /* STREAMS read queue full */
  DWORD etherRcvResources;      /* Receive resource alloc failure */
  DWORD etherDependent1;        /* Device dependent statistic */
  DWORD etherDependent2;        /* Device dependent statistic */
  DWORD etherDependent3;        /* Device dependent statistic */
  DWORD etherDependent4;        /* Device dependent statistic */
  DWORD etherDependent5;        /* Device dependent statistic */
} e100_etherstat_t;

/* Ethernet Frame Structure */
/*- Ethernet 6-byte Address */
typedef struct _eth_address_t {
  BYTE eth_node_addr[ETHERNET_ADDRESS_LENGTH];
} eth_address_t, *peth_address_t;


/*- Ethernet 14-byte Header */
typedef struct _eth_header_t {
  BYTE eth_dest[ETHERNET_ADDRESS_LENGTH];
  BYTE eth_src[ETHERNET_ADDRESS_LENGTH];
  WORD eth_typelen;
} eth_header_t, *peth_header_t __attribute__ ((__packed__));


/*- Ethernet Buffer (Including Ethernet Header) for Transmits */
typedef struct _eth_tx_buffer_t {
  eth_header_t tx_mac_hdr;
  BYTE tx_buff_data[(TCB_BUFFER_SIZE - sizeof (eth_header_t))];
} eth_tx_buffer_t, *peth_tx_buffer_t __attribute__ ((__packed__));

typedef struct _eth_rx_buffer_t {
  eth_header_t rx_mac_hdr;
  BYTE rx_buff_data[(RCB_BUFFER_SIZE - sizeof (eth_header_t)) + CHKSUM_SIZE];
} eth_rx_buffer_t , *peth_rx_buffer_t __attribute__ ((__packed__));

/* Changed for 82558 enhancement */
typedef struct _d101_scb_ext_t {
  volatile DWORD scb_rx_dma_cnt;    /* Rx DMA byte count */
  volatile BYTE scb_early_rx_int;   /* Early Rx DMA byte count */
  volatile BYTE scb_fc_thld;        /* Flow Control threshold */
  volatile BYTE scb_fc_xon_xoff;    /* Flow Control XON/XOFF values */
  volatile BYTE scb_pmdr;           /* Power Mgmt. Driver Reg */
} d101_scb_ext __attribute__ ((__packed__));

/* Changed for 82559 enhancement */
typedef struct _d101m_scb_ext_t {
  volatile DWORD scb_rx_dma_cnt;    /* Rx DMA byte count */
  volatile BYTE  scb_early_rx_int;  /* Early Rx DMA byte count */
  volatile BYTE  scb_fc_thld;       /* Flow Control threshold */
  volatile BYTE  scb_fc_xon_xoff;   /* Flow Control XON/XOFF values */
  volatile BYTE  scb_pmdr;          /* Power Mgmt. Driver Reg */
  volatile BYTE  scb_gen_ctrl;      /* General Control */
  volatile BYTE  scb_gen_stat;      /* General Status */
  volatile WORD  scb_reserved;      /* Reserved */
  volatile DWORD scb_function_event;/* Cardbus Function Event */
  volatile DWORD scb_function_event_mask;    /* Cardbus Function Mask */
  volatile DWORD scb_function_present_state; /* Cardbus Function state */
  volatile DWORD scb_force_event;   /* Cardbus Force Event */
} d101m_scb_ext __attribute__ ((__packed__));

/* Changed for 82550 enhancement */
typedef struct _d102_scb_ext_t {
  volatile DWORD scb_rx_dma_cnt;     /* Rx DMA byte count */
  volatile BYTE  scb_early_rx_int;   /* Early Rx DMA byte count */
  volatile BYTE  scb_fc_thld;        /* Flow Control threshold */
  volatile BYTE  scb_fc_xon_xoff;    /* Flow Control XON/XOFF values */
  volatile BYTE  scb_pmdr;           /* Power Mgmt. Driver Reg */
  volatile BYTE  scb_gen_ctrl;       /* General Control */
  volatile BYTE  scb_gen_stat;       /* General Status */
  volatile BYTE  scb_gen_ctrl2;
  volatile BYTE  scb_reserved;       /* Reserved */
  volatile DWORD scb_scheduling_reg;
  volatile DWORD scb_reserved2;
  volatile DWORD scb_function_event; /* Cardbus Function Event */
  volatile DWORD scb_function_event_mask;    /* Cardbus Function Mask */
  volatile DWORD scb_function_present_state; /* Cardbus Function state */
  volatile DWORD scb_force_event;    /* Cardbus Force Event */
} d102_scb_ext __attribute__ ((__packed__));


/*
 * 82557 status control block. this will be memory mapped & will hang of the
 * the bdd, which hangs of the bdp. This is the brain of it.
 */
typedef struct _scb_t {
  volatile WORD  scb_status;     /* SCB Status register */
  volatile BYTE  scb_cmd_low;    /* SCB Command register (low byte) */
  volatile BYTE  scb_cmd_hi;     /* SCB Command register (high byte) */
  volatile DWORD scb_gen_ptr;    /* SCB General pointer */
  volatile DWORD scb_port;       /* PORT register */
  volatile WORD  scb_flsh_cntrl; /* Flash Control register */
  volatile WORD  scb_eprm_cntrl; /* EEPROM control register */
  volatile DWORD scb_mdi_cntrl;  /* MDI Control Register */
  /* Changed for 82558 enhancement */
  union {
    volatile DWORD         scb_rx_dma_cnt; /* Rx DMA byte count */
    volatile d101_scb_ext  d101_scb;       /* 82558/9 specific fields */
    volatile d101m_scb_ext d101m_scb;      /* 82559 specific fields */
    volatile d102_scb_ext  d102_scb;
  } scb_ext;
} scb_t, *pscb_t __attribute__ ((__packed__));

/* Self test
 * This is used to dump results of the self test 
 */
typedef struct _self_test_t {
  DWORD st_sign;                /* Self Test Signature */
  DWORD st_result;              /* Self Test Results */
} self_test_t, *pself_test_t __attribute__ ((__packed__));


/* Error Counters */
typedef struct _err_cntr_t {
  DWORD xmt_gd_frames;          /* Good frames transmitted */
  DWORD xmt_max_coll;           /* Fatal frames -- had max collisions */
  DWORD xmt_late_coll;          /* Fatal frames -- had a late coll. */
  DWORD xmt_uruns;              /* Xmit underruns (fatal or re-transmit) */
  DWORD xmt_lost_crs;           /* Frames transmitted without CRS */
  DWORD xmt_deferred;           /* Deferred transmits */
  DWORD xmt_sngl_coll;          /* Transmits that had 1 and only 1 coll. */
  DWORD xmt_mlt_coll;           /* Transmits that had multiple coll. */
  DWORD xmt_ttl_coll;           /* Transmits that had 1+ collisions. */
  DWORD rcv_gd_frames;          /* Good frames received */
  DWORD rcv_crc_errs;           /* Aligned frames that had a CRC error */
  DWORD rcv_algn_errs;          /* Receives that had alignment errors */
  DWORD rcv_rsrc_err;           /* Good frame dropped cuz no resources */
  DWORD rcv_oruns;              /* Overrun errors - bus was busy */
  DWORD rcv_err_coll;           /* Received frms. that encountered coll. */
  DWORD rcv_shrt_frames;        /* Received frames that were to short */
  DWORD cmd_complete;           /* A005h indicates cmd completion */
} err_cntr_t, *perr_cntr_t;


/* Command Block (CB) Generic Header Structure */
typedef struct _cb_header_t {
  WORD  cb_status;              /* Command Block Status */
  WORD  cb_cmd;                 /* Command Block Command */
  DWORD cb_lnk_ptr;             /* Link To Next CB */
} cb_header_t, *pcb_header_t __attribute__ ((__packed__));


/* NOP Command Block (NOP_CB) */
typedef struct _nop_cb_t {
  cb_header_t nop_cb_hdr;
} nop_cb_struc, *pnop_cb_struc __attribute__ ((__packed__));


/* Individual Address Command Block (IA_CB) */
typedef struct _ia_cb_t {
  cb_header_t ia_cb_hdr;
  BYTE ia_addr[ETHERNET_ADDRESS_LENGTH];
} ia_cb_t, *pia_cb_t __attribute__ ((__packed__));


/* Configure Command Block (CONFIG_CB) */
typedef struct _config_cb_t {
  cb_header_t cfg_cbhdr;
  BYTE cfg_byte[CB_CFIG_BYTE_COUNT];
} config_cb_t, *pconfig_cb_t __attribute__ ((__packed__));


/* MultiCast Command Block (MULTICAST_CB) */
typedef struct _multicast_cb_t {
  cb_header_t mc_cbhdr;
  WORD mc_count;                /* Number of multicast addresses */
  BYTE mc_addr[(ETHERNET_ADDRESS_LENGTH * MAX_MULTICAST_ADDRS)];
} mltcst_cb_t, *pmltcst_cb_t __attribute__ ((__packed__));


/* Dump Command Block (DUMP_CB) */
typedef struct _dump_cb_t {
  cb_header_t DumpCBHeader;
  DWORD DumpAreaAddress;        /* Dump Buffer Area Address */
} dump_cb_t, *pdump_cb_t __attribute__ ((__packed__));


/* Dump Area structure definition */
typedef struct _dump_area_t {
  BYTE DumpBuffer[DUMP_BUFFER_SIZE];
} dump_area_t , *pdump_area_t;


/* Diagnose Command Block (DIAGNOSE_CB) */
typedef struct _diagnose_cb_t {
  cb_header_t DiagCBHeader;
} diagnose_cb_t, *pdiagnose_cb_t __attribute__ ((__packed__));

/* Load Microcode Command Block (LOAD_UCODE_CB) */
typedef struct _load_ucode_cb_t {
  cb_header_t load_ucode_cbhdr;
  DWORD ucode_dword[D102_NUM_MICROCODE_DWORDS];
} load_ucode_cb_t, *pload_ucode_cb_t __attribute__ ((__packed__));

/* NON_TRANSMIT_CB -- Generic Non-Transmit Command Block , doesnot include
 * MultiCast as the addresses need to be saved
 */
typedef struct _nxmit_cb_t {
  union {
    config_cb_t     config;
    ia_cb_t         setup;
    dump_cb_t       dump;
    load_ucode_cb_t load_ucode;
    mltcst_cb_t     multicast;
  } ntcb;
} nxmit_cb_t, *pnxmit_cb_t __attribute__ ((__packed__));


/* 82558 specific Extended TCB fields */
typedef struct _tcb_ext_t {
  DWORD tbd0_buf_addr;          /* Physical Transmit Buffer Address */
  DWORD tbd0_buf_cnt;           /* Actual Count Of Bytes */
  DWORD tbd1_buf_addr;          /* Physical Transmit Buffer Address */
  DWORD tbd1_buf_cnt;           /* Actual Count Of Bytes */
  eth_tx_buffer_t tcb_data;     /* Data buffer in TCB */
} tcb_ext_t __attribute__ ((__packed__));


/* some defines for the ipcb */
#define IPCB_HARDWAREPARSING_ENABLE 0x010
#define IPCB_IP_ACTIVATION_DEFAULT      IPCB_HARDWAREPARSING_ENABLE

/* d102 specific fields */
typedef struct _tcb_ipcb_t {
  DWORD scheduling:20;
  DWORD ip_activation:12;
  WORD  vlan;
  BYTE  ip_header_offset;
  BYTE  tcp_header_offset;
  union {
    DWORD sec_rec_phys_addr;
    DWORD tbd_zero_address;
  } tbd_sec_addr;
  union {
    WORD sec_rec_size;
    WORD tbd_zero_size;
  } tbd_sec_size;
  WORD total_tcp_payload;
  eth_tx_buffer_t tcb_data;
} tcb_ipcb_t __attribute__ ((__packed__));

/* Generic 82557 TCB fields */
typedef struct _tcb_gen_t {
  eth_tx_buffer_t tcb_data;     /* Data buffer in TCB */
  DWORD pad0;                   /* filler */
  DWORD pad1;                   /* filler */
  DWORD pad2;                   /* filler */
  DWORD pad3;                   /* filler */
} tcb_gen_t __attribute__ ((__packed__));

/* Transmit Command Block (TCB) */
struct _tcb_t {
  cb_header_t tcb_hdr;
  DWORD       tcb_tbd_ptr;       /* TBD address */
  WORD        tcb_cnt;           /* Data Bytes In TCB past header */
  BYTE        tcb_thrshld;       /* TX Threshold for FIFO Extender */
  BYTE        tcb_tbd_num;

  union {
    tcb_ipcb_t ipcb;             /* d102 ipcb fields */
    tcb_ext_t  tcb_ext;          /* 82558 extended TCB fields */
    tcb_gen_t  tcb_gen;          /* Generic 82557 fields */
  } tcbu;

  /* From here onward we can dump anything we want as long as the
   * size of the total structure is a multiple of a paragraph
   * boundary ( i.e. -16 bit aligned ).
   */
  sk_buff_t *tcb_skb;            /* the associated socket buffer */
  int        tcb_cbflag;         /* set if using the coalesce buffer */
  int        tcb_msgsz;
  DWORD      tcb_paddr;          /* phys addr of the TCB */
} __attribute__ ((__packed__));


#ifndef _TCB_T_
  #define _TCB_T_
  typedef struct _tcb_t tcb_t, *ptcb_t;
#endif


/* Transmit Buffer Descriptor (TBD) */
typedef struct _tbd_t {
  DWORD tbd_buf_addr;           /* Physical Transmit Buffer Address */
  DWORD tbd_buf_cnt;            /* Actual Count Of Bytes */
} tbd_t, *ptbd_t __attribute__ ((__packed__));


/* Receive Frame Descriptor (RFD) - will be using the simple model */
struct _rfd_t {
  /* 8255x */
  cb_header_t rfd_header;
  DWORD       rfd_rbd_ptr;      /* Receive Buffer Descriptor Addr */
  WORD        rfd_act_cnt;      /* Number Of Bytes Received */
  WORD        rfd_sz;           /* Number Of Bytes In RFD */
  /* D102 aka Gamla */
  WORD        vlanid;
  BYTE        rcvparserstatus;
  BYTE        reserved;
  WORD        securitystatus;
  BYTE        checksumstatus;
  BYTE        zerocopystatus;
  BYTE        fill[8];

  eth_rx_buffer_t rfd_buf;      /* Data buffer in RFD */
  sk_buff_t *prev;              /* skb containing previous RFD */
  sk_buff_t *next;              /* skb containing next RFD */
  device_t  *dev;               /* identify which dev this RFD belongs to */
} __attribute__ ((__packed__));

#ifndef _RFD_T_
  #define _RFD_T_
  typedef struct _rfd_t rfd_t, *prfd_t;
#endif


/* Receive Buffer Descriptor (RBD) */
typedef struct _rbd_t {
  WORD  rbd_act_cnt;            /* Number Of Bytes Received */
  WORD  rbd_filler;
  DWORD rbd_lnk_addr;           /* Link To Next RBD */
  DWORD rbd_rcb_addr;           /* Receive Buffer Address */
  WORD  rbd_sz;                 /* Receive Buffer Size */
  WORD  rbd_filler1;
} rbd_t, *prbd_t __attribute__ ((__packed__));

/*
 * This structure is used to maintain a FIFO access to a resource that is 
 * maintained as a circular queue. The resource to be maintained is pointed
 * to by the "data" field in the structure below. In this driver the TCBs', 
 * TBDs' & RFDs' are maintained  as a circular queue & are managed thru this
 * structure.
 */
typedef struct _buf_pool_t {
  DWORD head;                   /* index to first used resource */
  DWORD tail;                   /* index to last used resource */
  DWORD count;                  /* not used for tx pool */
  void *data;                   /* points to resource pool */
} buf_pool_t, *pbuf_pool_t;

typedef struct _bdd_t
{
  DWORD bd_number;              /*  Board Number */
  DWORD phy_addr;               /* address of PHY component */
  DWORD flags;
  DWORD io_base;                /* IO base address */
  size_t mem_size;              /* Memory size from devmem_size */
  pscb_t scbp;                  /* memory mapped ptr to 82557 scb */
  pself_test_t pselftest;       /* pointer to self test area */
  paddr_t selftest_paddr;       /* phys addr of selftest */
  perr_cntr_t pstats_counters;  /* pointer to stats table */
  paddr_t stat_cnt_paddr;       /* phys addr of stat counter area */
  pdump_area_t pdump_area;      /* pointer to 82557 reg. dump area */
  paddr_t dump_paddr;           /* phys addr of dump area */
  pnxmit_cb_t pntcb;            /* pointer to non xmit tcb */
  paddr_t nontx_paddr;          /* phys addr of  non-tx tcb */
  perr_stats_t perr_stats;      /* ptr to error statictics results */
  paddr_t estat_paddr;          /* phys addr of  err stat results */
  buf_pool_t tcb_pool;
  paddr_t tcb_paddr;            /* phys addr of start of TCBs */
  void *tcb_pool_base;
  buf_pool_t tbd_pool;
  paddr_t tbd_paddr;            /* phys addr of start of TBDs */
  sk_buff_t *rfd_head;
  sk_buff_t *rfd_tail;
  int skb_req;
  DWORD tx_per_underrun;
  WORD cur_line_speed;
  WORD cur_dplx_mode;
  DWORD cur_link_status;
  DWORD ans_line_speed;
  DWORD ans_dplx_mode;
  BYTE perm_node_address[ETHERNET_ADDRESS_LENGTH];
  DWORD pwa_no;                 /* PWA: xxxxxx-0xx */
  BYTE tx_thld;                 /* stores transmit threshold */
  BYTE prev_cu_cmd;             /* determines CU state - idle/susp */
  BYTE prev_scb_cmd;            /* last actual cmd */
  int brdcst_dsbl;              /* if set, disables broadcast */
  int mulcst_enbl;              /* if set, enables all multicast */
  int promisc;
  int prev_rx_mode;

  /* Changed for 82558 and 82559 enhancements */
  DWORD old_xmits;              /* number of good xmits */
  DWORD num_cna_interrupts;     /* number of interrupts */
  DWORD current_cna_backoff;    /* CNA Intr. Delay */
  DWORD xmits;                  /* num of xmits for e100_adjust_cid() */
  DWORD PhyState;
  DWORD PhyDelay;
  DWORD PhyId;
  WORD EEpromSize;

  /* PCI info */
  WORD ven_id;
  WORD dev_id;
  WORD sub_ven_id;
  WORD sub_dev_id;
  WORD pci_cmd_word;
  BYTE rev_id;
  BYTE dev_num;

  /* stuff out of the system dev structure */
  pci_dev_t *pci_dev;           /* pci device struct pointer */

  BYTE port_num;                /* For Ohio */
  BYTE ucode_loaded;            /* For rcv. inter ucode */

  /* flag to indicate Checksum offloading is enabled */
  BYTE checksum_offload_enabled;

  struct bdconfig *bdp;         /* pointer back to the bd_config */

  /* Linux statistics */
  struct net_device_stats net_stats;

  /* 82562EH variables */
  WORD Phy82562EHSampleCount;
  WORD Phy82562EHSampleDelay;
  WORD Phy82562EHSampleFilter;

  WORD rfd_size;
}
bdd_t , *pbdd_t;

enum zero_lock_state_e
{ ZLOCK_INITIAL, ZLOCK_READING, ZLOCK_SLEEPING };

struct bdconfig
{
#ifdef IANS
  void *iANSreserved;           /* reserved for ANS */
  iANSsupport_t *iANSdata;
#endif
  struct bdconfig *bd_next;     /* pointer to next bd in chain */
  struct bdconfig *bd_prev;     /* pointer to prev bd in chain */
  DWORD unit;                   /* from mdi_get_unit */
  DWORD io_start;               /* start of I/O base address       */
  DWORD io_end;                 /* end of I/O base address      */
  paddr_t mem_start;            /* start of base mem address       */
  paddr_t mem_end;              /* start of base mem address       */
  int irq_level;                /* interrupt request level      */
  int bd_number;                /* board number in multi-board setup */
  int flags;                    /* board management flags     */
  int tx_flags;                 /* tx management flags       */
  int rx_flags;                 /* rx management flags       */
  struct timer_list timer_id;   /* watchdog timer ID                  */
  int timer_val;                /* watchdog timer value              */
  int multicast_cnt;            /* count of multicast address sets   */
  e100_eaddr_t eaddr;           /* Ethernet address storage        */
  int Bound;                    /* flag for MAC_BIND_REQ */
  bdd_t *bddp;
  caddr_t rx_srv_count;
  caddr_t pci_dev_id;
  caddr_t irq_link;
  DWORD tx_count;

  /* these are linux only */
  struct sk_buff *last_mp_fail;
  device_t *device;
  BYTE pci_bus;
  BYTE pci_dev_fun;
  WORD vendor;
  int tx_out_res;
  enum zero_lock_state_e ZeroLockState;
  BYTE ZeroLockReadData[16];
  WORD ZeroLockReadCounter;
  DWORD ZeroLockSleepCounter;
#if (DEBUG_LINK > 0)
  WORD ErrorCounter;
#endif

};

#ifndef _BD_CONFIG_T_
#define _BD_CONFIG_T_
typedef struct bdconfig bd_config_t;
#endif

/* ====================================================================== */
/*                                externs                                 */
/* ====================================================================== */

/* 82562EH PHY Specific Registers */
#define PHY_82562EH_CONTROL_REG               0x10
/* 82562EH control register bit definitions. */
#define PHY_82562EH_CR_RAP            BIT_0
#define PHY_82562EH_CR_HP             BIT_1 /* High power 1=HIGH 0=LOW */
#define PHY_82562EH_CR_HS             BIT_2 /* High speed 1=HIGH 0=LOW */
#define PHY_82562EH_CR_IRC            BIT_15 /* Ignore Remote Commands */

/* 82562EH PHY address space constant define. */
#define PHY_82562EH_ADDR_SPACE_PAGE_0     0
#define PHY_82562EH_ADDR_SPACE_PAGE_1     1

/* 82562EH PHY Registers, Page 0 */
#define PHY_82562EH_STATUS_MODE_REG           0x11
#define PHY_82562EH_NOISE_PEAK_LVL_REG        0x18
#define PHY_82562EH_NSE_FLOOR_CEILING_REG     0x19
#define PHY_82562EH_NSE_ATTACK_EVENT_REG      0x1A
#define PHY_82562EH_RX_CONTROL_REG            0x1B

/* 82562EH PHY Registers, Page 1 */
#define PHY_82562EH_AFE_CONTROL1_REG          0x12
#define PHY_82562EH_AFE_CONTROL2_REG          0x13

/* 82562EH PHY Registers Initial Values, Page 0 */
#define PHY_82562EH_STATUS_MODE_INIT_VALUE    0x0100 /* Reg 0x11 */
#define PHY_82562EH_RX_CR_INIT_VALUE          0x0008 /* Reg 0x1B */

/* 82562EH PHY Registers Initial Values, Page 1 */
#define PHY_82562EH_AFE_CR1_INIT_VALUE        0x6600 /* Reg 0x12 */
#define PHY_82562EH_AFE_CR2_INIT_VALUE        0x0100 /* Reg 0x11 */

/* 82562EH PHY S/W Init Values */
#define PHY_82562EH_AFE_CR1_WA_INIT           0xA600 /* Page 1 Reg 0x12 */
#define PHY_82562EH_AFE_CR2_WA_INIT           0x0900 /* Page 1 Reg 0x11 */
#define PHY_82562EH_RX_CR_WA_INIT             0x0008 /* Page 0 Reg 0x1B */
#define PHY_82562EH_STATUS_MODE_WA_INIT       0x0100 /* Page 0 Reg 0x11 */
#define PHY_82562EH_NSE_ATTACK_EVENT_WA_INIT  0x0022 /* Page 0 Reg 0x1A */
#define PHY_82562EH_NOISE_PEAK_LVL_WA_INIT    0x7F00 /* Page 0 Reg 0x19 */

/* 82562EH PHY Working Mode Value */
#define PHY_82562EH_NSE_ATTACK_WORK_VALUE         0xFFF4 /* Reg 0x1A */
#define PHY_82562EH_AFE_CR1_WORK_VALUE            0x6600 /* Reg 0x12 */
#define PHY_82562EH_AFE_CR2_WORK_VALUE            0x0100 /* Reg 0x11 */

#define     MILLISECOND               1000
#define     PHY_82562EH_MAX_TABLE     20


/* ====================================================================== */
/*                              vendor_info                               */
/* ====================================================================== */

/* 
 * vendor_info_array
 *
 * This array contains the list of Subsystem IDs on which the driver
 * should load.
 *
 * The format of each entry of the array is as follows:
 * { VENDOR_ID, DEVICE_ID, SUBSYSTEM_VENDOR, SUBSYSTEM_DEV, REVISION, BRANDING_STRING }
 *
 * If there is a CATCHALL in the SUBSYSTEM_DEV field, the driver will
 * load on all subsystem device IDs of that vendor.
 * If there is a CATCHALL in the SUBSYSTEM_VENDOR field, the driver will
 * load on all cards, irrespective of the vendor.
 *
 * The last entry of the array must be:
 * { 0, 0, 0, 0, 0, null }
 */

#define CATCHALL 0xffff

typedef struct _e100_vendor_info_t
{
  WORD ven_id;
  WORD dev_id;
  WORD sub_ven;
  WORD sub_dev;
  WORD rev_id;
  char *idstr;                  // String to be printed for these IDs
} e100_vendor_info_t;

extern e100_vendor_info_t e100_vendor_info_array[];


/*************************************************************************/
/*            Receive Interrupt Bundling Microcode (CPUsaver)            */
/*************************************************************************/


/*************************************************************************
*  CPUSaver parameters
*
*  All CPUSaver parameters are 16-bit literals that are part of a
*  "move immediate value" instruction.  By changing the value of
*  the literal in the instruction before the code is loaded, the
*  driver can change algorithm.
*
*  CPUSAVER_DWORD - This is the location of the instruction that loads
*    the dead-man timer with its inital value.  By writing a 16-bit
*    value to the low word of this instruction, the driver can change
*    the timer value.  The current default is either x600 or x800;
*    experiments show that the value probably should stay within the
*    range of x200 - x1000.
*
*  CPUSAVER_BUNDLE_MAX_DWORD - This is the location of the instruction
*    that sets the maximum number of frames that will be bundled.  In
*    some situations, such as the TCP windowing algorithm, it may be
*    better to limit the growth of the bundle size than let it go as
*    high as it can, because that could cause too much added latency.
*    The default is six, because this is the number of packets in the
*    default TCP window size.  A value of 1 would make CPUSaver indicate
*    an interrupt for every frame received.  If you do not want to put
*    a limit on the bundle size, set this value to xFFFF.
*
*  CPUSAVER_MIN_SIZE_DWORD - This is the location of the instruction
*    that contains a bit-mask describing the minimum size frame that
*    will be bundled.  The default masks the lower 7 bits, which means
*    that any frame less than 128 bytes in length will not be bundled,
*    but will instead immediately generate an interrupt.  This does
*    not affect the current bundle in any way.  Any frame that is 128
*    bytes or large will be bundled normally.  This feature is meant
*    to provide immediate indication of ACK frames in a TCP environment.
*    Customers were seeing poor performance when a machine with CPUSaver
*    enabled was sending but not receiving.  The delay introduced when
*    the ACKs were received was enough to reduce total throughput, because
*    the sender would sit idle until the ACK was finally seen.
*
*    The current default is 0xFF80, which masks out the lower 7 bits.
*    This means that any frame which is x7F (127) bytes or smaller
*    will cause an immediate interrupt.  Because this value must be a 
*    bit mask, there are only a few valid values that can be used.  To
*    turn this feature off, the driver can write the value xFFFF to the
*    lower word of this instruction (in the same way that the other
*    parameters are used).  Likewise, a value of 0xF800 (2047) would
*    cause an interrupt to be generated for every frame, because all
*    standard Ethernet frames are <= 2047 bytes in length.
*************************************************************************/



/********************************************************/
/*  CPUSaver micro code for the D101A                   */
/********************************************************/

/*  Version 2.0  */

/*  This value is the same for both A and B step of 558.  */
#define D101_CPUSAVER_DWORD         72


#define     D101_A_RCVBUNDLE_UCODE \
{\
0x03B301BB, \
0x0046FFFF, \
0xFFFFFFFF, \
0x051DFFFF, \
0xFFFFFFFF, \
0xFFFFFFFF, \
0x000C0001, \
0x00101212, \
0x000C0008, \
0x003801BC, \
0x00000000, \
0x00124818, \
0x000C1000, \
0x00220809, \
0x00010200, \
0x00124818, \
0x000CFFFC, \
0x003803B5, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x0010009C, \
0x0024B81D, \
0x00130836, \
0x000C0001, \
0x0026081C, \
0x0020C81B, \
0x00130824, \
0x00222819, \
0x00101213, \
0x00041000, \
0x003A03B3, \
0x00010200, \
0x00101B13, \
0x00238081, \
0x00213049, \
0x0038003B, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x0010009C, \
0x0024B83E, \
0x00130826, \
0x000C0001, \
0x0026083B, \
0x00010200, \
0x00134824, \
0x000C0001, \
0x00101213, \
0x00041000, \
0x0038051E, \
0x00101313, \
0x00010400, \
0x00380521, \
0x00050600, \
0x00100824, \
0x00101310, \
0x00041000, \
0x00080600, \
0x00101B10, \
0x0038051E, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
}


/********************************************************/
/*  CPUSaver micro code for the D101B                   */
/********************************************************/

/*  Version 2.0  */

#define     D101_B0_RCVBUNDLE_UCODE \
{\
0x03B401BC, \
0x0047FFFF, \
0xFFFFFFFF, \
0x051EFFFF, \
0xFFFFFFFF, \
0xFFFFFFFF, \
0x000C0001, \
0x00101B92, \
0x000C0008, \
0x003801BD, \
0x00000000, \
0x00124818, \
0x000C1000, \
0x00220809, \
0x00010200, \
0x00124818, \
0x000CFFFC, \
0x003803B6, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x0010009C, \
0x0024B81D, \
0x0013082F, \
0x000C0001, \
0x0026081C, \
0x0020C81B, \
0x00130837, \
0x00222819, \
0x00101B93, \
0x00041000, \
0x003A03B4, \
0x00010200, \
0x00101793, \
0x00238082, \
0x0021304A, \
0x0038003C, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x0010009C, \
0x0024B83E, \
0x00130826, \
0x000C0001, \
0x0026083B, \
0x00010200, \
0x00134837, \
0x000C0001, \
0x00101B93, \
0x00041000, \
0x0038051F, \
0x00101313, \
0x00010400, \
0x00380522, \
0x00050600, \
0x00100837, \
0x00101310, \
0x00041000, \
0x00080600, \
0x00101790, \
0x0038051F, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
}


/********************************************************/
/*  CPUSaver micro code for the D101M (B-step only)     */
/********************************************************/

/*  Version 2.10  */

/*  Parameter values for the D101M B-step  */
#define D101M_CPUSAVER_DWORD                78
#define D101M_CPUSAVER_BUNDLE_MAX_DWORD     65
#define D101M_CPUSAVER_MIN_SIZE_DWORD       126


#define D101M_B_RCVBUNDLE_UCODE \
{\
0x00550215, \
0xFFFF0437, \
0xFFFFFFFF, \
0x06A70789, \
0xFFFFFFFF, \
0x0558FFFF, \
0x000C0001, \
0x00101312, \
0x000C0008, \
0x00380216, \
0x0010009C, \
0x00204056, \
0x002380CC, \
0x00380056, \
0x0010009C, \
0x00244C0B, \
0x00000800, \
0x00124818, \
0x00380438, \
0x00000000, \
0x00140000, \
0x00380555, \
0x00308000, \
0x00100662, \
0x00100561, \
0x000E0408, \
0x00134861, \
0x000C0002, \
0x00103093, \
0x00308000, \
0x00100624, \
0x00100561, \
0x000E0408, \
0x00100861, \
0x000C007E, \
0x00222C21, \
0x000C0002, \
0x00103093, \
0x00380C7A, \
0x00080000, \
0x00103090, \
0x00380C7A, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x0010009C, \
0x00244C2D, \
0x00010004, \
0x00041000, \
0x003A0437, \
0x00044010, \
0x0038078A, \
0x00000000, \
0x00100099, \
0x00206C7A, \
0x0010009C, \
0x00244C48, \
0x00130824, \
0x000C0001, \
0x00101213, \
0x00260C75, \
0x00041000, \
0x00010004, \
0x00130826, \
0x000C0006, \
0x002206A8, \
0x0013C926, \
0x00101313, \
0x003806A8, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00080600, \
0x00101B10, \
0x00050004, \
0x00100826, \
0x00101210, \
0x00380C34, \
0x00000000, \
0x00000000, \
0x0021155B, \
0x00100099, \
0x00206559, \
0x0010009C, \
0x00244559, \
0x00130836, \
0x000C0000, \
0x00220C62, \
0x000C0001, \
0x00101B13, \
0x00229C0E, \
0x00210C0E, \
0x00226C0E, \
0x00216C0E, \
0x0022FC0E, \
0x00215C0E, \
0x00214C0E, \
0x00380555, \
0x00010004, \
0x00041000, \
0x00278C67, \
0x00040800, \
0x00018100, \
0x003A0437, \
0x00130826, \
0x000C0001, \
0x00220559, \
0x00101313, \
0x00380559, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00130831, \
0x0010090B, \
0x00124813, \
0x000CFF80, \
0x002606AB, \
0x00041000, \
0x003806A8, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
}


/********************************************************/
/*  CPUSaver micro code for the D101S                   */
/********************************************************/

/*  Version 1.20  */

/*  Parameter values for the D101S  */
#define D101S_CPUSAVER_DWORD                78
#define D101S_CPUSAVER_BUNDLE_MAX_DWORD     67
#define D101S_CPUSAVER_MIN_SIZE_DWORD       129


#define D101S_RCVBUNDLE_UCODE \
{\
0x00550242, \
0xFFFF047E, \
0xFFFFFFFF, \
0x06FF0818, \
0xFFFFFFFF, \
0x05A6FFFF, \
0x000C0001, \
0x00101312, \
0x000C0008, \
0x00380243, \
0x0010009C, \
0x00204056, \
0x002380D0, \
0x00380056, \
0x0010009C, \
0x00244F8B, \
0x00000800, \
0x00124818, \
0x0038047F, \
0x00000000, \
0x00140000, \
0x003805A3, \
0x00308000, \
0x00100610, \
0x00100561, \
0x000E0408, \
0x00134861, \
0x000C0002, \
0x00103093, \
0x00308000, \
0x00100624, \
0x00100561, \
0x000E0408, \
0x00100861, \
0x000C007E, \
0x00222FA1, \
0x000C0002, \
0x00103093, \
0x00380F90, \
0x00080000, \
0x00103090, \
0x00380F90, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x0010009C, \
0x00244FAD, \
0x00010004, \
0x00041000, \
0x003A047E, \
0x00044010, \
0x00380819, \
0x00000000, \
0x00100099, \
0x00206FFD, \
0x0010009A, \
0x0020AFFD, \
0x0010009C, \
0x00244FC8, \
0x00130824, \
0x000C0001, \
0x00101213, \
0x00260FF8, \
0x00041000, \
0x00010004, \
0x00130826, \
0x000C0006, \
0x00220700, \
0x0013C926, \
0x00101313, \
0x00380700, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00080600, \
0x00101B10, \
0x00050004, \
0x00100826, \
0x00101210, \
0x00380FB6, \
0x00000000, \
0x00000000, \
0x002115A9, \
0x00100099, \
0x002065A7, \
0x0010009A, \
0x0020A5A7, \
0x0010009C, \
0x002445A7, \
0x00130836, \
0x000C0000, \
0x00220FE4, \
0x000C0001, \
0x00101B13, \
0x00229F8E, \
0x00210F8E, \
0x00226F8E, \
0x00216F8E, \
0x0022FF8E, \
0x00215F8E, \
0x00214F8E, \
0x003805A3, \
0x00010004, \
0x00041000, \
0x00278FE9, \
0x00040800, \
0x00018100, \
0x003A047E, \
0x00130826, \
0x000C0001, \
0x002205A7, \
0x00101313, \
0x003805A7, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00130831, \
0x0010090B, \
0x00124813, \
0x000CFF80, \
0x00260703, \
0x00041000, \
0x00380700, \
0x00000000, \
}


/********************************************************/
/*  CPUSaver micro code for the D102 B-step            */
/********************************************************/

/*  Version 1.00  */

/*  Parameter values for the D102 B-step  */
#define D102_B_CPUSAVER_DWORD                78
#define D102_B_CPUSAVER_BUNDLE_MAX_DWORD     67
#define D102_B_CPUSAVER_MIN_SIZE_DWORD       116


#define     D102_B_RCVBUNDLE_UCODE \
{\
0x006f0276, \
0xffff04d2, \
0xffffffff, \
0x0ed4ffff, \
0xffffffff, \
0x0d20ffff, \
0x00300001, \
0x0140D871, \
0x00300008, \
0x00E00277, \
0x01406C59, \
0x00804073, \
0x008700FA, \
0x00E00070, \
0x01406C59, \
0x00905F8B, \
0x00001000, \
0x01496F50, \
0x00E004D3, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00C08000, \
0x01600CA2, \
0x01600ADA, \
0x00380408, \
0x014D6FDA, \
0x00300002, \
0x0140DA72, \
0x00C08000, \
0x01700C0B, \
0x01600ADA, \
0x00380408, \
0x01406FDA, \
0x0030003E, \
0x00845FA1, \
0x00300002, \
0x0140DA72, \
0x00E01F90, \
0x00200000, \
0x0140DA6F, \
0x00E01F90, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x01406C51, \
0x0080DFF0, \
0x01406C52, \
0x00815FF0, \
0x01406C59, \
0x00905FC8, \
0x014C6FD9, \
0x00300001, \
0x0140D972, \
0x00941FEB, \
0x00102000, \
0x00048002, \
0x014C6FD8, \
0x00300006, \
0x00840ED5, \
0x014F71D8, \
0x0140D872, \
0x00E00ED5, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00200600, \
0x0140D76F, \
0x00148002, \
0x01406FD8, \
0x0140D96F, \
0x00E01FB6, \
0x00000000, \
0x00000000, \
0x00822D2B, \
0x01406C51, \
0x0080CD21, \
0x01406C52, \
0x00814D21, \
0x01406C59, \
0x00904D21, \
0x014C6FD7, \
0x00300000, \
0x00841FE0, \
0x00300001, \
0x0140D772, \
0x00E0129A, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00048002, \
0x00102000, \
0x00971FE5, \
0x00101000, \
0x00050200, \
0x00E804D2, \
0x014C6FD8, \
0x00300001, \
0x00840D21, \
0x0140D872, \
0x00E00D21, \
0x014C6F91, \
0x0150710B, \
0x01496F72, \
0x0030FF80, \
0x00940ED8, \
0x00102000, \
0x00E00ED5, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
}

/********************************************************/
/*  Micro code for the D102 C-step                      */
/********************************************************/

/*  Parameter values for the D102 C-step  */
#define D102_C_CPUSAVER_DWORD                46
#define D102_C_CPUSAVER_BUNDLE_MAX_DWORD     54
#define D102_C_CPUSAVER_MIN_SIZE_DWORD      133 /* not implemented */

#define     D102_C_RCVBUNDLE_UCODE \
{\
0x00700279, \
0x0e6604e2, \
0x02bf0cae, \
0x1519150c, \
0xFFFFFFFF, \
0xffffFFFF, \
0x00E014D8, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00E014DC, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00E014F4, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00E014E0, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00E014E7, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00141000, \
0x015D6F0D, \
0x00E002C0, \
0x00000000, \
0x00200600, \
0x00E0150D, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00300006, \
0x00E0151A, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
0x00000000, \
}

extern int e100_debug;

#endif
