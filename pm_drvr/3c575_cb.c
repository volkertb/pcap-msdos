/* EtherLinkXL.c: A 3Com EtherLink PCI III/XL ethernet driver for linux. */
/*
 * Written 1996-1999 by Donald Becker.
 * 
 * This software may be used and distributed according to the terms
 * of the GNU Public License, incorporated herein by reference.
 * 
 * This driver is for the 3Com "Vortex" and "Boomerang" series ethercards.
 * Members of the series include Fast EtherLink 3c590/3c592/3c595/3c597
 * and the EtherLink XL 3c900 and 3c905 cards.
 * 
 * The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
 * Center of Excellence in Space Data and Information Sciences
 * Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
 */

static char *version = "3c575_cb.c:v0.99L 5/28/99 Donald Becker http://cesdis.gsfc.nasa.gov/linux/drivers/vortex.html\n";

#include "pmdrvr.h"
#include "bios32.h"
#include "pci.h"
#include "module.h"
#include "3c575_cb.h"


/* Allow setting MTU to a larger size, bypassing the normal ethernet setup.
 */
static const int mtu = 1500;

/* Maximum events (Rx packets, etc.) to handle at each interrupt.
 */
static int max_interrupt_work = 20;

/* Put out somewhat more debugging messages. (0: no msg, 1 minimal .. 6).
 */
#ifndef VORTEX_DEBUG
#define VORTEX_DEBUG 0
#endif

int debug_3c575 LOCKED_VAR = VORTEX_DEBUG;


/* Set if a MII transceiver on any interface requires mdio preamble.
 * This only set with the original DP83840 on older 3c905 boards, so the
 * extra code size of a per-interface flag is not worthwhile.
 */
static char mii_preamble_required = 0;

/* Some values here only for performance evaluation and path-coverage
 * debugging.
 */
static int rx_nocopy = 0, rx_copy = 0;

/* "Knobs" that adjust features and parameters.
 * Set the copy breakpoint for the copy-only-tiny-frames scheme.
 * Setting to > 1512 effectively disables this feature.
 */
static const int rx_copybreak = 200;

/* A few values that may be tweaked.
 * Time in jiffies before concluding the transmitter is hung.
 */
#define TX_TIMEOUT  (2*HZ)

/* Keep the ring sizes a power of two for efficiency.
 */
#define TX_RING_SIZE  16
#define RX_RING_SIZE  32
#define PKT_BUF_SZ    1536        /* Size of each temporary Rx buffer. */

static struct device *vortex_probe1 (struct device *dev,
                                     int pci_bus, int pci_devfn, long io_addr,
                                     int irq, int dev_id, int card_id);


/* Operational definitions.
 * These are not used by other compilation units and thus are not
 * exported in a ".h" file.
 * 
 * First the windows.  There are eight register windows, with the command
 * and status registers available in each.
 */
#define EL3WINDOW(win) outw(SelectWindow + (win), ioaddr + EL3_CMD)
#define EL3_CMD        0x0E
#define EL3_STATUS     0x0E

/* The top five bits written to EL3_CMD are a command, the lower
 * 11 bits are the parameter, if applicable.
 * Note that 11 parameters bits was fine for ethernet, but the new chip
 * can handle FDDI length frames (~4500 octets) and now parameters count
 * 32-bit 'Dwords' rather than octets.
 */
enum vortex_cmd {
     TotalReset = 0 << 11,
     SelectWindow = 1 << 11,
     StartCoax = 2 << 11,
     RxDisable = 3 << 11,
     RxEnable = 4 << 11,
     RxReset = 5 << 11,
     UpStall = 6 << 11,
     UpUnstall = (6 << 11) + 1,
     DownStall = (6 << 11) + 2,
     DownUnstall = (6 << 11) + 3,
     RxDiscard = 8 << 11,
     TxEnable = 9 << 11,
     TxDisable = 10 << 11,
     TxReset = 11 << 11,
     FakeIntr = 12 << 11,
     AckIntr = 13 << 11,
     SetIntrEnb = 14 << 11,
     SetStatusEnb = 15 << 11,
     SetRxFilter = 16 << 11,
     SetRxThreshold = 17 << 11,
     SetTxThreshold = 18 << 11,
     SetTxStart = 19 << 11,
     StartDMAUp = 20 << 11,
     StartDMADown = (20 << 11) + 1,
     StatsEnable = 21 << 11,
     StatsDisable = 22 << 11,
     StopCoax = 23 << 11,
     SetFilterBit = 25 << 11,
   };

/* The SetRxFilter command accepts the following classes:
 */
enum RxFilter {
     RxStation = 1, RxMulticast = 2,
     RxBroadcast = 4, RxProm = 8
   };

/* Bits in the general status register.
 */
enum vortex_status {
     IntLatch = 0x0001,
     HostError = 0x0002,
     TxComplete = 0x0004,
     TxAvailable = 0x0008,
     RxComplete = 0x0010,
     RxEarly = 0x0020,
     IntReq = 0x0040,
     StatsFull = 0x0080,
     DMADone = 1 << 8,
     DownComplete = 1 << 9,
     UpComplete = 1 << 10,
     DMAInProgress = 1 << 11,      /* DMA controller is still busy. */
     CmdInProgress = 1 << 12,      /* EL3_CMD is still busy. */
   };

/* Register window 1 offsets, the window used in normal operation.
 * On the Vortex this window is always mapped at offsets 0x10-0x1f.
 */
enum Window1 {
     TX_FIFO = 0x10,
     RX_FIFO = 0x10,
     RxErrors = 0x14,
     RxStatus = 0x18,
     Timer = 0x1A,
     TxStatus = 0x1B,
     TxFree = 0x1C,            /* Remaining free bytes in Tx buffer. */
   };

enum Window0 {
     Wn0EepromCmd = 10,        /* Window 0: EEPROM command register. */
     Wn0EepromData = 12,       /* Window 0: EEPROM results register. */
     IntrStatus = 0x0E,        /* Valid in all windows. */
   };

enum Win0_EEPROM_bits {
     EEPROM_Read = 0x80,
     EEPROM_WRITE = 0x40,
     EEPROM_ERASE = 0xC0,
     EEPROM_EWENB = 0x30,      /* Enable erasing/writing for 10 msec. */
     EEPROM_EWDIS = 0x00,      /* Disable EWENB before 10 msec timeout. */
   };

/* EEPROM locations.
 */
enum eeprom_offset {
     PhysAddr01 = 0,
     PhysAddr23 = 1,
     PhysAddr45 = 2,
     ModelID = 3,
     EtherLink3ID = 7,
     IFXcvrIO = 8,
     IRQLine = 9,
     NodeAddr01 = 10,
     NodeAddr23 = 11,
     NodeAddr45 = 12,
     DriverTune = 13,
     Checksum = 15
   };

enum Window2 {                /* Window 2. */
     Wn2_ResetOptions = 12,
   };

enum Window3 {                /* Window 3: MAC/config bits. */
     Wn3_Config = 0,
     Wn3_MAC_Ctrl = 6,
     Wn3_Options = 8,
   };

union wn3_config {
      int    i;
      struct w3_config_fields {
        UINT ram_size  : 3;
        UINT ram_width : 1;
        UINT ram_speed : 2;
        UINT rom_size  : 2;
        int  pad8      : 8;
        UINT ram_split : 2;
        UINT pad18     : 2;
        UINT xcvr      : 4;
        UINT autoselect: 1;
        int  pad24     : 7;
      } u;
    };

enum Window4 {        /* Window 4: Xcvr/media bits. */
     Wn4_FIFODiag = 4,
     Wn4_NetDiag = 6,
     Wn4_PhysicalMgmt = 8,
     Wn4_Media = 10,
   };

enum Win4_Media_bits {
     Media_SQE = 0x0008,       /* Enable SQE error counting for AUI. */
     Media_10TP = 0x00C0,      /* Enable link beat and jabber for 10baseT. */
     Media_Lnk = 0x0080,       /* Enable just link beat for 100TX/100FX. */
     Media_LnkBeat = 0x0800,
   };

enum Window7 {              /* Window 7: Bus Master control. */
     Wn7_MasterAddr = 0,
     Wn7_MasterLen = 6,
     Wn7_MasterStatus = 12,
   };

/* Boomerang bus master control registers.
 */
enum MasterCtrl {
     PktStatus = 0x20,
     DownListPtr = 0x24,
     FragAddr = 0x28,
     FragLen = 0x2c,
     TxFreeThreshold = 0x2f,
     UpPktStatus = 0x30,
     UpListPtr = 0x38,
   };

/* The Rx and Tx descriptor lists.
 * Caution Alpha hackers: these types are 32 bits!  Note also the 8 byte
 * alignment contraint on tx_ring[] and rx_ring[].
 */
#define LAST_FRAG  0x80000000   /* Last Addr/Len pair in descriptor. */
struct boom_rx_desc {
       DWORD next;              /* Last entry points to 0.   */
       DWORD status;
       DWORD addr;              /* Up to 63 addr/len pairs possible. */
       DWORD length;            /* Set LAST_FRAG to indicate last pair. */
     };

/* Values for the Rx status entry.
 */
enum rx_desc_status {
     RxDComplete = 0x00008000,
     RxDError = 0x4000,
     /* See boomerang_rx() for actual error bits */
     IPChksumErr = 1 << 25,
     TCPChksumErr = 1 << 26,
     UDPChksumErr = 1 << 27,
     IPChksumValid = 1 << 29,
     TCPChksumValid = 1 << 30,
     UDPChksumValid = 1 << 31,
   };

struct boom_tx_desc {
       DWORD next;              /* Last entry points to 0.   */
       DWORD status;            /* bits 0:12 length, others see below.  */
       DWORD addr;
       DWORD length;
     };

