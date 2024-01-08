/* fmv18x.c: A network device driver for the Fujitsu FMV-181/182/183/184.
 * 
 * Original: at1700.c (1993-94 by Donald Becker).
 * Copyright 1993 United States Government as represented by the
 * Director, National Security Agency.
 *
 * The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
 * Center of Excellence in Space Data and Information Sciences
 * Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
 * 
 * Modified by Yutaka TAMIYA (tamy@flab.fujitsu.co.jp)
 * Copyright 1994 Fujitsu Laboratories Ltd.
 * Special thanks to:
 * Masayoshi UTAKA (utaka@ace.yk.fujitsu.co.jp)
 * for testing this driver.
 *
 * H. NEGISHI (agy, negishi@sun45.psd.cs.fujitsu.co.jp)
 * for suggestion of some program modification.
 *
 * Masahiro SEKIGUCHI <seki@sysrap.cs.fujitsu.co.jp>
 * for suggestion of some program modification.
 *
 * Kazutoshi MORIOKA (morioka@aurora.oaks.cs.fujitsu.co.jp)
 * for testing this driver.
 * 
 * This software may be used and distributed according to the terms
 * of the GNU Public License, incorporated herein by reference.
 * 
 * This is a device driver for the Fujitsu FMV-181/182/183/184, which
 * is a straight-forward Fujitsu MB86965 implementation.
 * 
 * Sources:
 * at1700.c
 * The Fujitsu MB86965 datasheet.
 * The Fujitsu FMV-181/182 user's guide
 */

static const char *version = "fmv18x.c:v1.3.71e 03/04/96  Yutaka TAMIYA (tamy@flab.fujitsu.co.jp)\n";

#include "pmdrvr.h"

static int fmv18x_probe_list[] = { 0x220, 0x240, 0x260, 0x280, 0x2a0, 0x2c0, 0x300, 0x340, 0 };

/* use 0 for production, 1 for verification, >2 for debug
 */
#ifndef NET_DEBUG
#define NET_DEBUG 1
#endif
int net_debug = NET_DEBUG;

/* Information that need to be kept for each board.
 */
struct net_local {
       struct net_device_stats stats;
       long   open_time;           /* Useless example local info. */
       int    tx_started:1;        /* Number of packet on the Tx queue. */
       BYTE   tx_queue;            /* Number of packet on the Tx queue. */
       WORD   tx_queue_len;        /* Current length of the Tx queue. */
     };


/* Offsets from the base address.
 */
#define STATUS      0
#define TX_STATUS   0
#define RX_STATUS   1
#define TX_INTR     2          /* Bit-mapped interrupt enable registers. */
#define RX_INTR     3
#define TX_MODE     4
#define RX_MODE     5
#define CONFIG_0    6		/* Misc. configuration settings. */
#define CONFIG_1    7

/* Run-time register bank 2 definitions.
 */
#define DATAPORT    8		/* Word-wide DMA or programmed-I/O dataport. */
#define TX_START    10
#define COL16CNTL   11
#define MODE13      13

/* Fujitsu FMV-18x Card Configuration
 */
#define FJ_STATUS0       0x10
#define FJ_STATUS1       0x11
#define FJ_CONFIG0       0x12
#define FJ_CONFIG1       0x13
#define FJ_MACADDR       0x14     /* 0x14 - 0x19 */
#define FJ_BUFCNTL       0x1A
#define FJ_BUFDATA       0x1C
#define FMV18X_IO_EXTENT 32

static int   fmv18x_probe1 (struct device *dev, short ioaddr);
static int   net_open (struct device *dev);
static int   net_send_packet (struct device *dev, void *buf, int len);
static void  net_interrupt (int irq);
static void  net_rx (struct device *dev);
static void  net_close (struct device *dev);
static void *net_get_stats (struct device *dev);
static void  set_multicast_list (struct device *dev);

/*
 * Check for a network adaptor of this type, and return '0' iff one exists.
 * If dev->base_addr == 0, probe all likely locations.
 * If dev->base_addr == 1, always return failure.
 * If dev->base_addr == 2, allocate space for the device and return success
 * (detachable devices only).
 */
