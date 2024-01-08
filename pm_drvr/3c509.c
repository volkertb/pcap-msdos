/*
 * 3c509.c: A 3c509 EtherLink3 ethernet driver for linux.
 *
 * Written 1993-1995 by Donald Becker.
 *
 * Copyright 1994,1995 by Donald Becker.
 * Copyright 1993 United States Government as represented by the
 * Director, National Security Agency.   This software may be used and
 * distributed according to the terms of the GNU Public License,
 * incorporated herein by reference.
 *
 * This driver is for the 3Com EtherLinkIII series.
 *
 * The author may be reached as becker@cesdis.gsfc.nasa.gov or
 * C/O Center of Excellence in Space Data and Information Sciences
 *   Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
 *
 * Known limitations:
 * Because of the way 3c509 ISA detection works it's difficult to predict
 * a priori which of several ISA-mode cards will be detected first.
 *
 * This driver does not use predictive interrupt mode, resulting in higher
 * packet latency but lower overhead.  If interrupts are disabled for an
 * unusually long time it could also result in missed packets, but in
 * practice this rarely happens.
 *
 *
 * FIXES:
 *   Alan Cox:       Removed the 'Unexpected interrupt' bug.
 *   Michael Meskes:  Upgraded to Donald Becker's version 1.07.
 */
 
static char *version = "3c509.c:1.07 6/15/95 becker@cesdis.gsfc.nasa.gov";

#include "pmdrvr.h"
#include "module.h"

#undef  STATIC
#define STATIC /* for.map-file */

/* To minimize the size of the driver source I only define operating
 * constants if they are used several times.  You'll need the manual
 * if you want to understand driver details
 */

/* Offsets from base I/O address
 */
#define EL3_DATA       0x00
#define EL3_CMD        0x0E
#define EL3_STATUS     0x0E
#define EEPROM_READ    0x80

#define EL3_IO_EXTENT  16

#define EL3WINDOW(w)   outw (SelectWindow + (w), ioaddr + EL3_CMD)

/* The top five bits written to EL3_CMD are a command, the lower
 * 11 bits are the parameter, if applicable.
 */
enum c509cmd {
     TotalReset     = (0<<11),
     SelectWindow   = (1<<11),
     StartCoax      = (2<<11),
     RxDisable      = (3<<11),
     RxEnable       = (4<<11),
     RxReset        = (5<<11),
     RxDiscard      = (8<<11),
     TxEnable       = (9<<11),
     TxDisable      = (10<<11),
     TxReset        = (11<<11),
     FakeIntr       = (12<<11),
     AckIntr        = (13<<11),
     SetIntrEnb     = (14<<11),
     SetStatusEnb   = (15<<11),
     SetRxFilter    = (16<<11),
     SetRxThreshold = (17<<11),
     SetTxThreshold = (18<<11),
     SetTxStart     = (19<<11),
     StatsEnable    = (21<<11),
     StatsDisable   = (22<<11),
     StopCoax       = (23<<11)
   };

enum c509status {
     IntLatch       = 0x0001,
     AdapterFailure = 0x0002,
     TxComplete     = 0x0004,
     TxAvailable    = 0x0008,
     RxComplete     = 0x0010,
     RxEarly        = 0x0020,
     IntReq         = 0x0040,
     StatsFull      = 0x0080,
     CmdBusy        = 0x1000,
     c509StatMask   = 0x1FFF
   };

/*
 * The SetRxFilter command accepts the following classes:
 */
enum RxFilter {
     RxStation   = 1,
     RxMulticast = 2,
     RxBroadcast = 4,
     RxProm      = 8    /* promiscous mode */
   };

/* 
 * Register window 1 offsets, the window used in normal operation.
 */
#define TX_FIFO    0x00
#define RX_FIFO    0x00
#define RX_STATUS  0x08
#define TX_STATUS  0x0B
#define TX_FREE    0x0C           /* Remaining free bytes in Tx buffer. */

#define WN0_IRQ    0x08           /* Window 0: Set IRQ line in bits 12-15. */
#define WN4_MEDIA  0x0A           /* Window 4: Various transcvr/media bits. */
#define   MEDIA_TP 0x00C0         /* Enable link beat and jabber for 10baseT. */


