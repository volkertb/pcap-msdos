/*****************************************************************************
 *****************************************************************************
Copyright (c) 1999 - 2001, Intel Corporation 

All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, 
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation 
    and/or other materials provided with the distribution.

 3. Neither the name of Intel Corporation nor the names of its contributors 
    may be used to endorse or promote products derived from this software 
    without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

 *****************************************************************************
 ****************************************************************************/

#ifndef _E1000_H_
#define _E1000_H_
 
 
/*
 *
 * e1000.h
 * 
 */

#ifndef E1000_MAIN_STATIC
#define __NO_VERSION__
#endif
#ifdef MODULE
#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <linux/module.h>
#endif

#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>

#include <linux/version.h>
#include <linux/string.h>
#include <linux/proc_fs.h>

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,3,18) )
#include <asm/spinlock.h>
#else
#include <linux/spinlock.h>
#endif

#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/irq.h>

#include "e1000_kcompat.h"
#include "e1000_fxhw.h"
#include "e1000_phy.h"

#ifdef IANS
#include "ans_driver.h"
#include "base_comm.h"
#endif

#define MDI

/* Unix Typedefs */
typedef unsigned char uchar_t;
typedef unsigned int uint_t;
typedef unsigned short ushort_t;
typedef unsigned long ulong_t;
typedef unsigned long paddr_t;
typedef ulong_t major_t;
typedef unsigned char boolean_t;
typedef unsigned long uintptr_t;

#define B_TRUE            1
#define B_FALSE         0
#define TRUE            1
#define FALSE            0

#ifndef _DEVICE_T
typedef struct net_device device_t;
#define _DEVICE_T
#endif
typedef struct pci_dev pci_dev_t;
typedef struct sk_buff sk_buff_t;
typedef struct net_device_stats net_device_stats_t;


/*
 * e1000_defines.h
 * 
 */

/* Supported RX Buffer Sizes */
/* Linux 2.2 TCP window size / NFS bug work-around */
#define E1000_RXBUFFER_2048		1522
#define E1000_RXBUFFER_4096		4096
#define E1000_RXBUFFER_8192		8192
#define E1000_RXBUFFER_16384	16384

/* Max and Min Number of Supported TX and RX Descriptors */ 
#define E1000_MAX_TXD	256
#define E1000_MIN_TXD	80
#define E1000_MAX_RXD	256
#define E1000_MIN_RXD	80
 
#define ETH_LENGTH_OF_ADDRESS 6

/* The number of entries in the CAM. The CAM (Content Addressable
* Memory) holds the directed and multicast addresses that we
* monitor. We reserve one of these spots for our directed address,
* allowing us E1000_CAM_ENTRIES - 1 multicast addresses.
*/


#define RET_STATUS_SUCCESS 0
#define RET_STATUS_FAILURE 1

/* E1000 specific hardware defines */
#define e1000regs E1000_REGISTERS
#define PE1000_ADAPTER bdd_t*

#ifdef MDI
#define   DL_MAC_ADDR_LEN 6

/*****************************************************************************
 * Structure for the Ethernet address ( 48 bits ) or 6 bytes
 *****************************************************************************/
typedef union {
    uchar_t bytes[DL_MAC_ADDR_LEN];
    ushort_t words[DL_MAC_ADDR_LEN / 2];
} DL_eaddr_t;

#define   DL_ID                  (ENETM_ID)
#define   ETHERNET_HEADER_SIZE   14

#if defined(DL_STRLOG) && !defined(lint)
#define DL_LOG(x)        DLstrlog ? x : 0
#else
#define DL_LOG(x)
#endif


#endif                            /* MDI */

/*
 * e1000_sw.h
 * 
 * This file has all the defines for all the software defined structures
 * that are necessary for the  hardware as well as the DLPI interface 
 */
#define E1000_PCI

/* The size in bytes of a standard ethernet header */
#define ENET_HEADER_SIZE    14
#define MAX_INTS            256
#define CRC_LENGTH 4 

#define MAXIMUM_ETHERNET_PACKET_SIZE    1514

#define MAX_NUM_MULTICAST_ADDRESSES     128
#define MAX_NUM_PHYS_FRAGS_PER_PACKET   16
#define E1000_NUM_MTA_REGISTERS         128
#define RCV_PKT_MUL              5

/*
 * This structure is used to define the characterstics of the message that is
 * sent by the upper layers for transmission. It contains the physical/virtual
 * address mapping for the physical fragments of the packet and also the length
 * of the fragment. 
 */
typedef struct _PHYS_ADDRESS_UNIT {
    caddr_t FragPhysAddr;
    caddr_t FragVirtAddr;
    UINT Length;
} PHYS_ADDRESS_UNIT, *PPHYS_ADDRESS_UNIT;


/*-----------------------------------------------------------------------------
 A structure that points to the next entry in the queue.

-----------------------------------------------------------------------------*/
typedef struct _SINGLE_LIST_LINK {
    struct _SINGLE_LIST_LINK *Flink;
} SINGLE_LIST_LINK, *PSINGLE_LIST_LINK;

/* This structure stores the additional information that
 * is associated with every packet to be transmitted. It stores
 * the message block pointer and the TBD addresses associated with
 * the m_blk and also the link to the next tcb in the chain
 */