int fmv18x_probe (struct device *dev)
{
  int i, base_addr = dev ? dev->base_addr : 0;

  if (base_addr > 0x1ff)	/* Check a single specified location. */
     return fmv18x_probe1 (dev, base_addr);

  if (base_addr != 0)		/* Don't probe at all. */
     return (0);

  for (i = 0; fmv18x_probe_list[i]; i++)
  {
    int ioaddr = fmv18x_probe_list[i];
    if (fmv18x_probe1 (dev, ioaddr))
      return (1);
  }
  return (0);
}

/*
 * The Fujitsu datasheet suggests that the NIC be probed for by checking its
 * "signature", the default bit pattern after a reset.  This *doesn't* work --
 * there is no way to reset the bus interface without a complete power-cycle!
 * 
 * It turns out that ATI came to the same conclusion I did: the only thing
 * that can be done is checking a few bits and then diving right into MAC
 * address check. */

int fmv18x_probe1 (struct device *dev, short ioaddr)
{
  char  irq, irqmap[4] = { 3, 7, 10, 15 };
  DWORD i;

  /* Resetting the chip doesn't reset the ISA interface, so don't bother.
   * That means we have to be careful with the register values we probe for.
   *
   * Check I/O address configuration and Fujitsu vendor code
   */
  if (fmv18x_probe_list[inb (ioaddr + FJ_CONFIG0) & 7] != ioaddr ||
      inb (ioaddr + FJ_MACADDR)   != 0x00 ||
      inb (ioaddr + FJ_MACADDR+1) != 0x00 ||
      inb (ioaddr + FJ_MACADDR+2) != 0x0e)
    return (0);

  irq = irqmap[(inb (ioaddr + FJ_CONFIG0) >> 6) & 0x03];

  /* Snarf the interrupt vector now.
   */
  if (!request_irq (irq, &net_interrupt))
  {
    printk ("FMV-18x found at %3x, but it's unusable due to a conflict on"
            "IRQ %d.\n", ioaddr, irq);
    return (0);
  }

  /* Allocate a new 'dev' if needed.
   */
  if (!dev)
    dev = init_etherdev (0, sizeof (struct net_local));

  printk ("%s: FMV-18x found at %3x, IRQ %d, address ",
          dev->name, ioaddr, irq);

  dev->base_addr = ioaddr;
  dev->irq       = irq;
  irq2dev_map[(int)irq] = dev;

  for (i = 0; i < ETH_ALEN; i++)
  {
    BYTE val = inb (ioaddr + FJ_MACADDR + i);
    printk ("%02x", val);
    dev->dev_addr[i] = val;
  }

  /* "FJ_STATUS0" 12 bit 0x0400 means use regular 100 ohm 10baseT signals,
   * rather than 150 ohm shielded twisted pair compensation.
   * 0x0000 == auto-sense the interface
   * 0x0800 == use TP interface
   * 0x1800 == use coax interface
   */
  {
    char *porttype[] = { "auto-sense", "10baseT", "auto-sense", "10base2/5" };
    WORD setup_value = inb (ioaddr + FJ_STATUS0);

    switch (setup_value & 0x07)
    {
      case 0x01 /* 10base5 */ :
      case 0x02 /* 10base2 */ :
	   dev->if_port = 0x18;
	   break;
      case 0x04 /* 10baseT */ :
	   dev->if_port = 0x08;
	   break;
      default /* auto-sense */ :
	   dev->if_port = 0x00;
	   break;
    }
    printk (" %s interface.\n", porttype[(dev->if_port >> 3) & 3]);
  }

  /* Initialize LAN Controller and LAN Card
   */
  outb (0xda, ioaddr + CONFIG_0);	/* Initialize LAN Controller */
  outb (0x00, ioaddr + CONFIG_1);	/* Stand by mode */
  outb (0x00, ioaddr + FJ_CONFIG1);	/* Disable IRQ of LAN Card */
  outb (0x00, ioaddr + FJ_BUFCNTL);	/* Reset ? I'm not sure (TAMIYA) */

  /* wait for a while
   */
  udelay (200);

  /* Set the station address in bank zero.
   */
  outb (0x00, ioaddr + CONFIG_1);
  for (i = 0; i < 6; i++)
     outb (dev->dev_addr[i], ioaddr + 8 + i);

  /* Switch to bank 1 and set the multicast table to accept none.
   */
  outb (0x04, ioaddr + CONFIG_1);
  for (i = 0; i < 8; i++)
     outb (0x00, ioaddr + 8 + i);

  /* Switch to bank 2 and lock our I/O address.
   */
  outb (0x08, ioaddr + CONFIG_1);
  outb (dev->if_port, ioaddr + MODE13);

  if (net_debug)
     printk (version);

  /* Initialize the device structure
   */
  dev->priv = k_calloc (sizeof (struct net_local), 1);
  if (!dev->priv)
     return (0);

  dev->open  = net_open;
  dev->close = net_close;
  dev->xmit  = net_send_packet;
  dev->get_stats          = net_get_stats;
  dev->set_multicast_list = set_multicast_list;

  /* Fill in the fields of 'dev' with ethernet-generic values.
   */
  ether_setup (dev);
  return (1);
}