int el3_debug    LOCKED_VAR = 0;  /* debug level */
int el3_max_loop LOCKED_VAR = 10; /* max # of loops allowed in el3_interrupt() */

static int id_port = 0x100;

static WORD read_eeprom_id (int index);
static WORD read_eeprom    (short ioaddr, int index);

static int   el3_open          (struct device *dev);
static int   el3_start_xmit    (struct device *dev, const void *buf, int len);
static void  el3_close         (struct device *dev);
static void *el3_get_stats     (struct device *dev);
static void  set_multicast_list(struct device *dev);

STATIC void  el3_receive   (struct device *dev)            LOCKED_FUNC;
STATIC void  el3_interrupt (int irq)                       LOCKED_FUNC;
STATIC void  el3_update    (int, struct device*, unsigned) LOCKED_FUNC;

int el3_probe (struct device *dev)
{
  static int current_tag = 0;
  short  lrs_state = 0xFF, i;
  WORD   ioaddr, iobase, irq, if_port;
  short *phys_addr = (short*) dev->dev_addr;  /* Ether address */

  /* First check all slots of the EISA bus.  The next slot address to
   * probe is kept in 'eisa_addr' to support multiple probe() calls
   */
  if (EISA_bus)
  {
    static int eisa_addr = 0x1000;

    while (eisa_addr < 0x9000)
    {
      ioaddr = eisa_addr;
      eisa_addr += 0x1000;

      /* Check the standard EISA ID register for an encoded '3Com'
       */
      if (inw (ioaddr + 0xC80) != 0x6d50)
	continue;

      /* Change the register set to the configuration window 0
       */
      outw (SelectWindow | 0, ioaddr + 0xC80 + EL3_CMD);

      irq = inw (ioaddr + WN0_IRQ) >> 12;
      if_port = inw (ioaddr + 6) >> 14;
      for (i = 0; i < 3; i++)
         phys_addr[i] = intel16 (read_eeprom (ioaddr, i));

      /* Restore the "Product ID" to the EEPROM read register
       */
      read_eeprom (ioaddr, 3);

      /* Was the EISA code an add-on hack?  Nahhhhh...
       */
      goto found;
    }
  }

  /* Reset the ISA PnP mechanism on 3c509b
   */
  outb (0x02, 0x279);      /* Select PnP config control register. */
  outb (0x02, 0xA79);      /* Return to WaitForKey state. */

  /* Select an open I/O location at 0x1*0 to do contention select
   */
  for (id_port = 0x100; id_port < 0x200; id_port += 0x10)
  {
    outb (0x00, id_port);
    outb (0xff, id_port);
    if (inb (id_port) & 1)
       break;
  }
  if (id_port >= 0x200)
  {
    /* Rare -- do we really need a warning?
     */
    printk ("  WARNING: No I/O port available for 3c509 activation.\n");
    return (0);
  }
 
  if (el3_debug > 5)
     printk ("Using ID port %04X.\n", id_port);

  /* Next check for all ISA bus boards by sending the ID sequence to the
   * ID_PORT.  We find cards past the first by setting the 'current_tag'
   * on cards as they are found.  Cards with their tag set will not
   * respond to subsequent ID sequences
   */
  outb (0x00, id_port);
  outb (0x00, id_port);
  for (i = 0; i < 255; i++)
  {
    outb (lrs_state, id_port);
    lrs_state <<= 1;
    lrs_state = lrs_state & 0x100 ? lrs_state ^ 0xcf : lrs_state;
  }

  /* For the first probe, clear all board's tag registers
   */
  if (current_tag == 0)
       outb (0xD0, id_port);
  else outb (0xD8, id_port);  /* Otherwise kill off already-found boards */

  if (read_eeprom_id(7) != 0x6D50)
  {
    if (el3_debug > 0)
       printk ("Wrong EEPROM signature. Card may be allocated.\n");
    return (0);
  }

  /* Read in EEPROM data, which does contention-select.
   * Only the lowest address board will stay "on-line".
   * 3Com got the byte order backwards
   */
  for (i = 0; i < 3; i++)
     phys_addr[i] = intel16 (read_eeprom_id(i));

  iobase  = read_eeprom_id (8);
  if_port = iobase >> 14;
  ioaddr  = 0x200 + ((iobase & 0x1f) << 4);

  if (dev->irq > 1 && dev->irq < 16)
       irq = dev->irq;
  else irq = read_eeprom_id (9) >> 12;

  if (dev->base_addr && dev->base_addr != (WORD)ioaddr)
     return (0);

  /* Set the adaptor tag so that the next card can be found
   */
  current_tag++;
  outb (0xd0 + current_tag, id_port);

  /* Activate the adaptor at the EEPROM location
   */
  outb ((ioaddr >> 4) | 0xe0, id_port);

  EL3WINDOW (0);
  if (inw (ioaddr) != 0x6D50)
  {
    if (el3_debug > 4)
       printk ("3c509: Failed to set EEPROM window.\n");
    return (0);
  }

  /* Free the interrupt so that some other card can use it
   */
  outw (0x0f00, ioaddr + WN0_IRQ);

found:
  dev->base_addr = ioaddr;
  dev->irq       = irq;
  if (dev->mem_start)
       dev->if_port = dev->mem_start & 3;
  else dev->if_port = if_port;

  {
    const char *if_names[] = { "10baseT",   "AUI",
                               "undefined", "BNC"
                             };

    if (el3_debug >= 1)
       printk ("%s: 3c509 at %X, tag %d, %s port, address ",
               dev->name, ioaddr, current_tag, if_names[dev->if_port]);
  }

  if (el3_debug >= 1)
  {
    /* Read in the station address
     */
    for (i = 0; i < sizeof(ETHER); i++)
    {
      BYTE *eth = (BYTE*) dev->dev_addr;
      printk ("%02x%c", eth[i], i < sizeof(ETHER)-1 ? ':' : ',');
    }
    printk (" IRQ %d.\n", irq);
  }

  /* Make up a EL3-specific-data structure
   */
  dev->priv = k_calloc (sizeof(struct net_device_stats), 1);
  if (!dev->priv)
     return (0);

  if (el3_debug >= 1)
     printk ("%s: %s\n", dev->name, version);

  /* The EL3-specific entries in the device structure
   */
  dev->open      = el3_open;
  dev->xmit      = el3_start_xmit;
  dev->close     = el3_close;
  dev->get_stats = el3_get_stats;
  dev->set_multicast_list = set_multicast_list;
  return (1);
}