typedef struct _TX_SW_PACKET_ {
    /* Link to the next TX_SW_PACKET in the list */
    SINGLE_LIST_LINK Link;

    sk_buff_t *Packet;            /* Pointer to message to be transmitted */

    ulong_t TxDescriptorBufferAddr;
    /* First buffer descriptor address */

    PE1000_TRANSMIT_DESCRIPTOR FirstDescriptor;
    PE1000_TRANSMIT_DESCRIPTOR LastDescriptor;
    UINT TxSwPacketNumber;
} TX_SW_PACKET, *PTX_SW_PACKET;



/* -----------------------------------------------------------------------------
* A "ListHead" structure that points to the head and tail of a queue
* --------------------------------------------------------------------------- */
typedef struct _LIST_DESCRIBER {
    struct _SINGLE_LIST_LINK *volatile Flink;
    struct _SINGLE_LIST_LINK *volatile Blink;
} LIST_DESCRIBER, *PLIST_DESCRIBER;


/* This structure is similar to the RX_SW_PACKET structure used for Ndis.
* This structure stores information about the 2k aligned receive buffer
* into which the E1000 DMA's frames. This structure is maintained as a 
* linked list of many receiver buffer pointers.
*/
typedef struct _RX_SW_PACKET {
    /* Link to the next RX_SW_PACKET in the list */
    SINGLE_LIST_LINK Link;

    struct sk_buff *skb;        /* Pointer to the received packet */

    /* A virtual pointer to the actual aligned buffer. */
    PVOID VirtualAddress;

    /* Debug only fields */
#if DBG
    PE1000_RECEIVE_DESCRIPTOR DescriptorVirtualAddress;
    UINT RxSwPacketNumber;
#endif

} RX_SW_PACKET, *PRX_SW_PACKET;

/* MultiCast Command Block (MULTICAST_CB)
*  The multicast structure contains an array of multicast addresses and 
*  also a count of the total number of addresses.
*/
typedef struct _multicast_cb_t {
    ushort mc_count;            /* Number of multicast addresses */
    uchar_t MulticastBuffer[(ETH_LENGTH_OF_ADDRESS *
                             MAX_NUM_MULTICAST_ADDRESSES)];
} mltcst_cb_t, *pmltcst_cb_t;


/* ----------------------------------------------------------------------------- QUEUE_INIT_LIST -- Macro which will initialize a queue to NULL.
---------------------------------------------------------------------------*/
#define QUEUE_INIT_LIST(_LH) \
    (_LH)->Flink = (_LH)->Blink = (PSINGLE_LIST_LINK)0

/*-----------------------------------------------------------------------------
* IS_QUEUE_EMPTY -- Macro which checks to see if a queue is empty.
*--------------------------------------------------------------------------- */
#define IS_QUEUE_EMPTY(_LH) \
    ((_LH)->Flink == (PSINGLE_LIST_LINK)0)


/* -------------------------------------------------------------------------
* QUEUE_GET_HEAD -- Macro which returns the head of the queue, but does not
*                   remove the head from the queue.
*------------------------------------------------------------------------- */
#define QUEUE_GET_HEAD(_LH) ((PSINGLE_LIST_LINK) ((_LH)->Flink))

/*-----------------------------------------------------------------------------
* QUEUE_REMOVE_HEAD -- Macro which removes the head of the head of a queue.
*-------------------------------------------------------------------------- */
#define QUEUE_REMOVE_HEAD(_LH) \
    { \
        PSINGLE_LIST_LINK ListElem; \
        if (ListElem = (_LH)->Flink) \
        { \
            if(!((_LH)->Flink = ListElem->Flink)) \
                (_LH)->Blink = (PSINGLE_LIST_LINK) 0; \
        } \
    }


/*-----------------------------------------------------------------------------
* QUEUE_POP_HEAD -- Macro which  will pop the head off of a queue (list), and
*                   return it (this differs from QUEUE_REMOVE_HEAD only in
*                   the 1st line).
*-----------------------------------------------------------------------------*/
#define QUEUE_POP_HEAD(_LH) \
    (PSINGLE_LIST_LINK) (_LH)->Flink; \
    { \
        PSINGLE_LIST_LINK ListElem; \
        if ((ListElem = (_LH)->Flink)) \
        { \
            if(!((_LH)->Flink = ListElem->Flink)) \
                (_LH)->Blink = (PSINGLE_LIST_LINK) 0; \
        } \
    }


/*-----------------------------------------------------------------------------
* QUEUE_GET_TAIL -- Macro which returns the tail of the queue, but does not
*                   remove the tail from the queue.
*-------------------------------------------------------------------------- */
#define QUEUE_GET_TAIL(_LH) ((PSINGLE_LIST_LINK)((_LH)->Blink))


/*-----------------------------------------------------------------------------
* QUEUE_PUSH_TAIL -- Macro which puts an element at the tail (end) of the queue
*
*--------------------------------------------------------------------------- */
#define QUEUE_PUSH_TAIL(_LH,_E) \
    if ((_LH)->Blink) \
    { \
        ((PSINGLE_LIST_LINK)(_LH)->Blink)->Flink = (PSINGLE_LIST_LINK)(_E); \
        (_LH)->Blink = (PSINGLE_LIST_LINK)(_E); \
    } else \
    { \
        (_LH)->Flink = \
            (_LH)->Blink = (PSINGLE_LIST_LINK)(_E); \
    } \
    (_E)->Flink = (PSINGLE_LIST_LINK)0;



