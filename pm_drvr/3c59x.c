/*
 * 3c59x.c: A 3Com 3c590/3c595 "Vortex" ethernet driver for linux.
 *
 * Written 1995 by Donald Becker.
 *
 * This software may be used and distributed according to the terms
 * of the GNU Public License, incorporated herein by reference.
 *
 * This driver is for the 3Com "Vortex" series ethercards.  Members of
 * the series include the 3c590 PCI EtherLink III and 3c595-Tx PCI Fast
 * EtherLink.
 *
 * The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
 * Center of Excellence in Space Data and Information Sciences
 *    Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
 */

#if 0
static char *version = "3c59x.c:v0.25 5/17/96 becker@cesdis.gsfc.nasa.gov\n";
#endif

#include "pmdrvr.h"
#include "module.h"
#include "bios32.h"
#include "pci.h"
#include "3c59x.h"

int vortex_debug LOCKED_VAR = VORTEX_DEBUG;

static int   product_ids[]   = { 0x5900, 0x5950, 0x5951,
                                 0x5952, 0, 0
                               };
static char *product_names[] = { "3c590 Vortex 10Mbps",
                                 "3c595 Vortex 100baseTX",
                                 "3c595 Vortex 100baseT4",
                                 "3c595 Vortex 100base-MII",
                                 "EISA Vortex 3c597"
                               };
struct vortex_private {
       char   devname[8];         /* "ethN" string, also for kernel debug. */
       const  char   *product_name;
       struct device *next_module;
       struct net_device_stats stats;
       struct timer_list timer;   /* Media selection timer */
       int    options;            /* User-settable misc. driver options */
       int    last_rx_packets;    /* For media autoselection */
       DWORD  available_media:8,  /* From Wn3_Options */
              media_override:3,   /* Passed-in media type */
              default_media:3,    /* Read from the EEPROM */
              full_duplex:1,
              bus_master:1,
              autoselect:1;
     };

/* The action to take with a media selection timer tick.
 * Note that we deviate from the 3Com order by checking 10base2 before AUI.
 */
static struct media_table {
       char  *name;
       DWORD  media_bits : 16; /* Bits to set in Wn4_Media register. */
       DWORD  mask : 8;        /* The transceiver-present bit in Wn3_Config.*/
       DWORD  next : 8;        /* The media type to try next. */
       short  wait;            /* Time before we check media status. */
     } media_tbl[] LOCKED_VAR = {

     { "10baseT",   Media_10TP,0x08, 3 /* 10baseT->10base2 */,   (14*HZ)/10 },
     { "10Mbs AUI", Media_SQE, 0x20, 8 /* AUI->default */,       (1*HZ)/10  },
     { "undefined", 0,         0x80, 0 /* Undefined */,          0          },
     { "10base2",   0,         0x10, 1 /* 10base2->AUI. */,      (1*HZ)/10  },
     { "100baseTX", Media_Lnk, 0x02, 5 /* 100baseTX->100baseFX*/,(14*HZ)/10 },
     { "100baseFX", Media_Lnk, 0x04, 6 /* 100baseFX->MII */,     (14*HZ)/10 },
     { "MII",       0,         0x40, 0 /* MII->10baseT */,       (14*HZ)/10 },
     { "undefined", 0,         0x01, 0 /* Undefined/100baseT4*/, 0          },
     { "Default",   0,         0xFF, 0 /* Use default */,        0          }
   };

static int   vortex_scan       (struct device *dev);
static void  vortex_found_dev  (struct device *dev, int, int, int, int);
static int   vortex_open       (struct device *dev);
static int   vortex_probe1     (struct device *dev);
static int   vortex_start_xmit (struct device *dev, const void* buf, int len);
static void *vortex_get_stats  (struct device *dev);
static void  vortex_close      (struct device *dev);

STATIC void  vortex_recv       (struct device *dev)           LOCKED_FUNC;
STATIC void  vortex_interrupt  (int irq)                      LOCKED_FUNC;
STATIC void  vortex_update     (int addr, struct device *dev) LOCKED_FUNC;
STATIC void  vortex_set_mode   (struct device *dev)           LOCKED_FUNC;

#if AUTOMEDIA
STATIC void  vortex_timer (DWORD arg) LOCKED_FUNC;
#endif



