/*
 * 3c515.c: A 3Com ISA EtherLink XL "Corkscrew" ethernet driver for linux.
 *
 * Written 1997-1998 by Donald Becker.
 * 
 * This software may be used and distributed according to the terms
 * of the GNU Public License, incorporated herein by reference.
 * 
 * This driver is for the 3Com ISA EtherLink XL "Corkscrew" 3c515 ethercard.
 * 
 * The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
 * Center of Excellence in Space Data and Information Sciences
 * Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
 */

static char *version = "3c515.c:v0.99 4/7/98 becker@cesdis.gsfc.nasa.gov\n";

#include "pmdrvr.h"

#undef  STATIC
#define STATIC                  /* for.map-file */

#define CORKSCREW 1

/* "Knobs" that adjust features and parameters.
 * Set the copy breakpoint for the copy-only-tiny-frames scheme.
 * Setting to > 1512 effectively disables this feature.
 */
static const int rx_copybreak = 200;

/* Allow setting MTU to a larger size, bypassing the normal ethernet setup.
 */
static const int mtu = 1500;

/* Maximum events (Rx packets, etc.) to handle at each interrupt.
 */
static int max_interrupt_work = 20;

/* Enable the automatic media selection code -- usually set.
 */
#define AUTOMEDIA 1

/* Allow the use of fragment bus master transfers instead of only
 * programmed-I/O for Vortex cards.  Full-bus-master transfers are always
 * enabled by default on Boomerang cards.  If VORTEX_BUS_MASTER is defined,
 * the feature may be turned on using 'options'.
 */
#define VORTEX_BUS_MASTER

/* A few values that may be tweaked.
 * Keep the ring sizes a power of two for efficiency.
 */
#define TX_RING_SIZE	16
#define RX_RING_SIZE	16
#define PKT_BUF_SZ      1536  /* Size of each temporary Rx buffer. */

/* "Knobs" for adjusting internal parameters.
 * Put out somewhat more debugging messages. (0 - no msg, 1 minimal msgs).
 */
#define DRIVER_DEBUG 1

/* Some values here only for performance evaluation and
 * path-coverage debugging.
 */
static int rx_nocopy = 0, rx_copy = 0, queued_packet = 0;

/* Number of times to check to see if the Tx FIFO has space, used
 * in some limited cases.
 */
#define WAIT_TX_AVAIL 200

/* Operational parameter that usually are not changed.
 */
#define TX_TIMEOUT  40  /* Time in jiffies before concluding Tx hung */

/* The size here is somewhat misleading: the Corkscrew also uses the
 * ISA aliased registers at <base>+0x400.
 */
#define CORKSCREW_TOTAL_SIZE 0x20

#ifdef DRIVER_DEBUG
int tc515_debug = DRIVER_DEBUG;
#else
int tc515_debug = 1;
#endif

#define CORKSCREW_ID 10

/*
 * Theory of Operation
 * 
 * I. Board Compatibility
 * 
 * This device driver is designed for the 3Com 3c515 ISA Fast EtherLink XL,
 * 3Com's ISA bus adapter for Fast Ethernet.  Due to the unique I/O port layout,
 * it's not practical to integrate this driver with the other EtherLink drivers.
 * 
 * II. Board-specific settings
 * 
 * The Corkscrew has an EEPROM for configuration, but no special settings are
 * needed for Linux.
 * 
 * III. Driver operation
 * 
 * The 3c515 series use an interface that's very similar to the 3c900 "Boomerang"
 * PCI cards, with the bus master interface extensively modified to work with
 * the ISA bus.
 * 
 * The card is capable of full-bus-master transfers with separate
 * lists of transmit and receive descriptors, similar to the AMD LANCE/PCnet,
 * DEC Tulip and Intel Speedo3.
 * 
 * This driver uses a "RX_COPYBREAK" scheme rather than a fixed intermediate
 * receive buffer.  This scheme allocates full-sized skbuffs as receive
 * buffers.  The value RX_COPYBREAK is used as the copying breakpoint: it is
 * chosen to trade-off the memory wasted by passing the full-sized skbuff to
 * the queue layer for all frames vs. the copying cost of copying a frame to a
 * correctly-sized skbuff.
 * 
 * 
 * IIIC. Synchronization
 * The driver runs as two independent, single-threaded flows of control.  One
 * is the send-packet routine, which enforces single-threaded use by the
 * dev->tbusy flag.  The other thread is the interrupt handler, which is single
 * threaded by the hardware and other software.
 * 
 * IV. Notes
 * 
 * Thanks to Terry Murphy of 3Com for providing documentation and a development
 * board.
 * 
 * The names "Vortex", "Boomerang" and "Corkscrew" are the internal 3Com
 * project names.  I use these names to eliminate confusion -- 3Com product
 * numbers and names are very similar and often confused.
 * 
 * The new chips support both ethernet (1.5K) and FDDI (4.5K) frame sizes!
 * This driver only supports ethernet frames because of the recent MTU limit
 * of 1.5K, but the changes to support 4.5K are minimal.
 */

/* Operational definitions.
 * These are not used by other compilation units and thus are not
 * exported in a ".h" file.
 * 
 * First the windows.  There are eight register windows, with the command
 * and status registers available in each.
 */
#define EL3WINDOW(win) outw(SelectWindow + (win), ioaddr + EL3_CMD)
#define EL3_CMD        0x0e
#define EL3_STATUS     0x0e

/* The top five bits written to EL3_CMD are a command, the lower
 * 11 bits are the parameter, if applicable.
 * Note that 11 parameters bits was fine for ethernet, but the new chips
 * can handle FDDI length frames (~4500 octets) and now parameters count
 * 32-bit 'Dwords' rather than octets.
 */
enum vortex_cmd {
     TotalReset     = (0 << 11),
     SelectWindow   = (1 << 11),
     StartCoax      = (2 << 11),
     RxDisable      = (3 << 11),
     RxEnable       = (4 << 11),
     RxReset        = (5 << 11),
     UpStall        = (6 << 11),
     UpUnstall      = (6 << 11) + 1,
     DownStall      = (6 << 11) + 2,
     DownUnstall    = (6 << 11) + 3,
     RxDiscard      = (8 << 11),
     TxEnable       = (9 << 11),
     TxDisable      = (10 << 11),
     TxReset        = (11 << 11),
     FakeIntr       = (12 << 11),
     AckIntr        = (13 << 11),
     SetIntrEnb     = (14 << 11),
     SetStatusEnb   = (15 << 11),
     SetRxFilter    = (16 << 11),
     SetRxThreshold = (17 << 11),
     SetTxThreshold = (18 << 11),
     SetTxStart     = (19 << 11),
     StartDMAUp     = (20 << 11),
     StartDMADown   = (20 << 11) + 1,
     StatsEnable    = (21 << 11),
     StatsDisable   = (22 << 11),
     StopCoax       = (23 << 11)
   };