/* Values for the Tx status entry.
 */
enum tx_desc_status {
     CRCDisable = 0x2000,
     TxDComplete = 0x8000,
     AddIPChksum = 0x02000000,
     AddTCPChksum = 0x04000000,
     AddUDPChksum = 0x08000000,
     TxIntrUploaded = 0x80000000,  /* IRQ when in FIFO, but maybe not sent. */
   };

/* Chip features we care about in vp->capabilities, read from the EEPROM.
 */
enum ChipCaps {
     CapBusMaster = 0x20,
     CapPwrMgmt = 0x2000
   };

struct vortex_private {
  /* The Rx and Tx rings should be quad-word-aligned.
   */
  struct boom_rx_desc rx_ring[RX_RING_SIZE];
  struct boom_tx_desc tx_ring[TX_RING_SIZE];

  struct net_device_stats stats;
  struct device          *next_module;

  void *rx_buf[RX_RING_SIZE];
  void *tx_buf[TX_RING_SIZE];
  void *tx_skb;             /* Packet being eaten by bus master ctrl.  */
  void *priv_addr;
  UINT  cur_rx, cur_tx;     /* The next free ring entry */
  UINT  dirty_rx, dirty_tx; /* The ring entries to be free()ed. */

  /* PCI configuration space information.
   */
  BYTE  pci_bus, pci_devfn;        /* PCI bus location, for power management. */
  char *cb_fn_base;                /* CardBus function status addr space. */
  int   chip_id;

  /* The remainder are related to chip state, mostly media selection.
   */
  struct timer_list timer;         /* Media selection timer. */
  DWORD  in_interrupt;
  int    options;                  /* User-settable misc. driver options. */

  UINT media_override:4, /* Passed-in media type. */
       default_media:4,            /* Read from the EEPROM/Wn3_Config. */
       full_duplex:1,
       force_fd:1,
       autoselect:1,
       bus_master:1,               /* Vortex can only do a fragment bus-m. */
       full_bus_master_tx:1,
       full_bus_master_rx:2,       /* Boomerang  */
       hw_csums:1,                 /* Has hardware checksums. */
       tx_full:1,
       open:1;

  WORD status_enable;
  WORD intr_enable;
  WORD available_media;            /* From Wn3_Options. */
  WORD capabilities,info1,info2;   /* Various, from EEPROM. */
  WORD advertising;                /* NWay media advertisement */
  BYTE phys[2];                    /* MII device addresses. */
};

/* The action to take with a media selection timer tick.
 * Note that we deviate from the 3Com order by checking 10base2 before AUI.
 */
enum xcvr_types {
     XCVR_10baseT = 0,
     XCVR_AUI,
     XCVR_10baseTOnly,
     XCVR_10base2,
     XCVR_100baseTx,
     XCVR_100baseFx,
     XCVR_MII = 6,
     XCVR_NWAY = 8,
     XCVR_ExtMII = 9,
     XCVR_Default = 10,
   };

static struct media_table {
       char *name;
       UINT media_bits:16, /* Bits to set in Wn4_Media register. */
            mask:8,        /* The transceiver-present bit in Wn3_Config. */
            next:8;        /* The media type to try next. */
       int  wait;          /* Time before we check media status. */
     } media_tbl[] = {
  { "10baseT",      Media_10TP, 0x08, XCVR_10base2,  (14 * HZ) / 10 } ,
  { "10Mbs AUI",    Media_SQE,  0x20, XCVR_Default,  (1 * HZ) / 10 } ,
  { "undefined",    0,          0x80, XCVR_10baseT,  10000 } ,
  { "10base2",      0,          0x10, XCVR_AUI,      (1 * HZ) / 10 } ,
  { "100baseTX",    Media_Lnk,  0x02, XCVR_100baseFx,(14 * HZ) / 10 } ,
  { "100baseFX",    Media_Lnk,  0x04, XCVR_MII,      (14 * HZ) / 10 } ,
  { "MII",          0,          0x41, XCVR_10baseT,  3 * HZ } ,
  { "undefined",    0,          0x01, XCVR_10baseT,  10000 } ,
  { "Autonegotiate",0,          0x41, XCVR_10baseT,  3 * HZ } ,
  { "MII-External", 0,          0x41, XCVR_10baseT,  3 * HZ } ,
  { "Default",      0,          0xFF, XCVR_10baseT,  10000 }
};

static int   vortex_scan (struct device *dev, struct pci_id_info *pci_tbl);
static void  vortex_up (struct device *dev);
static void  vortex_down (struct device *dev);
static int   vortex_open (struct device *dev);
static void  vortex_close (struct device *dev);
static int   vortex_start_xmit (struct device *dev, const void* buf, int len);
static void  vortex_timer (DWORD arg);
static void  vortex_interrupt (int irq);
static void  vortex_rx (struct device *dev);
static void *vortex_get_stats (struct device *dev);

static void mdio_sync (long ioaddr, int bits);
static int  mdio_read (long ioaddr, int phy_id, int location);
static void mdio_write (long ioaddr, int phy_id, int location, int value);
static int  boomerang_start_xmit (struct device *dev, const void* buf, int len);
static void boomerang_rx (struct device *dev);
static void update_stats (struct device *dev, long ioaddr);
static void set_rx_mode  (struct device *dev);

static void acpi_wake    (int pci_bus, int pci_devfn);
static void acpi_set_WOL (struct device *dev);

/* This driver uses 'options' to pass the media type, full-duplex flag, etc.
 *
 * Option count limit only -- unlimited interfaces are supported.
 */
#define MAX_UNITS 8
static int options    [MAX_UNITS] = { -1, -1, -1, -1, -1, -1, -1, -1, };
static int full_duplex[MAX_UNITS] = { -1, -1, -1, -1, -1, -1, -1, -1 };

/* A list of all installed Vortex devices, for removing the driver module.
 */
static struct device *root_vortex_dev = NULL;

static struct pci_id_info pci_tbl[] = {
    { "3c590 Vortex 10Mbps", 0x10B7, 0x5900, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_VORTEX, 32, vortex_probe1 },
    { "3c595 Vortex 100baseTx", 0x10B7, 0x5950, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_VORTEX, 32, vortex_probe1 },
    { "3c595 Vortex 100baseT4", 0x10B7, 0x5951, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_VORTEX, 32, vortex_probe1 },
    { "3c595 Vortex 100base-MII", 0x10B7, 0x5952, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_VORTEX, 32, vortex_probe1 },
    { "3Com Vortex", 0x10B7, 0x5900, 0xff00,
      PCI_USES_IO | PCI_USES_MASTER, IS_BOOMERANG, 64, vortex_probe1  },
    { "3c900 Boomerang 10baseT", 0x10B7, 0x9000, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_BOOMERANG, 64, vortex_probe1 },
    { "3c900 Boomerang 10Mbps Combo", 0x10B7, 0x9001, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_BOOMERANG, 64, vortex_probe1 },
    { "3c900 Cyclone 10Mbps Combo", 0x10B7, 0x9005, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_CYCLONE, 128, vortex_probe1 },
    { "3c900B-FL Cyclone 10base-FL", 0x10B7, 0x900A, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_CYCLONE, 128, vortex_probe1 },
    { "3c905 Boomerang 100baseTx", 0x10B7, 0x9050, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_BOOMERANG | HAS_MII, 64, vortex_probe1 },
    { "3c905 Boomerang 100baseT4", 0x10B7, 0x9051, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_BOOMERANG | HAS_MII, 64, vortex_probe1 },
    { "3c905B Cyclone 100baseTx", 0x10B7, 0x9055, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_CYCLONE | HAS_NWAY, 128, vortex_probe1 },
    { "3c905B Cyclone 10/100/BNC", 0x10B7, 0x9058, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_CYCLONE | HAS_NWAY, 128, vortex_probe1 },
    { "3c905B-FX Cyclone 100baseFx", 0x10B7, 0x905A, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_CYCLONE, 128, vortex_probe1 },
    { "3c905C Tornado", 0x10B7, 0x9200, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_CYCLONE, 128, vortex_probe1 },
    { "3c980 Cyclone", 0x10B7, 0x9800, 0xfff0,
      PCI_USES_IO | PCI_USES_MASTER, IS_CYCLONE, 128, vortex_probe1 },
    { "3cSOHO100-TX Hurricane", 0x10B7, 0x7646, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_CYCLONE, 128, vortex_probe1 },
    { "3c555 Laptop Hurricane", 0x10B7, 0x5055, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_CYCLONE, 128, vortex_probe1 },
    { "3c575 Boomerang CardBus", 0x10B7, 0x5057, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_BOOMERANG | HAS_MII, 64, vortex_probe1 },
    { "3CCFE575 Cyclone CardBus", 0x10B7, 0x5157, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_CYCLONE | HAS_NWAY | HAS_CB_FNS,
      128, vortex_probe1 },
    { "3CCFE575CT Cyclone CardBus", 0x10B7, 0x5257, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_CYCLONE | HAS_NWAY | HAS_CB_FNS,
      128, vortex_probe1 },
    { "3CCFE656 Cyclone CardBus", 0x10B7, 0x6560, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_CYCLONE | HAS_NWAY | HAS_CB_FNS,
      128, vortex_probe1 },
    { "3CCFEM656 Cyclone CardBus", 0x10B7, 0x6562, 0xffff,
      PCI_USES_IO | PCI_USES_MASTER, IS_CYCLONE | HAS_NWAY | HAS_CB_FNS,
      128, vortex_probe1 },
    { "3c575 series CardBus (unknown version)", 0x10B7, 0x5057, 0xf0ff,
      PCI_USES_IO | PCI_USES_MASTER, IS_BOOMERANG | HAS_MII, 64, vortex_probe1 },
    { "3Com Boomerang (unknown version)", 0x10B7, 0x9000, 0xff00,
      PCI_USES_IO | PCI_USES_MASTER, IS_BOOMERANG, 64, vortex_probe1 },
    { NULL }
  };