/*
 * Unlike the other PCI cards the 59x cards don't need a large contiguous
 * memory region, so making the driver a loadable module is feasible.
 *
 * The wrappers for vortex_scan()
 * vortex_scan()      The common routine that scans for PCI and EISA cards
 * vortex_found_dev() Allocate a device structure when we find a card.
 *                    Different versions exist for modules and built-in.
 * vortex_probe1()    Fill in the device structure -- this is separated
 *                    so that the modules code can put it in dev->init.
 */

/* This driver uses 'options' to pass the media type, full-duplex flag, etc.
 * Note: this is the only limit on the number of cards supported!!
 */
int vortex_options[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

int tc59x_probe (struct device *dev)
{
  return vortex_scan (dev);
}

static int vortex_scan (struct device *dev)
{
  int cards_found = 0;

  if (pcibios_present())
  {
    static int pci_index   = 0;
    static int board_index = 0;

    for (; product_ids[board_index]; board_index++, pci_index = 0)
    {
      for (; pci_index < 16; pci_index++)
      {
        BYTE  pci_device_fn, pci_bus;
        BYTE  pci_irq_line, pci_latency;
        DWORD pci_ioaddr;
        WORD  pci_command;

        if (vortex_debug > 2)
           printk ("Searching for vendor %04X, product %04X\n",
                   PCI_VENDOR_ID_3COM, product_ids[board_index]);

        if (pcibios_find_device(PCI_VENDOR_ID_3COM,
                                product_ids[board_index],
                                pci_index, &pci_bus, &pci_device_fn))
           break; /* failed */

        pcibios_read_config_byte  (pci_bus, pci_device_fn,
                                   PCI_INTERRUPT_LINE, &pci_irq_line);
        pcibios_read_config_dword (pci_bus, pci_device_fn,
                                   PCI_BASE_ADDRESS_0, &pci_ioaddr);
        /* Remove I/O space marker in bit 0
         */
        pci_ioaddr &= ~3;

#ifdef VORTEX_BUS_MASTER
        /* Get and check the bus-master and latency values.
         * Some PCI BIOSes fail to set the master-enable bit, and
         * the latency timer must be set to the maximum value to avoid
         * data corruption that occurs when the timer expires during
         * a transfer.  Yes, it's a bug
         */
        pcibios_read_config_word (pci_bus, pci_device_fn,
                                  PCI_COMMAND, &pci_command);
        if (!(pci_command & PCI_COMMAND_MASTER))
        {
          if (vortex_debug)
             printk ("  PCI Master Bit has not been set! Setting...\n");
          pci_command |= PCI_COMMAND_MASTER;
          pcibios_write_config_word (pci_bus, pci_device_fn,
                                     PCI_COMMAND, pci_command);
        }
        pcibios_read_config_byte (pci_bus, pci_device_fn,
                                  PCI_LATENCY_TIMER, &pci_latency);
        if (pci_latency != 255)
        {
          if (vortex_debug)
             printk ("  Overriding PCI latency timer (CFLT) setting of"
                     " %d, new value is 255.\n", pci_latency);
          pcibios_write_config_byte (pci_bus, pci_device_fn,
                                     PCI_LATENCY_TIMER, 255);
        }
#endif
        vortex_found_dev (dev, pci_ioaddr, pci_irq_line, board_index,
                          dev && dev->mem_start ? dev->mem_start
                                                : vortex_options[cards_found]);
        dev = 0;
        cards_found++;
      }
    }
  }

  /* Now check all slots of the EISA bus
   */
  if (EISA_bus)
  {
    static int ioaddr = 0x1000;

    for (ioaddr = 0x1000; ioaddr < 0x9000; ioaddr += 0x1000)
    {
      /* Check the standard EISA ID register for an encoded '3Com'
       */
      if (inw(ioaddr + 0xC80) != 0x6d50)
        continue;

      /* Check for a product that we support, 3c59{2,7} any rev
       */
      if ((inw(ioaddr + 0xC82) & 0xF0FF) != 0x7059 &&    /* 597 */
          (inw(ioaddr + 0xC82) & 0xF0FF) != 0x2059)      /* 592 */
        continue;

      vortex_found_dev (dev, ioaddr, inw(ioaddr + 0xC88) >> 12,
                        DEMON_INDEX, dev && dev->mem_start ?
                                     dev->mem_start : vortex_options[cards_found]);
      dev = 0;
      cards_found++;
    }
  }
  return (cards_found);
}

static void vortex_found_dev (struct device *dev, int ioaddr, int irq,
                              int product_index, int options)
{
  struct vortex_private *vp;

  if (dev)
     dev->priv = k_calloc (sizeof(struct vortex_private), 1);

  dev = init_etherdev (dev, sizeof(struct vortex_private));
  dev->base_addr = ioaddr;
  dev->irq       = irq;
  vp  = (struct vortex_private*) dev->priv;
  vp->product_name = product_names[product_index];
  vp->options      = options;

  if (options >= 0)
  {
    vp->media_override = ((options & 7) == 2) ? 0 : options & 7;
    vp->full_duplex    = (options  & 8)  ? 1 : 0;
    vp->bus_master     = (options  & 16) ? 1 : 0;
  }
  else
  {
    vp->media_override = 7;
    vp->full_duplex    = 0;
    vp->bus_master     = 0;
  }
  vortex_probe1 (dev);
}

static int vortex_probe1 (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private*)dev->priv;
  int    i, ioaddr = dev->base_addr;

  if (vortex_debug)
     printk ("%s: 3Com %s at %X,", dev->name, vp->product_name, ioaddr);

  /* Read the station address from the EEPROM
   */
  EL3WINDOW (0);
  for (i = 0; i < 3; i++)
  {
    short *phys_addr = (short*)dev->dev_addr;
    int    timer;
    outw (EEPROM_Read + PhysAddr01 + i, ioaddr + Wn0EepromCmd);

    /* Pause for at least 162 us. for the read to take place
     */
    for (timer = 162*4 + 400; timer >= 0; timer--)
    {
      SLOW_DOWN_IO();
      if ((inw(ioaddr + Wn0EepromCmd) & 0x8000) == 0)
        break;
    }
    phys_addr[i] = htons (inw(ioaddr + Wn0EepromData));
  }

  if (vortex_debug)
  {
    for (i = 0; i < 6; i++)
        printk ("%c%02x", i ? ':' : ' ', dev->dev_addr[i]);

    printk (", IRQ %d\n", dev->irq);
  }

  /* Tell them about an invalid IRQ
   */
  if (vortex_debug && (dev->irq <= 0 || dev->irq > 15))
     printk (" *** Warning: this IRQ is unlikely to work!\n");

  {
    char            *ram_split[] = { "5:3", "3:1", "1:1", "invalid" };
    union wn3_config config;

    EL3WINDOW (3);
    vp->available_media = inw (ioaddr + Wn3_Options);
    config.i = inl (ioaddr + Wn3_Config);
    if (vortex_debug > 1)
    {
      printk ("  Internal config register is %04X, transceivers %X.\n",
              (unsigned)config.i, inw(ioaddr + Wn3_Options));
      printk ("  %dK %s-wide RAM %s Rx:Tx split, %s%s interface.\n",
              8 << config.u.ram_size,
              config.u.ram_width ? "word" : "byte",
              ram_split[config.u.ram_split],
              config.u.autoselect ? "autoselect/" : "",
              media_tbl[config.u.xcvr].name);
    }

    dev->if_port      = config.u.xcvr;
    vp->default_media = config.u.xcvr;
    vp->autoselect    = config.u.autoselect;
  }

  /* The 3c59x-specific entries in the device structure
   */
  dev->open      = vortex_open;
  dev->xmit      = vortex_start_xmit;
  dev->close     = vortex_close;
  dev->get_stats = vortex_get_stats;
  dev->set_multicast_list = vortex_set_mode;
  return (1);
}