/* The SetRxFilter command accepts the following classes:
 */
enum RxFilter {
     RxStation   = 1,
     RxMulticast = 2,
     RxBroadcast = 4,
     RxProm      = 8
   };

/* Bits in the general status register.
 */
enum vortex_status {
     IntLatch       = 0x0001,
     AdapterFailure = 0x0002,
     TxComplete     = 0x0004,
     TxAvailable    = 0x0008,
     RxComplete     = 0x0010,
     RxEarly        = 0x0020,
     IntReq         = 0x0040,
     StatsFull      = 0x0080,
     DMADone        = 1 << 8,
     DownComplete   = 1 << 9,
     UpComplete     = 1 << 10,
     DMAInProgress  = 1 << 11,   /* DMA controller is still busy. */
     CmdInProgress  = 1 << 12    /* EL3_CMD is still busy. */
   };

/* Register window 1 offsets, the window used in normal operation.
 * On the Corkscrew this window is always mapped at offsets 0x10-0x1f.
 */
enum Window1 {
     TX_FIFO  = 0x10,
     RX_FIFO  = 0x10,
     RxErrors = 0x14,
     RxStatus = 0x18,
     Timer    = 0x1A,
     TxStatus = 0x1B,
     TxFree   = 0x1C          /* Remaining free bytes in Tx buffer. */
   };

enum Window0 {
     Wn0IRQ = 0x08,
#if defined(CORKSCREW)
     Wn0EepromCmd  = 0x200A,  /* Corkscrew EEPROM command register. */
     Wn0EepromData = 0x200C   /* Corkscrew EEPROM results register. */
#else
     Wn0EepromCmd  = 10,      /* Window 0: EEPROM command register. */
     Wn0EepromData = 12       /* Window 0: EEPROM results register. */
#endif
   };

enum Win0_EEPROM_bits {
     EEPROM_Read  = 0x80,
     EEPROM_WRITE = 0x40,
     EEPROM_ERASE = 0xC0,
     EEPROM_EWENB = 0x30,     /* Enable erasing/writing for 10 msec. */
     EEPROM_EWDIS = 0x00      /* Disable EWENB before 10 msec timeout. */
   };

/* EEPROM locations.
 */
enum eeprom_offset {
     PhysAddr01   = 0,
     PhysAddr23   = 1,
     PhysAddr45   = 2,
     ModelID      = 3,
     EtherLink3ID = 7
   };

enum Window3 {                /* Window 3: MAC/config bits. */
     Wn3_Config   = 0,
     Wn3_MAC_Ctrl = 6,
     Wn3_Options  = 8
   };

union wn3_config {
      DWORD  i;
      struct w3_config_fields {
        unsigned int ram_size  : 3;
        unsigned int ram_width : 1;
        unsigned int ram_speed : 2;
        unsigned int rom_size  : 2;
        unsigned int pad8      : 8;
        unsigned int ram_split : 2;
        unsigned int pad18     : 2;
        unsigned int xcvr      : 3;
        unsigned int pad21     : 1;
        unsigned int autoselect: 1;
        unsigned int pad24     : 7;
      } u;
    };

enum Window4 {
     Wn4_NetDiag = 6,
     Wn4_Media   = 10          /* Window 4: Xcvr/media bits. */
   };

enum Win4_Media_bits {
     Media_SQE     = 0x0008,   /* Enable SQE error counting for AUI. */
     Media_10TP    = 0x00C0,   /* Enable link beat and jabber for 10baseT. */
     Media_Lnk     = 0x0080,   /* Enable just link beat for 100TX/100FX. */
     Media_LnkBeat = 0x0800
   };

enum Window7 {                 /* Window 7: Bus Master control. */
     Wn7_MasterAddr   = 0,
     Wn7_MasterLen    = 6,
     Wn7_MasterStatus = 12
   };

/* Boomerang-style bus master control registers.  Note ISA aliases!
 */
enum MasterCtrl {
     PktStatus       = 0x400,
     DownListPtr     = 0x404,
     FragAddr        = 0x408,
     FragLen         = 0x40c,
     TxFreeThreshold = 0x40f,
     UpPktStatus     = 0x410,
     UpListPtr       = 0x418
   };

/* The Rx and Tx descriptor lists.
 * Caution Alpha hackers: these types are 32 bits!  Note also the 8 byte
 * alignment contraint on tx_ring[] and rx_ring[].
 */
struct boom_rx_desc {
       DWORD next;
       long  status;
       DWORD addr;
       long  length;
     };

/* Values for the Rx status entry.
 */
enum rx_desc_status {
     RxDComplete = 0x00008000,
     RxDError    = 0x4000, /* See boomerang_rx() for actual error bits */
   };

struct boom_tx_desc {
       DWORD next;
       long  status;
       DWORD addr;
       long  length;
     };

struct vortex_private {
       char devname[8];            /* "ethN" string, also for kernel debug. */
       const char *product_name;
       struct device *next_module;

       /* The Rx and Tx rings are here to keep them quad-word-aligned.
        */
       struct boom_rx_desc rx_ring[RX_RING_SIZE];
       struct boom_tx_desc tx_ring[TX_RING_SIZE];

       /* The addresses of transmit- and receive-in-place buffers.
        */
       char  *rx_buf [RX_RING_SIZE];
       char  *tx_buf [TX_RING_SIZE];
       DWORD  cur_rx, cur_tx;     /* The next free ring entry */
       DWORD  dirty_rx, dirty_tx; /* The ring entries to be free()ed. */

       void *dma_buffer;    /* linear address of DMA buffer */
       WORD  dma_selector;  /* selector of DMA buffer */

       NET_STATS stats;
       struct sk_buff *tx_skb;        /* Packet being eaten by bus master ctrl. */
       struct timer_list timer;       /* Media selection timer. */

       int capabilities;              /* Adapter capabilities word. */
       int options;                   /* User-settable misc. driver options. */
       int last_rx_packets;           /* For media autoselection. */

       unsigned available_media:8;    /* From Wn3_Options */
       unsigned media_override:3;     /* Passed-in media type. */
       unsigned default_media:3;      /* Read from the EEPROM. */
       unsigned full_duplex:1;
       unsigned autoselect:1;
       unsigned bus_master:1;         /* Vortex can only do a fragment bus-m. */
       unsigned full_bus_master_tx:1;
       unsigned full_bus_master_rx:1; /* Boomerang  */
       unsigned tx_full:1;
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
     XCVR_Default = 8,
   };