static int net_open (struct device *dev)
{
  struct net_local *lp = (struct net_local *) dev->priv;
  int    ioaddr = dev->base_addr;

  /* Set the configuration register 0 to 32K 100ns. byte-wide memory,
   * 16 bit bus access, and two 4K Tx, enable the Rx and Tx.
   */
  outb (0x5a, ioaddr + CONFIG_0);

  /* Powerup and switch to register bank 2 for the run-time registers.
  */
  outb (0xe8, ioaddr + CONFIG_1);

  lp->tx_started   = 0;
  lp->tx_queue     = 0;
  lp->tx_queue_len = 0;

  /* Clear Tx and Rx Status
   */
  outb (0xff, ioaddr + TX_STATUS);
  outb (0xff, ioaddr + RX_STATUS);
  lp->open_time = jiffies;

  dev->tx_busy = 0;
  dev->reentry = 0;
  dev->start   = 1;

  /* Enable the IRQ of the LAN Card
   */
  outb (0x80, ioaddr + FJ_CONFIG1);

  /* Enable both Tx and Rx interrupts
   */
  outw (0x8182, ioaddr + TX_INTR);
  return (1);
}

static int net_send_packet (struct device *dev, void *buf, int len)
{
  struct net_local *lp = (struct net_local *) dev->priv;
  int    ioaddr = dev->base_addr;

  if (dev->tx_busy)
  {
    /* If we get here, some higher level has decided we are broken.
     * There should really be a "kick me" function call instead.
     */
    int tickssofar = jiffies - dev->tx_start;

    if (tickssofar < 10)
       return (1);

    printk ("%s: transmit timed out with status %04x, %s?\n", dev->name,
	    htons (inw (ioaddr + TX_STATUS)),
            inb (ioaddr + TX_STATUS) & 0x80 ?
               "IRQ conflict" : "network cable problem");
    printk ("%s: timeout registers: %04x %04x %04x %04x %04x %04x %04x %04x.\n",
	    dev->name, htons (inw (ioaddr + 0)),
            htons (inw (ioaddr + 2)),
            htons (inw (ioaddr + 4)),
            htons (inw (ioaddr + 6)),
            htons (inw (ioaddr + 8)),
            htons (inw (ioaddr + 10)),
            htons (inw (ioaddr + 12)),
            htons (inw (ioaddr + 14)));
    printk ("eth card: %04x %04x\n",
            htons (inw (ioaddr + FJ_STATUS0)),
            htons (inw (ioaddr + FJ_CONFIG0)));
    lp->stats.tx_errors++;

    /* ToDo: We should try to restart the adaptor...
     */
    DISABLE();

    /* Initialize LAN Controller and LAN Card
     */
    outb (0xda, ioaddr + CONFIG_0);	/* Initialize LAN Controller */
    outb (0x00, ioaddr + CONFIG_1);	/* Stand by mode */
    outb (0x00, ioaddr + FJ_CONFIG1);	/* Disable IRQ of LAN Card */
    outb (0x00, ioaddr + FJ_BUFCNTL);	/* Reset ? I'm not sure */
    net_open (dev);
    ENABLE();
  }

  /* Block a timer-based transmit from overlapping.  This could better be
   * done with atomic_swap(1, dev->tx_busy), but set_bit() works as well.
   */
  if (set_bit (0, (void *) &dev->tx_busy))
     printk ("%s: Transmitter access conflict.\n", dev->name);
  else
  {
    short length = len < ETH_MIN ? ETH_MIN : len;

    if (length > ETH_MAX)
    {
      if (net_debug)
         printk ("%s: Attempting to send a large packet (%d bytes).\n",
                 dev->name, length);
      return (0);
    }

    if (net_debug > 4)
       printk ("%s: Transmitting a packet of length %d.\n",
               dev->name, len);

    /* Disable both interrupts.
     */
    outw (0x0000, ioaddr + TX_INTR);

    outw (length, ioaddr + DATAPORT);
    outsw (ioaddr + DATAPORT, buf, (length + 1) >> 1);

    lp->tx_queue++;
    lp->tx_queue_len += length + 2;

    if (lp->tx_started == 0)
    {
      /* If the Tx is idle, always trigger a transmit.
       */
      outb (0x80 | lp->tx_queue, ioaddr + TX_START);
      lp->tx_queue     = 0;
      lp->tx_queue_len = 0;
      dev->tx_start    = jiffies;
      lp->tx_started   = 1;
      dev->tx_busy     = 0;
    }
    else if (lp->tx_queue_len < 4096 - 1502)
    {
      /* Yes, there is room for one more packet.
       */
      dev->tx_busy = 0;
    }
    /* Re-enable interrupts
     */
    outw (0x8182, ioaddr + TX_INTR);
  }
  return (1);
}