static int vortex_open (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private *)dev->priv;
  union  wn3_config config;
  int    i, ioaddr = dev->base_addr;

  /* Before initializing select the active media port
   */
  EL3WINDOW (3);
  if (vp->full_duplex)
     outb (0x20, ioaddr + Wn3_MAC_Ctrl); /* Set the full-duplex bit. */
  config.i = inl (ioaddr + Wn3_Config);

  if (vp->media_override != 7)
  {
    if (vortex_debug > 1)
       printk ("%s: Media override to transceiver %d (%s).\n",
               dev->name, vp->media_override,
               media_tbl[vp->media_override].name);
    dev->if_port = vp->media_override;
  }
  else if (vp->autoselect)
  {
    /* Find first available media type, starting with 100baseTx
     */
    dev->if_port = 4;
    while (! (vp->available_media & media_tbl[dev->if_port].mask))
      dev->if_port = media_tbl[dev->if_port].next;

    if (vortex_debug > 1)
       printk ("%s: Initial media type %s.\n",
               dev->name, media_tbl[dev->if_port].name);

#if AUTOMEDIA
    init_timer (&vp->timer);
    vp->timer.expires  = RUN_AT (media_tbl[dev->if_port].wait);
    vp->timer.data     = (DWORD)dev;
    vp->timer.function = vortex_timer;    /* timer handler */
    add_timer (&vp->timer);
#endif
  }
  else
    dev->if_port = vp->default_media;

  config.u.xcvr = dev->if_port;
  outl (config.i, ioaddr + Wn3_Config);

  if (vortex_debug > 1)
     printk ("%s: vortex_open() InternalConfig %x.\n",
             dev->name, (unsigned)config.i);

  outw (TxReset, ioaddr + EL3_CMD);
  for (i = 20; i >= 0 ; i--)
    if (!inw(ioaddr + EL3_STATUS) & CmdInProgress)
      break;

  outw (RxReset, ioaddr + EL3_CMD);

  /* Wait a few ticks for the RxReset command to complete
   */
  for (i = 20; i >= 0 ; i--)
    if (!inw(ioaddr + EL3_STATUS) & CmdInProgress)
      break;

  outw (SetStatusEnb | 0x00, ioaddr + EL3_CMD);

  /* MUST set irq2dev_map first, because IRQ may come
   * before request_irq() returns.
   */
  irq2dev_map [dev->irq] = dev;

  /* Use the now-standard shared IRQ implementation
   */
  if (!request_irq(dev->irq, &vortex_interrupt))
  {
    irq2dev_map [dev->irq] = NULL;
    return (0);
  }

  if (vortex_debug > 1)
  {
    EL3WINDOW (4);
    printk ("%s: vortex_open() irq %d media status %04x.\n",
            dev->name, dev->irq, inw(ioaddr + Wn4_Media));
  }

  /* Set the station address and mask in window 2 each time opened
   */
  EL3WINDOW (2);
  for (i = 0; i < 6; i++)
      outb (dev->dev_addr[i], ioaddr + i);

  for (; i < 12; i+=2)
      outw (0, ioaddr + i);

  if (dev->if_port == 3)
  {
    /* Start the thinnet transceiver. We should really wait 50ms...*/
    outw (StartCoax, ioaddr + EL3_CMD);
  }
  EL3WINDOW (4);
  outw ((inw(ioaddr + Wn4_Media) & ~(Media_10TP|Media_SQE)) |
        media_tbl[dev->if_port].media_bits, ioaddr + Wn4_Media);

  /* Switch to the stats window, and clear all stats by reading
   */
  outw (StatsDisable, ioaddr + EL3_CMD);
  EL3WINDOW (6);
  for (i = 0; i < 10; i++)  
    inb (ioaddr + i);

  inw (ioaddr + 10);
  inw (ioaddr + 12);

  /* New: On the Vortex we must also clear the BadSSD counter
   */
  EL3WINDOW(4);
  inb (ioaddr + 12);

  /* Switch to register set 7 for normal use
   */
  EL3WINDOW(7);

  /* Set receiver mode: presumably accept b-case and phys addr only
   */
  vortex_set_mode (dev);
  outw (StatsEnable, ioaddr + EL3_CMD); /* Turn on statistics. */

  dev->tx_busy = 0;
  dev->reentry = 0;
  dev->start   = 1;

  outw (RxEnable, ioaddr + EL3_CMD); /* Enable the receiver. */
  outw (TxEnable, ioaddr + EL3_CMD); /* Enable transmitter. */

  /* Allow status bits to be seen
   */
  outw (SetStatusEnb | 0xff, ioaddr + EL3_CMD);

  /* Ack all pending events, and set active indicator mask
   */
  outw (AckIntr | IntLatch | TxAvailable | RxEarly | IntReq,
        ioaddr + EL3_CMD);
  outw (SetIntrEnb | IntLatch | TxAvailable | RxComplete | StatsFull | DMADone,
        ioaddr + EL3_CMD);

  return (1);
}

