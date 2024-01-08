/*
 * 
 * 8139too.c: A RealTek RTL-8139 Fast Ethernet driver for Linux.
 * 
 * Maintained by Jeff Garzik <jgarzik@mandrakesoft.com>
 * Copyright 2000,2001 Jeff Garzik
 * 
 * Much code comes from Donald Becker's rtl8139.c driver,
 * versions 1.13 and older.  This driver was originally based
 * on rtl8139.c version 1.07.  Header of rtl8139.c version 1.13:
 * 
 * -----<snip>-----
 * 
 * Written 1997-2001 by Donald Becker.
 * This software may be used and distributed according to the
 * terms of the GNU General Public License (GPL), incorporated
 * herein by reference.  Drivers based on or derived from this
 * code fall under the GPL and must retain the authorship,
 * copyright and license notice.  This file is not a complete
 * program and may only be used when the entire operating
 * system is licensed under the GPL.
 * 
 * This driver is for boards based on the RTL8129 and RTL8139
 * PCI ethernet chips.
 * 
 * The author may be reached as becker@scyld.com, or C/O Scyld
 * Computing Corporation 410 Severn Ave., Suite 210 Annapolis
 * MD 21403
 * 
 * Support and updates available at
 * http://www.scyld.com/network/rtl8139.html
 * 
 * Twister-tuning table provided by Kinston
 * <shangh@realtek.com.tw>.
 * 
 * -----<snip>-----
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 * 
 * Contributors:
 * 
 * Donald Becker - he wrote the original driver, kudos to him!
 * (but please don't e-mail him for support, this isn't his driver)
 * 
 * Tigran Aivazian - bug fixes, skbuff free cleanup
 * 
 * Martin Mares - suggestions for PCI cleanup
 * 
 * David S. Miller - PCI DMA and softnet updates
 * 
 * Ernst Gill - fixes ported from BSD driver
 * 
 * Daniel Kobras - identified specific locations of
 * posted MMIO write bugginess
 * 
 * Gerard Sharp - bug fix, testing and feedback
 * 
 * David Ford - Rx ring wrap fix
 * 
 * Dan DeMaggio - swapped RTL8139 cards with me, and allowed me
 * to find and fix a crucial bug on older chipsets.
 * 
 * Donald Becker/Chris Butterworth/Marcus Westergren -
 * Noticed various Rx packet size-related buglets.
 * 
 * Santiago Garcia Mantinan - testing and feedback
 * 
 * Jens David - 2.2.x kernel backports
 * 
 * Martin Dennett - incredibly helpful insight on undocumented
 * features of the 8139 chips
 * 
 * Jean-Jacques Michel - bug fix
 * 
 * Tobias Ringstr�m - Rx interrupt status checking suggestion
 * 
 * Andrew Morton - Clear blocked signals, avoid
 * buffer overrun setting current->comm.
 * 
 * Kalle Olavi Niemitalo - Wake-on-LAN ioctls
 * 
 * Robert Kuebel - Save kernel thread from dying on any signal.
 * 
 * Submitting bug reports:
 * 
 * "rtl8139-diag -mmmaaavvveefN" output
 * enable RTL8139_DEBUG below, and look at 'dmesg' or kernel log
 * 
 * See 8139too.txt for more details.
 * 
 */

#include "pmdrvr.h"
#include "bios32.h"
#include "pci.h"
#include "module.h"

int rtl8139_debug = 1;

#define DRV_NAME             "8139too"
#define DRV_VERSION          "0.9.22a"
#define RTL8139_DRIVER_NAME  DRV_NAME " Fast Ethernet driver " DRV_VERSION
#define PFX DRV_NAME         ": "

/* enable PIO instead of MMIO, if CONFIG_8139TOO_PIO is selected */
#ifdef CONFIG_8139TOO_PIO
#define USE_IO_OPS 1
#endif

/* define to 1 to enable copious debugging info */
#undef RTL8139_DEBUG

/* define to 1 to disable lightweight runtime debugging checks */
#undef RTL8139_NDEBUG


#ifdef RTL8139_DEBUG
/* note: prints function name for you */
#  define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