int tc90x_probe (struct device *dev)
{
  static int did_version = -1;
  int    rc = vortex_scan (dev, pci_tbl);

  if (rc && ++did_version <= 0)
     printk ("%s", version);
  return (rc);
}

static int vortex_scan (struct device *dev, struct pci_id_info *pci_tbl)
{
  int cards_found = 0;

  /* Ideally we would detect all cards in slot order.  That would
   * be best done a central PCI probe dispatch, which wouldn't work
   * well with the current structure.  So instead we detect 3Com cards
   * in slot order.
   */
  if (pcibios_present())
  {
    static int pci_index = 0;
    BYTE   pci_bus, pci_device_fn;

    for (; pci_index < 0xff; pci_index++)
    {
      WORD vendor, device;
      int  chip_idx;

      if (pcibios_find_class (PCI_CLASS_NETWORK_ETHERNET << 8, pci_index,
                              &pci_bus, &pci_device_fn) != PCIBIOS_SUCCESSFUL)
         break;

      pcibios_read_config_word (pci_bus, pci_device_fn, PCI_VENDOR_ID, &vendor);
      pcibios_read_config_word (pci_bus, pci_device_fn, PCI_DEVICE_ID, &device);

      for (chip_idx = 0; pci_tbl[chip_idx].vendor_id; chip_idx++)
          if (vendor == pci_tbl[chip_idx].vendor_id &&
              (device & pci_tbl[chip_idx].device_id_mask) ==
                        pci_tbl[chip_idx].device_id)
             break;

      if (pci_tbl[chip_idx].vendor_id == 0) /* Compiled out! */
         continue;

      /* The Cyclone requires config space re-write if powered down.
       */
      acpi_wake (pci_bus, pci_device_fn);
    }
  }

  /* Now check all slots of the EISA bus.
   */
  if (EISA_bus)
  {
    static long ioaddr = 0x1000;

    for (; ioaddr < 0x9000; ioaddr += 0x1000)
    {
      int device_id;

      /* Check the standard EISA ID register for an encoded '3Com'.
       */
      if (inw (ioaddr + 0xC80) != 0x6d50)
         continue;

      /* Check for a product that we support, 3c59{2,7} any rev.
       */
      device_id = (inb (ioaddr + 0xC82) << 8) + inb (ioaddr + 0xC83);
      if ((device_id & 0xFF00) != 0x5900)
         continue;

      vortex_probe1 (dev, 0, 0, ioaddr, inw (ioaddr + 0xC88) >> 12, 4,
                     cards_found);
      dev = 0;
      cards_found++;
    }
  }
  return (cards_found);
}

static struct device *vortex_probe1 (struct device *dev,
                                     int pci_bus, int pci_devfn, long ioaddr,
                                     int irq, int chip_idx, int card_idx)
{
  struct vortex_private *vp;
  UINT   eeprom[0x40], checksum = 0; /* EEPROM contents */
  int    i, option;

// !! dev = init_etherdev (dev, 0);

  printk ("%s: 3Com %s at 0x%lx, ", dev->name, pci_tbl[chip_idx].name, ioaddr);

  dev->base_addr = ioaddr;
  dev->irq       = irq;
  dev->mtu       = mtu;

  /* Make certain the descriptor lists are aligned. */
  {
    void *mem = k_malloc (sizeof(*vp) + 15);

    vp = (void*) (((long)mem + 15) & ~15);
    vp->priv_addr = mem;
  }
  memset (vp, 0, sizeof(*vp));
  dev->priv = vp;

  vp->next_module = root_vortex_dev;
  root_vortex_dev = dev;

  vp->chip_id   = chip_idx;
  vp->pci_bus   = pci_bus;
  vp->pci_devfn = pci_devfn;

  /* The lower four bits are the media type.
   */
  if (dev->mem_start)
       option = dev->mem_start;
  else if (card_idx < MAX_UNITS)
       option = options[card_idx];
  else option = -1;

  if (option >= 0)
  {
    vp->media_override = ((option & 7) == 2) ? 0 : option & 15;
    vp->full_duplex    = (option & 0x200) ? 1 : 0;
    vp->bus_master     = (option & 16) ? 1 : 0;
  }
  else
  {
    vp->media_override = 7;
    vp->full_duplex    = 0;
    vp->bus_master     = 0;
  }

  if (card_idx < MAX_UNITS && full_duplex[card_idx] > 0)
     vp->full_duplex = 1;

  vp->force_fd = vp->full_duplex;
  vp->options = option;

  /* Read the station address from the EEPROM.
   */
  EL3WINDOW (0);
  for (i = 0; i < 0x40; i++)
  {
    int timer;

#ifdef CARDBUS
    outw (0x230 + i, ioaddr + Wn0EepromCmd);
#else
    outw (EEPROM_Read + i, ioaddr + Wn0EepromCmd);
#endif

    /* Pause for at least 162 us. for the read to take place.
     */
    for (timer = 10; timer >= 0; timer--)
    {
      udelay (162);
      if ((inw (ioaddr + Wn0EepromCmd) & 0x8000) == 0)
        break;
    }
    eeprom[i] = inw (ioaddr + Wn0EepromData);
  }
  for (i = 0; i < 0x18; i++)
     checksum ^= eeprom[i];
  checksum = (checksum ^ (checksum >> 8)) & 0xff;

  if (checksum != 0)
  {                             /* Grrr, needless incompatible change 3Com. */
    while (i < 0x21)
      checksum ^= eeprom[i++];
    checksum = (checksum ^ (checksum >> 8)) & 0xff;
  }
  if (checksum != 0)
     printk (" ***INVALID CHECKSUM %x*** ", checksum);

  for (i = 0; i < 3; i++)
      ((WORD *) dev->dev_addr)[i] = htons (eeprom[i + 10]);
  for (i = 0; i < 6; i++)
      printk ("%c%2.2x", i ? ':' : ' ', dev->dev_addr[i]);

  EL3WINDOW (2);
  for (i = 0; i < 6; i++)
     outb (dev->dev_addr[i], ioaddr + i);

  printk (", IRQ %d\n", dev->irq);

  /* Tell them about an invalid IRQ.
   */
  if (debug_3c575 && (dev->irq <= 0 || dev->irq >= NUM_IRQS))
     printk (" *** Warning: IRQ %d is unlikely to work! ***\n", dev->irq);

#if 0 /* !!to-do */
  if (pci_tbl[vp->chip_id].drv_flags & HAS_CB_FNS)
  {
    DWORD fn_st_addr;             /* Cardbus function status space */

    pcibios_read_config_dword (pci_bus, pci_devfn, PCI_BASE_ADDRESS_2,
                               &fn_st_addr);
    if (fn_st_addr)
       vp->cb_fn_base = ioremap (fn_st_addr & ~3, 128);
    printk ("%s: CardBus functions mapped %x->%p\n",
            dev->name, fn_st_addr, vp->cb_fn_base);
  }
#endif

  /* Extract our information from the EEPROM data.
   */
  vp->info1        = eeprom[13];
  vp->info2        = eeprom[15];
  vp->capabilities = eeprom[16];

  if (vp->info1 & 0x8000)
     vp->full_duplex = 1;
  {
    static char *ram_split[] = { "5:3", "3:1", "1:1", "3:5" };
    union  wn3_config config;

    EL3WINDOW (3);
    vp->available_media = inw (ioaddr + Wn3_Options);
    if ((vp->available_media & 0xff) == 0) /* Broken 3c916 */
       vp->available_media = 0x40;

    config.i = inl (ioaddr + Wn3_Config);

    if (debug_3c575 > 1)
       printk ("  Internal config register is %x, "
               "transceivers %x.\n", config.i, inw (ioaddr + Wn3_Options));

    printk ("  %dK %s-wide RAM %s Rx:Tx split, %s%s interface.\n",
            8 << config.u.ram_size,
            config.u.ram_width ? "word" : "byte",
            ram_split[config.u.ram_split],
            config.u.autoselect ? "autoselect/" : "",
            config.u.xcvr > XCVR_ExtMII ? "<invalid transceiver>" :
                                          media_tbl[config.u.xcvr].name);
    vp->default_media = config.u.xcvr;
    vp->autoselect    = config.u.autoselect;
  }

  if (vp->media_override != 7)
  {
    printk ("  Media override to transceiver type %d (%s).\n",
            vp->media_override, media_tbl[vp->media_override].name);
    dev->if_port = vp->media_override;
  }
  else
    dev->if_port = vp->default_media;

  if (dev->if_port == XCVR_MII || dev->if_port == XCVR_NWAY)
  {
    int phy, phy_idx = 0;

    EL3WINDOW (4);
    mii_preamble_required++;
    mii_preamble_required++;
    mdio_read (ioaddr, 24, 1);
    for (phy = 1; phy <= 32 && phy_idx < sizeof (vp->phys); phy++)
    {
      int mii_status, phyx = phy & 0x1f;

      mii_status = mdio_read (ioaddr, phyx, 1);
      if (mii_status && mii_status != 0xffff)
      {
        vp->phys[phy_idx++] = phyx;
        printk ("  MII transceiver found at address %d, status %x.\n",
                phyx, mii_status);
        if ((mii_status & 0x0040) == 0)
           mii_preamble_required++;
      }
    }
    mii_preamble_required--;
    if (phy_idx == 0)
    {
      printk ("  ***WARNING*** No MII transceivers found!\n");
      vp->phys[0] = 24;
    }
    else
    {
      vp->advertising = mdio_read (ioaddr, vp->phys[0], 4);
      if (vp->full_duplex)
      {
        /* Only advertise the FD media types.
         */
        vp->advertising &= ~0x02A0;
        mdio_write (ioaddr, vp->phys[0], 4, vp->advertising);
      }
    }
  }

  if (vp->capabilities & CapPwrMgmt)
     acpi_set_WOL (dev);

  if (vp->capabilities & CapBusMaster)
  {
    vp->full_bus_master_tx = 1;
    printk ("  Enabling bus-master transmits and %s receives.\n",
            (vp->info2 & 1) ? "early" : "whole-frame");
    vp->full_bus_master_rx = (vp->info2 & 1) ? 1 : 2;
  }

  /* The 3c59x-specific entries in the device structure.
   */
  dev->open      = vortex_open;
  dev->xmit      = vortex_start_xmit;
  dev->close     = vortex_close;
  dev->get_stats = vortex_get_stats;
  dev->set_multicast_list = set_rx_mode;
  return (dev);
}