/*-----------------------------------------------------------------------------
* QUEUE_PUSH_HEAD -- Macro which puts an element at the head of the queue.
*--------------------------------------------------------------------------- */
#define QUEUE_PUSH_HEAD(_LH,_E) \
    if (!((_E)->Flink = (_LH)->Flink)) \
    { \
        (_LH)->Blink = (PSINGLE_LIST_LINK)(_E); \
    } \
    (_LH)->Flink = (PSINGLE_LIST_LINK)(_E);


/* Ethernet Frame Structure */
/*- Ethernet 6-byte Address */
typedef struct _eth_address_t {
    uchar_t eth_node_addr[ETH_LENGTH_OF_ADDRESS];
} eth_address_t, *peth_address_t;

typedef enum {
	ANE_ENABLED = 0,
	ANE_DISABLED,
	ANE_SUSPEND
} ANE_state_t;

/* This is the hardware dependant part of the board config structure called
 bdd_t structure, it is very similar to the Adapter structure in the
 driver. The bd_config structure defined by the DLPI interface has 
 pointer to this structure
*/
#define             TX_LOCKUP      0x01
typedef struct _ADAPTER_STRUCT {
    /*  Hardware defines for the E1000 */
    PE1000_REGISTERS HardwareVirtualAddress;    /* e1000 registers */

    /* Node Ethernet address */
    uchar_t perm_node_address[ETH_LENGTH_OF_ADDRESS];
    UCHAR CurrentNetAddress[ETH_LENGTH_OF_ADDRESS];

    /* command line options */
    UCHAR AutoNeg;
    UCHAR ForcedSpeedDuplex;
    USHORT AutoNegAdvertised;
    UCHAR WaitAutoNegComplete;

    BOOLEAN GetLinkStatus;

    UCHAR MacType;
    UCHAR MediaType;
    UINT32 PhyId;
	UINT32 PhyAddress;
	
    /* PCI device info */
    UINT16 VendorId;
    UINT16 DeviceId;
    UINT16 SubSystemId;
    UINT16 SubVendorId;
	UINT32 PartNumber;

    UCHAR DmaFairness;

    BOOLEAN LinkStatusChanged;
    
    
    /* Flags for various hardware-software defines */
    u_int e1000_flags;            /* misc. flags */
    UINT32 AdapterStopped;        /* Adapter has been reset */
    UINT32 FullDuplex;            /* current duplex mode */
    UINT32 FlowControl;            /* Enable/Disable flow control */
	UINT32 OriginalFlowControl;
    UINT RxPciPriority;            /* Receive data priority */
    uint_t flags;                /* Misc board flags    */

    BOOLEAN LongPacket;            /* Long Packet Enable Status */
	UINT	RxBufferLen;

	UINT32	tag_mode;	/* iANS tag mode (none, IEEE, ISL) */

	#ifdef IANS
	IANS_BD_LINK_STATUS ans_link;
	UINT32 ans_speed;
	UINT32 ans_duplex;
	#endif

    /* PCI configuration parameters  */
    UINT PciCommandWord;        /* Stores boot-up PCI command word */
    UINT DeviceNum;                /* The bus+dev value for the card */
    UINT PciConfigMethod;        /* CONFIG1 or 2 for scan */
    UINT RevID;                    /* Revision ID for the board */
    UINT32 LinkIsActive;            /* Link status */
    UINT DoRxResetFlag;
    UINT AutoNegFailed;            /* Auto-negotiation status flag */
    UINT TxcwRegValue;            /* Original Transmit control word */
    UINT TxIntDelay;            /* Transmit Interrupt delay */
    UINT RxIntDelay;            /* Receive Interrupt delay */
    UINT MaxDpcCount;            /* Interrupt optimization count */

    UCHAR ExternalSerDes;

    /* PCI information for the adapter */
    ulong_t io_base;            /* IO base address */
    ulong_t mem_base;            /* Memory base address */
    uchar_t irq;                /* IRQ level */
    int irq_level;

    int bd_number;                /*  Board Number */

    /*  Transmit descriptor definitions */
    PE1000_TRANSMIT_DESCRIPTOR FirstTxDescriptor;
    /* transmit descriptor ring start */
    PE1000_TRANSMIT_DESCRIPTOR LastTxDescriptor;
    /* transmit descriptor ring end */
    PE1000_TRANSMIT_DESCRIPTOR NextAvailTxDescriptor;
    /* next free tmd */
    PE1000_TRANSMIT_DESCRIPTOR OldestUsedTxDescriptor;
    /* next tmd to reclaim (used) */
    UINT NumTxDescriptorsAvail;
    UINT NumTxDescriptors;
    PE1000_TRANSMIT_DESCRIPTOR e1000_tbd_data;
    /* pointer to buffer descriptor area */

    /* Transmit software structure definitions */
    PTX_SW_PACKET e1000_TxSwPacketPtrArea;
    /* pointer to TxSwPacket Area */

    /* Define SwTxPacket structures */
    LIST_DESCRIBER FreeSwTxPacketList;
    /* Points to the head and tail of the "Free" packet list */

    LIST_DESCRIBER UsedSwTxPacketList;
    /* Points to the head and tail of the "Used" packet list */

    /* Receive definitions */

    UINT NumRxDescriptors;
    UINT MulticastFilterType;

    /*  Receive descriptor definitions  */
    PE1000_RECEIVE_DESCRIPTOR FirstRxDescriptor;
    /* receive descriptor ring 1 start */

    PE1000_RECEIVE_DESCRIPTOR LastRxDescriptor;
    /* receive descriptor ring 1 end */

    PE1000_RECEIVE_DESCRIPTOR NextRxDescriptorToCheck;
    /* next chip rmd ring 1 */

    PE1000_RECEIVE_DESCRIPTOR e1000_rlast1p;
    /* last free rmd ring 1 */


    /* Receive software defintions */
    PRX_SW_PACKET RxSwPacketPointerArea;
    /* Pointer to memory space allocated for RX_SW_PACKET structures */

    PE1000_RECEIVE_DESCRIPTOR e1000_rbd_data;
    /* pointer to rx buffer descriptor area */

    /* Points to the head and tail of the "RxSwPacketList" */
    LIST_DESCRIBER RxSwPacketList;

    pmltcst_cb_t pmc_buff;
    /* pointer to multi cast addrs */
    UINT NumberOfMcAddresses;
    UINT MaxNumReceivePackets;
    UINT MaxRcvPktMinThresh;
    UINT MaxRcvPktMaxThresh;

    /* Lock defintions for the driver */
    spinlock_t bd_intr_lock;    /* Interrupt lock */
    spinlock_t bd_tx_lock;        /* Transmit lock  */

    UINT32 cur_line_speed;        /* current line speed */
    uchar_t brdcst_dsbl;        /* if set, disables broadcast */
    uchar_t TransmitTimeout;    /* detects transmit lockup */

    uchar_t ReportTxEarly;        /* 1 => RS bit, 0 => RPS bit */

    pci_dev_t *pci_dev;            /* pci device struct pointer */
    char id_string[256];
   /*************************************************************************  
    * Hardware Statistics from the E1000 registers 
    *************************************************************************/
    UINT RcvCrcErrors;
    /* Receive packets with CRC errors */

    UINT RcvSymbolErrors;
    /* Receive packets with symbol errors */

    UINT RcvMissedPacketsErrors;
    /* Packets missed due to receive FIFO full */

    UINT DeferCount;
    /* Packet not transmitted due to either of the
     * reasons- transmitter busy
     *            IPG timer not expired
     *            Xoff frame received
     *            link not up
     */

    UINT RcvSequenceErrors;
    /* Receive sequence errors on the wire */

    UINT RcvLengthErrors;
    /* Counts undersized and oversized packets */

    UINT RcvXonFrame;
    /* Receive XON frame */

    UINT TxXonFrame;
    /* Transmit XON frame */

    UINT RcvXoffFrame;
    /* Receive XOFF frame */

    UINT TxXoffFrame;
    /* Transmit XOFF frame */

    UINT Rcv64;
    /* Received packets of size 64 */

    UINT Rcv65;
    /* Received packets of size 65-127 */

    UINT Rcv128;
    /* Received packets of size 128-255 */

    UINT Rcv256;
    /* Received packets of size 256-511 */

    UINT Rcv512;
    /* Received packets of size 512-1023 */

    UINT Rcv1024;
    /* Received packets of size 1024-1522 */

    UINT GoodReceives;
    /* Non-error legal length packets */

    UINT RcvBroadcastPkts;
    /* Received broadcast packets    */

    UINT RcvMulticastPkts;
    /* Received multicast packets      */

    UINT GoodTransmits;
    /* Good packets transmitted       */

    UINT Rnbc;
    /* No Receive buffers available ( Receive queue full) */

    UINT RcvUndersizeCnt;
    /* Received packet is less than 64 bytes */

    UINT RcvOversizeCnt;
    /* Received packet greater than max ethernet packet */

    UINT RcvJabberCnt;
    /*   Received packet > max size + bad CRC */

    UINT TotPktRcv;
    /* Total packets received */

    UINT TotPktTransmit;
    /* Total packets transmitted */

    UINT TrsPkt64;
    /* 64 byte packet transmitted */

    UINT TrsPkt65;
    /* Transmitted pkt 65-127 bytes */

    UINT TrsPkt128;
    /* Transmitted pkt 128-255 bytes */

    UINT TrsPkt256;
    /* Transmitted pkt 256-511 */

    UINT TrsPkt512;
    /* Transmitted pkt 512-1023 */

    UINT TrsPkt1024;
    /* Transmitted Pkt 1024-1522 */

    UINT TrsMulticastPkt;
    /* Transmitted Multicast Packet */

    UINT TrsBroadcastPkt;
    /* Transmitted broadcast packet */

    UINT TxAbortExcessCollisions;
    /* Excessive Coillision count  */

    UINT TxLateCollisions;
    /* Late collision count      */

    UINT TotalCollisions;
    /* Total collision count   */

    UINT SingleCollisions;
    UINT MultiCollisions;
    UINT FCUnsupported;
    UINT RcvGoodOct;
    UINT TrsGoodOct;
    UINT RcvFragment;
    UINT RcvTotalOct;
    UINT TrsTotalOct;

    /* Livengood Statistics */
    UINT AlignmentErrors;
    UINT TotalRcvErrors;
    UINT TrsUnderRun;
    UINT TrsNoCRS;
    UINT CarrierExtErrors;
    UINT RcvDMATooEarly;

} ADAPTER_STRUCT, *PADAPTER_STRUCT;