#ifdef RTL8139_NDEBUG
#  define assert(expr) do {} while (0)
#else
#  define assert(expr) \
        if(!(expr)) {					\
        printk( "Assertion failed! %s,%s,%s,line=%d\n",	\
        #expr,__FILE__,__FUNCTION__,__LINE__);		\
        }
#endif


/* A few user-configurable values. */
/* media options */
#define MAX_UNITS 8
static int media[MAX_UNITS] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int full_duplex[MAX_UNITS] = { -1, -1, -1, -1, -1, -1, -1, -1 };

/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static int max_interrupt_work = 20;

/* Maximum number of multicast addresses to filter (vs. Rx-all-multicast).
 * The RTL chips use a 64 element hash table based on the Ethernet CRC.  */
static int multicast_filter_limit = 32;

/* Size of the in-memory receive ring. */
#define RX_BUF_LEN_IDX	2        /* 0==8K, 1==16K, 2==32K, 3==64K */
#define RX_BUF_LEN	(8192 << RX_BUF_LEN_IDX)
#define RX_BUF_PAD	16
#define RX_BUF_WRAP_PAD 2048    /* spare padding to handle lack of packet wrap */
#define RX_BUF_TOT_LEN	(RX_BUF_LEN + RX_BUF_PAD + RX_BUF_WRAP_PAD)

/* Number of Tx descriptor registers. */
#define NUM_TX_DESC	4

/* max supported ethernet frame size -- must be at least (dev->mtu+14+4). */
#define MAX_ETH_FRAME_SIZE	1536

/* Size of the Tx bounce buffers -- must be at least (dev->mtu+14+4). */
#define TX_BUF_SIZE	MAX_ETH_FRAME_SIZE
#define TX_BUF_TOT_LEN	(TX_BUF_SIZE * NUM_TX_DESC)

/* PCI Tuning Parameters
 * Threshold is bytes transferred to chip before transmission starts. */
#define TX_FIFO_THRESH 256      /* In bytes, rounded down to 32 byte units. */

/* The following settings are log_2(bytes)-4:  0 == 16 bytes .. 6==1024, 7==end of packet. */
#define RX_FIFO_THRESH	7        /* Rx buffer level before first PCI xfer.  */
#define RX_DMA_BURST	7          /* Maximum PCI burst, '6' is 1024 */
#define TX_DMA_BURST	6          /* Maximum PCI burst, '6' is 1024 */
#define TX_RETRY	8              /* 0-15.  retries = 16 + (TX_RETRY * 16) */

/* Operational parameters that usually are not changed. */
/* Time in jiffies before concluding the transmitter is hung. */
#define TX_TIMEOUT  (6*HZ)

enum
{
  HAS_MII_XCVR = 0x010000,
  HAS_CHIP_XCVR = 0x020000,
  HAS_LNK_CHNG = 0x040000,
};

#define RTL_MIN_IO_SIZE 0x80
#define RTL8139B_IO_SIZE 256

#define RTL8129_CAPS	HAS_MII_XCVR
#define RTL8139_CAPS	HAS_CHIP_XCVR|HAS_LNK_CHNG

typedef enum
{
  RTL8139 = 0,
  RTL8139_CB,
  SMC1211TX,
  /*MPX5030, */
  DELTA8139,
  ADDTRON8139,
  DFE538TX,
  DFE690TXD,
  RTL8129,
}
board_t;


/* indexed by board_t, above */
static struct {
       const char *name;
       DWORD hw_flags;
     } board_info[] __devinitdata =
{
  {
  "RealTek RTL8139 Fast Ethernet", RTL8139_CAPS}
  ,
  {
  "RealTek RTL8139B PCI/CardBus", RTL8139_CAPS}
  ,
  {
  "SMC1211TX EZCard 10/100 (RealTek RTL8139)", RTL8139_CAPS}
  ,
    /* { MPX5030, "Accton MPX5030 (RealTek RTL8139)", RTL8139_CAPS }, */
  {
  "Delta Electronics 8139 10/100BaseTX", RTL8139_CAPS}
  ,
  {
  "Addtron Technolgy 8139 10/100BaseTX", RTL8139_CAPS}
  ,
  {
  "D-Link DFE-538TX (RealTek RTL8139)", RTL8139_CAPS}
  ,
  {
  "D-Link DFE-690TXD (RealTek RTL8139)", RTL8139_CAPS}
  ,
  {
  "RealTek RTL8129", RTL8129_CAPS}
,};


static struct pci_device_id rtl8139_pci_tbl[] __devinitdata = {
  {0x10ec, 0x8139, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RTL8139},
  {0x10ec, 0x8138, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RTL8139_CB},
  {0x1113, 0x1211, PCI_ANY_ID, PCI_ANY_ID, 0, 0, SMC1211TX},
  /* {0x1113, 0x1211, PCI_ANY_ID, PCI_ANY_ID, 0, 0, MPX5030 }, */
  {0x1500, 0x1360, PCI_ANY_ID, PCI_ANY_ID, 0, 0, DELTA8139},
  {0x4033, 0x1360, PCI_ANY_ID, PCI_ANY_ID, 0, 0, ADDTRON8139},
  {0x1186, 0x1300, PCI_ANY_ID, PCI_ANY_ID, 0, 0, DFE538TX},
  {0x1186, 0x1340, PCI_ANY_ID, PCI_ANY_ID, 0, 0, DFE690TXD},

#ifdef CONFIG_8139TOO_8129
  {0x10ec, 0x8129, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RTL8129},
#endif

  /* some crazy cards report invalid vendor ids like
   * 0x0001 here.  The other ids are valid and constant,
   * so we simply don't match on the main vendor id.
   */
  {PCI_ANY_ID, 0x8139, 0x10ec, 0x8139, 0, 0, RTL8139},
  {PCI_ANY_ID, 0x8139, 0x1186, 0x1300, 0, 0, DFE538TX},

  {0,}
};


/* The rest of these values should never change. */

/* Symbolic offsets to registers. */
enum RTL8139_registers
{
  MAC0 = 0,                     /* Ethernet hardware address. */
  MAR0 = 8,                     /* Multicast filter. */
  TxStatus0 = 0x10,             /* Transmit status (Four 32bit registers). */
  TxAddr0 = 0x20,               /* Tx descriptors (also four 32bit). */
  RxBuf = 0x30,
  ChipCmd = 0x37,
  RxBufPtr = 0x38,
  RxBufAddr = 0x3A,
  IntrMask = 0x3C,
  IntrStatus = 0x3E,
  TxConfig = 0x40,
  ChipVersion = 0x43,
  RxConfig = 0x44,
  Timer = 0x48,                 /* A general-purpose counter. */
  RxMissed = 0x4C,              /* 24 bits valid, write clears. */
  Cfg9346 = 0x50,
  Config0 = 0x51,
  Config1 = 0x52,
  FlashReg = 0x54,
  MediaStatus = 0x58,
  Config3 = 0x59,
  Config4 = 0x5A,               /* absent on RTL-8139A */
  HltClk = 0x5B,
  MultiIntr = 0x5C,
  TxSummary = 0x60,
  BasicModeCtrl = 0x62,
  BasicModeStatus = 0x64,
  NWayAdvert = 0x66,
  NWayLPAR = 0x68,
  NWayExpansion = 0x6A,
  /* Undocumented registers, but required for proper operation. */
  FIFOTMS = 0x70,               /* FIFO Control and test. */
  CSCR = 0x74,                  /* Chip Status and Configuration Register. */
  PARA78 = 0x78,
  PARA7c = 0x7c,                /* Magic transceiver parameter register. */
  Config5 = 0xD8,               /* absent on RTL-8139A */
};

enum ClearBitMasks
{
  MultiIntrClear = 0xF000,
  ChipCmdClear = 0xE2,
  Config1Clear = (1 << 7) | (1 << 6) | (1 << 3) | (1 << 2) | (1 << 1),
};

enum ChipCmdBits
{
  CmdReset = 0x10,
  CmdRxEnb = 0x08,
  CmdTxEnb = 0x04,
  RxBufEmpty = 0x01,
};

/* Interrupt register bits, using my own meaningful names. */
enum IntrStatusBits
{
  PCIErr = 0x8000,
  PCSTimeout = 0x4000,
  RxFIFOOver = 0x40,
  RxUnderrun = 0x20,
  RxOverflow = 0x10,
  TxErr = 0x08,
  TxOK = 0x04,
  RxErr = 0x02,
  RxOK = 0x01,

  RxAckBits = RxFIFOOver | RxOverflow | RxOK,
};

enum TxStatusBits
{
  TxHostOwns = 0x2000,
  TxUnderrun = 0x4000,
  TxStatOK = 0x8000,
  TxOutOfWindow = 0x20000000,
  TxAborted = 0x40000000,
  TxCarrierLost = 0x80000000,
};
enum RxStatusBits
{
  RxMulticast = 0x8000,
  RxPhysical = 0x4000,
  RxBroadcast = 0x2000,
  RxBadSymbol = 0x0020,
  RxRunt = 0x0010,
  RxTooLong = 0x0008,
  RxCRCErr = 0x0004,
  RxBadAlign = 0x0002,
  RxStatusOK = 0x0001,
};

/* Bits in RxConfig. */
enum rx_mode_bits
{
  AcceptErr = 0x20,
  AcceptRunt = 0x10,
  AcceptBroadcast = 0x08,
  AcceptMulticast = 0x04,
  AcceptMyPhys = 0x02,
  AcceptAllPhys = 0x01,
};

/* Bits in TxConfig. */
enum tx_config_bits
{
  TxIFG1 = (1 << 25),           /* Interframe Gap Time */
  TxIFG0 = (1 << 24),           /* Enabling these bits violates IEEE 802.3 */
  TxLoopBack = (1 << 18) | (1 << 17), /* enable loopback test mode */
  TxCRC = (1 << 16),            /* DISABLE appending CRC to end of Tx packets */
  TxClearAbt = (1 << 0),        /* Clear abort (WO) */
  TxDMAShift = 8,               /* DMA burst value (0-7) is shifted this many bits */
  TxRetryShift = 4,             /* TXRR value (0-15) is shifted this many bits */

  TxVersionMask = 0x7C800000,   /* mask out version bits 30-26, 23 */
};

/* Bits in Config1 */
enum Config1Bits
{
  Cfg1_PM_Enable = 0x01,
  Cfg1_VPD_Enable = 0x02,
  Cfg1_PIO = 0x04,
  Cfg1_MMIO = 0x08,
  LWAKE = 0x10,                 /* not on 8139, 8139A */
  Cfg1_Driver_Load = 0x20,
  Cfg1_LED0 = 0x40,
  Cfg1_LED1 = 0x80,
  SLEEP = (1 << 1),             /* only on 8139, 8139A */
  PWRDN = (1 << 0),             /* only on 8139, 8139A */
};

/* Bits in Config3 */
enum Config3Bits
{
  Cfg3_FBtBEn = (1 << 0),       /* 1 = Fast Back to Back */
  Cfg3_FuncRegEn = (1 << 1),    /* 1 = enable CardBus Function registers */
  Cfg3_CLKRUN_En = (1 << 2),    /* 1 = enable CLKRUN */
  Cfg3_CardB_En = (1 << 3),     /* 1 = enable CardBus registers */
  Cfg3_LinkUp = (1 << 4),       /* 1 = wake up on link up */
  Cfg3_Magic = (1 << 5),        /* 1 = wake up on Magic Packet (tm) */
  Cfg3_PARM_En = (1 << 6),      /* 0 = software can set twister parameters */
  Cfg3_GNTSel = (1 << 7),       /* 1 = delay 1 clock from PCI GNT signal */
};

/* Bits in Config4 */
enum Config4Bits
{
  LWPTN = (1 << 2),             /* not on 8139, 8139A */
};

/* Bits in Config5 */
enum Config5Bits
{
  Cfg5_PME_STS = (1 << 0),      /* 1 = PCI reset resets PME_Status */
  Cfg5_LANWake = (1 << 1),      /* 1 = enable LANWake signal */
  Cfg5_LDPS = (1 << 2),         /* 0 = save power when link is down */
  Cfg5_FIFOAddrPtr = (1 << 3),  /* Realtek internal SRAM testing */
  Cfg5_UWF = (1 << 4),          /* 1 = accept unicast wakeup frame */
  Cfg5_MWF = (1 << 5),          /* 1 = accept multicast wakeup frame */
  Cfg5_BWF = (1 << 6),          /* 1 = accept broadcast wakeup frame */
};

enum RxConfigBits
{
  /* rx fifo threshold */
  RxCfgFIFOShift = 13,
  RxCfgFIFONone = (7 << RxCfgFIFOShift),

  /* Max DMA burst */
  RxCfgDMAShift = 8,
  RxCfgDMAUnlimited = (7 << RxCfgDMAShift),

  /* rx ring buffer length */
  RxCfgRcv8K = 0,
  RxCfgRcv16K = (1 << 11),
  RxCfgRcv32K = (1 << 12),
  RxCfgRcv64K = (1 << 11) | (1 << 12),

  /* Disable packet wrap at end of Rx buffer */
  RxNoWrap = (1 << 7),
};


/* Twister tuning parameters from RealTek.
 * Completely undocumented, but required to tune bad links. */
enum CSCRBits
{
  CSCR_LinkOKBit = 0x0400,
  CSCR_LinkChangeBit = 0x0800,
  CSCR_LinkStatusBits = 0x0f000,
  CSCR_LinkDownOffCmd = 0x003c0,
  CSCR_LinkDownCmd = 0x0f3c0,
};


enum Cfg9346Bits
{
  Cfg9346_Lock = 0x00,
  Cfg9346_Unlock = 0xC0,
};


#define PARA78_default	0x78fa8388
#define PARA7c_default	0xcb38de43 /* param[0][3] */
#define PARA7c_xxx		0xcb38de43
static const unsigned long param[4][4] = {
  {0xcb39de43, 0xcb39ce43, 0xfb38de03, 0xcb38de43},
  {0xcb39de43, 0xcb39ce43, 0xcb39ce83, 0xcb39ce83},
  {0xcb39de43, 0xcb39ce43, 0xcb39ce83, 0xcb39ce83},
  {0xbb39de43, 0xbb39ce43, 0xbb39ce83, 0xbb39ce83}
};

typedef enum
{
  CH_8139 = 0,
  CH_8139_K,
  CH_8139A,
  CH_8139B,
  CH_8130,
  CH_8139C,
}
chip_t;

enum chip_flags
{
  HasHltClk = (1 << 0),
  HasLWake = (1 << 1),
};


/* directly indexed by chip_t, above */
const static struct
{
  const char *name;
  u8 version;                   /* from RTL8139C docs */
  DWORD RxConfigMask;             /* should clear the bits supported by this chip */
  DWORD flags;
}
rtl_chip_info[] =
{
  {
    "RTL-8139", 0x40, 0xf0fe0040, /* XXX copied from RTL8139A, verify */
  HasHltClk,}
  ,
  {
  "RTL-8139 rev K", 0x60, 0xf0fe0040, HasHltClk,}
  ,
  {
    "RTL-8139A", 0x70, 0xf0fe0040, HasHltClk, /* XXX undocumented? */
  }
  ,
  {
  "RTL-8139B", 0x78, 0xf0fc0040, HasLWake,}
  ,
  {
    "RTL-8130", 0x7C, 0xf0fe0040, /* XXX copied from RTL8139A, verify */
  HasLWake,}
  ,
  {
    "RTL-8139C", 0x74, 0xf0fc0040, /* XXX copied from RTL8139B, verify */
  HasLWake,}
,};

struct rtl_extra_stats
{
  unsigned long early_rx;
  unsigned long tx_buf_mapped;
  unsigned long tx_timeouts;
};

struct rtl8139_private
{
  void *mmio_addr;
  int drv_flags;
  struct pci_dev *pci_dev;
  struct net_device_stats stats;
  unsigned char *rx_ring;
  unsigned int cur_rx;          /* Index into the Rx buffer of next Rx pkt. */
  unsigned int tx_flag;
  unsigned long cur_tx;
  unsigned long dirty_tx;
  unsigned char *tx_buf[NUM_TX_DESC]; /* Tx bounce buffers */
  unsigned char *tx_bufs;       /* Tx bounce buffer region. */
  dma_addr_t rx_ring_dma;
  dma_addr_t tx_bufs_dma;
  signed char phys[4];          /* MII device addresses. */
  char twistie, twist_row, twist_col; /* Twister tune state. */
  unsigned int full_duplex:1;   /* Full-duplex operation requested. */
  unsigned int duplex_lock:1;
  unsigned int default_port:4;  /* Last dev->if_port value. */
  unsigned int media2:4;        /* Secondary monitored media port. */
  unsigned int medialock:1;     /* Don't sense media type. */
  unsigned int mediasense:1;    /* Media sensing in progress. */
  spinlock_t lock;
  chip_t chipset;
  pid_t thr_pid;
  wait_queue_head_t thr_wait;
  struct completion thr_exited;
  DWORD rx_config;
  struct rtl_extra_stats xstats;
  int time_to_die;
};

static int read_eeprom (void *ioaddr, int location, int addr_len);
static int rtl8139_open (struct net_device *dev);
static int mdio_read (struct net_device *dev, int phy_id, int location);
static void mdio_write (struct net_device *dev, int phy_id, int location, int val);
static int rtl8139_thread (void *data);
static void rtl8139_tx_timeout (struct net_device *dev);
static void rtl8139_init_ring (struct net_device *dev);
static int rtl8139_start_xmit (struct sk_buff *skb, struct net_device *dev);
static void rtl8139_interrupt (int irq, void *dev_instance, struct pt_regs *regs);
static int rtl8139_close (struct net_device *dev);
static int netdev_ioctl (struct net_device *dev, struct ifreq *rq, int cmd);
static struct net_device_stats *rtl8139_get_stats (struct net_device *dev);
static inline DWORD ether_crc (int length, unsigned char *data);
static void rtl8139_set_rx_mode (struct net_device *dev);
static void __set_rx_mode (struct net_device *dev);
static void rtl8139_hw_start (struct net_device *dev);

#ifdef USE_IO_OPS

#define RTL_R8(reg)		inb (((unsigned long)ioaddr) + (reg))
#define RTL_R16(reg)		inw (((unsigned long)ioaddr) + (reg))
#define RTL_R32(reg)		((unsigned long) inl (((unsigned long)ioaddr) + (reg)))
#define RTL_W8(reg, val8)	outb ((val8), ((unsigned long)ioaddr) + (reg))
#define RTL_W16(reg, val16)	outw ((val16), ((unsigned long)ioaddr) + (reg))
#define RTL_W32(reg, val32)	outl ((val32), ((unsigned long)ioaddr) + (reg))
#define RTL_W8_F		RTL_W8
#define RTL_W16_F		RTL_W16
#define RTL_W32_F		RTL_W32
#undef readb
#undef readw
#undef readl
#undef writeb
#undef writew
#undef writel
#define readb(addr) inb((unsigned long)(addr))
#define readw(addr) inw((unsigned long)(addr))
#define readl(addr) inl((unsigned long)(addr))
#define writeb(val,addr) outb((val),(unsigned long)(addr))
#define writew(val,addr) outw((val),(unsigned long)(addr))
#define writel(val,addr) outl((val),(unsigned long)(addr))

#else

/* write MMIO register, with flush */
/* Flush avoids rtl8139 bug w/ posted MMIO writes */
#define RTL_W8_F(reg, val8)	do { writeb ((val8), ioaddr + (reg)); readb (ioaddr + (reg)); } while (0)
#define RTL_W16_F(reg, val16)	do { writew ((val16), ioaddr + (reg)); readw (ioaddr + (reg)); } while (0)
#define RTL_W32_F(reg, val32)	do { writel ((val32), ioaddr + (reg)); readl (ioaddr + (reg)); } while (0)


#define MMIO_FLUSH_AUDIT_COMPLETE 1
#if MMIO_FLUSH_AUDIT_COMPLETE

/* write MMIO register */
#define RTL_W8(reg, val8)	writeb ((val8), ioaddr + (reg))
#define RTL_W16(reg, val16)	writew ((val16), ioaddr + (reg))
#define RTL_W32(reg, val32)	writel ((val32), ioaddr + (reg))

#else

/* write MMIO register, then flush */
#define RTL_W8		RTL_W8_F
#define RTL_W16		RTL_W16_F
#define RTL_W32		RTL_W32_F

#endif     /* MMIO_FLUSH_AUDIT_COMPLETE */

/* read MMIO register */
#define RTL_R8(reg)		readb (ioaddr + (reg))
#define RTL_R16(reg)		readw (ioaddr + (reg))
#define RTL_R32(reg)		((unsigned long) readl (ioaddr + (reg)))

#endif     /* USE_IO_OPS */


static const u16 rtl8139_intr_mask = PCIErr | PCSTimeout | RxUnderrun |
                                     RxOverflow | RxFIFOOver | TxErr |
                                     TxOK | RxErr | RxOK;

static const unsigned int rtl8139_rx_config = RxCfgRcv32K | RxNoWrap |
        (RX_FIFO_THRESH << RxCfgFIFOShift) | (RX_DMA_BURST << RxCfgDMAShift);

static const unsigned int rtl8139_tx_config = (TX_DMA_BURST << TxDMAShift) |
                                              (TX_RETRY << TxRetryShift);

static void __rtl8139_cleanup_dev (struct net_device *dev)
{
  struct rtl8139_private *tp;
  struct pci_dev *pdev;

  assert (dev != NULL);
  assert (dev->priv != NULL);

  tp = dev->priv;
  assert (tp->pci_dev != NULL);
  pdev = tp->pci_dev;

#ifndef USE_IO_OPS
  if (tp->mmio_addr)
    iounmap (tp->mmio_addr);
#endif     /* !USE_IO_OPS */

  /* it's ok to call this even if we have no regions to free */
  pci_release_regions (pdev);

#ifndef RTL8139_NDEBUG
  /* poison memory before freeing */
  memset (dev, 0xBC, sizeof (struct net_device) + sizeof (struct rtl8139_private));
#endif     /* RTL8139_NDEBUG */

  kfree (dev);

  pci_set_drvdata (pdev, NULL);
}


static void rtl8139_chip_reset (void *ioaddr)
{
  int i;

  /* Soft reset the chip. */
  RTL_W8 (ChipCmd, CmdReset);

  /* Check that the chip has finished the reset. */
  for (i = 1000; i > 0; i--)
  {
    barrier ();
    if ((RTL_R8 (ChipCmd) & CmdReset) == 0)
      break;
    udelay (10);
  }
}


static int __devinit rtl8139_init_board (struct pci_dev *pdev, struct net_device **dev_out)
{
  void *ioaddr;
  struct net_device *dev;
  struct rtl8139_private *tp;
  u8 tmp8;
  int rc;
  unsigned int i;
  DWORD pio_start, pio_end, pio_flags, pio_len;
  unsigned long mmio_start, mmio_end, mmio_flags, mmio_len;
  DWORD tmp;

  assert (pdev != NULL);

  *dev_out = NULL;

  /* dev and dev->priv zeroed in alloc_etherdev */
  dev = alloc_etherdev (sizeof (*tp));
  if (dev == NULL)
  {
    printk (KERN_ERR PFX "%s: Unable to alloc new net device\n", pdev->slot_name);
    return -ENOMEM;
  }
  tp = dev->priv;
  tp->pci_dev = pdev;

  /* enable device (incl. PCI PM wakeup and hotplug setup) */
  rc = pci_enable_device (pdev);
  if (rc)
    goto err_out;

  pio_start = pci_resource_start (pdev, 0);
  pio_end = pci_resource_end (pdev, 0);
  pio_flags = pci_resource_flags (pdev, 0);
  pio_len = pci_resource_len (pdev, 0);

  mmio_start = pci_resource_start (pdev, 1);
  mmio_end = pci_resource_end (pdev, 1);
  mmio_flags = pci_resource_flags (pdev, 1);
  mmio_len = pci_resource_len (pdev, 1);

  /* set this immediately, we need to know before
   * we talk to the chip directly */
  DPRINTK ("PIO region size == 0x%02X\n", pio_len);
  DPRINTK ("MMIO region size == 0x%02lX\n", mmio_len);

#ifdef USE_IO_OPS
  /* make sure PCI base addr 0 is PIO */
  if (!(pio_flags & IORESOURCE_IO))
  {
    printk (KERN_ERR PFX "%s: region #0 not a PIO resource, aborting\n", pdev->slot_name);
    rc = -ENODEV;
    goto err_out;
  }
  /* check for weird/broken PCI region reporting */
  if (pio_len < RTL_MIN_IO_SIZE)
  {
    printk (KERN_ERR PFX "%s: Invalid PCI I/O region size(s), aborting\n", pdev->slot_name);
    rc = -ENODEV;
    goto err_out;
  }
#else
  /* make sure PCI base addr 1 is MMIO */
  if (!(mmio_flags & IORESOURCE_MEM))
  {
    printk (KERN_ERR PFX "%s: region #1 not an MMIO resource, aborting\n", pdev->slot_name);
    rc = -ENODEV;
    goto err_out;
  }
  if (mmio_len < RTL_MIN_IO_SIZE)
  {
    printk (KERN_ERR PFX "%s: Invalid PCI mem region size(s), aborting\n", pdev->slot_name);
    rc = -ENODEV;
    goto err_out;
  }
#endif

  rc = pci_request_regions (pdev, "8139too");
  if (rc)
    goto err_out;

  /* enable PCI bus-mastering */
  pci_set_master (pdev);

#ifdef USE_IO_OPS
  ioaddr = (void *) pio_start;
  dev->base_addr = pio_start;
  tp->mmio_addr = ioaddr;
#else
  /* ioremap MMIO region */
  ioaddr = ioremap (mmio_start, mmio_len);
  if (ioaddr == NULL)
  {
    printk (KERN_ERR PFX "%s: cannot remap MMIO, aborting\n", pdev->slot_name);
    rc = -EIO;
    goto err_out;
  }
  dev->base_addr = (long) ioaddr;
  tp->mmio_addr = ioaddr;
#endif     /* USE_IO_OPS */

  /* Bring old chips out of low-power mode. */
  RTL_W8 (HltClk, 'R');

  /* check for missing/broken hardware */
  if (RTL_R32 (TxConfig) == 0xFFFFFFFF)
  {
    printk (KERN_ERR PFX "%s: Chip not responding, ignoring board\n", pdev->slot_name);
    rc = -EIO;
    goto err_out;
  }

  /* identify chip attached to board */
  tmp = RTL_R8 (ChipVersion);
  for (i = 0; i < ARRAY_SIZE (rtl_chip_info); i++)
    if (tmp == rtl_chip_info[i].version)
    {
      tp->chipset = i;
      goto match;
    }

  /* if unknown chip, assume array element #0, original RTL-8139 in this case */
  printk (PFX "%s: unknown chip version, assuming RTL-8139\n", pdev->slot_name);
  printk (PFX "%s: TxConfig = 0x%lx\n", pdev->slot_name, RTL_R32 (TxConfig));
  tp->chipset = 0;

match:
  DPRINTK ("chipset id (%d) == index %d, '%s'\n", tmp, tp->chipset, rtl_chip_info[tp->chipset].name);

  if (tp->chipset >= CH_8139B)
  {
    u8 new_tmp8 = tmp8 = RTL_R8 (Config1);

    DPRINTK ("PCI PM wakeup\n");
    if ((rtl_chip_info[tp->chipset].flags & HasLWake) && (tmp8 & LWAKE))
      new_tmp8 &= ~LWAKE;
    new_tmp8 |= Cfg1_PM_Enable;
    if (new_tmp8 != tmp8)
    {
      RTL_W8 (Cfg9346, Cfg9346_Unlock);
      RTL_W8 (Config1, tmp8);
      RTL_W8 (Cfg9346, Cfg9346_Lock);
    }
    if (rtl_chip_info[tp->chipset].flags & HasLWake)
    {
      tmp8 = RTL_R8 (Config4);
      if (tmp8 & LWPTN)
        RTL_W8 (Config4, tmp8 & ~LWPTN);
    }
  }
  else
  {
    DPRINTK ("Old chip wakeup\n");
    tmp8 = RTL_R8 (Config1);
    tmp8 &= ~(SLEEP | PWRDN);
    RTL_W8 (Config1, tmp8);
  }

  rtl8139_chip_reset (ioaddr);

  *dev_out = dev;
  return 0;

err_out:
  __rtl8139_cleanup_dev (dev);
  return rc;
}


static int __devinit rtl8139_init_one (struct pci_dev *pdev, const struct pci_device_id *ent)
{
  struct net_device *dev = NULL;
  struct rtl8139_private *tp;
  int i, addr_len, option;
  void *ioaddr;
  static int board_idx = -1;
  u8 pci_rev;

  assert (pdev != NULL);
  assert (ent != NULL);

  board_idx++;

  /* when we're built into the kernel, the driver version message
   * is only printed if at least one 8139 board has been found
   */
  {
    static int printed_version;

    if (!printed_version++)
      printk (RTL8139_DRIVER_NAME "\n");
  }

  pci_read_config_byte (pdev, PCI_REVISION_ID, &pci_rev);

  if (pdev->vendor == PCI_VENDOR_ID_REALTEK && pdev->device == PCI_DEVICE_ID_REALTEK_8139 && pci_rev >= 0x20)
  {
    printk (PFX "pci dev %s (id %04x:%04x rev %02x) is an enhanced 8139C+ chip\n",
            pdev->slot_name, pdev->vendor, pdev->device, pci_rev);
    printk (PFX "Use the \"8139cp\" driver for improved performance and stability.\n");
  }

  i = rtl8139_init_board (pdev, &dev);
  if (i < 0)
    return i;

  tp = dev->priv;
  ioaddr = tp->mmio_addr;

  assert (ioaddr != NULL);
  assert (dev != NULL);
  assert (tp != NULL);

  addr_len = read_eeprom (ioaddr, 0, 8) == 0x8129 ? 8 : 6;
  for (i = 0; i < 3; i++)
    ((u16 *) (dev->dev_addr))[i] = le16_to_cpu (read_eeprom (ioaddr, i + 7, addr_len));

  /* The Rtl8139-specific entries in the device structure. */
  dev->open = rtl8139_open;
  dev->hard_start_xmit = rtl8139_start_xmit;
  dev->stop = rtl8139_close;
  dev->get_stats = rtl8139_get_stats;
  dev->set_multicast_list = rtl8139_set_rx_mode;
  dev->do_ioctl = netdev_ioctl;
  dev->tx_timeout = rtl8139_tx_timeout;
  dev->watchdog_timeo = TX_TIMEOUT;
  dev->features |= NETIF_F_SG | NETIF_F_HW_CSUM;

  dev->irq = pdev->irq;

  /* dev->priv/tp zeroed and aligned in init_etherdev */
  tp = dev->priv;

  /* note: tp->chipset set in rtl8139_init_board */
  tp->drv_flags = board_info[ent->driver_data].hw_flags;
  tp->mmio_addr = ioaddr;
  spin_lock_init (&tp->lock);
  init_waitqueue_head (&tp->thr_wait);
  init_completion (&tp->thr_exited);

  /* dev is fully set up and ready to use now */
  DPRINTK ("about to register device named %s (%p)...\n", dev->name, dev);
  i = register_netdev (dev);
  if (i)
    goto err_out;

  pci_set_drvdata (pdev, dev);

  printk ("%s: %s at 0x%lx, "
          "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x, "
          "IRQ %d\n",
          dev->name,
          board_info[ent->driver_data].name,
          dev->base_addr,
          dev->dev_addr[0], dev->dev_addr[1],
          dev->dev_addr[2], dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5], dev->irq);

  printk ("%s:  Identified 8139 chip type '%s'\n", dev->name, rtl_chip_info[tp->chipset].name);

  /* Find the connected MII xcvrs.
   * Doing this in open() would allow detecting external xcvrs later, but
   * takes too much time. */
#ifdef CONFIG_8139TOO_8129
  if (tp->drv_flags & HAS_MII_XCVR)
  {
    int phy, phy_idx = 0;

    for (phy = 0; phy < 32 && phy_idx < sizeof (tp->phys); phy++)
    {
      int mii_status = mdio_read (dev, phy, 1);

      if (mii_status != 0xffff && mii_status != 0x0000)
      {
        u16 advertising = mdio_read (dev, phy, 4);

        tp->phys[phy_idx++] = phy;
        printk ("%s: MII transceiver %d status 0x%4.4x "
                "advertising %4.4x.\n", dev->name, phy, mii_status, advertising);
      }
    }
    if (phy_idx == 0)
    {
      printk ("%s: No MII transceivers found!  Assuming SYM " "transceiver.\n", dev->name);
      tp->phys[0] = 32;
    }
  }
  else
#endif
    tp->phys[0] = 32;

  /* The lower four bits are the media type. */
  option = (board_idx >= MAX_UNITS) ? 0 : media[board_idx];
  if (option > 0)
  {
    tp->full_duplex = (option & 0x210) ? 1 : 0;
    tp->default_port = option & 0xFF;
    if (tp->default_port)
      tp->medialock = 1;
  }
  if (board_idx < MAX_UNITS && full_duplex[board_idx] > 0)
    tp->full_duplex = full_duplex[board_idx];
  if (tp->full_duplex)
  {
    printk ("%s: Media type forced to Full Duplex.\n", dev->name);
    /* Changing the MII-advertised media because might prevent
     * re-connection. */
    tp->duplex_lock = 1;
  }
  if (tp->default_port)
  {
    printk ("  Forcing %dMbps %s-duplex operation.\n", (option & 0x20 ? 100 : 10), (option & 0x10 ? "full" : "half"));
    mdio_write (dev, tp->phys[0], 0, ((option & 0x20) ? 0x2000 : 0) | /* 100Mbps? */
                ((option & 0x10) ? 0x0100 : 0)); /* Full duplex? */
  }

  /* Put the chip into low-power mode. */
  if (rtl_chip_info[tp->chipset].flags & HasHltClk)
    RTL_W8 (HltClk, 'H');       /* 'R' would leave the clock running. */

  return 0;

err_out:
  __rtl8139_cleanup_dev (dev);
  return i;
}