static struct media_table {
       char    *name;
       unsigned media_bits:16; /* Bits to set in Wn4_Media register. */
       unsigned mask:8;        /* The transceiver-present bit in Wn3_Config. */
       unsigned next:8;        /* The media type to try next. */
       short    wait;          /* Time before we check media status. */
     } media_tbl[] = {
       { "10baseT",   Media_10TP, 0x08, XCVR_10base2,  (14 * HZ) / 10 },
       { "10Mbs AUI", Media_SQE,  0x20, XCVR_Default,  (1 * HZ) / 10  },
       { "undefined", 0,          0x80, XCVR_10baseT,  10000          },
       { "10base2",   0,          0x10, XCVR_AUI,      (1 * HZ) / 10  },
       { "100baseTX", Media_Lnk,  0x02, XCVR_100baseFx,(14 * HZ) / 10 },
       { "100baseFX", Media_Lnk,  0x04, XCVR_MII,      (14 * HZ) / 10 },
       { "MII",       0,          0x40, XCVR_10baseT,  3 * HZ         },
       { "undefined", 0,          0x01, XCVR_10baseT,  10000          },
       { "Default",   0,          0xFF, XCVR_10baseT,  10000          }
     };

static int   vortex_scan       (struct device *dev);
static int   vortex_open       (struct device *dev);
static void  vortex_probe1     (struct device *dev);
static int   vortex_start_xmit (struct device *dev, const void *buf, int len);
static void  vortex_rx         (struct device *dev);
static void  vortex_close      (struct device *dev);
static void *vortex_get_stats  (struct device *dev);
static void  vortex_interrupt  (int irq);
static void  vortex_timer      (DWORD arg);

static void  update_stats (int addr, struct device *dev);
static void  boomerang_rx (struct device *dev);
static void  set_rx_mode  (struct device *dev);

static struct device *vortex_found_device (struct device *dev, int ioaddr,
                                           int irq, int product_index,
                                           int options);


/*
 * Unlike the other PCI cards the 59x cards don't need a large contiguous
 * memory region, so making the driver a loadable module is feasible.
 *
 * Unfortunately maximizing the shared code between the integrated and
 * module version of the driver results in a complicated set of initialization
 * procedures.
 * init_module() -- modules /  tc59x_init()  -- built-in
 *                  The wrappers for vortex_scan()
 * vortex_scan()    The common routine that scans for PCI and EISA cards
 * vortex_found_device() Allocate a device structure when we find a card.
 *                       Different versions exist for modules and built-in.
 * vortex_probe1()       Fill in the device structure -- this is separated
 *                       so that the modules code can put it in dev->init.
 */

/* This driver uses 'options' to pass the media type, full-duplex flag, etc.
 *
 * Note: this is the only limit on the number of cards supported!!
 */
static int options[8] = { -1, -1, -1, -1, -1, -1, -1, -1, };

int tc515_probe (struct device *dev)
{
  int cards_found = vortex_scan (dev);

  if (tc515_debug > 0 && cards_found)
     printk (version);

  return (cards_found ? 1 : 0);
}

static int vortex_scan (struct device *dev)
{
  static int ioaddr = 0x100;
  int    cards_found = 0;

  /* Check all locations on the ISA bus -- evil!
   */
  for (; ioaddr < 0x400; ioaddr += 0x20)
  {
    int irq;

    /* Check the resource configuration for a matching ioaddr.
     */
    if ((inw(ioaddr + 0x2002) & 0x1F0) != (ioaddr & 0x1F0))
       continue;

    /* Verify by reading the device ID from the EEPROM. */
    {
      int timer;

      outw (EEPROM_Read + 7, ioaddr + Wn0EepromCmd);

      /* Pause for at least 162 us. for the read to take place.
       */
      for (timer = 4; timer >= 0; timer--)
      {
        delay (1);
        if ((inw(ioaddr + Wn0EepromCmd) & 0x0200) == 0)
           break;
      }
      if (inw(ioaddr + Wn0EepromData) != 0x6D50)
         continue;
    }
    printk ("3c515 Resource configuration register %04x, DCR %04x.\n",
            (unsigned)inl(ioaddr + 0x2002), inw(ioaddr + 0x2000));
    irq = inw (ioaddr + 0x2002) & 15;
    vortex_found_device (dev, ioaddr, irq, CORKSCREW_ID,
                         dev && dev->mem_start ? dev->mem_start :
                                                 options[cards_found]);
    dev = 0;
    cards_found++;
  }
  return (cards_found);
}

static struct device *vortex_found_device (struct device *dev, int ioaddr,
                                           int irq, int product_index,
                                           int options)
{
  struct vortex_private *vp;

  if (dev)
     dev->priv = k_calloc (sizeof(struct vortex_private), 1);

  dev = init_etherdev (dev, sizeof (struct vortex_private));

  dev->base_addr = ioaddr;
  dev->irq = irq;
  dev->dma = (product_index == CORKSCREW_ID) ? inw(ioaddr+0x2000) & 7 : 0;

  vp = (struct vortex_private*) dev->priv;
  vp->product_name = "3c515";
  vp->options = options;
  if (options >= 0)
  {
    vp->media_override = ((options & 7) == 2) ? 0 : options & 7;
    vp->full_duplex    = (options & 8) ? 1 : 0;
    vp->bus_master     = (options & 16) ? 1 : 0;
  }
  else
  {
    vp->media_override = 7;
    vp->full_duplex    = 0;
    vp->bus_master     = 0;
  }
  vortex_probe1 (dev);
  return (dev);
}