#if AUTOMEDIA
STATIC void vortex_timer (DWORD data)
{
  struct device         *dev = (struct device*)data;
  struct vortex_private *vp  = (struct vortex_private*)dev->priv;
  int    ioaddr = dev->base_addr;
  int    ok     = 0;

  if (vortex_debug > 1)
     printk ("%s: Media selection timer tick happened, %s.\n",
             dev->name, media_tbl[dev->if_port].name);

  {
    int old_window = inw (ioaddr + EL3_CMD) >> 13;
    int media_status;

    EL3WINDOW (4);
    media_status = inw (ioaddr + Wn4_Media);

    switch (dev->if_port)
    {
      case 0:     /* 10baseT, 100baseTX, 100baseFX  */
      case 4:
      case 5:
           if (media_status & Media_LnkBeat)
           {
             ok = 1;
             if (vortex_debug > 1)
                printk ("%s: Media %s has link beat, %x.\n",
                        dev->name, media_tbl[dev->if_port].name, media_status);
           }
           else if (vortex_debug > 1)
                printk ("%s: Media %s has no link beat, %x.\n",
                        dev->name, media_tbl[dev->if_port].name, media_status);
           break;
      default:          /* Other media types handled by Tx timeouts. */
          if (vortex_debug > 1)
             printk ("%s: Media %s has no indication, %x.\n",
                     dev->name, media_tbl[dev->if_port].name, media_status);
          ok = 1;
    }
    if (!ok)
    {
      union wn3_config config;

      do {
        dev->if_port = media_tbl[dev->if_port].next;
      }
      while (!(vp->available_media & media_tbl[dev->if_port].mask));

      if (dev->if_port == 8)  /* Go back to default. */
      {
        dev->if_port = vp->default_media;
        if (vortex_debug > 1)
           printk ("%s: Media selection failing, using default %s port.\n",
                   dev->name, media_tbl[dev->if_port].name);
      }
      else
      {
        if (vortex_debug > 1)
           printk ("%s: Media selection failed, now trying %s port.\n",
                   dev->name, media_tbl[dev->if_port].name);
        vp->timer.expires = RUN_AT (media_tbl[dev->if_port].wait);
        add_timer (&vp->timer);
      }
      outw ((media_status & ~(Media_10TP|Media_SQE)) |
            media_tbl[dev->if_port].media_bits, ioaddr + Wn4_Media);

      EL3WINDOW (3);
      config.i = inl (ioaddr + Wn3_Config);
      config.u.xcvr = dev->if_port;

      outl (config.i, ioaddr + Wn3_Config);
      outw (dev->if_port == 3 ? StartCoax : StopCoax, ioaddr + EL3_CMD);
    }
    EL3WINDOW (old_window);
  }

  if (vortex_debug > 1)
     printk ("%s: Media selection timer finished, %s.\n",
             dev->name, media_tbl[dev->if_port].name);
}
#endif /* AUTOMEDIA*/