static void __devexit rtl8139_remove_one (struct pci_dev *pdev)
{
  struct net_device *dev = pci_get_drvdata (pdev);
  struct rtl8139_private *np;

  assert (dev != NULL);
  np = dev->priv;
  assert (np != NULL);

  unregister_netdev (dev);

  __rtl8139_cleanup_dev (dev);
}


/* Serial EEPROM section. */

/*  EEPROM_Ctrl bits. */
#define EE_SHIFT_CLK	0x04       /* EEPROM shift clock. */
#define EE_CS			0x08            /* EEPROM chip select. */
#define EE_DATA_WRITE	0x02      /* EEPROM chip data in. */
#define EE_WRITE_0		0x00
#define EE_WRITE_1		0x02
#define EE_DATA_READ	0x01       /* EEPROM chip data out. */
#define EE_ENB			(0x80 | EE_CS)

/* Delay between EEPROM clock transitions.
 * No extra delay is needed with 33Mhz PCI, but 66Mhz may change this.
 */

#define eeprom_delay()	readl(ee_addr)

/* The EEPROM commands include the alway-set leading bit. */
#define EE_WRITE_CMD	(5)
#define EE_READ_CMD		(6)
#define EE_ERASE_CMD	(7)

static int __devinit read_eeprom (void *ioaddr, int location, int addr_len)
{
  int i;
  unsigned retval = 0;
  void *ee_addr = ioaddr + Cfg9346;
  int read_cmd = location | (EE_READ_CMD << addr_len);

  writeb (EE_ENB & ~EE_CS, ee_addr);
  writeb (EE_ENB, ee_addr);
  eeprom_delay ();

  /* Shift the read command bits out. */
  for (i = 4 + addr_len; i >= 0; i--)
  {
    int dataval = (read_cmd & (1 << i)) ? EE_DATA_WRITE : 0;

    writeb (EE_ENB | dataval, ee_addr);
    eeprom_delay ();
    writeb (EE_ENB | dataval | EE_SHIFT_CLK, ee_addr);
    eeprom_delay ();
  }
  writeb (EE_ENB, ee_addr);
  eeprom_delay ();

  for (i = 16; i > 0; i--)
  {
    writeb (EE_ENB | EE_SHIFT_CLK, ee_addr);
    eeprom_delay ();
    retval = (retval << 1) | ((readb (ee_addr) & EE_DATA_READ) ? 1 : 0);
    writeb (EE_ENB, ee_addr);
    eeprom_delay ();
  }

  /* Terminate the EEPROM access. */
  writeb (~EE_CS, ee_addr);
  eeprom_delay ();

  return retval;
}