static void wait_for_completion (struct device *dev, int cmd)
{
  int i = 2000;

  outw (cmd, dev->base_addr + EL3_CMD);
  while (--i > 0)
       if (!(inw (dev->base_addr + EL3_STATUS) & CmdInProgress))
          break;
  if (i == 0)
     printk ("%s: command 0x%04x did not complete!\n", dev->name, cmd);
}

static void vortex_up (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private *) dev->priv;
  union  wn3_config config;
  long   ioaddr = dev->base_addr;
  int    i;

  acpi_wake (vp->pci_bus, vp->pci_devfn);

  /* Before initializing select the active media port.
   */
  EL3WINDOW (3);
  config.i = inl (ioaddr + Wn3_Config);

  if (vp->media_override != 7)
  {
    if (debug_3c575 > 1)
       printk ("%s: Media override to transceiver %d (%s).\n",
              dev->name, vp->media_override,
              media_tbl[vp->media_override].name);
    dev->if_port = vp->media_override;
  }
  else if (vp->autoselect)
  {
    if (pci_tbl[vp->chip_id].drv_flags & HAS_NWAY)
       dev->if_port = XCVR_NWAY;
    else
    {
      /* Find first available media type, starting with 100baseTx.
       */
      dev->if_port = XCVR_100baseTx;
      while (!(vp->available_media & media_tbl[dev->if_port].mask))
         dev->if_port = media_tbl[dev->if_port].next;
    }
  }
  else
    dev->if_port = vp->default_media;

  init_timer (&vp->timer);
  vp->timer.expires  = RUN_AT (media_tbl[dev->if_port].wait);
  vp->timer.data     = (DWORD) dev;
  vp->timer.function = vortex_timer; /* timer handler */
  add_timer (&vp->timer);

  if (debug_3c575 > 1)
     printk ("%s: Initial media type %s.\n",
             dev->name, media_tbl[dev->if_port].name);

  vp->full_duplex = vp->force_fd;
  config.u.xcvr = dev->if_port;
  if (!(pci_tbl[vp->chip_id].drv_flags & HAS_NWAY))
     outl (config.i, ioaddr + Wn3_Config);

  if (dev->if_port == XCVR_MII || dev->if_port == XCVR_NWAY)
  {
    int mii_reg1, mii_reg5;

    EL3WINDOW (4);

    /* Read BMSR (reg1) only to clear old status.
     */
    mii_reg1 = mdio_read (ioaddr, vp->phys[0], 1);
    mii_reg5 = mdio_read (ioaddr, vp->phys[0], 5);
    if (mii_reg5 == 0xffff || mii_reg5 == 0x0000)
       ;                    /* No MII device or no link partner report */
    else if ((mii_reg5 & 0x0100) != 0 ||     /* 100baseTx-FD */
             (mii_reg5 & 0x00C0) == 0x0040)  /* 10T-FD, but not 100-HD */
      vp->full_duplex = 1;

    if (debug_3c575 > 1)
       printk ("%s: MII #%d status %x, link partner capability %x,"
               " setting %s-duplex.\n",
               dev->name, vp->phys[0], mii_reg1, mii_reg5,
               vp->full_duplex ? "full" : "half");
    EL3WINDOW (3);
  }

  /* Set the full-duplex bit. */
  outb (((vp->info1 & 0x8000) || vp->full_duplex ? 0x20 : 0) |
        (dev->mtu > 1500 ? 0x40 : 0), ioaddr + Wn3_MAC_Ctrl);

  if (debug_3c575 > 1)
     printk ("%s: vortex_open() InternalConfig %8.8x.\n",
             dev->name, config.i);

  wait_for_completion (dev, TxReset);
  wait_for_completion (dev, RxReset);

  outw (SetStatusEnb | 0x00, ioaddr + EL3_CMD);

  if (debug_3c575 > 1)
  {
    EL3WINDOW (4);
    printk ("%s: vortex_open() irq %d media status %x.\n",
            dev->name, dev->irq, inw (ioaddr + Wn4_Media));
  }

  /* Set the station address and mask in window 2 each time opened.
   */
  EL3WINDOW (2);
  for (i = 0; i < 6; i++)
     outb (dev->dev_addr[i], ioaddr + i);

  for (; i < 12; i += 2)
     outw (0, ioaddr + i);

  if (vp->cb_fn_base)
  {
    WORD n = inw (ioaddr + Wn2_ResetOptions);

    /* Inverted LED polarity */
    if (pci_tbl[vp->chip_id].device_id != 0x5257)
       n |= 0x0010;

    /* Inverted polarity of MII power bit */
    if ((pci_tbl[vp->chip_id].device_id == 0x6560) ||
        (pci_tbl[vp->chip_id].device_id == 0x6562) ||
        (pci_tbl[vp->chip_id].device_id == 0x5257))
      n |= 0x4000;
    outw (n, ioaddr + Wn2_ResetOptions);
  }

  if (dev->if_port == XCVR_10base2)
  {
    /* Start the thinnet transceiver. We should really wait 50ms... */
    outw (StartCoax, ioaddr + EL3_CMD);
  }
  if (dev->if_port != XCVR_NWAY)
  {
    EL3WINDOW (4);
    outw ((inw (ioaddr + Wn4_Media) & ~(Media_10TP | Media_SQE)) |
          media_tbl[dev->if_port].media_bits, ioaddr + Wn4_Media);
  }

  /* Switch to the stats window, and clear all stats by reading.
   */
  outw (StatsDisable, ioaddr + EL3_CMD);
  EL3WINDOW (6);
  for (i = 0; i < 10; i++)
    inb (ioaddr + i);
  inw (ioaddr + 10);
  inw (ioaddr + 12);

  /* New: On the Vortex we must also clear the BadSSD counter.
   */
  EL3WINDOW (4);
  inb (ioaddr + 12);

  /* ..and on the Boomerang we enable the extra statistics bits.
   */
  outw (0x0040, ioaddr + Wn4_NetDiag);

  /* Switch to register set 7 for normal use.
   */
  EL3WINDOW (7);

  if (vp->full_bus_master_rx)
  {                             /* Boomerang bus master. */
    vp->cur_rx = vp->dirty_rx = 0;
    /* Initialize the RxEarly register as recommended.
     */
    outw (SetRxThreshold + (1536 >> 2), ioaddr + EL3_CMD);
    outl (0x0020, ioaddr + PktStatus);
    outl (VIRT_TO_PHYS (&vp->rx_ring[0]), ioaddr + UpListPtr);
  }

  if (vp->full_bus_master_tx)
  {                             /* Boomerang bus master Tx. */
    dev->xmit  = boomerang_start_xmit;
    vp->cur_tx = vp->dirty_tx = 0;
    outb (PKT_BUF_SZ >> 8, ioaddr + TxFreeThreshold); /* Room for a packet. */

    /* Clear the Tx ring.
     */
    for (i = 0; i < TX_RING_SIZE; i++)
        vp->tx_buf[i] = NULL;
    outl (0, ioaddr + DownListPtr);
  }

  /* Set receiver mode: presumably accept b-case and phys addr only.
   */
  set_rx_mode (dev);
  outw (StatsEnable, ioaddr + EL3_CMD); /* Turn on statistics. */

  vp->in_interrupt = 0;
  dev->tx_busy = 0;
  dev->reentry = 0;
  dev->start   = 1;

  outw (RxEnable, ioaddr + EL3_CMD); /* Enable the receiver. */
  outw (TxEnable, ioaddr + EL3_CMD); /* Enable transmitter. */

  /* Allow status bits to be seen.
   */
  vp->status_enable = SetStatusEnb|HostError|IntReq|StatsFull|TxComplete|
    (vp->full_bus_master_tx ? DownComplete : TxAvailable) |
    (vp->full_bus_master_rx ? UpComplete : RxComplete) |
    (vp->bus_master ? DMADone : 0);

  vp->intr_enable = SetIntrEnb | IntLatch | TxAvailable | RxComplete |
                    StatsFull | HostError | TxComplete | IntReq |
                    (vp->bus_master ? DMADone : 0) | UpComplete | DownComplete;
  outw (vp->status_enable, ioaddr + EL3_CMD);

  /* Ack all pending events, and set active indicator mask.
   */
  outw (AckIntr | IntLatch | TxAvailable | RxEarly | IntReq, ioaddr + EL3_CMD);
  outw (vp->intr_enable, ioaddr + EL3_CMD);

  if (vp->cb_fn_base)       /* The PCMCIA people are idiots.  */
     writel (0x8000, vp->cb_fn_base + 4);
}