static void vortex_probe1 (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private*)dev->priv;
  DWORD  eeprom[0x40], checksum = 0; /* EEPROM contents */
  int    i, ioaddr = dev->base_addr;

  printk ("%s: 3Com %s at %#3x,", dev->name, vp->product_name, ioaddr);

  /* Read the station address from the EEPROM.
   */
  EL3WINDOW (0);
  for (i = 0; i < 0x18; i++)
  {
    short *phys_addr = (short*)dev->dev_addr;
    int    timer;

    outw (EEPROM_Read + i, ioaddr + Wn0EepromCmd);

    /* Pause for at least 162 us. for the read to take place.
     */
    for (timer = 4; timer >= 0; timer--)
    {
      delay (1);
      if ((inw(ioaddr + Wn0EepromCmd) & 0x0200) == 0)
         break;
    }
    eeprom[i] = inw (ioaddr + Wn0EepromData);
    checksum ^= eeprom[i];
    if (i < 3)
       phys_addr[i] = htons (eeprom[i]);
  }
  checksum = (checksum ^ (checksum >> 8)) & 0xff;
  if (checksum != 0x00)
     printk (" ***INVALID CHECKSUM %04x*** ", (WORD)checksum);

  for (i = 0; i < 6; i++)
     printk ("%c%2.2x", i ? ':' : ' ', dev->dev_addr[i]);

  if (eeprom[16] == 0x11c7)
  {                             /* Corkscrew */
    if (_dma_request (dev->dma, "3c515"))
    {
      printk (", DMA %d allocation failed", dev->dma);
      dev->dma = 0;
    }
    else
      printk (", DMA %d", dev->dma);
  }
  if (tc515_debug >= 1)
     printk (", IRQ %d\n", dev->irq);

  /* Tell them about an invalid IRQ.
   */
  if (tc515_debug && (dev->irq <= 0 || dev->irq > 15))
     printk (" *** Warning: this IRQ is unlikely to work! ***\n");

  {
    char *ram_split[] = { "5:3", "3:1", "1:1", "3:5" };
    union wn3_config config;

    EL3WINDOW (3);
    vp->available_media = inw (ioaddr + Wn3_Options);
    config.i = inl (ioaddr + Wn3_Config);
    if (tc515_debug > 1)
       printk ("  Internal config register is %04x, transceivers %04x.\n",
               (int)config.i, inw(ioaddr + Wn3_Options));

    printk ("  %dK %s-wide RAM %s Rx:Tx split, %s%s interface.\n",
            8 << config.u.ram_size,
            config.u.ram_width ? "word" : "byte",
            ram_split[config.u.ram_split],
            config.u.autoselect ? "autoselect/" : "",
            media_tbl[config.u.xcvr].name);
    dev->if_port = config.u.xcvr;
    vp->default_media = config.u.xcvr;
    vp->autoselect = config.u.autoselect;
  }
  if (vp->media_override != 7)
  {
    printk ("  Media override to transceiver type %d (%s).\n",
            vp->media_override, media_tbl[vp->media_override].name);
    dev->if_port = vp->media_override;
  }

  vp->capabilities = eeprom[16];
  vp->full_bus_master_tx = (vp->capabilities & 0x20) ? 1 : 0;

  /* Rx is broken at 10mbps, so we always disable it.
   * vp->full_bus_master_rx = 0;
   */
  vp->full_bus_master_rx = (vp->capabilities & 0x20) ? 1 : 0;

  /* The 3c59x-specific entries in the device structure.
   */
  dev->open      = vortex_open;
  dev->xmit      = vortex_start_xmit;
  dev->close     = vortex_close;
  dev->get_stats = vortex_get_stats;
  dev->set_multicast_list = &set_rx_mode;
}


static int vortex_open (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private*)dev->priv;
  union  wn3_config config;
  int    i, ioaddr = dev->base_addr;

  /* Before initializing select the active media port.
   */
  EL3WINDOW (3);
  if (vp->full_duplex)
     outb (0x20, ioaddr + Wn3_MAC_Ctrl); /* Set the full-duplex bit. */

  config.i = inl (ioaddr + Wn3_Config);

  if (vp->media_override != 7)
  {
    if (tc515_debug > 1)
       printk ("%s: Media override to transceiver %d (%s).\n",
               dev->name, vp->media_override,
               media_tbl[vp->media_override].name);
    dev->if_port = vp->media_override;
  }
  else if (vp->autoselect)
  {
    /* Find first available media type, starting with 100baseTx.
     */
    dev->if_port = 4;
    while (!(vp->available_media & media_tbl[dev->if_port].mask))
          dev->if_port = media_tbl[dev->if_port].next;

    if (tc515_debug > 1)
       printk ("%s: Initial media type %s.\n",
               dev->name, media_tbl[dev->if_port].name);

    init_timer (&vp->timer);
    vp->timer.expires  = RUN_AT (media_tbl[dev->if_port].wait);
    vp->timer.data     = (unsigned long) dev;
    vp->timer.function = vortex_timer; /* timer handler */
    add_timer (&vp->timer);
  }
  else
    dev->if_port = vp->default_media;

  config.u.xcvr = dev->if_port;
  outl (config.i, ioaddr + Wn3_Config);

  if (tc515_debug > 1)
     printk ("%s: vortex_open() InternalConfig %08x.\n",
             dev->name, (unsigned)config.i);

  outw (TxReset, ioaddr + EL3_CMD);
  for (i = 20; i >= 0; i--)
      if (!(inw(ioaddr + EL3_STATUS) & CmdInProgress))
         break;

  outw (RxReset, ioaddr + EL3_CMD);

  /* Wait a few ticks for the RxReset command to complete.
   */
  for (i = 20; i >= 0; i--)
      if (!(inw(ioaddr + EL3_STATUS) & CmdInProgress))
         break;

  outw (SetStatusEnb | 0x00, ioaddr + EL3_CMD);

  /* Use the now-standard shared IRQ implementation.
   */
  if (vp->capabilities == 0x11c7)
  {
    /* Corkscrew: Cannot share ISA resources. */
    if (dev->irq == 0 || dev->dma == 0 ||
        !request_irq (dev->irq, &vortex_interrupt))
       return (0);

    enable_dma (dev->dma);
    set_dma_mode (dev->dma, DMA_MODE_CASCADE);
  }
  else if (!request_irq (dev->irq, &vortex_interrupt))
    return (0);

  if (tc515_debug > 1)
  {
    EL3WINDOW (4);
    printk ("%s: vortex_open() irq %d media status %4.4x.\n",
            dev->name, dev->irq, inw(ioaddr+Wn4_Media));
  }

  /* Set the station address and mask in window 2 each time opened.
   */
  EL3WINDOW (2);
  for (i = 0; i < 6; i++)
     outb (dev->dev_addr[i], ioaddr + i);
  for (; i < 12; i += 2)
     outw (0, ioaddr + i);

  if (dev->if_port == 3)
  {
    /* Start the thinnet transceiver. We should really wait 50ms... */
    outw (StartCoax, ioaddr + EL3_CMD);
  }
  EL3WINDOW (4);
  outw ((inw(ioaddr + Wn4_Media) & ~(Media_10TP | Media_SQE)) |
        media_tbl[dev->if_port].media_bits, ioaddr + Wn4_Media);

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

  if (vp->full_bus_master_rx)  /* Boomerang bus master Rx. */
  {
    DWORD addr;

    vp->cur_rx = vp->dirty_rx = 0;
    if (tc515_debug > 2)
       printk ("%s:  Filling in the Rx ring.\n", dev->name);

    for (i = 0; i < RX_RING_SIZE; i++)
    {
      if (i < (RX_RING_SIZE - 1))
           vp->rx_ring[i].next = VIRT_TO_BUS (&vp->rx_ring[i+1]);
      else vp->rx_ring[i].next = NULL;

      vp->rx_ring[i].status = 0;  /* Clear complete bit. */
      vp->rx_ring[i].length = PKT_BUF_SZ | 0x80000000;
      vp->rx_ring[i].addr   = 0;  // !!
    }
    addr = VIRT_TO_BUS (&vp->rx_ring[0]);
    vp->rx_ring[i-1].next = addr;    /* Wrap the ring. */
    outl (addr, ioaddr + UpListPtr);
  }

  if (vp->full_bus_master_tx)   /* Boomerang bus master Tx. */
  {                             
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

  dev->tx_busy = 0;
  dev->reentry = 0;
  dev->start   = 1;

  outw (RxEnable, ioaddr + EL3_CMD); /* Enable the receiver. */
  outw (TxEnable, ioaddr + EL3_CMD); /* Enable transmitter. */

  /* Allow status bits to be seen.
   */
  outw (SetStatusEnb | AdapterFailure | IntReq | StatsFull |
        (vp->full_bus_master_tx ? DownComplete : TxAvailable) |
        (vp->full_bus_master_rx ? UpComplete : RxComplete) |
        (vp->bus_master ? DMADone : 0), ioaddr + EL3_CMD);

  /* Ack all pending events, and set active indicator mask.
   */
  outw (AckIntr | IntLatch | TxAvailable | RxEarly | IntReq, ioaddr + EL3_CMD);
  outw (SetIntrEnb | IntLatch | TxAvailable | RxComplete | StatsFull
        | (vp->bus_master ? DMADone : 0) | UpComplete | DownComplete,
        ioaddr + EL3_CMD);

  return (1);
}