/* MII serial management: mostly bogus for now. */
/* Read and write the MII management registers using software-generated
 * serial MDIO protocol.
 * The maximum data clock rate is 2.5 Mhz.  The minimum timing is usually
 * met by back-to-back PCI I/O cycles, but we insert a delay to avoid
 * "overclocking" issues. */
#define MDIO_DIR		0x80
#define MDIO_DATA_OUT	0x04
#define MDIO_DATA_IN	0x02
#define MDIO_CLK		0x01
#define MDIO_WRITE0 (MDIO_DIR)
#define MDIO_WRITE1 (MDIO_DIR | MDIO_DATA_OUT)

#define mdio_delay(mdio_addr)	readb(mdio_addr)


static char mii_2_8139_map[8] = {
  BasicModeCtrl,
  BasicModeStatus,
  0,
  0,
  NWayAdvert,
  NWayLPAR,
  NWayExpansion,
  0
};


#ifdef CONFIG_8139TOO_8129
/* Syncronize the MII management interface by shifting 32 one bits out. */
static void mdio_sync (void *mdio_addr)
{
  int i;

  for (i = 32; i >= 0; i--)
  {
    writeb (MDIO_WRITE1, mdio_addr);
    mdio_delay (mdio_addr);
    writeb (MDIO_WRITE1 | MDIO_CLK, mdio_addr);
    mdio_delay (mdio_addr);
  }
}
#endif

static int mdio_read (struct net_device *dev, int phy_id, int location)
{
  struct rtl8139_private *tp = dev->priv;
  int retval = 0;

#ifdef CONFIG_8139TOO_8129
  void *mdio_addr = tp->mmio_addr + Config4;
  int mii_cmd = (0xf6 << 10) | (phy_id << 5) | location;
  int i;
#endif

  if (phy_id > 31)
  {                             /* Really a 8139.  Use internal registers. */
    return location < 8 && mii_2_8139_map[location] ? readw (tp->mmio_addr + mii_2_8139_map[location]) : 0;
  }

#ifdef CONFIG_8139TOO_8129
  mdio_sync (mdio_addr);
  /* Shift the read command bits out. */
  for (i = 15; i >= 0; i--)
  {
    int dataval = (mii_cmd & (1 << i)) ? MDIO_DATA_OUT : 0;

    writeb (MDIO_DIR | dataval, mdio_addr);
    mdio_delay (mdio_addr);
    writeb (MDIO_DIR | dataval | MDIO_CLK, mdio_addr);
    mdio_delay (mdio_addr);
  }

  /* Read the two transition, 16 data, and wire-idle bits. */
  for (i = 19; i > 0; i--)
  {
    writeb (0, mdio_addr);
    mdio_delay (mdio_addr);
    retval = (retval << 1) | ((readb (mdio_addr) & MDIO_DATA_IN) ? 1 : 0);
    writeb (MDIO_CLK, mdio_addr);
    mdio_delay (mdio_addr);
  }
#endif

  return (retval >> 1) & 0xffff;
}


static void mdio_write (struct net_device *dev, int phy_id, int location, int value)
{
  struct rtl8139_private *tp = dev->priv;

#ifdef CONFIG_8139TOO_8129
  void *mdio_addr = tp->mmio_addr + Config4;
  int mii_cmd = (0x5002 << 16) | (phy_id << 23) | (location << 18) | value;
  int i;
#endif

  if (phy_id > 31)
  {                             /* Really a 8139.  Use internal registers. */
    void *ioaddr = tp->mmio_addr;

    if (location == 0)
    {
      RTL_W8 (Cfg9346, Cfg9346_Unlock);
      RTL_W16 (BasicModeCtrl, value);
      RTL_W8 (Cfg9346, Cfg9346_Lock);
    }
    else if (location < 8 && mii_2_8139_map[location])
      RTL_W16 (mii_2_8139_map[location], value);
    return;
  }

#ifdef CONFIG_8139TOO_8129
  mdio_sync (mdio_addr);

  /* Shift the command bits out. */
  for (i = 31; i >= 0; i--)
  {
    int dataval = (mii_cmd & (1 << i)) ? MDIO_WRITE1 : MDIO_WRITE0;

    writeb (dataval, mdio_addr);
    mdio_delay (mdio_addr);
    writeb (dataval | MDIO_CLK, mdio_addr);
    mdio_delay (mdio_addr);
  }
  /* Clear out extra bits. */
  for (i = 2; i > 0; i--)
  {
    writeb (0, mdio_addr);
    mdio_delay (mdio_addr);
    writeb (MDIO_CLK, mdio_addr);
    mdio_delay (mdio_addr);
  }
#endif
}