/*- Ethernet 14-byte Header */
typedef struct _eth_header_t {
    uchar_t eth_dest[ETH_LENGTH_OF_ADDRESS];
    uchar_t eth_src[ETH_LENGTH_OF_ADDRESS];
    ushort eth_typelen;
} eth_header_t, *peth_header_t;






/*
 * e1000_dlpi.h
 * 
 *   This file supports all the defines that are specific to the Streams
 *   sturcture and the DLPI interface
 */

/* 
 *  STREAMS structures
 */
/*
 * All the DLPI functions that are global and have been pre-fixed with DL should
 * be given the prefix e1000 
 */
#define   DL_NAME      "e1000"
#define   DLdevflag    e1000devflag
#define   DLinfo       e1000info
#define   DLrminfo     e1000rminfo
#define   DLwminfo     e1000wminfo
#define   DLrinit      e1000rinit
#define   DLwinit      e1000winit
#define   DLstrlog     e1000strlog
#define   DLifstats    e1000ifstats
#define   DLboards     e1000boards
#define   DLconfig     e1000config
#define   DLid_string  e1000id_string
#define   MBLK_LEN( X )   (X->b_wptr - X->b_rptr)
#define   PHYS_ADDR( X )   vtop((caddr_t)( X ), NULL )
#define   MBLK_NEXT( X )   ( X->b_cont )