static int vortex_start_xmit (struct device *dev, const void *buf, int len)
{
  struct vortex_private *vp = (struct vortex_private *)dev->priv;
  int    ioaddr = dev->base_addr;

  /* Part of the following code is inspired by code from Giuseppe Ciaccio,
   * ciaccio@disi.unige.it.
   * It works around a ?bug? in the 8K Vortex that only occurs on some
   * systems: the TxAvailable interrupt seems to be lost.
   * The ugly work-around is to busy-wait for room available in the Tx
   * buffer before deciding the transmitter is actually hung.
   * This busy-wait should never really occur, since the problem is that
   * there actually *is*  room in the Tx FIFO.
   *
   * This pointed out an optimization -- we can ignore dev->tbusy if
   * we actually have room for this packet.
   */

  if (dev->tx_busy)
  {
    /* Transmitter timeout, serious problems
     */
    int   i;
    DWORD tickssofar = jiffies - dev->tx_start;

    if (tickssofar < 2)    /* We probably aren't empty */
       return (1);

    /* Wait a while to see if there really is room
     */
    for (i = WAIT_TX_AVAIL; i >= 0; i--)
    {
      if (inw(ioaddr + TxFree) > len)
         break;
    }
    if ( i < 0)
    {
      BYTE tx_stat;
      WORD el_stat;

      if (tickssofar < TX_TIMEOUT)
         return (1);

      tx_stat = inb (ioaddr + TxStatus);
      el_stat = inw (ioaddr + EL3_STATUS);
      if (vortex_debug)
         printk ("%s: transmit timed out, tx_status %02x status %04x.\n",
                 dev->name, tx_stat, el_stat);

      /* Issue TX_RESET and TX_START commands
       */
      outw (TxReset, ioaddr + EL3_CMD);
      for (i = 20; i >= 0 ; i--)
          if (!inw(ioaddr + EL3_STATUS) & CmdInProgress)                                        break;
             outw (TxEnable, ioaddr + EL3_CMD);

      dev->tx_start = jiffies;
      dev->tx_busy  = 0;
      vp->stats.tx_errors++;
      vp->stats.tx_dropped++;
      return (0);      /* Yes, silently *drop* the packet! */
    }
  }

  if (dev->tx_busy)
  {
    if (vortex_debug)
       printk ("%s: Transmitter access conflict.\n", dev->name);
    return (0);
  }

  /* Put out the doubleword header...
   */
  outl (len, ioaddr + TX_FIFO);

#ifdef VORTEX_BUS_MASTER
  if (vp->bus_master)
  {
    /* Set the bus-master controller to transfer the packet
     */
    outl ((unsigned)buf, ioaddr + Wn7_MasterAddr);
    outw ((len + 3) & ~3, ioaddr + Wn7_MasterLen);
    outw (StartDMADown, ioaddr + EL3_CMD);
    /* dev->tx_busy will be cleared at the DMADone interrupt.
     */
  }
  else
  {
    /* ... and the packet rounded to a doubleword
     */
    rep_outsl (ioaddr + TX_FIFO, buf, (len+3) >> 2);
    if (inw(ioaddr + TxFree) > 1536)
       dev->tx_busy = 0;
    else
    {
      /* Interrupt us when the FIFO has room for max-sized packet.
       */
      outw (SetTxThreshold + (1536>>2), ioaddr + EL3_CMD);
    }
  }
#else
  /* ... and the packet rounded to a doubleword
   */
  rep_outsl (ioaddr + TX_FIFO, buf, (len + 3) >> 2);
  if (inw(ioaddr + TxFree) > 1536)
    dev->tx_busy = 0;
  else
  {
    /* Interrupt us when the FIFO has room for max-sized packet.
     */
    outw (SetTxThreshold + (1536>>2), ioaddr + EL3_CMD);
  }
#endif  /* bus master */

  dev->tx_start = jiffies;

  /* Clear the Tx status stack. */
  {
    short tx_status;
    int   i = 4;

    while (--i > 0 && (tx_status = inb(ioaddr + TxStatus)) > 0)
    {
      if (tx_status & 0x3C)   /* A Tx-disabling error occurred */
      {
        if (vortex_debug > 2)
           printk ("%s: Tx error, status %02x.\n",
                   dev->name, tx_status);
        if (tx_status & 0x04)
           vp->stats.tx_fifo_errors++;

        if (tx_status & 0x38)
           vp->stats.tx_aborted_errors++;

        if (tx_status & 0x30)
        {
          int j;
          outw (TxReset, ioaddr + EL3_CMD);
          for (j = 20; j >= 0 ; j--)
            if (!(inw(ioaddr + EL3_STATUS) & CmdInProgress))
              break;
        }
        outw (TxEnable, ioaddr + EL3_CMD);
      }
      outb (0x00, ioaddr + TxStatus); /* Pop the status stack. */
    }
  }
  return (1);
}