static int rtl8139_open (struct net_device *dev)
{
  struct rtl8139_private *tp = dev->priv;
  int retval;

#ifdef RTL8139_DEBUG
  void *ioaddr = tp->mmio_addr;
#endif

  retval = request_irq (dev->irq, rtl8139_interrupt, SA_SHIRQ, dev->name, dev);
  if (retval)
    return retval;

  tp->tx_bufs = pci_alloc_consistent (tp->pci_dev, TX_BUF_TOT_LEN, &tp->tx_bufs_dma);
  tp->rx_ring = pci_alloc_consistent (tp->pci_dev, RX_BUF_TOT_LEN, &tp->rx_ring_dma);
  if (tp->tx_bufs == NULL || tp->rx_ring == NULL)
  {
    free_irq (dev->irq, dev);

    if (tp->tx_bufs)
      pci_free_consistent (tp->pci_dev, TX_BUF_TOT_LEN, tp->tx_bufs, tp->tx_bufs_dma);
    if (tp->rx_ring)
      pci_free_consistent (tp->pci_dev, RX_BUF_TOT_LEN, tp->rx_ring, tp->rx_ring_dma);

    return -ENOMEM;

  }

  tp->full_duplex = tp->duplex_lock;
  tp->tx_flag = (TX_FIFO_THRESH << 11) & 0x003f0000;
  tp->twistie = 1;
  tp->time_to_die = 0;

  rtl8139_init_ring (dev);
  rtl8139_hw_start (dev);

  DPRINTK ("%s: rtl8139_open() ioaddr %#lx IRQ %d"
           " GP Pins %2.2x %s-duplex.\n",
           dev->name, pci_resource_start (tp->pci_dev, 1),
           dev->irq, RTL_R8 (MediaStatus), tp->full_duplex ? "full" : "half");

  tp->thr_pid = kernel_thread (rtl8139_thread, dev, CLONE_FS | CLONE_FILES);
  if (tp->thr_pid < 0)
    printk ("%s: unable to start kernel thread\n", dev->name);

  return 0;
}


static void rtl_check_media (struct net_device *dev)
{
  struct rtl8139_private *tp = dev->priv;

  if (tp->phys[0] >= 0)
  {
    u16 mii_reg5 = mdio_read (dev, tp->phys[0], 5);

    if (mii_reg5 == 0xffff)
      ;                         /* Not there */
    else if ((mii_reg5 & 0x0100) == 0x0100 || (mii_reg5 & 0x00C0) == 0x0040)
      tp->full_duplex = 1;

    printk ("%s: Setting %s%s-duplex based on"
            " auto-negotiated partner ability %4.4x.\n",
            dev->name, mii_reg5 == 0 ? "" :
            (mii_reg5 & 0x0180) ? "100mbps " : "10mbps ", tp->full_duplex ? "full" : "half", mii_reg5);
  }
}

/* Start the hardware at open or resume. */
static void rtl8139_hw_start (struct net_device *dev)
{
  struct rtl8139_private *tp = dev->priv;
  void *ioaddr = tp->mmio_addr;
  DWORD i;
  u8 tmp;

  /* Bring old chips out of low-power mode. */
  if (rtl_chip_info[tp->chipset].flags & HasHltClk)
    RTL_W8 (HltClk, 'R');

  rtl8139_chip_reset (ioaddr);

  /* unlock Config[01234] and BMCR register writes */
  RTL_W8_F (Cfg9346, Cfg9346_Unlock);
  /* Restore our idea of the MAC address. */
  RTL_W32_F (MAC0 + 0, cpu_to_le32 (*(DWORD *) (dev->dev_addr + 0)));
  RTL_W32_F (MAC0 + 4, cpu_to_le32 (*(DWORD *) (dev->dev_addr + 4)));

  /* Must enable Tx/Rx before setting transfer thresholds! */
  RTL_W8 (ChipCmd, CmdRxEnb | CmdTxEnb);

  tp->rx_config = rtl8139_rx_config | AcceptBroadcast | AcceptMyPhys;
  RTL_W32 (RxConfig, tp->rx_config);

  /* Check this value: the documentation for IFG contradicts ifself. */
  RTL_W32 (TxConfig, rtl8139_tx_config);

  tp->cur_rx = 0;

  rtl_check_media (dev);

  if (tp->chipset >= CH_8139B)
  {
    /* Disable magic packet scanning, which is enabled
     * when PM is enabled in Config1.  It can be reenabled
     * via ETHTOOL_SWOL if desired.  */
    RTL_W8 (Config3, RTL_R8 (Config3) & ~Cfg3_Magic);
  }

  DPRINTK ("init buffer addresses\n");

  /* Lock Config[01234] and BMCR register writes */
  RTL_W8 (Cfg9346, Cfg9346_Lock);

  /* init Rx ring buffer DMA address */
  RTL_W32_F (RxBuf, tp->rx_ring_dma);

  /* init Tx buffer DMA addresses */
  for (i = 0; i < NUM_TX_DESC; i++)
    RTL_W32_F (TxAddr0 + (i * 4), tp->tx_bufs_dma + (tp->tx_buf[i] - tp->tx_bufs));

  RTL_W32 (RxMissed, 0);

  rtl8139_set_rx_mode (dev);

  /* no early-rx interrupts */
  RTL_W16 (MultiIntr, RTL_R16 (MultiIntr) & MultiIntrClear);

  /* make sure RxTx has started */
  tmp = RTL_R8 (ChipCmd);
  if ((!(tmp & CmdRxEnb)) || (!(tmp & CmdTxEnb)))
    RTL_W8 (ChipCmd, CmdRxEnb | CmdTxEnb);

  /* Enable all known interrupts by setting the interrupt mask. */
  RTL_W16 (IntrMask, rtl8139_intr_mask);

  netif_start_queue (dev);
}


/* Initialize the Rx and Tx rings, along with various 'dev' bits. */
static void rtl8139_init_ring (struct net_device *dev)
{
  struct rtl8139_private *tp = dev->priv;
  int i;

  tp->cur_rx = 0;
  tp->cur_tx = 0;
  tp->dirty_tx = 0;

  for (i = 0; i < NUM_TX_DESC; i++)
    tp->tx_buf[i] = &tp->tx_bufs[i * TX_BUF_SIZE];
}


/* This must be global for CONFIG_8139TOO_TUNE_TWISTER case */
static int next_tick = 3 * HZ;

#ifndef CONFIG_8139TOO_TUNE_TWISTER
static inline void rtl8139_tune_twister (struct net_device *dev, struct rtl8139_private *tp)
{
}
#else
static void rtl8139_tune_twister (struct net_device *dev, struct rtl8139_private *tp)
{
  int linkcase;
  void *ioaddr = tp->mmio_addr;

  /* This is a complicated state machine to configure the "twister" for
   * impedance/echos based on the cable length.
   * All of this is magic and undocumented.
   */
  switch (tp->twistie)
  {
       case 1:
         if (RTL_R16 (CSCR) & CSCR_LinkOKBit)
         {
           /* We have link beat, let us tune the twister. */
           RTL_W16 (CSCR, CSCR_LinkDownOffCmd);
           tp->twistie = 2;     /* Change to state 2. */
           next_tick = HZ / 10;
         }
         else
         {
           /* Just put in some reasonable defaults for when beat returns. */
           RTL_W16 (CSCR, CSCR_LinkDownCmd);
           RTL_W32 (FIFOTMS, 0x20); /* Turn on cable test mode. */
           RTL_W32 (PARA78, PARA78_default);
           RTL_W32 (PARA7c, PARA7c_default);
           tp->twistie = 0;     /* Bail from future actions. */
         }
         break;
       case 2:
         /* Read how long it took to hear the echo. */
         linkcase = RTL_R16 (CSCR) & CSCR_LinkStatusBits;
         if (linkcase == 0x7000)
           tp->twist_row = 3;
         else if (linkcase == 0x3000)
           tp->twist_row = 2;
         else if (linkcase == 0x1000)
           tp->twist_row = 1;
         else
           tp->twist_row = 0;
         tp->twist_col = 0;
         tp->twistie = 3;       /* Change to state 2. */
         next_tick = HZ / 10;
         break;
       case 3:
         /* Put out four tuning parameters, one per 100msec. */
         if (tp->twist_col == 0)
           RTL_W16 (FIFOTMS, 0);
         RTL_W32 (PARA7c, param[(int) tp->twist_row][(int) tp->twist_col]);
         next_tick = HZ / 10;
         if (++tp->twist_col >= 4)
         {
           /* For short cables we are done.
            * For long cables (row == 3) check for mistune. */
           tp->twistie = (tp->twist_row == 3) ? 4 : 0;
         }
         break;
       case 4:
         /* Special case for long cables: check for mistune. */
         if ((RTL_R16 (CSCR) & CSCR_LinkStatusBits) == 0x7000)
         {
           tp->twistie = 0;
           break;
         }
         else
         {
           RTL_W32 (PARA7c, 0xfb38de03);
           tp->twistie = 5;
           next_tick = HZ / 10;
         }
         break;
       case 5:
         /* Retune for shorter cable (column 2). */
         RTL_W32 (FIFOTMS, 0x20);
         RTL_W32 (PARA78, PARA78_default);
         RTL_W32 (PARA7c, PARA7c_default);
         RTL_W32 (FIFOTMS, 0x00);
         tp->twist_row = 2;
         tp->twist_col = 0;
         tp->twistie = 3;
         next_tick = HZ / 10;
         break;

       default:
         /* do nothing */
         break;
  }
}
#endif     /* CONFIG_8139TOO_TUNE_TWISTER */


static inline void rtl8139_thread_iter (struct net_device *dev, struct rtl8139_private *tp, void *ioaddr)
{
  int mii_reg5;

  mii_reg5 = mdio_read (dev, tp->phys[0], 5);

  if (!tp->duplex_lock && mii_reg5 != 0xffff)
  {
    int duplex = (mii_reg5 & 0x0100) || (mii_reg5 & 0x01C0) == 0x0040;

    if (tp->full_duplex != duplex)
    {
      tp->full_duplex = duplex;

      if (mii_reg5)
      {
        printk ("%s: Setting %s-duplex based on MII #%d link"
                " partner ability of %4.4x.\n", dev->name, tp->full_duplex ? "full" : "half", tp->phys[0], mii_reg5);
      }
      else
      {
        printk ("%s: media is unconnected, link down, or incompatible connection\n", dev->name);
      }
#if 0
      RTL_W8 (Cfg9346, Cfg9346_Unlock);
      RTL_W8 (Config1, tp->full_duplex ? 0x60 : 0x20);
      RTL_W8 (Cfg9346, Cfg9346_Lock);
#endif
    }
  }

  next_tick = HZ * 60;

  rtl8139_tune_twister (dev, tp);

  DPRINTK ("%s: Media selection tick, Link partner %4.4x.\n", dev->name, RTL_R16 (NWayLPAR));
  DPRINTK ("%s:  Other registers are IntMask %4.4x IntStatus %4.4x\n",
           dev->name, RTL_R16 (IntrMask), RTL_R16 (IntrStatus));
  DPRINTK ("%s:  Chip config %2.2x %2.2x.\n", dev->name, RTL_R8 (Config0), RTL_R8 (Config1));
}