/*
 *  Flow control definnitions for STREAMS
 */
#define   DL_MIN_PACKET         0
#define   DL_MAX_PACKET         1500
#define   DL_MAX_PACKET_LLC      (DL_MAX_PACKET - 3)
#define   DL_MAX_PACKET_SNAP   (DL_MAX_PACKET_LLC - 5)
#define   DL_HIWATER            (64 * 1024)
#define   DL_LOWATER            (20 * 1024)    /* set to 20k */
#define   USER_MAX_SIZE         DL_MAX_PACKET
#define   USER_MIN_SIZE         46

#ifdef MDI
/*
 *  Flag definitions used for various states
 */
#define  DRV_LOADED        0x800
#define  BOARD_PRESENT     0x01
#define  BOARD_DISABLED    0x02
#define  PROMISCUOUS       0x100
#define  ALL_MULTI         0x200
#define  BOARD_OPEN        0x010
/*
 *  Board structure definition only necessary for a MDI driver. This structure
 *  defines all the NOS related fields that would be associated with each board.
 *  This definition is a part of the dlpi_ether header file for
 *  the UW212 driver, so it is not defined for the UW212 driver
 */
typedef struct bdconfig {
	#ifdef IANS
	void *iANSReserved;
	piANSsupport_t iANSdata;
	#endif
	struct bdconfig *bd_next;    /* pointer to next bd in chain */
    struct bdconfig *bd_prev;    /* pointer to prev bd in chain */
    uint unit;                    /* from mdi_get_unit                */
    major_t major;                /* major number for device            */
    ulong_t io_start;            /* start of I/O base address          */
    ulong_t io_end;                /* end of I/O base address            */
    paddr_t mem_start;            /* start of base mem address          */
    paddr_t mem_end;            /* start of base mem address          */
    int irq_level;                /* interrupt request level            */
    int open_cnt;
    int bd_number;                /* board number in multi-board setup */
    int flags;                    /* board management flags            */
    int tx_flags;                /* tx management flags                */
    int rx_flags;                /* rx management flags                */
    int OutQlen;                /* number of transmit pkts queued   */
    struct timer_list timer_id;    /* watchdog timer ID                  */
    int timer_val;                /* watchdog timer value             */
    int multicast_cnt;            /* count of multicast address sets  */
    DL_eaddr_t eaddr;            /* Ethernet address storage           */

    ADAPTER_STRUCT *bddp;        /* board struct     */
    mltcst_cb_t *mc_data;        /* pointer to the mc addr for the bd */

    uint_t tx_count;

    /* these are linux only */
    struct sk_buff *last_mp_fail;
    device_t *device;
    uchar_t pci_bus;
    uchar_t pci_dev_fun;
    ushort_t vendor;
    int tx_out_res;
    void *base_tx_tbds;
    void *base_rx_rbds;
    struct net_device_stats net_stats;

} bd_config_t;

#endif                            /* MDI */

/*************************************************************************
*                                                                        *
* Module Name:                                                           *
*   e1000_pci.h                                                          *
* Abstract:                                                              *
*   This header file contains PCI related constants and bit definitions. *
*                                                                        *
*   This driver runs on the following hardware:                          *
*   - E10001000 based PCI gigabit ethernet adapters (aka Kodiak)         *
*                                                                        *
* Environment:                                                           *
*   Kernel Mode -                                                        *
*                                                                        *
* Source History:                                                        *
*   The contents of this file is based somewhat on code developed for    *
*   Intel Pro/100 family (Speedo1 and Speedo3).                          *
*                                                                        *
*   March 7, 1997                                                        *
*   1st created - Ported from E100B pci.h file                           *
*                                                                        *
*************************************************************************/

/* typedefs for each NOS */
#if defined LINUX

/* PCI Device ID - for internal use */
#define PCI_DEV_NO              0x00FF
#define PCI_BUS_NO              0xFF00

/* max number of pci buses */
#define MAX_PCI_BUSES   0xFF

/* number of PCI config bytes to access */
#define PCI_BYTE   1
#define PCI_WORD   2
#define PCI_DWORD   4