static int vortex_open (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private *) dev->priv;
  int    i;

  /* Use the now-standard shared IRQ implementation.
   */
  if (!request_irq (dev->irq, &vortex_interrupt))
     return (0);

  if (vp->full_bus_master_rx)
  {                             /* Boomerang bus master. */
    if (debug_3c575 > 2)
       printk ("%s:  Filling in the Rx ring.\n", dev->name);

    for (i = 0; i < RX_RING_SIZE; i++)
    {
      void *buf;

      vp->rx_ring[i].next   = VIRT_TO_PHYS (&vp->rx_ring[i+1]);
      vp->rx_ring[i].status = 0; /* Clear complete bit. */
      vp->rx_ring[i].length = (PKT_BUF_SZ | LAST_FRAG);
      buf = k_malloc (PKT_BUF_SZ);
      vp->rx_buf[i] = buf;
      if (!buf)
         break;                 /* Bad news!  */

      vp->rx_ring[i].addr = VIRT_TO_PHYS (buf);
    }
    /* Wrap the ring. */
    vp->rx_ring[i-1].next = VIRT_TO_PHYS (&vp->rx_ring[0]);
  }
  if (vp->full_bus_master_tx)
     dev->xmit = boomerang_start_xmit;

  vortex_up (dev);
  vp->open = 1;
  return (1);
}

static void vortex_timer (DWORD data)
{
  struct device         *dev = (struct device*) data;
  struct vortex_private *vp  = (struct vortex_private*) dev->priv;
  long   ioaddr    = dev->base_addr;
  int    next_tick = 60 * HZ;
  int    ok        = 0;
  int    media_status, mii_status, old_window;

  if (debug_3c575 > 1)
     printk ("%s: Media selection timer tick happened, %s.\n",
             dev->name, media_tbl[dev->if_port].name);

  disable_irq (dev->irq);
  old_window = inw (ioaddr + EL3_CMD) >> 13;
  EL3WINDOW (4);
  media_status = inw (ioaddr + Wn4_Media);

  switch (dev->if_port)
  {
    case XCVR_10baseT:
    case XCVR_100baseTx:
    case XCVR_100baseFx:
         if (media_status & Media_LnkBeat)
         {
           ok = 1;
           if (debug_3c575 > 1)
             printk ("%s: Media %s has link beat, %x.\n",
                     dev->name, media_tbl[dev->if_port].name, media_status);
         }
         else if (debug_3c575 > 1)
           printk ("%s: Media %s is has no link beat, %x.\n",
                   dev->name, media_tbl[dev->if_port].name, media_status);
         break;

    case XCVR_MII:
    case XCVR_NWAY:
         mii_status = mdio_read (ioaddr, vp->phys[0], 1);
         ok = 1;
         if (debug_3c575 > 1)
            printk ("%s: MII transceiver has status %x.\n",
                    dev->name, mii_status);

         if (mii_status & 0x0004)
         {
           int mii_reg5 = mdio_read (ioaddr, vp->phys[0], 5);

           if (!vp->force_fd && mii_reg5 != 0xffff)
           {
             int duplex = (mii_reg5 & 0x0100) || (mii_reg5 & 0x01C0) == 0x0040;

             if (vp->full_duplex != duplex)
             {
               vp->full_duplex = duplex;
               printk ("%s: Setting %s-duplex based on MII "
                       "#%d link partner capability of %x.\n",
                       dev->name, vp->full_duplex ? "full" : "half",
                       vp->phys[0], mii_reg5);
               /* Set the full-duplex bit. */
               EL3WINDOW (3);
               outb ((vp->full_duplex ? 0x20 : 0) |
                     (dev->mtu > 1500 ? 0x40 : 0), ioaddr + Wn3_MAC_Ctrl);
             }
             next_tick = 60 * HZ;
           }
         }
         break;

    default:                /* Other media types handled by Tx timeouts. */
         if (debug_3c575 > 1)
            printk ("%s: Media %s is has no indication, %x.\n",
                    dev->name, media_tbl[dev->if_port].name, media_status);
         ok = 1;
  }

  if (!ok)
  {
    union wn3_config config;

    do
    {
      dev->if_port = media_tbl[dev->if_port].next;
    }
    while (!(vp->available_media & media_tbl[dev->if_port].mask));

    if (dev->if_port == XCVR_Default)
    {                           /* Go back to default. */
      dev->if_port = vp->default_media;
      if (debug_3c575 > 1)
         printk ("%s: Media selection failing, using default "
                 "%s port.\n", dev->name, media_tbl[dev->if_port].name);
    }
    else
    {
      if (debug_3c575 > 1)
         printk ("%s: Media selection failed, now trying "
                 "%s port.\n", dev->name, media_tbl[dev->if_port].name);
      next_tick = media_tbl[dev->if_port].wait;
    }
    outw ((media_status & ~(Media_10TP | Media_SQE)) | media_tbl[dev->if_port].media_bits, ioaddr + Wn4_Media);

    EL3WINDOW (3);
    config.i = inl (ioaddr + Wn3_Config);
    config.u.xcvr = dev->if_port;
    outl (config.i, ioaddr + Wn3_Config);

    outw (dev->if_port == XCVR_10base2 ? StartCoax : StopCoax,
          ioaddr + EL3_CMD);
  }
  EL3WINDOW (old_window);
  enable_irq (dev->irq);

  if (debug_3c575 > 2)
     printk ("%s: Media selection timer finished, %s.\n",
             dev->name, media_tbl[dev->if_port].name);

  vp->timer.expires = RUN_AT (next_tick);
  add_timer (&vp->timer);
}

static void vortex_tx_timeout (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private *) dev->priv;
  long   ioaddr = dev->base_addr;

  printk ("%s: transmit timed out, tx_status %2.2x status %4.4x.\n",
          dev->name, inb (ioaddr + TxStatus), inw (ioaddr + EL3_STATUS));

  /* Slight code bloat to be user friendly.
   */
  if ((inb (ioaddr + TxStatus) & 0x88) == 0x88)
     printk ("%s: Transmitter encountered 16 collisions -- "
             "network cable problem?\n", dev->name);
  if (inw (ioaddr + EL3_STATUS) & IntLatch)
  {
    printk ("%s: Interrupt posted but not delivered --"
            " IRQ blocked by another device?\n", dev->name);
    /* Bad idea here.. but we might as well handle a few events. */
    vortex_interrupt (dev->irq);
  }

#if !defined(final_version)
  if (vp->full_bus_master_tx)
  {
    int i;

    printk ("  Flags; bus-master %d, full %d; dirty %d "
            "current %d.\n", vp->full_bus_master_tx, vp->tx_full,
            vp->dirty_tx, vp->cur_tx);
    printk ("  Transmit list %lx vs. %p.\n",
            inl (ioaddr + DownListPtr), &vp->tx_ring[vp->dirty_tx % TX_RING_SIZE]);
    for (i = 0; i < TX_RING_SIZE; i++)
    {
      printk ("  %d: @%p  length %lx status %lx\n",
              i, &vp->tx_ring[i], vp->tx_ring[i].length,
              vp->tx_ring[i].status);
    }
  }
#endif

  wait_for_completion (dev, TxReset);

  vp->stats.tx_errors++;
  if (vp->full_bus_master_tx)
  {
    if (debug_3c575 > 0)
       printk ("%s: Resetting the Tx ring pointer.\n", dev->name);

    if (vp->cur_tx - vp->dirty_tx > 0 && inl (ioaddr + DownListPtr) == 0)
       outl (VIRT_TO_PHYS(&vp->tx_ring[vp->dirty_tx % TX_RING_SIZE]),
             ioaddr + DownListPtr);

    if (vp->tx_full && (vp->cur_tx - vp->dirty_tx <= TX_RING_SIZE - 1))
    {
      vp->tx_full  = 0;
      dev->tx_busy = 0;
    }
    outb (PKT_BUF_SZ >> 8, ioaddr + TxFreeThreshold);
    outw (DownUnstall, ioaddr + EL3_CMD);
  }
  else
    vp->stats.tx_dropped++;

  /* Issue Tx Enable
   */
  outw (TxEnable, ioaddr + EL3_CMD);
  dev->tx_start = jiffies;

  /* Switch to register set 7 for normal use.
   */
  EL3WINDOW (7);
}

/*
 * Handle uncommon interrupt sources.  This is a separate routine to minimize
 * the cache impact.
 */