static int rtl8139_thread (void *data)
{
  struct net_device *dev = data;
  struct rtl8139_private *tp = dev->priv;
  unsigned long timeout;

  daemonize ();
  reparent_to_init ();
  spin_lock_irq (&current->sigmask_lock);
  sigemptyset (&current->blocked);
  recalc_sigpending (current);
  spin_unlock_irq (&current->sigmask_lock);

  strncpy (current->comm, dev->name, sizeof (current->comm) - 1);
  current->comm[sizeof (current->comm) - 1] = '\0';

  while (1)
  {
    timeout = next_tick;
    do
    {
      timeout = interruptible_sleep_on_timeout (&tp->thr_wait, timeout);
    }
    while (!signal_pending (current) && (timeout > 0));

    if (signal_pending (current))
    {
      spin_lock_irq (&current->sigmask_lock);
      flush_signals (current);
      spin_unlock_irq (&current->sigmask_lock);
    }

    if (tp->time_to_die)
      break;

    rtnl_lock ();
    rtl8139_thread_iter (dev, tp, tp->mmio_addr);
    rtnl_unlock ();
  }

  complete_and_exit (&tp->thr_exited, 0);
}


static void rtl8139_tx_clear (struct rtl8139_private *tp)
{
  tp->cur_tx = 0;
  tp->dirty_tx = 0;

  /* XXX account for unsent Tx packets in tp->stats.tx_dropped */
}


static void rtl8139_tx_timeout (struct net_device *dev)
{
  struct rtl8139_private *tp = dev->priv;
  void *ioaddr = tp->mmio_addr;
  int i;
  u8 tmp8;
  unsigned long flags;

  DPRINTK ("%s: Transmit timeout, status %2.2x %4.4x "
           "media %2.2x.\n", dev->name, RTL_R8 (ChipCmd), RTL_R16 (IntrStatus), RTL_R8 (MediaStatus));

  tp->xstats.tx_timeouts++;

  /* disable Tx ASAP, if not already */
  tmp8 = RTL_R8 (ChipCmd);
  if (tmp8 & CmdTxEnb)
    RTL_W8 (ChipCmd, CmdRxEnb);

  /* Disable interrupts by clearing the interrupt mask. */
  RTL_W16 (IntrMask, 0x0000);

  /* Emit info to figure out what went wrong. */
  printk ("%s: Tx queue start entry %ld  dirty entry %ld.\n", dev->name, tp->cur_tx, tp->dirty_tx);
  for (i = 0; i < NUM_TX_DESC; i++)
    printk ("%s:  Tx descriptor %d is %8.8lx.%s\n",
            dev->name, i, RTL_R32 (TxStatus0 + (i * 4)), i == tp->dirty_tx % NUM_TX_DESC ? " (queue head)" : "");

  /* Stop a shared interrupt from scavenging while we are. */
  spin_lock_irqsave (&tp->lock, flags);
  rtl8139_tx_clear (tp);
  spin_unlock_irqrestore (&tp->lock, flags);

  /* ...and finally, reset everything */
  rtl8139_hw_start (dev);

  netif_wake_queue (dev);
}


static int rtl8139_start_xmit (struct sk_buff *skb, struct net_device *dev)
{
  struct rtl8139_private *tp = dev->priv;
  void *ioaddr = tp->mmio_addr;
  unsigned int entry;

  /* Calculate the next Tx descriptor entry. */
  entry = tp->cur_tx % NUM_TX_DESC;

  if (likely (skb->len < TX_BUF_SIZE))
  {
    skb_copy_and_csum_dev (skb, tp->tx_buf[entry]);
    dev_kfree_skb (skb);
  }
  else
  {
    dev_kfree_skb (skb);
    tp->stats.tx_dropped++;
    return 0;
  }

  /* Note: the chip doesn't have auto-pad! */
  spin_lock_irq (&tp->lock);
  RTL_W32_F (TxStatus0 + (entry * sizeof (DWORD)), tp->tx_flag | (skb->len >= ETH_ZLEN ? skb->len : ETH_ZLEN));

  dev->trans_start = jiffies;

  tp->cur_tx++;
  wmb ();

  if ((tp->cur_tx - NUM_TX_DESC) == tp->dirty_tx)
    netif_stop_queue (dev);
  spin_unlock_irq (&tp->lock);

  DPRINTK ("%s: Queued Tx packet at %p size %u to slot %d.\n", dev->name, skb->data, skb->len, entry);

  return 0;
}


static void rtl8139_tx_interrupt (struct net_device *dev, struct rtl8139_private *tp, void *ioaddr)
{
  unsigned long dirty_tx, tx_left;

  assert (dev != NULL);
  assert (tp != NULL);
  assert (ioaddr != NULL);

  dirty_tx = tp->dirty_tx;
  tx_left = tp->cur_tx - dirty_tx;
  while (tx_left > 0)
  {
    int entry = dirty_tx % NUM_TX_DESC;
    int txstatus;

    txstatus = RTL_R32 (TxStatus0 + (entry * sizeof (DWORD)));

    if (!(txstatus & (TxStatOK | TxUnderrun | TxAborted)))
      break;                    /* It still hasn't been Txed */

    /* Note: TxCarrierLost is always asserted at 100mbps. */
    if (txstatus & (TxOutOfWindow | TxAborted))
    {
      /* There was an major error, log it. */
      DPRINTK ("%s: Transmit error, Tx status %8.8x.\n", dev->name, txstatus);
      tp->stats.tx_errors++;
      if (txstatus & TxAborted)
      {
        tp->stats.tx_aborted_errors++;
        RTL_W32 (TxConfig, TxClearAbt);
        RTL_W16 (IntrStatus, TxErr);
        wmb ();
      }
      if (txstatus & TxCarrierLost)
        tp->stats.tx_carrier_errors++;
      if (txstatus & TxOutOfWindow)
        tp->stats.tx_window_errors++;
#ifdef ETHER_STATS
      if ((txstatus & 0x0f000000) == 0x0f000000)
        tp->stats.collisions16++;
#endif
    }
    else
    {
      if (txstatus & TxUnderrun)
      {
        /* Add 64 to the Tx FIFO threshold. */
        if (tp->tx_flag < 0x00300000)
          tp->tx_flag += 0x00020000;
        tp->stats.tx_fifo_errors++;
      }
      tp->stats.collisions += (txstatus >> 24) & 15;
      tp->stats.tx_bytes += txstatus & 0x7ff;
      tp->stats.tx_packets++;
    }

    dirty_tx++;
    tx_left--;
  }

#ifndef RTL8139_NDEBUG
  if (tp->cur_tx - dirty_tx > NUM_TX_DESC)
  {
    printk (KERN_ERR "%s: Out-of-sync dirty pointer, %ld vs. %ld.\n", dev->name, dirty_tx, tp->cur_tx);
    dirty_tx += NUM_TX_DESC;
  }
#endif     /* RTL8139_NDEBUG */

  /* only wake the queue if we did work, and the queue is stopped */
  if (tp->dirty_tx != dirty_tx)
  {
    tp->dirty_tx = dirty_tx;
    mb ();
    if (netif_queue_stopped (dev))
      netif_wake_queue (dev);
  }
}


/* TODO: clean this up!  Rx reset need not be this intensive */
static void rtl8139_rx_err (DWORD rx_status, struct net_device *dev, struct rtl8139_private *tp, void *ioaddr)
{
  u8 tmp8;
  int tmp_work;

  DPRINTK ("%s: Ethernet frame had errors, status %8.8x.\n", dev->name, rx_status);
  if (rx_status & RxTooLong)
  {
    DPRINTK ("%s: Oversized Ethernet frame, status %4.4x!\n", dev->name, rx_status);
    /* A.C.: The chip hangs here. */
  }
  tp->stats.rx_errors++;
  if (rx_status & (RxBadSymbol | RxBadAlign))
    tp->stats.rx_frame_errors++;
  if (rx_status & (RxRunt | RxTooLong))
    tp->stats.rx_length_errors++;
  if (rx_status & RxCRCErr)
    tp->stats.rx_crc_errors++;

  /* Reset the receiver, based on RealTek recommendation. (Bug?) */

  /* disable receive */
  RTL_W8_F (ChipCmd, CmdTxEnb);
  tmp_work = 200;
  while (--tmp_work > 0)
  {
    udelay (1);
    tmp8 = RTL_R8 (ChipCmd);
    if (!(tmp8 & CmdRxEnb))
      break;
  }
  if (tmp_work <= 0)
    printk (PFX "rx stop wait too long\n");
  /* restart receive */
  tmp_work = 200;
  while (--tmp_work > 0)
  {
    RTL_W8_F (ChipCmd, CmdRxEnb | CmdTxEnb);
    udelay (1);
    tmp8 = RTL_R8 (ChipCmd);
    if ((tmp8 & CmdRxEnb) && (tmp8 & CmdTxEnb))
      break;
  }
  if (tmp_work <= 0)
    printk (PFX "tx/rx enable wait too long\n");

  /* and reinitialize all rx related registers */
  RTL_W8_F (Cfg9346, Cfg9346_Unlock);
  /* Must enable Tx/Rx before setting transfer thresholds! */
  RTL_W8 (ChipCmd, CmdRxEnb | CmdTxEnb);

  tp->rx_config = rtl8139_rx_config | AcceptBroadcast | AcceptMyPhys;
  RTL_W32 (RxConfig, tp->rx_config);
  tp->cur_rx = 0;

  DPRINTK ("init buffer addresses\n");

  /* Lock Config[01234] and BMCR register writes */
  RTL_W8 (Cfg9346, Cfg9346_Lock);

  /* init Rx ring buffer DMA address */
  RTL_W32_F (RxBuf, tp->rx_ring_dma);

  /* A.C.: Reset the multicast list. */
  __set_rx_mode (dev);
}