/*
 * Read a word from the EEPROM using the regular EEPROM access register.
 * Assume that we are in register window zero
 */
static WORD read_eeprom (short ioaddr, int index)
{
  outw (EEPROM_READ + index, ioaddr + 10);

  /* Pause for at least 162us for the read to take place
   */
  delay (1);
  return inw (ioaddr+12);
}

/*
 * Read a word from the EEPROM when in the ISA ID probe state
 */
static WORD read_eeprom_id (int index)
{
  int  bit;
  WORD word = 0;

  /* Issue read command, and pause for at least 162 us. for it to complete.
   * Assume extra-fast 16Mhz bus
   */
  outb (EEPROM_READ + index, id_port);

  /* Pause for at least 162 us. for the read to take place
   */
  delay (1);

  for (bit = 15; bit >= 0; bit--)
     word = (word << 1) + (inb(id_port) & 1);

  if (el3_debug > 6)
     printk ("  3c509 EEPROM word %d %04X.\n", index, (int)word);
  return (word);
}


static int el3_open (struct device *dev)
{
  int i, ioaddr = dev->base_addr;

  outw (TxReset, ioaddr + EL3_CMD);
  outw (RxReset, ioaddr + EL3_CMD);
  outw (SetStatusEnb | 0x00, ioaddr + EL3_CMD);

  /* MUST set irq2dev_map first, because IRQ may come
   * before request_irq() returns.
   */
  irq2dev_map [dev->irq] = dev;

  if (!request_irq (dev->irq, &el3_interrupt))
  {
    irq2dev_map [dev->irq] = NULL;
    return (0);
  }

  EL3WINDOW (0);
  if (el3_debug > 3)
     printk ("%s: Opening, IRQ %d   status @%X %04X.\n", dev->name,
             dev->irq, (int)(ioaddr + EL3_STATUS),
             (int)inw(ioaddr + EL3_STATUS));

  /* Activate board: this is probably unnecessary
   */
  outw (1, ioaddr + 4);

  /* Set the IRQ line
   */
  outw ((dev->irq << 12) | 0x0f00, ioaddr + WN0_IRQ);

  /* Set the station address in window 2 each time opened
   */
  EL3WINDOW (2);

  for (i = 0; i < 6; i++)
     outb (dev->dev_addr[i], ioaddr + i);

  if (dev->if_port == 3)
  {
    /* Start the thinnet transceiver. We should really wait 50ms... */
    delay (10);
    outw (StartCoax, ioaddr + EL3_CMD);
  }
  else if (dev->if_port == 0)
  {
    /* 10baseT interface, enabled link beat and jabber check. */
    EL3WINDOW (4);
    outw (inw (ioaddr + WN4_MEDIA) | MEDIA_TP, ioaddr + WN4_MEDIA);
  }

  /* Switch to the stats window, and clear all stats by reading
   */
  outw (StatsDisable, ioaddr + EL3_CMD);
  EL3WINDOW (6);
  for (i = 0; i < 9; i++)
    inb (ioaddr + i);

  inw (ioaddr + 10);
  inw (ioaddr + 12);

  /* Switch to register set 1 for normal use
   */
  EL3WINDOW (1);

  /* Accept b-case and phys addr only
   */
  outw (SetRxFilter | RxStation | RxBroadcast, ioaddr + EL3_CMD);
  outw (StatsEnable, ioaddr + EL3_CMD);    /* Turn on statistics */

  dev->reentry = 0;
  dev->tx_busy = 0;
  dev->start   = 1;

  outw (RxEnable, ioaddr + EL3_CMD);       /* Enable the receiver */
  outw (TxEnable, ioaddr + EL3_CMD);       /* Enable transmitter  */

  /* Allow status bits to be seen
   */
  outw (SetStatusEnb | 0xff, ioaddr + EL3_CMD);

  /* Ack all pending events, and set active indicator mask
   */
  outw (AckIntr | IntLatch | TxAvailable | RxEarly | IntReq,
        ioaddr + EL3_CMD);
  outw (SetIntrEnb | IntLatch | TxAvailable | TxComplete | RxComplete | StatsFull,
        ioaddr + EL3_CMD);

  if (el3_debug > 3)
     printk ("%s: Opened 3c509, IRQ %d, status %04X.\n",
             dev->name, dev->irq, inw (ioaddr + EL3_STATUS));

  return (1);
}