static void vortex_error (struct device *dev, int status)
{
  struct vortex_private *vp = (struct vortex_private *) dev->priv;
  long   ioaddr = dev->base_addr;
  int    do_tx_reset = 0;

  if (status & TxComplete)
  {                             /* Really "TxError" for us. */
    BYTE tx_status = inb (ioaddr + TxStatus);

    /* Presumably a tx-timeout. We must merely re-enable. */
    if (debug_3c575 > 2 || (tx_status != 0x88 && debug_3c575 > 0))
       printk ("%s: Transmit error, Tx status register %x.\n",
               dev->name, tx_status);

    if (tx_status & 0x14)
       vp->stats.tx_fifo_errors++;
    if (tx_status & 0x38)
       vp->stats.tx_aborted_errors++;
    outb (0, ioaddr + TxStatus);

    if (tx_status & 0x30)
         do_tx_reset = 1;
    else outw (TxEnable, ioaddr + EL3_CMD); /* re-enable the transmitter. */
  }

  if (status & RxEarly)
  {                             /* Rx early is unused. */
    vortex_rx (dev);
    outw (AckIntr | RxEarly, ioaddr + EL3_CMD);
  }
  if (status & StatsFull)
  {                             /* Empty statistics. */
    static int DoneDidThat = 0;

    if (debug_3c575 > 4)
       printk ("%s: Updating stats.\n", dev->name);
    update_stats (dev, ioaddr);

    /* HACK: Disable statistics as an interrupt source.
     * This occurs when we have the wrong media type!
     */
    if (DoneDidThat == 0 && inw (ioaddr + EL3_STATUS) & StatsFull)
    {
      printk ("%s: Updating statistics failed, disabling "
              "stats as an interrupt source.\n", dev->name);
      EL3WINDOW (5);
      outw (SetIntrEnb | (inw (ioaddr + 10) & ~StatsFull), ioaddr + EL3_CMD);
      EL3WINDOW (7);
      DoneDidThat++;
    }
  }

  if (status & IntReq)
  {                             /* Restore all interrupt sources.  */
    outw (vp->status_enable, ioaddr + EL3_CMD);
    outw (vp->intr_enable, ioaddr + EL3_CMD);
  }

  if (status & HostError)
  {
    WORD fifo_diag;

    EL3WINDOW (4);
    fifo_diag = inw (ioaddr + Wn4_FIFODiag);
    printk ("%s: Host error, FIFO diagnostic register %x.\n",
            dev->name, fifo_diag);

    /* Adapter failure requires Tx/Rx reset and reinit.
     */
    if (vp->full_bus_master_tx)
    {
      wait_for_completion (dev, TotalReset | 0xff);
      /* Re-enable the receiver. */
      outw (RxEnable, ioaddr + EL3_CMD);
      outw (TxEnable, ioaddr + EL3_CMD);
    }
    else if (fifo_diag & 0x0400)
      do_tx_reset = 1;

    if (fifo_diag & 0x3000)
    {
      wait_for_completion (dev, RxReset);
      /* Set the Rx filter to the current state.
       */
      set_rx_mode (dev);
      outw (RxEnable, ioaddr + EL3_CMD); /* Re-enable the receiver. */
      outw (AckIntr | HostError, ioaddr + EL3_CMD);
    }
  }

  if (do_tx_reset)
  {
    wait_for_completion (dev, TxReset);
    outw (TxEnable, ioaddr + EL3_CMD);
  }

}

static int vortex_start_xmit (struct device *dev, const void *buf, int len)
{
  struct vortex_private *vp = (struct vortex_private *) dev->priv;
  long   ioaddr = dev->base_addr;

  if (dev->tx_busy)
  {
    if (jiffies - dev->tx_start >= TX_TIMEOUT)
       vortex_tx_timeout (dev);
    return (0);
  }

  dev->tx_busy = 1;

  /* Put out the doubleword header...
   */
  outl (len, ioaddr + TX_FIFO);

  if (vp->bus_master)
  {
    void *tx_buf = k_malloc (len);

    if (!tx_buf)
       return (0);

    /* Set the bus-master controller to transfer the packet.
     */
    memcpy (tx_buf, buf, len);
    outl (VIRT_TO_PHYS(tx_buf), ioaddr + Wn7_MasterAddr);
    outw ((len + 3) & ~3, ioaddr + Wn7_MasterLen);
    vp->tx_skb = tx_buf;
    outw (StartDMADown, ioaddr + EL3_CMD);

    /* dev->tx_busy will be cleared at the DMADone interrupt. */
  }
  else
  {
    /* ... and the packet rounded to a doubleword.
     */
    rep_outsl (ioaddr + TX_FIFO, buf, (len + 3) >> 2);
    if (inw (ioaddr + TxFree) > 1536)
    {
      dev->tx_busy = 0;
    }
    else
    {
      /* Interrupt us when the FIFO has room for max-sized packet. */
      outw (SetTxThreshold + (1536 >> 2), ioaddr + EL3_CMD);
    }
  }

  dev->tx_start = jiffies;

  /* Clear the Tx status stack. */
  {
    int tx_status;
    int i = 32;

    while (--i > 0 && (tx_status = inb (ioaddr + TxStatus)) > 0)
    {
      if (tx_status & 0x3C)
      {                         /* A Tx-disabling error occurred.  */
        if (debug_3c575 > 2)
           printk ("%s: Tx error, status %2.2x.\n", dev->name, tx_status);
        if (tx_status & 0x04)
           vp->stats.tx_fifo_errors++;
        if (tx_status & 0x38)
           vp->stats.tx_aborted_errors++;
        if (tx_status & 0x30)
           wait_for_completion (dev, TxReset);

        outw (TxEnable, ioaddr + EL3_CMD);
      }
      outb (0x00, ioaddr + TxStatus); /* Pop the status stack. */
    }
  }
  return (1);
}

static int boomerang_start_xmit (struct device *dev, const void *buf, int len)
{
  struct vortex_private *vp = (struct vortex_private *) dev->priv;
  struct boom_tx_desc   *prev_entry;
  long   ioaddr = dev->base_addr;
  int    entry;
  void  *tx_buf;

  if (dev->tx_busy)
  {
    if (jiffies - dev->tx_start >= TX_TIMEOUT)
       vortex_tx_timeout (dev);
    return (0);
  }

  /* Calculate the next Tx descriptor entry.
   */
  entry = vp->cur_tx % TX_RING_SIZE;
  prev_entry = &vp->tx_ring[(vp->cur_tx - 1) % TX_RING_SIZE];

  if (debug_3c575 > 3)
     printk ("%s: Trying to send a packet, Tx index %d.\n",
             dev->name, vp->cur_tx);

  if (vp->tx_full || (tx_buf = k_malloc(len)) == NULL)
  {
    if (debug_3c575 > 0)
       printk ("%s: Tx Ring full, refusing to send buffer.\n", dev->name);
    return (0);
  }
  memcpy (tx_buf, buf, len);
  vp->tx_buf[entry] = tx_buf;
  vp->tx_ring[entry].next   = 0;
  vp->tx_ring[entry].addr   = VIRT_TO_PHYS(buf);
  vp->tx_ring[entry].length = len | LAST_FRAG;
  vp->tx_ring[entry].status = len | TxIntrUploaded;

  DISABLE();

  /* Wait for the stall to complete.
   */
  wait_for_completion (dev, DownStall);
  prev_entry->next = VIRT_TO_PHYS (&vp->tx_ring[entry]);
  if (inl(ioaddr + DownListPtr) == 0)
     outl (VIRT_TO_PHYS(&vp->tx_ring[entry]), ioaddr + DownListPtr);

  outw (DownUnstall, ioaddr + EL3_CMD);
  ENABLE();

  vp->cur_tx++;
  if (vp->cur_tx - vp->dirty_tx > TX_RING_SIZE - 1)
     vp->tx_full = 1;
  else
  {                           /* Clear previous interrupt enable. */
#if defined(tx_interrupt_mitigation)
    prev_entry->status &= ~TxIntrUploaded;
#endif
    dev->tx_busy = 0;
  }

  dev->tx_start = jiffies;
  return (1);
}

/*
 * The interrupt handler does all of the Rx thread work and cleans up
 * after the Tx thread.
 */
static void vortex_interrupt (int irq)
{
  struct device         *dev = irq2dev_map[irq];
  struct vortex_private *vp  = (struct vortex_private*) dev->priv;
  long   ioaddr;
  int    latency, status;
  int    work_done = max_interrupt_work;

  if (!dev || dev->irq != irq)
  {
    printk ("vortex_interrupt(): irq %d for unknown device.", irq);
    return;
  }

  if (dev->reentry)
     printk ("%s: Re-entering the interrupt handler.\n", dev->name);

  dev->reentry = 1;

  ioaddr  = dev->base_addr;
  latency = inb (ioaddr + Timer);
  status  = inw (ioaddr + EL3_STATUS);

  if (status == 0xffff)
     goto handler_exit;

  if (debug_3c575 > 4)
     printk ("%s: interrupt, status %4.4x, latency %d ticks.\n",
             dev->name, status, latency);
  do
  {
    if (debug_3c575 > 5)
       printk ("%s: In interrupt loop, status %4x.\n", dev->name, status);
    if (status & RxComplete)
       vortex_rx (dev);
    if (status & UpComplete)
    {
      outw (AckIntr | UpComplete, ioaddr + EL3_CMD);
      boomerang_rx (dev);
    }

    if (status & TxAvailable)
    {
      if (debug_3c575 > 5)
         printk ("    TX room bit was handled.\n");

      /* There's room in the FIFO for a full-sized packet.
       */
      outw (AckIntr | TxAvailable, ioaddr + EL3_CMD);
      dev->tx_busy = 0;
    }

    if (status & DownComplete)
    {
      UINT dirty_tx = vp->dirty_tx;

      outw (AckIntr | DownComplete, ioaddr + EL3_CMD);
      while (vp->cur_tx - dirty_tx > 0)
      {
        int entry = dirty_tx % TX_RING_SIZE;

        if (inl (ioaddr + DownListPtr) == VIRT_TO_PHYS(&vp->tx_ring[entry]))
           break;            /* It still hasn't been processed. */

        if (vp->tx_buf[entry])
        {
          k_free (vp->tx_buf[entry]);
          vp->tx_buf[entry] = NULL;
        }
        /* vp->stats.tx_packets++;  Counted below.
         */
        dirty_tx++;
      }
      vp->dirty_tx = dirty_tx;
      if (vp->tx_full && (vp->cur_tx - dirty_tx <= TX_RING_SIZE - 1))
      {
        vp->tx_full = 0;
        dev->tx_busy = 0;
      }
    }
    if (status & DMADone)
    {
      if (inw (ioaddr + Wn7_MasterStatus) & 0x1000)
      {
        outw (0x1000, ioaddr + Wn7_MasterStatus); /* Ack the event. */
        k_free (vp->tx_skb);       /* Release the transfered buffer */
        if (inw (ioaddr + TxFree) > 1536)
          dev->tx_busy = 0;
        else          /* Interrupt when FIFO has room for max-sized packet. */
          outw (SetTxThreshold + (1536 >> 2), ioaddr + EL3_CMD);
      }
    }

    /* Check for all uncommon interrupts at once.
     */
    if (status & (HostError | RxEarly | StatsFull | TxComplete | IntReq))
    {
      if (status == 0xffff)
        break;
      vortex_error (dev, status);
    }

    if (--work_done < 0)
    {
      if ((status & (0x7fe - (UpComplete | DownComplete))) == 0)
      {
        /* Just ack these and return. */
        outw (AckIntr | UpComplete | DownComplete, ioaddr + EL3_CMD);
      }
      else
      {
        printk ("%s: Too much work in interrupt, status "
                "%x.  Temporarily disabling functions (%x).\n",
                dev->name, status, SetStatusEnb | ((~status) & 0x7FE));

        /* Disable all pending interrupts.
         */
        outw (SetStatusEnb | ((~status) & 0x7FE), ioaddr + EL3_CMD);
        outw (AckIntr | 0x7FF, ioaddr + EL3_CMD);
        /* The timer will reenable interrupts.
         */
        break;
      }
    }
    /* Acknowledge the IRQ.
     */
    outw (AckIntr | IntReq | IntLatch, ioaddr + EL3_CMD);
    if (vp->cb_fn_base)         /* The PCMCIA people are idiots.  */
      writel (0x8000, vp->cb_fn_base + 4);

  }
  while ((status = inw (ioaddr + EL3_STATUS)) & (IntLatch | RxComplete));

  if (debug_3c575 > 4)
     printk ("%s: exiting interrupt, status %x.\n", dev->name, status);

handler_exit:
  dev->reentry = 0;
}