/*
 * The typical workload of the driver:
 * Handle the network interface interrupts.
 */
static void net_interrupt (int irq)
{
  struct device    *dev = irq2dev_map[irq];
  struct net_local *lp;
  int    ioaddr, status;

  if (!dev)
  {
    printk ("fmv18x_interrupt(): irq %d for unknown device.\n", irq);
    return;
  }
  dev->reentry = 1;

  ioaddr = dev->base_addr;
  lp = (struct net_local *) dev->priv;

  /* Avoid multiple interrupts.
   */
  outw (0x0000, ioaddr + TX_INTR);

  status = inw (ioaddr + TX_STATUS);
  outw (status, ioaddr + TX_STATUS);

  if (net_debug > 4)
     printk ("%s: Interrupt with status %04x.\n", dev->name, status);

  if (status & 0xff00 || (inb (ioaddr + RX_MODE) & 0x40) == 0)
     net_rx (dev);                 /* Got a packet(s). */

  if (status & 0x00ff)
  {
    if (status & 0x80)
    {
      lp->stats.tx_packets++;
      if (lp->tx_queue)
      {
	outb (0x80 | lp->tx_queue, ioaddr + TX_START);
	lp->tx_queue = 0;
	lp->tx_queue_len = 0;
	dev->tx_start = jiffies;
	dev->tx_busy = 0;
      }
      else
      {
	lp->tx_started = 0;
	dev->tx_busy = 0;
      }
    }
    if (status & 0x02)
    {
      if (net_debug > 4)
	printk ("%s: 16 Collision occur during Txing.\n", dev->name);
      /* Retry to send the packet */
      outb (0x02, ioaddr + COL16CNTL);
    }
  }
  dev->reentry = 0;
  outw (0x8182, ioaddr + TX_INTR);
}

/*
 * We have a good packet(s), get it/them out of the buffers.
 */
