#ifndef __3C990_H
#define __3C990_H

extern int tc990_debug;
extern int tc990_probe (struct device *dev);

#ifdef IMPLEMENT_3C990

#define UPDATE_INDEX(index, size, entries) do { \
          index += size; \
          if (index == (entries * size)) \
             index = 0; \
        } while (0)

#define POLYNOMIAL 0x04C11DB7

extern inline void *pci_alloc_consistent (size_t size, DWORD *bus_addr)
{
  void *virt_ptr = k_malloc (size);

  *bus_addr = VIRT_TO_BUS (virt_ptr);
  return (virt_ptr);
}

extern inline void pci_free_consistent (void *buf)
{
  k_free (buf);
}

/*
 * SoftNet
 */
#define dev_kfree_skb_irq(a)    dev_kfree_skb(a)
#define netif_wake_queue(dev)    clear_bit(0, &dev->tbusy)

#define netif_queue_stopped(dev) dev->tx_busy
#define netif_running(dev)       dev->start

#define tc990_VENDOR_ID 0x10B7

#define TX_ENTRIES        128
#define RX_ENTRIES        128
#define CMD_ENTRIES        64
#define RESPONSE_ENTRIES   64

/*
 * 3cr990 commands
 */
#define tc990_CMD_TX_ENABLE             0x1
#define tc990_CMD_TX_DISABLE            0x2
#define tc990_CMD_RX_ENABLE             0x3
#define tc990_CMD_RX_DISABLE            0x4
#define tc990_CMD_RX_FILT_WRITE         0x5
#define tc990_CMD_READ_STATS            0x7
#define tc990_CMD_XCVR_SELECT           0x13
#define tc990_CMD_MAX_PKT_SIZE_WRITE    0x1A
#define tc990_CMD_MCAST_HASH_MASK_WRITE 0x25
#define tc990_CMD_STATION_ADR_READ      0x27

/*
 * Descriptor type
 */
#define CMD_DSC_TYPE_CMD_FRAME       0x2
#define CMD_DSC_FRAME_VALID          (1 << 7)
#define CMD_DSC_RESPONSE_NEEDED      (1 << 6)
#define CMD_DSC_RESPONSE_NOT_NEEDED  0

#define RESPONSE_DSC_ERROR_SET       (1 << 6)
#define tc990_STATUS_CMD_FOUND       2

/*
 * 3cr990 registers
 */
#define tc990_SOFT_RESET_REG         0x0
#define tc990_INT_STATUS_REG         0x4
#define tc990_INT_ENABLE_REG         0x8
#define tc990_INT_MASK_REG           0xC
#define tc990_HostTo3XP_COMM_5_REG   0x1C
#define tc990_HostTo3XP_COMM_4_REG   0x20
#define tc990_HostTo3XP_COMM_3_REG   0x24
#define tc990_HostTo3XP_COMM_2_REG   0x28
#define tc990_HostTo3XP_COMM_1_REG   0x2C
#define tc990_HostTo3XP_COMM_0_REG   0x30
#define tc990_3XP2HOST_COMM_0_REG    0x40

#define tc990_MASK_ALL_INT        0xFFFFFFFF
#define tc990_UNMASK_ALL_INT      0x0
#define tc990_ENABLE_ALL_INT      0xFFFFFFEF
#define tc990_RESET_ALL           0x7F

/*
 * 3cr990 commands for handshake with firmware
 */
#define tc990_NULL_CMD                  0x00
#define tc990_BOOTCMD_REG_BOOT_RECORD   0x0FF
#define tc990_BOOTCMD_RUNTIME_IMAGE     0xFD
#define tc990_BOOTCMD_DOWNLOAD_COMPLETE 0xFB
#define tc990_BOOTCMD_SEGMENT_AVAILABLE 0xFC

/*
 * Waiting for boot signature
 */
#define tc990_WAITING_FOR_BOOT          0x7
#define tc990_WAITING_FOR_HOST_REQUEST  0xD
#define tc990_WAITING_FOR_SEGMENT       0x10

#define tc990_3XP_COMM_INT0  0x2

/*
 * Resource allocations
 */