/* PCI access methods */
#define P_CONF_T1   1
#define P_CONF_T2   2
#define P_TEST_PATN     0xCDEF

#define PO_DEV_NO       11
#define PO_BUS_NO       16
#define P_CSPACE   0x80000000
#endif


/*----------------------------------------------------------------------*/

#ifndef _PCI_H
#define _PCI_H


/* Maximum number of PCI devices */
#define PCI_MAX_DEVICES         32

/*-------------------------------------------------------------------------*/
/* PCI configuration hardware ports                                        */
/*-------------------------------------------------------------------------*/
#define CF1_CONFIG_ADDR_REGISTER    0x0CF8
#define CF1_CONFIG_DATA_REGISTER    0x0CFC
#define CF2_SPACE_ENABLE_REGISTER   0x0CF8
#define CF2_FORWARD_REGISTER        0x0CFA
#define CF2_BASE_ADDRESS            0xC000


/*-------------------------------------------------------------------------*/
/* Configuration Space Header                                              */
/*-------------------------------------------------------------------------*/
typedef struct _PCI_CONFIG_STRUC {
    USHORT PciVendorId;            /* PCI Vendor ID */
    USHORT PciDeviceId;            /* PCI Device ID */
    USHORT PciCommand;
    USHORT PciStatus;
    UCHAR PciRevisionId;
    UCHAR PciClassCode[3];
    UCHAR PciCacheLineSize;
    UCHAR PciLatencyTimer;
    UCHAR PciHeaderType;
    UCHAR PciBIST;
    ULONG PciBaseReg0;
    ULONG PciBaseReg1;
    ULONG PciBaseReg2;
    ULONG PciBaseReg3;
    ULONG PciBaseReg4;
    ULONG PciBaseReg5;
    ULONG PciCardbusCISPtr;
    USHORT PciSubSysVendorId;
    USHORT PciSubSysDeviceId;
    ULONG PciExpROMAddress;
    ULONG PciReserved2;
    ULONG PciReserved3;
    UCHAR PciInterruptLine;
    UCHAR PciInterruptPin;
    UCHAR PciMinGnt;
    UCHAR PciMaxLat;
} PCI_CONFIG_STRUC, *PPCI_CONFIG_STRUC;

/*-------------------------------------------------------------------------*/
/* PCI Configuration Space Register Offsets                                */
/* Refer To The PCI Specification For Detailed Explanations                */
/*-------------------------------------------------------------------------*/
#define PCI_VENDOR_ID_REGISTER      0x00    /* PCI Vendor ID Register */
#define PCI_DEVICE_ID_REGISTER      0x02    /* PCI Device ID Register */
#define PCI_CONFIG_ID_REGISTER      0x00    /* PCI Configuration ID Register */
#define PCI_COMMAND_REGISTER        0x04    /* PCI Command Register */
#define PCI_STATUS_REGISTER         0x06    /* PCI Status Register */
#define PCI_REV_ID_REGISTER         0x08    /* PCI Revision ID Register */
#define PCI_CLASS_CODE_REGISTER     0x09    /* PCI Class Code Register */
#define PCI_CACHE_LINE_REGISTER     0x0C    /* PCI Cache Line Register */

#define PCI_BIST_REGISTER           0x0F    /* PCI Built-In SelfTest Register */
#define PCI_BAR_0_REGISTER          0x10    /* PCI Base Address Register 0 */
#define PCI_BAR_1_REGISTER          0x14    /* PCI Base Address Register 1 */
#define PCI_BAR_2_REGISTER          0x18    /* PCI Base Address Register 2 */
#define PCI_BAR_3_REGISTER          0x1C    /* PCI Base Address Register 3 */
#define PCI_BAR_4_REGISTER          0x20    /* PCI Base Address Register 4 */
#define PCI_BAR_5_REGISTER          0x24    /* PCI Base Address Register 5 */
#define PCI_SUBVENDOR_ID_REGISTER   0x2C    /* PCI SubVendor ID Register */
#define PCI_SUBDEVICE_ID_REGISTER   0x2E    /* PCI SubDevice ID Register */
#define PCI_EXPANSION_ROM           0x30    /* PCI Expansion ROM Base Register */
#define PCI_MIN_GNT_REGISTER        0x3E    /* PCI Min-Gnt Register */
#define PCI_MAX_LAT_REGISTER        0x3F    /* PCI Max_Lat Register */
#define PCI_TRDY_TIMEOUT_REGISTER   0x40    /* PCI TRDY Timeout Register */
#define PCI_RETRY_TIMEOUT_REGISTER  0x41    /* PCI Retry Timeout Register */



/*-------------------------------------------------------------------------*/
/* PCI Class Code Definitions                                              */
/* Configuration Space Header                                              */
/*-------------------------------------------------------------------------*/
#define PCI_BASE_CLASS      0x02    /* Base Class - Network Controller */
#define PCI_SUB_CLASS       0x00    /* Sub Class - Ethernet Controller */
#define PCI_PROG_INTERFACE  0x00    /* Prog I/F - Ethernet COntroller */