static void vortex_timer (unsigned long data)
{
#ifdef AUTOMEDIA
  struct device         *dev = (struct device*)data;
  struct vortex_private *vp  = (struct vortex_private*)dev->priv;
  int    ok     = 0;
  int    ioaddr = dev->base_addr;

  if (tc515_debug > 1)
     printk ("%s: Media selection timer tick happened, %s.\n",
             dev->name, media_tbl[dev->if_port].name);

  DISABLE();
  {
    int old_window = inw (ioaddr + EL3_CMD) >> 13;
    int media_status;

    EL3WINDOW (4);
    media_status = inw (ioaddr + Wn4_Media);

    switch (dev->if_port)
    {
      case 0:
      case 4:
      case 5:               /* 10baseT, 100baseTX, 100baseFX  */
           if (media_status & Media_LnkBeat)
           {
             ok = 1;
             if (tc515_debug > 1)
                printk ("%s: Media %s has link beat, %x.\n",
                        dev->name, media_tbl[dev->if_port].name, media_status);
           }
           else if (tc515_debug > 1)
             printk ("%s: Media %s is has no link beat, %x.\n",
                     dev->name, media_tbl[dev->if_port].name, media_status);
           break;
      default:              /* Other media types handled by Tx timeouts. */
           if (tc515_debug > 1)
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

      if (dev->if_port == 8)
      {                         /* Go back to default. */
        dev->if_port = vp->default_media;
        if (tc515_debug > 1)
           printk ("%s: Media selection failing, using default %s port.\n",
                   dev->name, media_tbl[dev->if_port].name);
      }
      else
      {
        if (tc515_debug > 1)
           printk ("%s: Media selection failed, now trying %s port.\n",
                   dev->name, media_tbl[dev->if_port].name);
        vp->timer.expires = RUN_AT (media_tbl[dev->if_port].wait);
        add_timer (&vp->timer);
      }
      outw ((media_status & ~(Media_10TP | Media_SQE)) |
            media_tbl[dev->if_port].media_bits, ioaddr + Wn4_Media);

      EL3WINDOW (3);
      config.i = inl (ioaddr + Wn3_Config);
      config.u.xcvr = dev->if_port;
      outl (config.i, ioaddr + Wn3_Config);

      outw (dev->if_port == 3 ? StartCoax : StopCoax, ioaddr + EL3_CMD);
    }
    EL3WINDOW (old_window);
  }
  ENABLE();
  if (tc515_debug > 1)
     printk ("%s: Media selection timer finished, %s.\n",
             dev->name, media_tbl[dev->if_port].name);

#endif     /* AUTOMEDIA */
}