static void net_rx (struct device *dev)
{
  struct net_local *lp = (struct net_local *) dev->priv;
  int    ioaddr = dev->base_addr;
  int    boguscount = 10;          /* 5 -> 10: by agy 19940922 */

  while ((inb (ioaddr + RX_MODE) & 0x40) == 0)
  {
    /* Clear PKT_RDY bit: by agy 19940922
     * outb(0x80, ioaddr + RX_STATUS);
     */
    WORD status = inw (ioaddr + DATAPORT);

    if (net_debug > 4)
       printk ("%s: Rxing packet mode %02x status %04x.\n",
               dev->name, inb (ioaddr + RX_MODE), status);
#ifndef final_version
    if (status == 0)
    {
      outb (0x05, ioaddr + 14);
      break;
    }
#endif

    if ((status & 0xF0) != 0x20)
    {				/* There was an error. */
      lp->stats.rx_errors++;
      if (status & 0x08)
	lp->stats.rx_length_errors++;
      if (status & 0x04)
	lp->stats.rx_frame_errors++;
      if (status & 0x02)
	lp->stats.rx_crc_errors++;
      if (status & 0x01)
	lp->stats.rx_over_errors++;
    }
    else
    {
      WORD  pkt_len = inw (ioaddr + DATAPORT);
      BYTE *pkt_buf = NULL;

      if (pkt_len > ETH_MAX+10)
      {
        printk ("%s: The FMV-18x claimed a very large packet, size %d.\n",
                dev->name, pkt_len);
	outb (0x05, ioaddr + 14);
	lp->stats.rx_errors++;
	break;
      }

      if (dev->get_rx_buf)
      {
        pkt_buf = (*dev->get_rx_buf) (pkt_len);
        if (pkt_buf)
             insw (ioaddr + DATAPORT, pkt_buf, (pkt_len + 1) >> 1);
        else lp->stats.rx_dropped++;
      }

      if (pkt_buf && net_debug > 5)
      {
	int i;

        printk ("%s: Rx'ed packet of length %d: ", dev->name, pkt_len);
	for (i = 0; i < 14; i++)
           printk (" %02x", pkt_buf[i]);
	printk (".\n");
      }
      lp->stats.rx_packets++;
    }
    if (--boguscount <= 0)
       break;
  }

  /* If any worth-while packets have been received, dev_rint()
   * has done a mark_bh(NET_BH) for us and will work on them
   * when we get to the bottom-half routine.
   */
  {
    int i;

    for (i = 0; i < 20; i++)
    {
      if ((inb (ioaddr + RX_MODE) & 0x40) == 0x40)
	break;
      inw (ioaddr + DATAPORT);	/* dummy status read */
      outb (0x05, ioaddr + 14);
    }
    if (net_debug > 5 && i > 0)
       printk ("%s: Exint Rx packet with mode %02x after %d ticks.\n",
               dev->name, inb (ioaddr + RX_MODE), i);
  }
}

/*
 * The inverse routine to net_open().
 */
static void net_close (struct device *dev)
{
  int ioaddr = dev->base_addr;

  ((struct net_local*) dev->priv)->open_time = 0;

  dev->tx_busy = 1;
  dev->start = 0;

  /* Set configuration register 0 to disable Tx and Rx.
   */
  outb (0xda, ioaddr + CONFIG_0);

  net_get_stats (dev);

  /* Power-down the chip.  Green, green, green!
   */
  outb (0x00, ioaddr + CONFIG_1);

  /* Set the ethernet adaptor disable IRQ
   */
  outb (0x00, ioaddr + FJ_CONFIG1);
}

/*
 * Get the current statistics.  This may be called with the card open or
 * closed.
 */
static void *net_get_stats (struct device *dev)
{
  struct net_local *lp = (struct net_local *) dev->priv;

  DISABLE();
  /* ToDo: Update the statistics from the device registers. */
  ENABLE();
  return (void*)&lp->stats;
}

/*
 * Set or clear the multicast filter for this adaptor.
 * num_addrs == -1  Promiscuous mode, receive all packets
 * num_addrs == 0  Normal mode, clear multicast list
 * num_addrs > 0  Multicast mode, receive normal and MC packets, and do
 * best-effort filtering.
 */
static void set_multicast_list (struct device *dev)
{
  short ioaddr = dev->base_addr;

  if (dev->mc_count || dev->flags & (IFF_PROMISC | IFF_ALLMULTI))
  {
    /* We must make the kernel realise we had to move
     * into promisc mode or we start all out war on
     * the cable. - AC
     */
    dev->flags |= IFF_PROMISC;
    outb (3, ioaddr + RX_MODE);	/* Enable promiscuous mode */
  }
  else
    outb (2, ioaddr + RX_MODE);	/* Disable promiscuous, use normal mode */
}