static int el3_start_xmit (struct device *dev, const void *buf, int len)
{
  struct net_device_stats *stats = (struct net_device_stats*) dev->priv;
  int    ioaddr = dev->base_addr;

  /* Transmitter timeout, serious problems
   */
  if (dev->tx_busy)
  {
    int tickssofar = jiffies - dev->tx_start;

    if (tickssofar < 40 * HZ / 100)
       return (0);

    if (el3_debug >= 1)
       printk ("%s: transmit timed out, Tx_status %02X status %04X "
               "Tx FIFO room %d.\n",
               dev->name, inb (ioaddr + TX_STATUS),
               inw (ioaddr + EL3_STATUS),
               inw (ioaddr + TX_FREE));
    stats->tx_errors++;
    dev->tx_start = jiffies;

    /* Issue TX_RESET and TX_START commands
     */
    outw (TxReset, ioaddr + EL3_CMD);
    outw (TxEnable,ioaddr + EL3_CMD);
    dev->tx_busy = 0;
  }

  if (el3_debug > 4)
     printk ("%s: el3_start_xmit (length = %u) called, status %04X.\n",
             dev->name, len, inw (ioaddr + EL3_STATUS));

#if 0   /* Error-checking code, delete someday. */
  {
    WORD status = inw (ioaddr + EL3_STATUS);

    if ((status & 1) &&        /* IRQ line active, missed one */
        (inw (ioaddr + EL3_STATUS) & 1))
    {                          /* Make sure. */
      printk ("%s: Missed interrupt, status then %04X now %04X"
              "  Tx %02X Rx %04X.\n", dev->name, status,
              inw (ioaddr + EL3_STATUS), inb (ioaddr + TX_STATUS),
              inw (ioaddr + RX_STATUS));

      /* Fake interrupt trigger by masking, acknowledge interrupts
       */
      outw (SetStatusEnb | 0x00, ioaddr + EL3_CMD);
      outw (AckIntr | IntLatch | TxAvailable | RxEarly | IntReq,
           ioaddr + EL3_CMD);
      outw (SetStatusEnb | 0xff, ioaddr + EL3_CMD);
    }
  }
#endif

  if (set_bit(0,(void*)&dev->tx_busy))
     printk ("%s: Transmitter access conflict.\n", dev->name);
  else
  {
    stats->tx_bytes += len;

    /* Put out the doubleword header...
     */
    outw (len, ioaddr + TX_FIFO);
    outw (0x00,ioaddr + TX_FIFO);

    /* ... and the packet rounded to a doubleword
     */
    rep_outsl (ioaddr + TX_FIFO, buf, (len+3) >> 2);

    dev->tx_start = jiffies;
    if (inw(ioaddr + TX_FREE) > 1536)
       dev->tx_busy = 0;

    else
    {
      /* Interrupt us when the FIFO has room for max-sized packet.
       */
      outw (SetTxThreshold + 1536, ioaddr + EL3_CMD);
    }
  }

  /* Clear the Tx status stack
   */
  {
    short tx_status;
    int   i = 4;

    while (--i > 0 && (tx_status = inb (ioaddr + TX_STATUS)) > 0)
    {
      if (tx_status & 0x38)
         stats->tx_aborted_errors++;

      if (tx_status & 0x30)
         outw (TxReset, ioaddr + EL3_CMD);

      if (tx_status & 0x3C)
         outw (TxEnable, ioaddr + EL3_CMD);

      outb (0x00, ioaddr + TX_STATUS);  /* Pop the status stack */
    }
  }
  return (1);
}