static int vortex_start_xmit (struct device *dev, const void *buf, int len)
{
  struct vortex_private *vp = (struct vortex_private*)dev->priv;
  int    ioaddr = dev->base_addr;

  if (dev->tx_busy)
  {
    int i, tickssofar = jiffies - dev->tx_start;

    /* Min. wait before assuming a Tx failed == 400ms.
     */
    if (tickssofar < 400 * HZ / 1000) /* We probably aren't empty. */
       return (1);

    printk ("%s: transmit timed out, tx_status %2.2x status %4.4x.\n",
            dev->name, inb(ioaddr+TxStatus), inw(ioaddr+EL3_STATUS));

    /* Slight code bloat to be user friendly.
     */
    if ((inb(ioaddr+TxStatus) & 0x88) == 0x88)
       printk ("%s: Transmitter encountered 16 collisions -- network"
               " network cable problem?\n", dev->name);

#ifndef final_version
    printk ("  Flags; bus-master %d, full %d; dirty %lu current %lu.\n",
            vp->full_bus_master_tx, vp->tx_full, vp->dirty_tx, vp->cur_tx);
    printk ("  Down list %08lx vs. %p.\n",
            inl(ioaddr + DownListPtr), &vp->tx_ring[0]);

    for (i = 0; i < TX_RING_SIZE; i++)
        printk ("  %d: %p  length %08lx status %08lx\n",
                i, &vp->tx_ring[i], vp->tx_ring[i].length,
                vp->tx_ring[i].status);
#endif

    /* Issue TX_RESET and TX_START commands.
     */
    outw (TxReset, ioaddr + EL3_CMD);
    for (i = 20; i >= 0; i--)
        if (!(inw(ioaddr + EL3_STATUS) & CmdInProgress))
           break;

    outw (TxEnable, ioaddr + EL3_CMD);
    dev->tx_start = jiffies;
 /* dev->tx_busy = 0; */
    vp->stats.tx_errors++;
    vp->stats.tx_dropped++;
    return (0);                  /* Yes, silently *drop* the packet! */
  }

  /* Block a timer-based transmit from overlapping.  This could better be
   * done with atomic_swap(1, dev->tx_busy), but set_bit() works as well.
   * If this ever occurs the queue layer is doing something evil!
   */
  if (set_bit (0, (void*)&dev->tx_busy) != 0)
  {
    printk ("%s: Transmitter access conflict.\n", dev->name);
    return (1);
  }

  if (vp->full_bus_master_tx)   /* BOOMERANG bus-master */
  {
    /* Calculate the next Tx descriptor entry.
     */
    struct boom_tx_desc *prev_entry;
    int    i, entry = vp->cur_tx % TX_RING_SIZE;

    if (vp->tx_full)            /* No room to transmit with */
       return (1);

    if (vp->cur_tx != 0)
         prev_entry = &vp->tx_ring[(vp->cur_tx-1) % TX_RING_SIZE];
    else prev_entry = NULL;

    if (tc515_debug > 3)
       printk ("%s: Trying to send a packet, Tx index %lu.\n",
               dev->name, vp->cur_tx);

 /* vp->tx_full = 1; */
    vp->tx_buf[entry]         = (char*)buf;
    vp->tx_ring[entry].next   = 0;
    vp->tx_ring[entry].addr   = VIRT_TO_BUS (buf);
    vp->tx_ring[entry].length = len | 0x80000000;
    vp->tx_ring[entry].status = len | 0x80000000;

    DISABLE();
    outw (DownStall, ioaddr + EL3_CMD);

    /* Wait for the stall to complete.
     */
    for (i = 20; i >= 0; i--)
        if ((inw(ioaddr + EL3_STATUS) & CmdInProgress) == 0)
           break;

    if (prev_entry)
        prev_entry->next = VIRT_TO_BUS (&vp->tx_ring[entry]);

    if (inl (ioaddr + DownListPtr) == 0)
    {
      outl (VIRT_TO_BUS(&vp->tx_ring[entry]), ioaddr + DownListPtr);
      queued_packet++;
    }
    outw (DownUnstall, ioaddr + EL3_CMD);

    vp->cur_tx++;
    if (vp->cur_tx - vp->dirty_tx > TX_RING_SIZE - 1)
       vp->tx_full = 1;
    else
    {                           /* Clear previous interrupt enable. */
      if (prev_entry)
        prev_entry->status &= ~0x80000000;
      dev->tx_busy = 0;
    }
    dev->tx_start = jiffies;
    return (0);
  }

  /* Put out the doubleword header...
   */
  outl ((DWORD)len, ioaddr + TX_FIFO);

#ifdef VORTEX_BUS_MASTER
  if (vp->bus_master)
  {
    /* Set the bus-master controller to transfer the packet. */
    outl ((DWORD)buf, ioaddr + Wn7_MasterAddr);
    outw ((len + 3) & ~3, ioaddr + Wn7_MasterLen);
    vp->tx_skb = (struct sk_buff*)buf;
    outw (StartDMADown, ioaddr + EL3_CMD);
    /* dev->tx_busy will be cleared at the DMADone interrupt. */
  }
  else
  {
    /* ... and the packet rounded to a doubleword. */
    outsl (ioaddr + TX_FIFO, buf, (len + 3) >> 2);
    if (inw(ioaddr + TxFree) > 1536)
       dev->tx_busy = 0;
    else
    {
      /* Interrupt us when the FIFO has room for max-sized packet. */
      outw (SetTxThreshold + (1536 >> 2), ioaddr + EL3_CMD);
    }
  }
#else
  /* ... and the packet rounded to a doubleword. */
  outsl (ioaddr + TX_FIFO, buf, (len + 3) >> 2);
  if (inw(ioaddr + TxFree) > 1536)
     dev->tx_busy = 0;
  else
  {
    /* Interrupt us when the FIFO has room for max-sized packet. */
    outw (SetTxThreshold + (1536 >> 2), ioaddr + EL3_CMD);
  }
#endif  /* VORTEX_BUS_MASTER */

  dev->tx_start = jiffies;

  /* Clear the Tx status stack.
   */
  {
    short tx_status;
    int   i = 4;

    while (--i > 0 && (tx_status = inb(ioaddr + TxStatus)) > 0)
    {
      if (tx_status & 0x3C)
      {                         /* A Tx-disabling error occurred.  */
        if (tc515_debug > 2)
           printk ("%s: Tx error, status %2.2x.\n", dev->name, tx_status);
        if (tx_status & 0x04)
           vp->stats.tx_fifo_errors++;
        if (tx_status & 0x38)
           vp->stats.tx_aborted_errors++;
        if (tx_status & 0x30)
        {
          int j;

          outw (TxReset, ioaddr + EL3_CMD);
          for (j = 20; j >= 0; j--)
              if (!(inw (ioaddr + EL3_STATUS) & CmdInProgress))
                 break;
        }
        outw (TxEnable, ioaddr + EL3_CMD);
      }
      outb (0, ioaddr + TxStatus); /* Pop the status stack. */
    }
  }
  vp->stats.tx_bytes += len;
  return (1);
}

/*
 * The interrupt handler does all of the Rx thread work and cleans up
 * after the Tx thread.
 */