static void rtl8139_rx_interrupt (struct net_device *dev, struct rtl8139_private *tp, void *ioaddr)
{
  unsigned char *rx_ring;
  u16 cur_rx;

  assert (dev != NULL);
  assert (tp != NULL);
  assert (ioaddr != NULL);

  rx_ring = tp->rx_ring;
  cur_rx = tp->cur_rx;

  DPRINTK ("%s: In rtl8139_rx(), current %4.4x BufAddr %4.4x,"
           " free to %4.4x, Cmd %2.2x.\n", dev->name, cur_rx,
           RTL_R16 (RxBufAddr), RTL_R16 (RxBufPtr), RTL_R8 (ChipCmd));

  while ((RTL_R8 (ChipCmd) & RxBufEmpty) == 0)
  {
    int ring_offset = cur_rx % RX_BUF_LEN;
    DWORD rx_status;
    unsigned int rx_size;
    unsigned int pkt_size;
    struct sk_buff *skb;

    rmb ();

    /* read size+status of next frame from DMA ring buffer */
    rx_status = le32_to_cpu (*(DWORD *) (rx_ring + ring_offset));
    rx_size = rx_status >> 16;
    pkt_size = rx_size - 4;

    DPRINTK ("%s:  rtl8139_rx() status %4.4x, size %4.4x," " cur %4.4x.\n", dev->name, rx_status, rx_size, cur_rx);
#if RTL8139_DEBUG > 2
    {
      int i;

      DPRINTK ("%s: Frame contents ", dev->name);
      for (i = 0; i < 70; i++)
        printk (" %2.2x", rx_ring[ring_offset + i]);
      printk (".\n");
    }
#endif

    /* Packet copy from FIFO still in progress.
     * Theoretically, this should never happen
     * since EarlyRx is disabled.
     */
    if (rx_size == 0xfff0)
    {
      tp->xstats.early_rx++;
      break;
    }

    /* If Rx err or invalid rx_size/rx_status received
     * (which happens if we get lost in the ring),
     * Rx process gets reset, so we abort any further
     * Rx processing.
     */
    if ((rx_size > (MAX_ETH_FRAME_SIZE + 4)) || (rx_size < 8) || (!(rx_status & RxStatusOK)))
    {
      rtl8139_rx_err (rx_status, dev, tp, ioaddr);
      return;
    }

    /* Malloc up new buffer, compatible with net-2e. */
    /* Omit the four octet CRC from the length. */

    /* TODO: consider allocating skb's outside of
     * interrupt context, both to speed interrupt processing,
     * and also to reduce the chances of having to
     * drop packets here under memory pressure.
     */

    skb = dev_alloc_skb (pkt_size + 2);
    if (skb)
    {
      skb->dev = dev;
      skb_reserve (skb, 2);     /* 16 byte align the IP fields. */

      eth_copy_and_sum (skb, &rx_ring[ring_offset + 4], pkt_size, 0);
      skb_put (skb, pkt_size);

      skb->protocol = eth_type_trans (skb, dev);
      netif_rx (skb);
      dev->last_rx = jiffies;
      tp->stats.rx_bytes += pkt_size;
      tp->stats.rx_packets++;
    }
    else
    {
      printk (KERN_WARNING "%s: Memory squeeze, dropping packet.\n", dev->name);
      tp->stats.rx_dropped++;
    }

    cur_rx = (cur_rx + rx_size + 4 + 3) & ~3;
    RTL_W16 (RxBufPtr, cur_rx - 16);

    if (RTL_R16 (IntrStatus) & RxAckBits)
      RTL_W16_F (IntrStatus, RxAckBits);
  }

  DPRINTK ("%s: Done rtl8139_rx(), current %4.4x BufAddr %4.4x,"
           " free to %4.4x, Cmd %2.2x.\n", dev->name, cur_rx,
           RTL_R16 (RxBufAddr), RTL_R16 (RxBufPtr), RTL_R8 (ChipCmd));

  tp->cur_rx = cur_rx;
}


static void rtl8139_weird_interrupt (struct net_device *dev,
                                     struct rtl8139_private *tp, void *ioaddr, int status, int link_changed)
{
  DPRINTK ("%s: Abnormal interrupt, status %8.8x.\n", dev->name, status);

  assert (dev != NULL);
  assert (tp != NULL);
  assert (ioaddr != NULL);

  /* Update the error count. */
  tp->stats.rx_missed_errors += RTL_R32 (RxMissed);
  RTL_W32 (RxMissed, 0);

  if ((status & RxUnderrun) && link_changed && (tp->drv_flags & HAS_LNK_CHNG))
  {
    /* Really link-change on new chips. */
    int lpar = RTL_R16 (NWayLPAR);
    int duplex = (lpar & 0x0100) || (lpar & 0x01C0) == 0x0040 || tp->duplex_lock;

    if (tp->full_duplex != duplex)
    {
      tp->full_duplex = duplex;
#if 0
      RTL_W8 (Cfg9346, Cfg9346_Unlock);
      RTL_W8 (Config1, tp->full_duplex ? 0x60 : 0x20);
      RTL_W8 (Cfg9346, Cfg9346_Lock);
#endif
    }
    status &= ~RxUnderrun;
  }

  /* XXX along with rtl8139_rx_err, are we double-counting errors? */
  if (status & (RxUnderrun | RxOverflow | RxErr | RxFIFOOver))
    tp->stats.rx_errors++;

  if (status & PCSTimeout)
    tp->stats.rx_length_errors++;
  if (status & (RxUnderrun | RxFIFOOver))
    tp->stats.rx_fifo_errors++;
  if (status & PCIErr)
  {
    u16 pci_cmd_status;

    pci_read_config_word (tp->pci_dev, PCI_STATUS, &pci_cmd_status);
    pci_write_config_word (tp->pci_dev, PCI_STATUS, pci_cmd_status);

    printk (KERN_ERR "%s: PCI Bus error %4.4x.\n", dev->name, pci_cmd_status);
  }
}


/* The interrupt handler does all of the Rx thread work and cleans up
 * after the Tx thread. */
static void rtl8139_interrupt (int irq, void *dev_instance, struct pt_regs *regs)
{
  struct net_device *dev = (struct net_device *) dev_instance;
  struct rtl8139_private *tp = dev->priv;
  int boguscnt = max_interrupt_work;
  void *ioaddr = tp->mmio_addr;
  int ackstat, status;
  int link_changed = 0;         /* avoid bogus "uninit" warning */

  spin_lock (&tp->lock);

  do
  {
    status = RTL_R16 (IntrStatus);

    /* h/w no longer present (hotplug?) or major error, bail */
    if (status == 0xFFFF)
      break;

    if ((status & (PCIErr | PCSTimeout | RxUnderrun | RxOverflow | RxFIFOOver | TxErr | TxOK | RxErr | RxOK)) == 0)
      break;

    /* Acknowledge all of the current interrupt sources ASAP, but
     * an first get an additional status bit from CSCR. */
    if (status & RxUnderrun)
      link_changed = RTL_R16 (CSCR) & CSCR_LinkChangeBit;

    /* The chip takes special action when we clear RxAckBits,
     * so we clear them later in rtl8139_rx_interrupt
     */
    ackstat = status & ~(RxAckBits | TxErr);
    RTL_W16 (IntrStatus, ackstat);

    DPRINTK ("%s: interrupt  status=%#4.4x ackstat=%#4.4x new intstat=%#4.4x.\n",
             dev->name, ackstat, status, RTL_R16 (IntrStatus));

    if (netif_running (dev) && (status & RxAckBits))
      rtl8139_rx_interrupt (dev, tp, ioaddr);

    /* Check uncommon events with one test. */
    if (status & (PCIErr | PCSTimeout | RxUnderrun | RxOverflow | RxFIFOOver | RxErr))
      rtl8139_weird_interrupt (dev, tp, ioaddr, status, link_changed);

    if (netif_running (dev) && (status & (TxOK | TxErr)))
    {
      rtl8139_tx_interrupt (dev, tp, ioaddr);
      if (status & TxErr)
        RTL_W16 (IntrStatus, TxErr);
    }

    boguscnt--;
  }
  while (boguscnt > 0);

  if (boguscnt <= 0)
  {
    printk ("%s: Too much work at interrupt, " "IntrStatus=0x%4.4x.\n", dev->name, status);

    /* Clear all interrupt sources. */
    RTL_W16 (IntrStatus, 0xffff);
  }

  spin_unlock (&tp->lock);

  DPRINTK ("%s: exiting interrupt, intr_status=%#4.4x.\n", dev->name, RTL_R16 (IntrStatus));
}


static int rtl8139_close (struct net_device *dev)
{
  struct rtl8139_private *tp = dev->priv;
  void *ioaddr = tp->mmio_addr;
  int ret = 0;
  unsigned long flags;

  netif_stop_queue (dev);

  if (tp->thr_pid >= 0)
  {
    tp->time_to_die = 1;
    wmb ();
    ret = kill_proc (tp->thr_pid, SIGTERM, 1);
    if (ret)
    {
      printk (KERN_ERR "%s: unable to signal thread\n", dev->name);
      return ret;
    }
    wait_for_completion (&tp->thr_exited);
  }

  DPRINTK ("%s: Shutting down ethercard, status was 0x%4.4x.\n", dev->name, RTL_R16 (IntrStatus));

  spin_lock_irqsave (&tp->lock, flags);

  /* Stop the chip's Tx and Rx DMA processes. */
  RTL_W8 (ChipCmd, 0);

  /* Disable interrupts by clearing the interrupt mask. */
  RTL_W16 (IntrMask, 0);

  /* Update the error counts. */
  tp->stats.rx_missed_errors += RTL_R32 (RxMissed);
  RTL_W32 (RxMissed, 0);

  spin_unlock_irqrestore (&tp->lock, flags);

  synchronize_irq ();
  free_irq (dev->irq, dev);

  rtl8139_tx_clear (tp);

  pci_free_consistent (tp->pci_dev, RX_BUF_TOT_LEN, tp->rx_ring, tp->rx_ring_dma);
  pci_free_consistent (tp->pci_dev, TX_BUF_TOT_LEN, tp->tx_bufs, tp->tx_bufs_dma);
  tp->rx_ring = NULL;
  tp->tx_bufs = NULL;

  /* Green! Put the chip in low-power mode. */
  RTL_W8 (Cfg9346, Cfg9346_Unlock);

  if (rtl_chip_info[tp->chipset].flags & HasHltClk)
    RTL_W8 (HltClk, 'H');       /* 'R' would leave the clock running. */

  return 0;
}


/* Get the ethtool settings.  Assumes that eset points to kernel
 * memory, *eset has been initialized as {ETHTOOL_GSET}, and other
 * threads or interrupts aren't messing with the 8139.  */
static void netdev_get_eset (struct net_device *dev, struct ethtool_cmd *eset)
{
  struct rtl8139_private *np = dev->priv;
  void *ioaddr = np->mmio_addr;
  u16 advert;

  eset->supported = SUPPORTED_10baseT_Half
    | SUPPORTED_10baseT_Full | SUPPORTED_100baseT_Half | SUPPORTED_100baseT_Full | SUPPORTED_Autoneg | SUPPORTED_TP;

  eset->advertising = ADVERTISED_TP | ADVERTISED_Autoneg;
  advert = mdio_read (dev, np->phys[0], 4);
  if (advert & 0x0020)
    eset->advertising |= ADVERTISED_10baseT_Half;
  if (advert & 0x0040)
    eset->advertising |= ADVERTISED_10baseT_Full;
  if (advert & 0x0080)
    eset->advertising |= ADVERTISED_100baseT_Half;
  if (advert & 0x0100)
    eset->advertising |= ADVERTISED_100baseT_Full;

  eset->speed = (RTL_R8 (MediaStatus) & 0x08) ? 10 : 100;
  /* (KON)FIXME: np->full_duplex is set or reset by the thread,
   * which means this always shows half duplex if the interface
   * isn't up yet, even if it has already autonegotiated.  */
  eset->duplex = np->full_duplex ? DUPLEX_FULL : DUPLEX_HALF;
  eset->port = PORT_TP;
  /* (KON)FIXME: Is np->phys[0] correct?  starfire.c uses that.  */
  eset->phy_address = np->phys[0];
  eset->transceiver = XCVR_INTERNAL;
  eset->autoneg = (mdio_read (dev, np->phys[0], 0) & 0x1000) != 0;
  eset->maxtxpkt = 1;
  eset->maxrxpkt = 1;
}