static void vortex_rx (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private *) dev->priv;
  long   ioaddr = dev->base_addr;
  int    i;
  short  rx_status;

  if (debug_3c575 > 5)
     printk ("   In rx_packet(), status %4.4x, rx_status %4.4x.\n",
             inw (ioaddr + EL3_STATUS), inw (ioaddr + RxStatus));

  while ((rx_status = inw (ioaddr + RxStatus)) > 0)
  {
    if (rx_status & 0x4000)
    {                           /* Error, update stats. */
      BYTE rx_error = inb (ioaddr + RxErrors);

      if (debug_3c575 > 2)
         printk (" Rx error: status %2.2x.\n", rx_error);
      vp->stats.rx_errors++;
      if (rx_error & 0x01)
         vp->stats.rx_over_errors++;
      if (rx_error & 0x02)
         vp->stats.rx_length_errors++;
      if (rx_error & 0x04)
         vp->stats.rx_frame_errors++;
      if (rx_error & 0x08)
         vp->stats.rx_crc_errors++;
      if (rx_error & 0x10)
         vp->stats.rx_length_errors++;
    }
    else
    {
      /* The packet length: up to 4.5K!. */
      int   pkt_len = rx_status & 0x1fff;
      char *pkt_buf;

      if (debug_3c575 > 4)
         printk ("Receiving packet size %d status %x.\n", pkt_len, rx_status);

      if (dev->get_rx_buf && (pkt_buf = (*dev->get_rx_buf) (pkt_len)) != NULL)
      {
        if (vp->bus_master && !(inw (ioaddr + Wn7_MasterStatus) & 0x8000))
        {
          outl (VIRT_TO_PHYS(pkt_buf), ioaddr + Wn7_MasterAddr);
          outw ((pkt_len + 3) & ~3, ioaddr + Wn7_MasterLen);
          outw (StartDMAUp, ioaddr + EL3_CMD);
          while (inw (ioaddr + Wn7_MasterStatus) & 0x8000)
              ;
        }
        else
        {
          rep_insl (ioaddr + RX_FIFO, (DWORD*)pkt_buf, (pkt_len+3) >> 2);
        }

        outw (RxDiscard, ioaddr + EL3_CMD); /* Pop top Rx packet. */

        dev->last_rx = jiffies;
        vp->stats.rx_packets++;

        /* Wait a limited time to go to next packet.
         */
        for (i = 200; i >= 0; i--)
            if (!(inw (ioaddr + EL3_STATUS) & CmdInProgress))
               break;
      }
      else
      {
        outw (RxDiscard, ioaddr + EL3_CMD);
        vp->stats.rx_dropped++;
      }
    }

    /* Wait a limited time to skip this packet.
     */
    for (i = 200; i >= 0; i--)
      if (!(inw (ioaddr + EL3_STATUS) & CmdInProgress))
        break;
  }
}

static void boomerang_rx (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private*) dev->priv;
  int    entry              = vp->cur_rx % RX_RING_SIZE;
  long   ioaddr             = dev->base_addr;
  int    rx_work_limit      = vp->dirty_rx + RX_RING_SIZE - vp->cur_rx;
  int    rx_status;

  if (debug_3c575 > 5)
     printk ("  In boomerang_rx(), status %x, rx_status "
             "%x.\n", inw (ioaddr + EL3_STATUS), inw (ioaddr + RxStatus));

  while ((rx_status = vp->rx_ring[entry].status) & RxDComplete)
  {
    if (--rx_work_limit < 0)
       break;

    if (rx_status & RxDError)
    {                           /* Error, update stats. */
      BYTE rx_error = rx_status >> 16;

      if (debug_3c575 > 2)
         printk (" Rx error: status %x.\n", rx_error);
      vp->stats.rx_errors++;
      if (rx_error & 0x01)
         vp->stats.rx_over_errors++;
      if (rx_error & 0x02)
         vp->stats.rx_length_errors++;
      if (rx_error & 0x04)
         vp->stats.rx_frame_errors++;
      if (rx_error & 0x08)
         vp->stats.rx_crc_errors++;
      if (rx_error & 0x10)
         vp->stats.rx_length_errors++;
    }
    else
    {
      char *buf;
      int   len = rx_status & 0x1fff; /* The packet length: up to 4.5K!. */

      if (debug_3c575 > 4)
         printk ("Receiving packet size %d status %x.\n", len, rx_status);

      if (dev->get_rx_buf && (buf = (*dev->get_rx_buf)(len)) != NULL)
      {
        /* Check if the packet is long enough to just accept without
         * copying to a properly sized ring-buf.
         */
        if (len < rx_copybreak)
        {
          memcpy (buf, PHYS_TO_VIRT(vp->rx_ring[entry].addr), len);
          rx_copy++;
        }
        else
        {
          /* Pass up the buffer already on the Rx ring.
           */
          memcpy (buf, vp->rx_buf[entry], len);
          vp->rx_buf[entry] = NULL;
          rx_nocopy++;
        }
        dev->last_rx = jiffies;
        vp->stats.rx_packets++;
      }
      else
      {
        vp->stats.rx_dropped++;
      }
    }
    entry = (++vp->cur_rx) % RX_RING_SIZE;
  }

  /* Refill the Rx ring buffers.
   */
  for (; vp->dirty_rx < vp->cur_rx; vp->dirty_rx++)
  {
    entry = vp->dirty_rx % RX_RING_SIZE;
    if (!vp->rx_buf[entry])
    {
      void *buf = k_malloc (PKT_BUF_SZ);

      if (!buf)
         break;                 /* Bad news!  */

      vp->rx_ring[entry].addr = VIRT_TO_PHYS (buf);
      vp->rx_buf[entry] = buf;
    }
    vp->rx_ring[entry].status = 0; /* Clear complete bit. */
    outw (UpUnstall, ioaddr + EL3_CMD);
  }
}

static void vortex_down (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private *) dev->priv;
  long   ioaddr             = dev->base_addr;

  dev->start   = 0;
  dev->tx_busy = 1;

  del_timer (&vp->timer);

  /* Turn off statistics ASAP.  We update vp->stats below. */
  outw (StatsDisable, ioaddr + EL3_CMD);

  /* Disable the receiver and transmitter. */
  outw (RxDisable, ioaddr + EL3_CMD);
  outw (TxDisable, ioaddr + EL3_CMD);

  if (dev->if_port == XCVR_10base2)
    /* Turn off thinnet power.  Green! */
    outw (StopCoax, ioaddr + EL3_CMD);

  outw (SetIntrEnb | 0x0000, ioaddr + EL3_CMD);

  update_stats (dev, ioaddr);
  if (vp->full_bus_master_rx)
     outl (0, ioaddr + UpListPtr);
  if (vp->full_bus_master_tx)
     outl (0, ioaddr + DownListPtr);

  if (vp->capabilities & CapPwrMgmt)
    acpi_set_WOL (dev);
}

static void vortex_close (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private *) dev->priv;
  long   ioaddr = dev->base_addr;
  int    i;

  if (dev->start)
     vortex_down (dev);

  if (debug_3c575 > 1)
  {
    printk ("%s: vortex_close() status %4.4x, Tx status %x.\n",
            dev->name, inw (ioaddr + EL3_STATUS), inb (ioaddr + TxStatus));
    printk ("%s: vortex close stats: rx_nocopy %d rx_copy %d.\n",
            dev->name, rx_nocopy, rx_copy);
  }

  free_irq (dev->irq);
  irq2dev_map[dev->irq] = NULL;

  if (vp->full_bus_master_rx)
  {                             /* Free Boomerang bus master Rx buffers. */
    for (i = 0; i < RX_RING_SIZE; i++)
      if (vp->rx_buf[i])
      {
        k_free (vp->rx_buf[i]);
        vp->rx_buf[i] = NULL;
      }
  }
  if (vp->full_bus_master_tx)
  {                             /* Free Boomerang bus master Tx buffers. */
    for (i = 0; i < TX_RING_SIZE; i++)
      if (vp->tx_buf[i])
      {
        k_free (vp->tx_buf[i]);
        vp->tx_buf[i] = NULL;
      }
  }
  vp->open = 0;
}

static void *vortex_get_stats (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private *) dev->priv;

  if (dev->start)
  {
    DISABLE();
    update_stats (dev, dev->base_addr);
    ENABLE();
  }
  return (void*)&vp->stats;
}