#define TC990_SHARED_MEMORY_ALLOCATED           0x1
#define TC990_SLOT_MEMORY_ALLOCATED             0x2
#define TC990_BASE_ADDR_RESERVED                0x4
#define TC990_IO_MAPPED                         0x8
#define TC990_BASE_MEMORY_RESERVED              0x10
#define TC990_IRQ_RESERVED                      0x20
#define TC990_DEVICE_REGISTERED                 0x40
#define TC990_DEVICE_IN_ROOT_CHAIN              0x80
/*
 * Delay in microseconds to wait for the reset to be over
 */
#define tc990_WAIT_COUNTER        100000

#define ETHERNET_MAXIMUM_FRAME_SIZE 1514

#define tc990_RX_FILT_DIRECTED          0x1
#define tc990_RX_FILT_ALL_MULTICAST     0x2
#define tc990_RX_FILT_BROADCAST         0x4
#define tc990_RX_FILT_PROMISCUOUS       0x8
#define tc990_RX_FILT_HASH_MULTICAST    0x10

#define tc990_MULTICAST_BITS            0x3f

/*
 * Slot information
 */
typedef struct SLOT {
        DWORD physical_addr_lo;
        DWORD physical_addr_hi;
        DWORD virtual_addr_lo;
        DWORD virtual_addr_hi;
        DWORD buffer_length;
        DWORD skb;
      } SLOT;


typedef struct _tc990_RING {
        DWORD RingBase;           /* ring address (sharedmemory) */
        DWORD NoEntries;          /* number of entries in ring */
        DWORD LastWriteUL;        /* send: last insert */
      } tc990_RING;

typedef struct _TX_RING {
  DWORD RingBase;                 /* ring address (sharedmemory) */
  DWORD NoEntries;                /* number of entries in ring */
  DWORD LastWriteUL;              /* send: last insert */
  DWORD LastReadUL;               /* cleanup: last read - current read */
  DWORD WriteRegister;            /* register used to single NIC */
  DWORD PacketPendingNo;          /* Number of packets sent - not completed */
} TX_RING;

/*
 * This structure is updated by the driver and reaad by the f/w
 */
typedef struct _HOST_WRITE_INDEXES {
  volatile DWORD regRxHiReadUL;
  volatile DWORD regRxLoReadUL;
  volatile DWORD regRxBuffWriteUL;
  volatile DWORD regRespReadUL;
} HOST_WRITE_INDEXES;

/*
 * This structure is updated by the f/w and read by the driver
 */
typedef struct _HOST_READ_INDEXES {
  volatile DWORD regTxLoReadUL;
  volatile DWORD regTxHiReadUL;
  volatile DWORD regRxLoWriteUL;
  volatile DWORD regRxBuffReadUL;
  volatile DWORD regCmdReadUL;
  volatile DWORD regRespWriteUL;
  volatile DWORD regRxHiWriteUL;
} HOST_READ_INDEXES;

/*
 * Main variable structure
 */
typedef struct _HOST_VAR_S {
  HOST_WRITE_INDEXES hvWriteS;
  HOST_READ_INDEXES hvReadS;
} HOST_VAR_S;

typedef struct _HOST_INIT_S {
  HOST_VAR_S *hostVarsPS;       /* points to host variable data struct */
  DWORD hostVarsHiUL;
  DWORD hostTxLoStartUL;          /* Tx Lo priority ring */
  DWORD hostTxLoStartHiUL;
  DWORD hostTxLoSizeUL;
  DWORD hostTxHiStartUL;          /* Tx Hi priority ring */
  DWORD hostTxHiStartHiUL;
  DWORD hostTxHiSizeUL;
  DWORD hostRxLoStartUL;          /* Rx Lo priority ring */
  DWORD hostRxLoStartHiUL;
  DWORD hostRxLoSizeUL;
  DWORD hostRxFreeStartUL;        /* Rx free buffer ring */
  DWORD hostRxFreeStartHiUL;
  DWORD hostRxFreeSizeUL;
  DWORD hostCtrlStartUL;          /* Comand ring */
  DWORD hostCtrlStartHiUL;
  DWORD hostCtrlSizeUL;
  DWORD hostRespStartUL;          /* Command Response ring */
  DWORD hostRespStartHiUL;
  DWORD hostRespSizeUL;
  DWORD hostZeroWordUL;           /* (lo) physical address of zero word */
  DWORD hostZeroWordHiUL;         /* (Hi) used for dma */
  DWORD hostRxHiStartUL;          /* Rx Hi priority ring */
  DWORD hostRxHiStartHiUL;
  DWORD hostRxHiSizeUL;
} HOST_INIT_S;