/*
 * The EtherLink3 interrupt handler
 */
STATIC void el3_interrupt (int irq)
{
  struct device *dev = irq2dev_map[irq];
  int    ioaddr, status;
  int    loop = el3_max_loop;

  if (!dev || dev->irq != irq)
  {
    printk ("el3_interrupt(): irq %d for unknown device.", irq);
    return;
  }

  if (dev->reentry)
     printk ("%s: Re-entering the interrupt handler.\n", dev->name);

  dev->reentry = 1;

  ioaddr = dev->base_addr;
  status = inw (ioaddr + EL3_STATUS);

  if (el3_debug > 5)
  {
    beep();
    printk ("%s: interrupt, status %04X.\n", dev->name, status);
  }

#if 1
  while ((status = inw (ioaddr + EL3_STATUS)) &
                   (IntLatch | RxComplete | StatsFull))
  {
#else
  while (1)
  {
    status = inw (ioaddr + EL3_STATUS);
    if (!(status & (IntLatch | RxComplete | StatsFull | TxAvailable)))
       break;
#endif

    if (status & RxComplete)
       el3_receive (dev);

    if (status & TxAvailable)
    {
      if (el3_debug > 5)
         printk ("  Tx room in FIFO.\n");

      /* There's room in the FIFO for a full-sized packet
       */
      outw (AckIntr | TxAvailable, ioaddr + EL3_CMD);
      dev->tx_busy = 0;
    }

    if (status & (AdapterFailure | RxEarly | StatsFull | TxComplete))
    {
      /* Handle all uncommon interrupts
       */
      if (status & StatsFull)   /* Empty statistics */
         el3_update (ioaddr, dev, __LINE__);

      if (status & RxEarly)     /* Rx early is unused */
      {   
        el3_receive (dev);
        outw (AckIntr | RxEarly, ioaddr + EL3_CMD);
      }
      if (status & TxComplete)  /* Really Tx error. */
      {
        struct net_device_stats *stats = (struct net_device_stats*) dev->priv;
        short  tx_status;
        int    i = 4;
  
        if (el3_debug > 5)
           printk ("  Tx complete.\n");

        while (--i > 0 && (tx_status = inb(ioaddr + TX_STATUS)) > 0)
        {
          if (tx_status & 0x38)
             stats->tx_aborted_errors++;
          if (tx_status & 0x30)
             outw (TxReset, ioaddr + EL3_CMD);
          if (tx_status & 0x3C)
             outw (TxEnable, ioaddr + EL3_CMD);
          outb (0x00, ioaddr + TX_STATUS); /* Pop the status stack. */
        }
      }
      if (status & AdapterFailure)
      {
        /* Adapter failure requires Rx reset and reinit
         */
        outw (RxReset, ioaddr + EL3_CMD);

        /* Set the Rx filter to the current state
         */
        outw (SetRxFilter | RxStation | RxBroadcast |
              (dev->flags & IFF_ALLMULTI ? RxMulticast : 0) |
              (dev->flags & IFF_PROMISC  ? RxProm      : 0),
              ioaddr + EL3_CMD);
        outw (RxEnable, ioaddr + EL3_CMD);  /* Re-enable the receiver */
        outw (AckIntr | AdapterFailure, ioaddr + EL3_CMD);

        if (el3_debug > 5)
           printk ("  adapter fail.\n");
      }
    }

    if (--loop < 0)
    {
      /* Clear all interrupts
       */
      outw (AckIntr | 0xFF, ioaddr + EL3_CMD);

      if (el3_debug > 3)
         printk ("  Infinite loop in interrupt, status %04X.\n", status);
      break;
    }

    /* Acknowledge the IRQ
     */
    outw (AckIntr | IntReq | IntLatch, ioaddr + EL3_CMD);
  }

  if (el3_debug > 5)
     printk ("  exiting interrupt, status %04X.\n", inw(ioaddr + EL3_STATUS));

  dev->reentry = 0;
}


static void *el3_get_stats (struct device *dev)
{
  DISABLE();
  el3_update (dev->base_addr, dev, __LINE__);
  ENABLE();
  return (dev->priv);
}

/*
 * Update statistics.  We change to register window 6, so this should be
 * run single-threaded if the device is active.
 * This is expected to be a rare operation, and it's simpler for the rest
 * of the driver to assume that window 1 is always valid rather than use
 * a special window-state variable
 */
STATIC void el3_update (int ioaddr, DEVICE *dev, unsigned line)
{
  struct net_device_stats *stats = (struct net_device_stats*) dev->priv;

  if (el3_debug > 5)
     printk ("  (%u): updating statistics.\n", line);

  /* Turn off statistics updates while reading
   */
  outw (StatsDisable, ioaddr + EL3_CMD);

  /* Switch to the stats window, and read everything
   */
  EL3WINDOW (6);
  stats->tx_carrier_errors   += inb (ioaddr + 0);
  stats->tx_heartbeat_errors += inb (ioaddr + 1);

  /* Multiple collisions. */    inb (ioaddr + 2);
  stats->tx_collisions       += inb (ioaddr + 3);
  stats->tx_window_errors    += inb (ioaddr + 4);
  stats->rx_fifo_errors      += inb (ioaddr + 5);
  stats->tx_packets          += inb (ioaddr + 6);
  /* Rx packets  */       (void)inb (ioaddr + 7);
  /* Tx deferrals */      (void)inb (ioaddr + 8);
  /* Total Rx octets */   (void)inw (ioaddr + 10);
  /* Total Tx octets */   (void)inw (ioaddr + 12);

  /* Back to window 1, and turn statistics back on
   */
  EL3WINDOW (1);
  outw (StatsEnable, ioaddr + EL3_CMD);
}

STATIC void el3_receive (struct device *dev)
{
  struct net_device_stats *stats = (struct net_device_stats*) dev->priv;
  int    ioaddr = dev->base_addr;
  short  rx_status;

  if (el3_debug > 5)
     printk ("  In rx_packet(), status %04x, rx_status %04x.\n",
             inw (ioaddr + EL3_STATUS), inw (ioaddr + RX_STATUS));

  while ((rx_status = inw (ioaddr + RX_STATUS)) > 0)
  {
    if (rx_status & 0x4000)   /* Error, update stats */
    {                          
      short error = (rx_status & 0x3800);

      outw (RxDiscard, ioaddr + EL3_CMD);
      stats->rx_errors++;
      switch (error)
      {
        case 0x0000:
             stats->rx_over_errors++;
	     break;
        case 0x0800:
             stats->rx_length_errors++;
	     break;
        case 0x1000:
        case 0x2000:
             stats->rx_frame_errors++;
	     break;
        case 0x1800:
             stats->rx_length_errors++;
	     break;
        case 0x2800:
             stats->rx_crc_errors++;
	     break;
      }
#if 0
      i = 0;
      while ((rx_status = inw (ioaddr+EL3_STATUS)) & 0x1000)
      {
        if (el3_debug > 3)
           printk ("  Waiting for 3c509 to discard packet, status %04X.\n",
                   rx_status);
        if (++i > el3_max_loop)         /* stuck card!? */
           break;
      }
#endif
    }
    else
    {
      int  len = (rx_status & 0x7ff);
      char *buf;

      stats->rx_packets++;
      stats->rx_bytes += len;

      if (dev->get_rx_buf && (buf = (*dev->get_rx_buf) (len)) != NULL)
      {
        rep_insl (ioaddr + RX_FIFO, (DWORD*)buf, (len + 3) >> 2);
        if (el3_debug > 4)
           printk ("  Rx packet size %d.\n", len);
      }
      else
      {
        stats->rx_dropped++;
        if (el3_debug > 3)
           printk ("  Rx queue full/non-existant (len %d)\n", len);
      }
     
      /* Pop top Rx packet
       */
      outw (RxDiscard, ioaddr + EL3_CMD);
    }

    inw (ioaddr + EL3_STATUS);      /* Delay. */

    while (inw(ioaddr + EL3_STATUS) & 0x1000)
          printk ("  waiting for 3c509 to discard packet, status %04X.\n",
                  inw(ioaddr + EL3_STATUS))
         ;
  }
}

/*
 *  Set or clear the multicast filter for this adaptor.
 */
static void set_multicast_list (struct device *dev)
{
  short ioaddr = dev->base_addr;
  short mode;

  if (el3_debug > 1)
  {
    static int old = 0;

    if (old != dev->mc_count)
    {
      old = dev->mc_count;
      printk ("%s: Setting Rx mode to %d addresses.\n",
              dev->name, dev->mc_count);
    }
  }
  mode = (RxStation | RxBroadcast);

  if (dev->mc_count || (dev->flags & IFF_ALLMULTI))
     mode |= RxMulticast;

  if (dev->flags & IFF_PROMISC)
     mode |= RxMulticast | RxProm;

  outw (SetRxFilter | mode, ioaddr + EL3_CMD);
}

static void el3_close (struct device *dev)
{
  int ioaddr = dev->base_addr;

  if (el3_debug > 2)
     printk ("%s: Shutting down ethercard.\n", dev->name);

  dev->tx_busy = 1;
  dev->start   = 0;

  /* Turn off statistics ASAP. We update lp->stats below
   */
  outw (StatsDisable, ioaddr + EL3_CMD);

  /* Disable the receiver and transmitter
   */
  outw (RxDisable, ioaddr + EL3_CMD);
  outw (TxDisable, ioaddr + EL3_CMD);

#if 0
  if (dev->if_port == 3)
  {
    /* Turn off thinnet power.  This seems to require a
     * cold-reboot to reenable card
     */
    outw (StopCoax, ioaddr + EL3_CMD);
  }
#endif

  if (dev->if_port == 0)
  {
    /* 10BaseT; Disable link beat and jabber, if_port may change
     * on next open()
     */
    EL3WINDOW (4);
    outw (inw (ioaddr + WN4_MEDIA) & ~MEDIA_TP, ioaddr + WN4_MEDIA);
  }

  /* Switching back to window 0 disables the IRQ
   */
  EL3WINDOW (0);

  /* But we explicitly zero the IRQ line select anyway
   */
  outw (0x0f00, ioaddr + WN0_IRQ);

  el3_update (ioaddr, dev, __LINE__);

  free_irq (dev->irq);
  irq2dev_map[dev->irq] = NULL;
}


#ifdef _MODULE

#include <libc/file.h>

struct device el3_dev = {
              "elnk3",
              "EtherLink III (3C509)",
              0,
              0,0,0,0,0,0,
              NULL,
              el3_probe
            };

#ifndef USE_DXE3

int  errno = 0;
FILE __dj_stdout = { 0, 0, 0, 0, _IOWRT | _IOFBF, 1 };
FILE __dj_stderr = { 0, 0, 0, 0, _IOWRT | _IONBF, 2 };

struct libc_import imports;
#endif

struct device *wlm_main (struct libc_import *li, int debug_level)
{
  unsigned *src = (unsigned*) li;
  unsigned *dst = (unsigned*) &imports;

  while (*src)
    *dst++ = *src++;

  if (debug_level > 0)
     el3_debug = debug_level;
  _printk_init (1024, NULL);
  return (&el3_dev);
}
#endif