/*
 * The interrupt handler does all of the Rx thread work and
 * cleans up after the Tx thread
 */
STATIC void vortex_interrupt (int irq)
{
  struct device *dev = irq2dev_map[irq];
  int    ioaddr, status, latency;
  int    i = 0;

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

  if (vortex_debug > 4)
     printk ("%s: interrupt, status %04x, timer %d.\n",
             dev->name, status, latency);

  if ((status & 0xE000) != 0xE000)
  {
    static int done_did_this LOCKED_VAR = 0;

    /* Some interrupt controllers store a bogus interrupt from boot-time.
     * Ignore a single early interrupt, but don't hang the machine for
     * other interrupt problems
     */
    if (done_did_this++ > 1)
    {
      printk ("%s: Bogus interrupt, bailing. Status %04x, start=%d.\n",
              dev->name, status, dev->start);
      free_irq (dev->irq);
      irq2dev_map[dev->irq] = NULL;
    }
  }

  do
  {
    if (vortex_debug > 5)
       printk ("%s: In interrupt loop, status %04x.\n",
               dev->name, status);
    if (status & RxComplete)
       vortex_recv (dev);

    if (status & TxAvailable)
    {
      if (vortex_debug > 5)
         printk ("  TX room bit was handled.\n");

      /* There's room in the FIFO for a full-sized packet
       */
      outw (AckIntr | TxAvailable, ioaddr + EL3_CMD);
      dev->tx_busy = 0;
    }
#ifdef VORTEX_BUS_MASTER
    if (status & DMADone)
    {
      outw (0x1000, ioaddr + Wn7_MasterStatus); /* Ack the event. */
      dev->tx_busy = 0;
    }
#endif
    if (status & (AdapterFailure | RxEarly | StatsFull))
    {
      /* Handle all uncommon interrupts at once
       */
      if (status & RxEarly)        /* Rx early is unused */
      {
        vortex_recv (dev);
        outw (AckIntr | RxEarly, ioaddr + EL3_CMD);
      }
      if (status & StatsFull)      /* Empty statistics */
      {
        static int DoneDidThat LOCKED_VAR = 0;

        if (vortex_debug > 4)
           printk ("%s: Updating stats.\n", dev->name);

        vortex_update (ioaddr, dev);

        /* DEBUG HACK: Disable statistics as an interrupt source.
         * This occurs when we have the wrong media type!
         */
        if (DoneDidThat == 0 && (inw(ioaddr+EL3_STATUS) & StatsFull))
        {
          int win, reg;

          printk ("%s: Updating stats failed, disabling stats as an"
                  " interrupt source.\n", dev->name);
          for (win = 0; win < 8; win++)
          {
            EL3WINDOW (win);
            printk ("\n Vortex window %d:", win);
            for (reg = 0; reg < 16; reg++)
                printk (" %02x", inb(ioaddr+reg));
          }
          EL3WINDOW (7);
          outw (SetIntrEnb | 0x18, ioaddr + EL3_CMD);
          DoneDidThat++;
        }
      }
      if (status & AdapterFailure)
      {
        /* Adapter failure requires Rx reset and reinit
         */
        outw (RxReset, ioaddr + EL3_CMD);
        /* Set the Rx filter to the current state
         */
        vortex_set_mode (dev);
        outw (RxEnable, ioaddr + EL3_CMD); /* Re-enable the receiver */
        outw (AckIntr | AdapterFailure, ioaddr + EL3_CMD);
      }
    }

    if (++i > 10)
    {
      if (vortex_debug)
         printk ("%s: Infinite loop in interrupt, status %04x.  "
                 "Disabling functions (%04x).\n",
                 dev->name, status, SetStatusEnb | ((~status) & 0xFE));

      /* Disable all pending interrupts.
       */
      outw (SetStatusEnb | ((~status) & 0xFE), ioaddr + EL3_CMD);
      outw (AckIntr | 0xFF, ioaddr + EL3_CMD);
      break;
    }
    /* Acknowledge the IRQ
     */
    outw (AckIntr | IntReq | IntLatch, ioaddr + EL3_CMD);
  }
  while ((status = inw(ioaddr + EL3_STATUS)) & (IntLatch | RxComplete));

  if (vortex_debug > 4)
     printk ("%s: exiting interrupt, status %04x.\n", dev->name, status);

  dev->reentry = 0;
}