/*
 * Receive descriptor. This structure is updated by the f/w
 */
typedef struct _RX_DSC {
  DWORD Flags:8;
  DWORD num_descriptor:8;
  DWORD FrameLength:16;
  DWORD virtual_addr_lo;
  DWORD virtual_addr_hi;
  DWORD RxStatus;
  WORD  FilterResults;
  WORD  res1;
  DWORD res2;
} RX_DSC;

/*
 * Receive free descriptor. This structure is filled by the driver to
 * give a buffer to the firmware
 */     
typedef struct _RX_FREE_DSC {
  DWORD physical_addr_lo;
  DWORD physical_addr_hi;
  DWORD virtual_addr_lo;
  DWORD virtual_addr_hi;
} RX_FREE_DSC;

/*
 * Usef for both commands and responses
 */
typedef struct {
  DWORD Flags:8;
  DWORD num_descriptor:8;
  DWORD Command:16;
  DWORD SequenceNo:16;
  DWORD Parameter1:16;
  DWORD Parameter2;
  DWORD Parameter3;
} CMD_DSC;

/*
 * Stats response labels 
 */
typedef struct {
  DWORD reserved;
  DWORD reservedB;
  DWORD pkt;
  DWORD byte;
} rtnTx;

typedef struct {
  DWORD reserved;
  DWORD deferred;
  DWORD lateCollision;
  DWORD collisions;
} rtnTxMore;

typedef struct {
  DWORD carrierLost;
  DWORD multCollision;
  DWORD reserved;
  DWORD reservedB;
} rtnTxCrit;

typedef struct {
  DWORD reserved;
  DWORD txFiltered;
  DWORD rxPkt;
  DWORD rxByte;
} rtnTxRx;

typedef struct {
  DWORD reserved;
  DWORD fifoOverrun;
  DWORD badSSD;
  DWORD crcError;
} rtnRx;

typedef struct {
  DWORD oversize;
  DWORD reserved;
  DWORD reservedB;
  DWORD reservedC;
} rtnRxGeneral;

typedef struct {
  DWORD filtered;
  DWORD reserved;
  DWORD reservedB;
  DWORD reservedC;
} rtnRxEtc;

typedef union {
  CMD_DSC nrml;                 /* "normal" response values */
  rtnTx tx;
  rtnTxMore txMore;
  rtnTxCrit txCrit;
  rtnTxRx txRx;
  rtnRx rx;
  rtnRxGeneral rxGeneral;
  rtnRxEtc rxEtc;
} RESPONSE_DSC;

/* individual descriptors for each Fragment of a packet
 */
typedef struct _TX_DESC {
  BYTE fragFlagsUC;
  BYTE fragReservedUC;
  WORD fragLenUW;

  DWORD fragHostAddrLoUL;
  DWORD fragHostAddrHiUL;
  DWORD fragSpareUL;
} TX_DESC;

typedef struct {
  BYTE frFlagsUC;
  BYTE frNumDescUC;               /* No of fragments in this packet */
  WORD frPktLen;
} TX_PKT_FLAGS;

typedef struct _TX_FRAME_DESC {
  union {
    TX_PKT_FLAGS frFlagsS;
    DWORD frFlagsUL;
  } frU;

  DWORD pktPtrLoUL;
  DWORD pktPtrHiUL;
  DWORD frProcFlagsUL;
} TX_FRAME_DESC;

/* bit defines for frFlagsUC
 */
#define FRAME_TYPE_FRAG_HDR 0x00 /* bottom 2 bits describe pkt */
#define FRAME_TYPE_PKT_HDR  0x01 /* bottom 2 bits describe pkt */

#define HOST_DESC_VALID     0x80 /* set when this pkt contains data */

typedef struct _tc990_FILE_HEADER {
  BYTE tagID[8];
  DWORD Version;
  DWORD NumSections;
  DWORD ExecuteAddress;
} tc990_FILE_HEADER;

typedef struct _tc990_FILE_SECTION {
  DWORD num_bytes;
  WORD checksum;
  WORD reserved;
  DWORD start_address;
} tc990_FILE_SECTION;

struct pci_id_info {
  const char *name;
  WORD device_id;
};

#endif /* IMPLEMENT_3C990 */
#endif