static void vortex_interrupt (int irq)
{
  struct device *dev = irq2dev_map[irq];
  struct vortex_private *lp;
  int    ioaddr, status;
  int    latency, i = max_interrupt_work;

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
  lp      = (struct vortex_private*)dev->priv;
  status  = inw (ioaddr + EL3_STATUS);

  if (tc515_debug > 4)
     printk ("%s: interrupt, status %4.4x, timer %d.\n",
             dev->name, status, latency);

  if ((status & 0xE000) != 0xE000)
  {
    static int donedidthis = 0;

    /* Some interrupt controllers store a bogus interrupt from boot-time.
     * Ignore a single early interrupt, but don't hang the machine for
     * other interrupt problems.
     */
    if (donedidthis++ > 100)
    {
      printk ("%s: Bogus interrupt, bailing. Status %4.4x, start=%d.\n",
              dev->name, status, dev->start);
      free_irq (dev->irq);
      irq2dev_map[irq] = NULL;
      dev->reentry = 0;
    }
  }

  do
  {
    if (tc515_debug > 5)
       printk ("%s: In interrupt loop, status %04x.\n", dev->name, status);

    if (status & RxComplete)
       vortex_rx (dev);

    if (status & TxAvailable)
    {
      if (tc515_debug > 5)
         printk ("  Tx room bit was handled.\n");

      /* There's room in the FIFO for a full-sized packet.
       */
      outw (AckIntr | TxAvailable, ioaddr + EL3_CMD);
      dev->tx_busy = 0;
    }
    if (status & DownComplete)
    {
      DWORD dirty_tx = lp->dirty_tx;

      while (lp->cur_tx - dirty_tx > 0)
      {
        int entry = dirty_tx % TX_RING_SIZE;

        if (inl (ioaddr + DownListPtr) == VIRT_TO_BUS (&lp->tx_ring[entry]))
           break;               /* It still hasn't been processed. */

        if (lp->tx_buf[entry])
        {
          k_free (lp->tx_buf[entry]);
          lp->tx_buf[entry] = NULL;
        }
        dirty_tx++;
      }
      lp->dirty_tx = dirty_tx;
      outw (AckIntr | DownComplete, ioaddr + EL3_CMD);
      if (lp->tx_full && (lp->cur_tx - dirty_tx <= TX_RING_SIZE - 1))
      {
        lp->tx_full  = 0;
        dev->tx_busy = 0;
      }
    }

#ifdef VORTEX_BUS_MASTER
    if (status & DMADone)
    {
      outw (0x1000, ioaddr + Wn7_MasterStatus); /* Ack the event. */
      dev->tx_busy = 0;
      k_free (lp->tx_skb); /* Release the transfered buffer */
    }
#endif
    if (status & UpComplete)
    {
      boomerang_rx (dev);
      outw (AckIntr | UpComplete, ioaddr + EL3_CMD);
    }

    if (status & (AdapterFailure | RxEarly | StatsFull))
    {
      /* Handle all uncommon interrupts at once. */
      if (status & RxEarly)
      {                         /* Rx early is unused. */
        vortex_rx (dev);
        outw (AckIntr | RxEarly, ioaddr + EL3_CMD);
      }
      if (status & StatsFull)
      {                         /* Empty statistics. */
        static int DoneDidThat = 0;

        if (tc515_debug > 4)
           printk ("%s: Updating stats.\n", dev->name);
        update_stats (ioaddr, dev);

        /* DEBUG HACK: Disable statistics as an interrupt source.
         * This occurs when we have the wrong media type!
         */
        if (DoneDidThat == 0 && inw (ioaddr + EL3_STATUS) & StatsFull)
        {
          int win, reg;

          printk ("%s: Updating stats failed, disabling stats as an"
                  " interrupt source.\n", dev->name);
          for (win = 0; win < 8; win++)
          {
            EL3WINDOW (win);
            printk ("\n Vortex window %d:", win);
            for (reg = 0; reg < 16; reg++)
                printk (" %2.2x", inb(ioaddr+reg));
          }
          EL3WINDOW (7);
          outw (SetIntrEnb | TxAvailable | RxComplete | AdapterFailure |
                UpComplete | DownComplete | TxComplete, ioaddr + EL3_CMD);
          DoneDidThat++;
        }
      }
      if (status & AdapterFailure)
      {
        /* Adapter failure requires Rx reset and reinit.
         */
        outw (RxReset, ioaddr + EL3_CMD);

        /* Set the Rx filter to the current state.
         */
        set_rx_mode (dev);
        outw (RxEnable, ioaddr + EL3_CMD); /* Re-enable the receiver. */
        outw (AckIntr | AdapterFailure, ioaddr + EL3_CMD);
      }
    }

    if (--i < 0)
    {
      printk ("%s: Too much work in interrupt, status %4.4x.  "
              "Disabling functions (%4.4x).\n",
              dev->name, status, SetStatusEnb | ((~status) & 0x7FE));

      /* Disable all pending interrupts.
       */
      outw (SetStatusEnb | ((~status) & 0x7FE), ioaddr + EL3_CMD);
      outw (AckIntr | 0x7FF, ioaddr + EL3_CMD);
      break;
    }
    /* Acknowledge the IRQ.
     */
    outw (AckIntr | IntReq | IntLatch, ioaddr + EL3_CMD);
  }
  while ((status = inw (ioaddr + EL3_STATUS)) & (IntLatch | RxComplete));

  if (tc515_debug > 4)
     printk ("%s: exiting interrupt, status %4.4x.\n", dev->name, status);

  dev->reentry = 0;
}

static void vortex_rx (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private*)dev->priv;
  int    i, ioaddr = dev->base_addr;
  short  rx_status;

  if (tc515_debug > 5)
     printk ("   In rx_packet(), status %4.4x, rx_status %4.4x.\n",
             inw (ioaddr + EL3_STATUS), inw (ioaddr + RxStatus));

  while ((rx_status = inw (ioaddr + RxStatus)) > 0)
  {
    if (rx_status & 0x4000)
    {                           /* Error, update stats. */
      BYTE rx_error = inb (ioaddr + RxErrors);

      if (tc515_debug > 2)
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
      /* The packet length: up to 4.5K!.
       */
      short len = rx_status & 0x1fff;
      char *buf;

      if (tc515_debug > 4)
         printk ("Receiving packet size %d status %04x.\n",
                 len, rx_status);

      if (dev->get_rx_buf && (buf = (*dev->get_rx_buf) (len)) != NULL)
      {
        rep_insl (ioaddr + RX_FIFO, (DWORD*)buf, (len + 3) >> 2);
        outw (RxDiscard, ioaddr + EL3_CMD); /* Pop top Rx packet. */

        dev->last_rx = jiffies;
        vp->stats.rx_packets++;
        vp->stats.rx_bytes += len;

        /* Wait a limited time to go to next packet.
         */
        for (i = 200; i >= 0; i--)
            if (!(inw (ioaddr + EL3_STATUS) & CmdInProgress))
               break;
        continue;
      }
      else
      {
        vp->stats.rx_dropped++;
        if (tc515_debug)
           printk ("%s: Couldn't allocate a buf of size %d.\n",
                   dev->name, len);
      }
    }

    outw (RxDiscard, ioaddr + EL3_CMD);
    vp->stats.rx_dropped++;

    /* Wait a limited time to skip this packet.
     */
    for (i = 200; i >= 0; i--)
        if (!(inw (ioaddr + EL3_STATUS) & CmdInProgress))
           break;
  }
}

static void boomerang_rx (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private*)dev->priv;
  int    entry  = vp->cur_rx % RX_RING_SIZE;
  int    ioaddr = dev->base_addr;
  int    rx_status;

  if (tc515_debug > 5)
     printk ("   In boomerang_rx(), status %4.4x, rx_status %4.4x.\n",
             inw (ioaddr + EL3_STATUS), inw (ioaddr + RxStatus));

  while ((rx_status = vp->rx_ring[entry].status) & RxDComplete)
  {
    if (rx_status & RxDError)
    {                           /* Error, update stats. */
      BYTE rx_error = rx_status >> 16;

      if (tc515_debug > 2)
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
      short pkt_len = rx_status & 0x1fff;
      char *buf;

      vp->stats.rx_bytes += pkt_len;
      if (tc515_debug > 4)
         printk ("Receiving packet size %d status %04x.\n",
                 pkt_len, rx_status);

      /* Check if the packet is long enough to just accept without
       * copying to a properly sized skbuff.
       */
      if (pkt_len < rx_copybreak && dev->get_rx_buf &&
          (buf = (*dev->get_rx_buf) (pkt_len)) != NULL)
      {
        memcpy (buf, BUS_TO_VIRT (vp->rx_ring[entry].addr), pkt_len);
        rx_copy++;
      }
      else
      {
        /* Pass up the skbuff already on the Rx ring.
         */
#if 0 // fix me!!
        memcpy (vp->rx_buf[entry], /* ?? */, PKT_BUF_SZ);
#endif
        vp->rx_buf[entry] = NULL;

        /* Remove this checking code for final release.
         */
        if (BUS_TO_VIRT(vp->rx_ring[entry].addr) != vp->rx_buf[entry])
           printk ("%s: Warning -- the skbuff addresses do not match"
                   " in boomerang_rx: %pu vs. %p.\n",
                   dev->name, BUS_TO_VIRT(vp->rx_ring[entry].addr),
                   vp->rx_buf[entry]);
        rx_nocopy++;
      }
      dev->last_rx = jiffies;
      vp->stats.rx_packets++;
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
      char *buf;

      if (!dev->get_rx_buf ||
          (buf = (*dev->get_rx_buf)(PKT_BUF_SZ)) == NULL)
         break;

      vp->rx_ring[entry].addr = VIRT_TO_BUS (buf);
      vp->rx_buf[entry]       = buf;
    }
    vp->rx_ring[entry].status = 0;   /* Clear complete bit. */
  }
}