STATIC void vortex_recv (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private *)dev->priv;
  int    i,  ioaddr = dev->base_addr;
  short  rx_status;

  if (vortex_debug > 5)
     printk ("   In rx_packet(), status %04x, rx_status %04x.\n",
             inw(ioaddr+EL3_STATUS), inw(ioaddr+RxStatus));

  while ((rx_status = inw(ioaddr + RxStatus)) > 0)
  {
    if (rx_status & 0x4000)  /* Error, update stats. */
    {
      BYTE rx_error = inb (ioaddr + RxErrors);

      if (vortex_debug > 4)
         printk (" Rx error: status %02x.\n", rx_error);

      vp->stats.rx_errors++;
      if (rx_error & 0x01)  vp->stats.rx_over_errors++;
      if (rx_error & 0x02)  vp->stats.rx_length_errors++;
      if (rx_error & 0x04)  vp->stats.rx_frame_errors++;
      if (rx_error & 0x08)  vp->stats.rx_crc_errors++;
      if (rx_error & 0x10)  vp->stats.rx_length_errors++;
    }
    else
    {
      /* The packet length: up to 4.5K!.
       */
      short pkt_len = rx_status & 0x1fff;

      if (vortex_debug > 4)
         printk ("Receiving packet size %d status %4x.\n",
                 pkt_len, rx_status);

      if (dev->get_rx_buf)
      {
        char *buf = (*dev->get_rx_buf) (pkt_len);
        if (buf)
             rep_insl (ioaddr + RX_FIFO, (DWORD*)buf, (pkt_len+3) >> 2);
        else vp->stats.rx_dropped++;
      }
      outw (RxDiscard, ioaddr + EL3_CMD); /* Pop top Rx packet. */

      /* Wait a limited time to go to next packet
       */
      for (i = 200; i >= 0; i--)
         if (!inw(ioaddr + EL3_STATUS) & CmdInProgress)
            break;
      vp->stats.rx_packets++;
      continue;
    }
    vp->stats.rx_dropped++;
    outw (RxDiscard, ioaddr + EL3_CMD);

    /* Wait a limited time to skip this packet
     */
    for (i = 200; i >= 0; i--)
      if (!inw(ioaddr + EL3_STATUS) & CmdInProgress)
        break;
  }
}