/*  Update statistics.
 * Unlike with the EL3 we need not worry about interrupts changing
 * the window setting from underneath us, but we must still guard
 * against a race condition with a StatsUpdate interrupt updating the
 * table.  This is done by checking that the ASM (!) code generated uses
 * atomic updates with '+='.
 */
static void update_stats (struct device *dev, long ioaddr)
{
  struct vortex_private *vp = (struct vortex_private*) dev->priv;
  int    old_window         = inw (ioaddr + EL3_CMD);
  BYTE   up;

  if (old_window == 0xffff)     /* Chip suspended or ejected. */
     return;

  /* Unlike the 3c5x9 we need not turn off stats updates while reading.
   * Switch to the stats window, and read everything.
   */
  EL3WINDOW (6);
  vp->stats.tx_carrier_errors   += inb (ioaddr + 0);
  vp->stats.tx_heartbeat_errors += inb (ioaddr + 1);

  /* Multiple collisions. */ (void) inb (ioaddr + 2);
  vp->stats.tx_collisions    += inb (ioaddr + 3);
  vp->stats.tx_window_errors += inb (ioaddr + 4);
  vp->stats.rx_fifo_errors   += inb (ioaddr + 5);
  vp->stats.tx_packets       += inb (ioaddr + 6);
  vp->stats.tx_packets       += (inb (ioaddr + 9) & 0x30) << 4;
  /* Rx packets */   (void) inb (ioaddr + 7); /* Must read to clear */
  /* Tx deferrals */ (void) inb (ioaddr + 8);

  /* Don't bother with register 9, an extension of registers 6&7.
   * If we do use the 6&7 values the atomic update assumption above
   * is invalid.
   */
  vp->stats.rx_bytes += inw (ioaddr + 10);
  vp->stats.tx_bytes += inw (ioaddr + 12);

  /* New: On the Vortex we must also clear the BadSSD counter.
   */
  EL3WINDOW (4);
  inb (ioaddr + 12);

  up = inb (ioaddr + 13);
  vp->stats.rx_bytes += (up & 0x0f) << 16;
  vp->stats.tx_bytes += (up & 0xf0) << 12;

  /* We change back to window 7 (not 1) with the Vortex.
   */
  EL3WINDOW (old_window >> 13);
}


/*
 * Pre-Cyclone chips have no documented multicast filter, so the only
 * multicast setting is to receive all multicast frames.  At least
 * the chip has a very clean way to set the mode, unlike many others.
 */
static void set_rx_mode (struct device *dev)
{
  long ioaddr = dev->base_addr;
  int  new_mode;

  if (dev->flags & IFF_PROMISC)
  {
    if (debug_3c575 > 0)
       printk ("%s: Setting promiscuous mode.\n", dev->name);
    new_mode = SetRxFilter | RxStation | RxMulticast | RxBroadcast | RxProm;
  }
  else if ((dev->mc_list) || (dev->flags & IFF_ALLMULTI))
  {
    new_mode = SetRxFilter | RxStation | RxMulticast | RxBroadcast;
  }
  else
    new_mode = SetRxFilter | RxStation | RxBroadcast;

  outw (new_mode, ioaddr + EL3_CMD);
}

/* MII transceiver control section.
 * Read and write the MII registers using software-generated serial
 * MDIO protocol.  See the MII specifications or DP83840A data sheet
 * for details.
 */

/*
 * The maximum data clock rate is 2.5 Mhz.  The minimum timing is usually
 * met by back-to-back PCI I/O cycles, but we insert a delay to avoid
 * "overclocking" issues.
 */
#define mdio_delay() inl(mdio_addr)

#define MDIO_SHIFT_CLK    0x01
#define MDIO_DIR_WRITE    0x04
#define MDIO_DATA_WRITE0 (0x00 | MDIO_DIR_WRITE)
#define MDIO_DATA_WRITE1 (0x02 | MDIO_DIR_WRITE)
#define MDIO_DATA_READ    0x02
#define MDIO_ENB_IN       0x00

/*
 * Generate the preamble required for initial synchronization and
 * a few older transceivers.
 */
static void mdio_sync (long ioaddr, int bits)
{
  long mdio_addr = ioaddr + Wn4_PhysicalMgmt;

  /* Establish sync by sending at least 32 logic ones.
   */
  while (--bits >= 0)
  {
    outw (MDIO_DATA_WRITE1, mdio_addr);
    mdio_delay();
    outw (MDIO_DATA_WRITE1 | MDIO_SHIFT_CLK, mdio_addr);
    mdio_delay();
  }
}

static int mdio_read (long ioaddr, int phy_id, int location)
{
  int      i, read_cmd = (0xf6 << 10) | (phy_id << 5) | location;
  unsigned retval = 0;
  long     mdio_addr = ioaddr + Wn4_PhysicalMgmt;

  if (mii_preamble_required)
     mdio_sync (ioaddr, 32);

  /* Shift the read command bits out. */
  for (i = 14; i >= 0; i--)
  {
    int dataval = (read_cmd & (1 << i)) ? MDIO_DATA_WRITE1 : MDIO_DATA_WRITE0;

    outw (dataval, mdio_addr);
    mdio_delay ();
    outw (dataval | MDIO_SHIFT_CLK, mdio_addr);
    mdio_delay ();
  }
  /* Read the two transition, 16 data, and wire-idle bits. */
  for (i = 19; i > 0; i--)
  {
    outw (MDIO_ENB_IN, mdio_addr);
    mdio_delay ();
    retval = (retval << 1) | ((inw (mdio_addr) & MDIO_DATA_READ) ? 1 : 0);
    outw (MDIO_ENB_IN | MDIO_SHIFT_CLK, mdio_addr);
    mdio_delay ();
  }
  return retval & 0x20000 ? 0xffff : retval >> 1 & 0xffff;
}

static void mdio_write (long ioaddr, int phy_id, int location, int value)
{
  int  i, write_cmd = 0x50020000 | (phy_id << 23) | (location << 18) | value;
  long mdio_addr    = ioaddr + Wn4_PhysicalMgmt;

  if (mii_preamble_required)
     mdio_sync (ioaddr, 32);

  /* Shift the command bits out.
   */
  for (i = 31; i >= 0; i--)
  {
    int dataval = (write_cmd & (1 << i)) ? MDIO_DATA_WRITE1 : MDIO_DATA_WRITE0;

    outw (dataval, mdio_addr);
    mdio_delay();
    outw (dataval | MDIO_SHIFT_CLK, mdio_addr);
    mdio_delay();
  }

  /* Leave the interface idle.
   */
  for (i = 1; i >= 0; i--)
  {
    outw (MDIO_ENB_IN, mdio_addr);
    mdio_delay();
    outw (MDIO_ENB_IN | MDIO_SHIFT_CLK, mdio_addr);
    mdio_delay();
  }
}

/*
 * ACPI: Advanced Configuration and Power Interface.
 * Set Wake-On-LAN mode and put the board into D3 (power-down) state.
 */
static void acpi_set_WOL (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private*) dev->priv;
  long   ioaddr             = dev->base_addr;

  /* Power up on: 1==Downloaded Filter, 2==Magic Packets, 4==Link Status.
   */
  EL3WINDOW (7);
  outw (2, ioaddr + 0x0c);

  /* The RxFilter must accept the WOL frames.
   */
  outw (SetRxFilter | RxStation | RxMulticast | RxBroadcast, ioaddr + EL3_CMD);
  outw (RxEnable, ioaddr + EL3_CMD);

  /* Change the power state to D3; RxEnable doesn't take effect.
   */
  pcibios_write_config_word (vp->pci_bus, vp->pci_devfn, 0xe0, 0x8103);
}

/*
 * Change from D3 (sleep) to D0 (active).
 * Problem: The Cyclone forgets all PCI config info during the transition!
 */
static void acpi_wake (int bus, int devfn)
{
  DWORD base0, base1, romaddr;
  WORD  pci_command, pwr_command;
  BYTE  pci_latency, pci_cacheline, irq;

  pcibios_read_config_word (bus, devfn, 0xe0, &pwr_command);
  if ((pwr_command & 3) == 0)
     return;

  pcibios_read_config_word (bus, devfn, PCI_COMMAND, &pci_command);
  pcibios_read_config_dword (bus, devfn, PCI_BASE_ADDRESS_0, &base0);
  pcibios_read_config_dword (bus, devfn, PCI_BASE_ADDRESS_1, &base1);
  pcibios_read_config_dword (bus, devfn, PCI_ROM_ADDRESS, &romaddr);
  pcibios_read_config_byte (bus, devfn, PCI_LATENCY_TIMER, &pci_latency);
  pcibios_read_config_byte (bus, devfn, PCI_CACHE_LINE_SIZE, &pci_cacheline);
  pcibios_read_config_byte (bus, devfn, PCI_INTERRUPT_LINE, &irq);

  pcibios_write_config_word (bus, devfn, 0xe0, 0x0000);
  pcibios_write_config_dword (bus, devfn, PCI_BASE_ADDRESS_0, base0);
  pcibios_write_config_dword (bus, devfn, PCI_BASE_ADDRESS_1, base1);
  pcibios_write_config_dword (bus, devfn, PCI_ROM_ADDRESS, romaddr);
  pcibios_write_config_byte (bus, devfn, PCI_INTERRUPT_LINE, irq);
  pcibios_write_config_byte (bus, devfn, PCI_LATENCY_TIMER, pci_latency);
  pcibios_write_config_byte (bus, devfn, PCI_CACHE_LINE_SIZE, pci_cacheline);
  pcibios_write_config_word (bus, devfn, PCI_COMMAND, pci_command | 5);
}