static void vortex_close (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private*)dev->priv;
  int    i, ioaddr = dev->base_addr;

  dev->start   = 0;
  dev->tx_busy = 1;

  if (tc515_debug > 1)
  {
    printk ("%s: vortex_close() status %4.4x, Tx status %2.2x.\n",
            dev->name, inw(ioaddr + EL3_STATUS), inb(ioaddr + TxStatus));
    printk ("%s: vortex close stats: rx_nocopy %d rx_copy %d"
            " tx_queued %d.\n", dev->name, rx_nocopy, rx_copy, queued_packet);
  }

  del_timer (&vp->timer);

  /* Turn off statistics ASAP.  We update lp->stats below.
   */
  outw (StatsDisable, ioaddr + EL3_CMD);

  /* Disable the receiver and transmitter.
   */
  outw (RxDisable, ioaddr + EL3_CMD);
  outw (TxDisable, ioaddr + EL3_CMD);

  if (dev->if_port == XCVR_10base2)
  {
    /* Turn off thinnet power.  Green! */
    outw (StopCoax, ioaddr + EL3_CMD);
  }

  free_irq (dev->irq);
  irq2dev_map[dev->irq] = NULL;

  outw (SetIntrEnb | 0x0000, ioaddr + EL3_CMD);

  update_stats (ioaddr, dev);
  if (vp->full_bus_master_rx)
  {                             /* Free Boomerang bus master Rx buffers. */
    outl (0, ioaddr + UpListPtr);
    for (i = 0; i < RX_RING_SIZE; i++)
      if (vp->rx_buf[i])
      {
        k_free (vp->rx_buf[i]);
        vp->rx_buf[i] = NULL;
      }
  }
  if (vp->full_bus_master_tx)
  {                             /* Free Boomerang bus master Tx buffers. */
    outl (0, ioaddr + DownListPtr);
    for (i = 0; i < TX_RING_SIZE; i++)
      if (vp->tx_buf[i])
      {
        k_free (vp->tx_buf[i]);
        vp->tx_buf[i] = NULL;
      }
  }
}

static void *vortex_get_stats (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private*)dev->priv;

  if (dev->start)
  {
    DISABLE();
    update_stats (dev->base_addr, dev);
    ENABLE();
  }
  return (void*)&vp->stats;
}

/*
 * Update statistics.
 *   Unlike with the EL3 we need not worry about interrupts changing
 *   the window setting from underneath us, but we must still guard
 *   against a race condition with a StatsUpdate interrupt updating the
 *   table.  This is done by checking that the ASM (!) code generated uses
 *   atomic updates with '+='.
 */
static void update_stats (int ioaddr, struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private*)dev->priv;

  /* Unlike the 3c5x9 we need not turn off stats updates while reading.
   * Switch to the stats window, and read everything.
   */
  EL3WINDOW (6);
  vp->stats.tx_carrier_errors   += inb (ioaddr + 0);
  vp->stats.tx_heartbeat_errors += inb (ioaddr + 1);

  /* Multiple collisions. */    inb (ioaddr + 2);
  vp->stats.tx_collisions    += inb (ioaddr + 3);
  vp->stats.tx_window_errors += inb (ioaddr + 4);
  vp->stats.rx_fifo_errors   += inb (ioaddr + 5);
  vp->stats.tx_packets       += inb (ioaddr + 6);
  vp->stats.tx_packets       += (inb (ioaddr + 9) & 0x30) << 4;
  /* Rx packets */              inb (ioaddr + 7);

  /* Must read to clear. Else status interrupt comes again */
  /* Tx deferrals */            inb (ioaddr + 8);

  /* Don't bother with register 9, an extension of registers 6&7.
   * If we do use the 6&7 values the atomic update assumption above
   * is invalid.
   */
  inw (ioaddr + 10);            /* Total Rx and Tx octets. */
  inw (ioaddr + 12);

  /* New: On the Vortex we must also clear the BadSSD counter.
   */
  EL3WINDOW (4);
  inb (ioaddr + 12);

  /* We change back to window 7 (not 1) with the Vortex.
   */
  EL3WINDOW (7);
  return;
}

/*
 * This new version of set_rx_mode() supports v1.4 kernels.
 * The Vortex chip has no documented multicast filter, so the only
 * multicast setting is to receive all multicast frames.  At least
 * the chip has a very clean way to set the mode, unlike many others.
 */
static void set_rx_mode (struct device *dev)
{
  int ioaddr = dev->base_addr;
  short new_mode;

  if (dev->flags & IFF_PROMISC)
  {
    if (tc515_debug > 3)
       printk ("%s: Setting promiscuous mode.\n", dev->name);
    new_mode = SetRxFilter | RxStation | RxMulticast | RxBroadcast | RxProm;
  }
  else if ((dev->mc_list) || (dev->flags & IFF_ALLMULTI))
    new_mode = SetRxFilter | RxStation | RxMulticast | RxBroadcast;

  else
    new_mode = SetRxFilter | RxStation | RxBroadcast;

  outw (new_mode, ioaddr + EL3_CMD);
}

#ifdef __DLX__

#define SYSTEM_ID   ASCII ('_','W','A','T','T','3','2','_')
#define DRIVER_ID   ASCII ('3','C','5','1','5', 0,0,0)
#define VERSION_ID  ASCII ('v','1','.','0',     0,0,0,0)

DLXUSE_BEGIN
  LIBLOADS_BEGIN               /* no module dependencies */
    /* LIBLOAD ("pcpkt32.wlm") !!to-do, load common code */
  LIBLOADS_END

  LIBEXPORT_BEGIN
    LIBEXPORT(wlm_init)
    LIBENTRY (wlm_entry)
  LIBEXPORT_END

  LIBVERSION_BEGIN
    LIBVERSION (SYSTEM_ID)
    LIBVERSION (DRIVER_ID)
    LIBVERSION (VERSION_ID)
  LIBVERSION_END
DLXUSE_END

#endif