/*-------------------------------------------------------------------------*/
/* PCI Command Register Bit Definitions */
/* Configuration Space Header */
/*-------------------------------------------------------------------------*/
#define CMD_IO_SPACE                0x0001    /* BIT_0 */
#define CMD_MEMORY_SPACE            0x0002    /* BIT_1 */
#define CMD_BUS_MASTER              0x0004    /* BIT_2 */
#define CMD_SPECIAL_CYCLES          0x0008    /* BIT_3 */
#define CMD_MEM_WRT_INVALIDATE      0x0010    /* BIT_4 */
#define CMD_VGA_PALLETTE_SNOOP      0x0020    /* BIT_5 */
#define CMD_PARITY_RESPONSE         0x0040    /* BIT_6 */
#define CMD_WAIT_CYCLE_CONTROL      0x0080    /* BIT_7 */
#define CMD_SERR_ENABLE             0x0100    /* BIT_8 */
#define CMD_BACK_TO_BACK            0x0200    /* BIT_9 */

/*-------------------------------------------------------------------------*/
/* PCI Status Register Bit Definitions                                     */
/* Configuration Space Header                                              */
/*-------------------------------------------------------------------------*/
#define STAT_BACK_TO_BACK           0x0080    /* BIT_7 */
#define STAT_DATA_PARITY            0x0100    /* BIT_8 */
#define STAT_DEVSEL_TIMING          0x0600    /* BIT_9_10 */
#define STAT_SIGNAL_TARGET_ABORT    0x0800    /* BIT_11 */
#define STAT_RCV_TARGET_ABORT       0x1000    /* BIT_12 */
#define STAT_RCV_MASTER_ABORT       0x2000    /* BIT_13 */
#define STAT_SIGNAL_MASTER_ABORT    0x4000    /* BIT_14 */
#define STAT_DETECT_PARITY_ERROR    0x8000    /* BIT_15 */

/*-------------------------------------------------------------------------*/
/* PCI Base Address Register For Memory (BARM) Bit Definitions */
/* Configuration Space Header */
/*-------------------------------------------------------------------------*/
#define BARM_LOCATE_BELOW_1_MEG     0x0002    /* BIT_1 */
#define BARM_LOCATE_IN_64_SPACE     0x0004    /* BIT_2 */
#define BARM_PREFETCHABLE           0x0008    /* BIT_3 */

/*-------------------------------------------------------------------------*/
/* PCI Base Address Register For I/O (BARIO) Bit Definitions               */
/* Configuration Space Header                                              */
/*-------------------------------------------------------------------------*/
#define BARIO_SPACE_INDICATOR       0x0001    /* BIT_0 */

/*-------------------------------------------------------------------------*/
/* PCI BIOS Definitions                                                    */
/* Refer To The PCI BIOS Specification                                     */
/*-------------------------------------------------------------------------*/
/*- Function Code List */
#define PCI_FUNCTION_ID         0xB1    /* AH Register */
#define PCI_BIOS_PRESENT        0x01    /* AL Register */
#define FIND_PCI_DEVICE         0x02    /* AL Register */
#define FIND_PCI_CLASS_CODE     0x03    /* AL Register */
#define GENERATE_SPECIAL_CYCLE  0x06    /* AL Register */
#define READ_CONFIG_BYTE        0x08    /* AL Register */
#define READ_CONFIG_WORD        0x09    /* AL Register */
#define READ_CONFIG_DWORD       0x0A    /* AL Register */
#define WRITE_CONFIG_BYTE       0x0B    /* AL Register */
#define WRITE_CONFIG_WORD       0x0C    /* AL Register */
#define WRITE_CONFIG_DWORD      0x0D    /* AL Register */

/*- Function Return Code List */
#define SUCCESSFUL              0x00
#define FUNC_NOT_SUPPORTED      0x81
#define BAD_VENDOR_ID           0x83
#define DEVICE_NOT_FOUND        0x86
#define BAD_REGISTER_NUMBER     0x87

/*- PCI BIOS Calls */
#define PCI_BIOS_INTERRUPT      0x1A    /* PCI BIOS Int 1Ah Function Call */
#define PCI_PRESENT_CODE        0x20494350    /* Hex Equivalent Of 'PCI ' */

#define PCI_SERVICE_IDENTIFIER  0x49435024    /* ASCII Codes for 'ICP$' */

/*-------------------------------------------------------------------------*/
/* Device and Vendor IDs                                                   */
/*-------------------------------------------------------------------------*/
#define E1000_DEVICE_ID        0x1000
#define WISEMAN_DEVICE_ID      0x1000
#define LIVENGOOD_FIBER_DEVICE_ID    0x1001
#define LIVENGOOD_COPPER_DEVICE_ID   0x1004
#define E1000_VENDOR_ID        0x8086

#define SPLASH_DEVICE_ID        0x1226
#define SPLASH_VENDOR_ID        0x8086
#define SPEEDO_DEVICE_ID        0x1227
#define SPEEDO_VENDOR_ID        0x8086
#define D100_DEVICE_ID          0x1229
#define D100_VENDOR_ID          0x8086
#define NITRO3_DEVICE_ID        0x5201
#define NITRO3_VENDOR_ID        0x8086
#define XXPS_BRIDGE_DEVICE_ID   0x1225
#define XXPS_BRIDGE_VENDOR_ID   0x8086
#define OPB0_BRIDGE_DEVICE_ID   0x84C4
#define OPB0_BRIDGE_VENDOR_ID   0x8086


#endif                            /* PCI_H */




/*
 * e1000_externs.h
 * 
 *  This file has all the defines for the functions in various header files
 */