/* Get the ethtool Wake-on-LAN settings.  Assumes that wol points to
 * kernel memory, *wol has been initialized as {ETHTOOL_GWOL}, and
 * other threads or interrupts aren't messing with the 8139.  */
static void netdev_get_wol (struct net_device *dev, struct ethtool_wolinfo *wol)
{
  struct rtl8139_private *np = dev->priv;
  void *ioaddr = np->mmio_addr;

  if (rtl_chip_info[np->chipset].flags & HasLWake)
  {
    u8 cfg3 = RTL_R8 (Config3);
    u8 cfg5 = RTL_R8 (Config5);

    wol->supported = WAKE_PHY | WAKE_MAGIC | WAKE_UCAST | WAKE_MCAST | WAKE_BCAST;

    wol->wolopts = 0;
    if (cfg3 & Cfg3_LinkUp)
      wol->wolopts |= WAKE_PHY;
    if (cfg3 & Cfg3_Magic)
      wol->wolopts |= WAKE_MAGIC;
    /* (KON)FIXME: See how netdev_set_wol() handles the
     * following constants.  */
    if (cfg5 & Cfg5_UWF)
      wol->wolopts |= WAKE_UCAST;
    if (cfg5 & Cfg5_MWF)
      wol->wolopts |= WAKE_MCAST;
    if (cfg5 & Cfg5_BWF)
      wol->wolopts |= WAKE_BCAST;
  }
}


/* Set the ethtool Wake-on-LAN settings.  Return 0 or -errno.  Assumes
 * that wol points to kernel memory and other threads or interrupts
 * aren't messing with the 8139.  */
static int netdev_set_wol (struct net_device *dev, const struct ethtool_wolinfo *wol)
{
  struct rtl8139_private *np = dev->priv;
  void *ioaddr = np->mmio_addr;
  DWORD support;
  u8 cfg3, cfg5;

  support = ((rtl_chip_info[np->chipset].flags & HasLWake)
             ? (WAKE_PHY | WAKE_MAGIC | WAKE_UCAST | WAKE_MCAST | WAKE_BCAST) : 0);
  if (wol->wolopts & ~support)
    return -EINVAL;

  cfg3 = RTL_R8 (Config3) & ~(Cfg3_LinkUp | Cfg3_Magic);
  if (wol->wolopts & WAKE_PHY)
    cfg3 |= Cfg3_LinkUp;
  if (wol->wolopts & WAKE_MAGIC)
    cfg3 |= Cfg3_Magic;
  RTL_W8 (Cfg9346, Cfg9346_Unlock);
  RTL_W8 (Config3, cfg3);
  RTL_W8 (Cfg9346, Cfg9346_Lock);

  cfg5 = RTL_R8 (Config5) & ~(Cfg5_UWF | Cfg5_MWF | Cfg5_BWF);
  /* (KON)FIXME: These are untested.  We may have to set the
   * CRC0, Wakeup0 and LSBCRC0 registers too, but I have no
   * documentation.  */
  if (wol->wolopts & WAKE_UCAST)
    cfg5 |= Cfg5_UWF;
  if (wol->wolopts & WAKE_MCAST)
    cfg5 |= Cfg5_MWF;
  if (wol->wolopts & WAKE_BCAST)
    cfg5 |= Cfg5_BWF;
  RTL_W8 (Config5, cfg5);       /* need not unlock via Cfg9346 */

  return 0;
}


static int netdev_ethtool_ioctl (struct net_device *dev, void *useraddr)
{
  struct rtl8139_private *np = dev->priv;
  DWORD ethcmd;

  /* dev_ioctl() in ../../net/core/dev.c has already checked
   * capable(CAP_NET_ADMIN), so don't bother with that here.  */

  if (copy_from_user (&ethcmd, useraddr, sizeof (ethcmd)))
    return -EFAULT;

  switch (ethcmd)
  {
       case ETHTOOL_GSET:
         {
           struct ethtool_cmd eset = { ETHTOOL_GSET };

           spin_lock_irq (&np->lock);
           netdev_get_eset (dev, &eset);
           spin_unlock_irq (&np->lock);
           if (copy_to_user (useraddr, &eset, sizeof (eset)))
             return -EFAULT;
           return 0;
         }

         /* TODO: ETHTOOL_SSET */

       case ETHTOOL_GDRVINFO:
         {
           struct ethtool_drvinfo info = { ETHTOOL_GDRVINFO };

           strcpy (info.driver, DRV_NAME);
           strcpy (info.version, DRV_VERSION);
           strcpy (info.bus_info, np->pci_dev->slot_name);
           if (copy_to_user (useraddr, &info, sizeof (info)))
             return -EFAULT;
           return 0;
         }

       case ETHTOOL_GWOL:
         {
           struct ethtool_wolinfo wol = { ETHTOOL_GWOL };

           spin_lock_irq (&np->lock);
           netdev_get_wol (dev, &wol);
           spin_unlock_irq (&np->lock);
           if (copy_to_user (useraddr, &wol, sizeof (wol)))
             return -EFAULT;
           return 0;
         }

       case ETHTOOL_SWOL:
         {
           struct ethtool_wolinfo wol;
           int rc;

           if (copy_from_user (&wol, useraddr, sizeof (wol)))
             return -EFAULT;
           spin_lock_irq (&np->lock);
           rc = netdev_set_wol (dev, &wol);
           spin_unlock_irq (&np->lock);
           return rc;
         }

       default:
         break;
  }

  return -EOPNOTSUPP;
}


static int netdev_ioctl (struct net_device *dev, struct ifreq *rq, int cmd)
{
  struct rtl8139_private *tp = dev->priv;
  struct mii_ioctl_data *data = (struct mii_ioctl_data *) &rq->ifr_data;
  int rc = 0;
  int phy = tp->phys[0] & 0x3f;

  if (cmd != SIOCETHTOOL)
  {
    /* With SIOCETHTOOL, this would corrupt the pointer.  */
    data->phy_id &= 0x1f;
    data->reg_num &= 0x1f;
  }

  switch (cmd)
  {
       case SIOCETHTOOL:
         return netdev_ethtool_ioctl (dev, (void *) rq->ifr_data);

       case SIOCGMIIPHY:       /* Get the address of the PHY in use. */
       case SIOCDEVPRIVATE:    /* binary compat, remove in 2.5 */
         data->phy_id = phy;
         /* Fall Through */

       case SIOCGMIIREG:       /* Read the specified MII register. */
       case SIOCDEVPRIVATE + 1: /* binary compat, remove in 2.5 */
         data->val_out = mdio_read (dev, data->phy_id, data->reg_num);
         break;

       case SIOCSMIIREG:       /* Write the specified MII register */
       case SIOCDEVPRIVATE + 2: /* binary compat, remove in 2.5 */
         if (!capable (CAP_NET_ADMIN))
         {
           rc = -EPERM;
           break;
         }

         if (data->phy_id == phy)
         {
           u16 value = data->val_in;

           switch (data->reg_num)
           {
                case 0:
                  /* Check for autonegotiation on or reset. */
                  tp->medialock = (value & 0x9000) ? 0 : 1;
                  if (tp->medialock)
                    tp->full_duplex = (value & 0x0100) ? 1 : 0;
                  break;
                case 4:        /* tp->advertising = value; */
                  break;
           }
         }
         mdio_write (dev, data->phy_id, data->reg_num, data->val_in);
         break;

       default:
         rc = -EOPNOTSUPP;
         break;
  }

  return rc;
}


static struct net_device_stats *rtl8139_get_stats (struct net_device *dev)
{
  struct rtl8139_private *tp = dev->priv;
  void *ioaddr = tp->mmio_addr;
  unsigned long flags;

  if (netif_running (dev))
  {
    spin_lock_irqsave (&tp->lock, flags);
    tp->stats.rx_missed_errors += RTL_R32 (RxMissed);
    RTL_W32 (RxMissed, 0);
    spin_unlock_irqrestore (&tp->lock, flags);
  }

  return &tp->stats;
}

/* Set or clear the multicast filter for this adaptor.
 * This routine is not state sensitive and need not be SMP locked. */

static unsigned const ethernet_polynomial = 0x04c11db7U;
static inline DWORD ether_crc (int length, unsigned char *data)
{
  int crc = -1;

  while (--length >= 0)
  {
    unsigned char current_octet = *data++;
    int bit;

    for (bit = 0; bit < 8; bit++, current_octet >>= 1)
      crc = (crc << 1) ^ ((crc < 0) ^ (current_octet & 1) ? ethernet_polynomial : 0);
  }

  return crc;
}


static void __set_rx_mode (struct net_device *dev)
{
  struct rtl8139_private *tp = dev->priv;
  void *ioaddr = tp->mmio_addr;
  DWORD mc_filter[2];             /* Multicast hash filter */
  int i, rx_mode;
  DWORD tmp;

  DPRINTK ("%s:   rtl8139_set_rx_mode(%4.4x) done -- Rx config %8.8lx.\n", dev->name, dev->flags, RTL_R32 (RxConfig));

  /* Note: do not reorder, GCC is clever about common statements. */
  if (dev->flags & IFF_PROMISC)
  {
    /* Unconditionally log net taps. */
    printk (KERN_NOTICE "%s: Promiscuous mode enabled.\n", dev->name);
    rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys | AcceptAllPhys;
    mc_filter[1] = mc_filter[0] = 0xffffffff;
  }
  else if ((dev->mc_count > multicast_filter_limit) || (dev->flags & IFF_ALLMULTI))
  {
    /* Too many to filter perfectly -- accept all multicasts. */
    rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
    mc_filter[1] = mc_filter[0] = 0xffffffff;
  }
  else
  {
    struct dev_mc_list *mclist;

    rx_mode = AcceptBroadcast | AcceptMyPhys;
    mc_filter[1] = mc_filter[0] = 0;
    for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count; i++, mclist = mclist->next)
    {
      int bit_nr = ether_crc (ETH_ALEN, mclist->dmi_addr) >> 26;

      mc_filter[bit_nr >> 5] |= cpu_to_le32 (1 << (bit_nr & 31));
      rx_mode |= AcceptMulticast;
    }
  }

  /* We can safely update without stopping the chip. */
  tmp = rtl8139_rx_config | rx_mode;
  if (tp->rx_config != tmp)
  {
    RTL_W32_F (RxConfig, tmp);
    tp->rx_config = tmp;
  }
  RTL_W32_F (MAR0 + 0, mc_filter[0]);
  RTL_W32_F (MAR0 + 4, mc_filter[1]);
}

static void rtl8139_set_rx_mode (struct net_device *dev)
{
  unsigned long flags;
  struct rtl8139_private *tp = dev->priv;

  spin_lock_irqsave (&tp->lock, flags);
  __set_rx_mode (dev);
  spin_unlock_irqrestore (&tp->lock, flags);
}