static void vortex_close (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private*) dev->priv;
  int    ioaddr = dev->base_addr;

  dev->start   = 0;
  dev->tx_busy = 1;

  if (vortex_debug > 1)
     printk ("%s: vortex_close() status %04x, Tx status %02x.\n",
             dev->name, inw(ioaddr + EL3_STATUS), inb(ioaddr + TxStatus));

  del_timer (&vp->timer);

  /* Turn off statistics ASAP.  We update lp->stats below
   */
  outw (StatsDisable, ioaddr + EL3_CMD);

  /* Disable the receiver and transmitter
   */
  outw (RxDisable, ioaddr + EL3_CMD);
  outw (TxDisable, ioaddr + EL3_CMD);

  if (dev->if_port == 3)
  {
    /* Turn off ThinNet power.  Green! */
    outw (StopCoax, ioaddr + EL3_CMD);
  }

  vortex_update (ioaddr, dev);

  free_irq (dev->irq);
  irq2dev_map[dev->irq] = NULL;
}

static void *vortex_get_stats (struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private*)dev->priv;

  DISABLE();
  vortex_update (dev->base_addr, dev);
  ENABLE();
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
STATIC void vortex_update (int ioaddr, struct device *dev)
{
  struct vortex_private *vp = (struct vortex_private*) dev->priv;

  /* Unlike the 3c5x9 we need not turn off stats updates while reading.
   * Switch to the stats window, and read everything
   */
  EL3WINDOW (6);
  vp->stats.tx_carrier_errors    += inb (ioaddr + 0);
  vp->stats.tx_heartbeat_errors  += inb (ioaddr + 1);
  /* Multiple collisions */         inb (ioaddr + 2);
  vp->stats.tx_collisions        += inb (ioaddr + 3);
  vp->stats.tx_window_errors     += inb (ioaddr + 4);
  vp->stats.rx_fifo_errors       += inb (ioaddr + 5);
  vp->stats.tx_packets           += inb (ioaddr + 6);
  vp->stats.tx_packets           += (inb(ioaddr + 9) & 0x30) << 4;
  /* Rx packets  */                 inb (ioaddr + 7);  /* Must read to clear */
  /* Tx deferrals */                inb (ioaddr + 8);

  /* Don't bother with register 9, an extension of registers 6&7.
   * If we do use the 6&7 values the atomic update assumption above
   * is invalid
   */
  inw (ioaddr + 10);  /* Total Rx and Tx octets. */
  inw (ioaddr + 12);

  /* New: On the Vortex we must also clear the BadSSD counter
   */
  EL3WINDOW (4);
  inb (ioaddr + 12);

  /* We change back to window 7 (not 1) with the Vortex
   */
  EL3WINDOW (7);
}

/*
 * This new version of vortex_set_mode() supports v1.4 kernels.
 * The Vortex chip has no documented multicast filter, so the only
 * multicast setting is to receive all multicast frames.  At least
 * the chip has a very clean way to set the mode, unlike many others
 */
STATIC void vortex_set_mode (struct device *dev)
{
  short ioaddr = dev->base_addr;
  short new_mode;

  if (dev->flags & IFF_PROMISC)
  {
    if (vortex_debug > 3)
       printk ("%s: Setting promiscuous mode.\n", dev->name);
    new_mode = SetRxFilter | RxStation | RxMulticast | RxBroadcast | RxProm;
  }
  else if (dev->mc_list || (dev->flags & IFF_ALLMULTI))
  {
    new_mode = SetRxFilter | RxStation | RxMulticast | RxBroadcast;
  }
  else
    new_mode = SetRxFilter | RxStation | RxBroadcast;

  outw (new_mode, ioaddr + EL3_CMD);
}


#ifdef __DLX__

#define SYSTEM_ID   ASCII ('_','W','A','T','T','3','2','_')
#define DRIVER_ID   ASCII ('3','C','5','9','x', 0,0,0)
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