#ifdef E1000_MAIN_STATIC
static void e1000_print_brd_conf(bd_config_t *);
static int e1000_init(bd_config_t *);
static int e1000_runtime_init(bd_config_t *);

static boolean_t e1000_sw_init(bd_config_t *);
static void *malloc_contig(int);
static void free_contig(void *);
static void e1000_dealloc_space(bd_config_t *);
static bd_config_t *e1000_alloc_space(void);

static boolean_t e1000_find_pci_device(pci_dev_t *, PADAPTER_STRUCT);

static void e1000_watchdog(device_t *);
static void e1000_intr(int, void *, struct pt_regs *);

static void SetupTransmitStructures(PADAPTER_STRUCT, boolean_t);
static int SetupReceiveStructures(bd_config_t *, boolean_t, boolean_t);
static int ReadNodeAddress(PADAPTER_STRUCT, PUCHAR);
static int e1000_set_promisc(bd_config_t *, int flag);
static void e1000DisableInterrupt(PADAPTER_STRUCT);
static void e1000EnableInterrupt(PADAPTER_STRUCT);
static void e1000DisableInterrupt(PADAPTER_STRUCT);
static void ProcessTransmitInterrupts(bd_config_t *);
static void ProcessReceiveInterrupts(bd_config_t *);
static void UpdateStatsCounters(bd_config_t *);
static uint_t SendBuffer(PTX_SW_PACKET, bd_config_t *);

int e1000_probe(void);
static int e1000_open(device_t *);
static int e1000_close(device_t *);
static int e1000_xmit_frame(struct sk_buff *, device_t *);
static struct net_device_stats *e1000_get_stats(device_t *);
static int e1000_change_mtu(device_t *, int);
static int e1000_set_mac(device_t *, void *);
static void e1000_set_multi(device_t *);
static void e1000_check_options(int board);
static boolean_t DetectKnownChipset(PADAPTER_STRUCT);
static int e1000_GetBrandingMesg(PADAPTER_STRUCT Adapter);
#endif
extern void AdapterStop(PADAPTER_STRUCT);
extern ushort_t ReadEepromWord(PADAPTER_STRUCT, ushort_t);
extern boolean_t ValidateEepromChecksum(PADAPTER_STRUCT);
extern void CheckForLink(PADAPTER_STRUCT);
extern BOOLEAN InitializeHardware(PADAPTER_STRUCT Adapter);
extern BOOLEAN SetupFlowControlAndLink(PADAPTER_STRUCT Adapter);
extern VOID GetSpeedAndDuplex(PADAPTER_STRUCT Adapter, PUINT16 Speed, PUINT16 Duplex);
extern VOID ConfigFlowControlAfterLinkUp(PADAPTER_STRUCT Adapter);
extern VOID ClearHwStatsCounters(PADAPTER_STRUCT Adapter);

/*************************************************************************
*                                                                        *
* Module Name:                                                           *
*   pci.h                                                                *
* Abstract:                                                              *
*   This header file contains PCI related constants and bit definitions. *
*                                                                        *
*   This driver runs on the following hardware:                          *
*   - E10001000 based PCI gigabit ethernet adapters (aka Kodiak)         *
*                                                                        *
* Environment:                                                           *
*   Kernel Mode -                                                        *
*                                                                        *
* Source History:                                                        *
*   The contents of this file is based somewhat on code developed for    *
*   Intel Pro/100 family (Speedo1 and Speedo3).                          *
*                                                                        *
*   March 7, 1997                                                        *
*   1st created - Ported from E100B pci.h file                           *
*                                                                        *
*************************************************************************/
#ifndef _E1K_PCI_H
#define _E1K_PCI_H

/*-------------------------------------------------------------------------*/
/* Device and Vendor IDs                                                   */
/*-------------------------------------------------------------------------*/
#define E1000_DEVICE_ID        0x1000
#define E1000_VENDOR_ID        0x8086

#define SPLASH_DEVICE_ID        0x1226
#define SPLASH_VENDOR_ID        0x8086
#define SPEEDO_DEVICE_ID        0x1227
#define SPEEDO_VENDOR_ID        0x8086
#define D100_DEVICE_ID          0x1229
#define D100_VENDOR_ID          0x8086
#define NITRO3_DEVICE_ID        0x5201
#define NITRO3_VENDOR_ID        0x8086
#define XXPS_BRIDGE_DEVICE_ID   0x1225
#define XXPS_BRIDGE_VENDOR_ID   0x8086
#define OPB0_BRIDGE_DEVICE_ID   0x84C4
#define OPB0_BRIDGE_VENDOR_ID   0x8086

#define INTEL_440BX_AGP         0x7192
#define INTEL_440BX             0x7190
#define INTEL_440GX             0x71A0
#define INTEL_440LX_EX          0x7180
#define INTEL_440FX             0x1237
#define INTEL_430TX             0x7100
#define INTEL_450NX_PXB         0x84CB
#define INTEL_450KX_GX_PB       0x84C4


/**********************************************************************
** Other component's device number in PCI config space
**********************************************************************/

#define PXB_0A_DEVNO            0x12
#define PXB_0B_DEVNO            0x13
#define PXB_1A_DEVNO            0x14
#define PXB_1B_DEVNO            0x15

#define PB0_DEVNO               0x19
#define PB1_DEVNO               0x1A

#define PXB_C0_REV_ID           0x4


#endif                            /* PCI_H */
#endif                            /* _E1000_H_ */
