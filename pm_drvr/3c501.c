/* 3c501.c: A 3Com 3c501 ethernet driver for linux. */
/*
 *  Written 1992,1993,1994  Donald Becker
 *
 *  Copyright 1993 United States Government as represented by the
 *  Director, National Security Agency.  This software may be used and
 *  distributed according to the terms of the GNU Public License,
 *  incorporated herein by reference.
 *
 *  This is a device driver for the 3Com Etherlink 3c501.
 *  Do not purchase this card, even as a joke.  It's performance is horrible,
 *  and it breaks in many ways.
 *
 *  The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
 *  Center of Excellence in Space Data and Information Sciences
 *     Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
 *
 *  Fixed (again!) the missing interrupt locking on TX/RX shifting.
 *      Alan Cox <Alan.Cox@linux.org>
 *
 *  Removed calls to init_etherdev since they are no longer needed, and
 *  cleaned up modularization just a bit. The driver still allows only
 *  the default address for cards when loaded as a module, but that's
 *  really less braindead than anyone using a 3c501 board. :)
 *      19950208 (invid@msen.com)
 *
 *  Added traps for interrupts hitting the window as we clear and TX load
 *  the board. Now getting 150K/second FTP with a 3c501 card. Still playing
 *  with a TX-TX optimisation to see if we can touch 180-200K/second as seems
 *  theoretically maximum.
 *      19950402 Alan Cox <Alan.Cox@linux.org>
 *
 *  Some notes on this thing if you have to hack it.  [Alan]
 *
 *  1]  Some documentation is available from 3Com. Due to the boards age
 *    standard responses when you ask for this will range from 'be serious'
 *    to 'give it to a museum'. The documentation is incomplete and mostly
 *    of historical interest anyway.
 *
 *  2]  The basic system is a single buffer which can be used to receive or
 *    transmit a packet. A third command mode exists when you are setting
 *    things up.
 *
 *  3]  If it's transmitting it's not receiving and vice versa. In fact the
 *    time to get the board back into useful state after an operation is
 *    quite large.
 *
 *  4]  The driver works by keeping the board in receive mode waiting for a
 *    packet to arrive. When one arrives it is copied out of the buffer
 *    and delivered to the kernel. The card is reloaded and off we go.
 *
 *  5]  When transmitting dev->tx_busy is set and the card is reset (from
 *    receive mode) [possibly losing a packet just received] to command
 *    mode. A packet is loaded and transmit mode triggered. The interrupt
 *    handler runs different code for transmit interrupts and can handle
 *    returning to receive mode or retransmissions (yes you have to help
 *    out with those too).
 *
 *  Problems:
 *    There are a wide variety of undocumented error returns from the card
 *  and you basically have to kick the board and pray if they turn up. Most
 *  only occur under extreme load or if you do something the board doesn't
 *  like (eg touching a register at the wrong time).
 *
 *    The driver is less efficient than it could be. It switches through
 *  receive mode even if more transmits are queued. If this worries you buy
 *  a real ethernet card.
 *
 *    The combination of slow receive restart and no real multicast
 *  filter makes the board unusable with a kernel compiled for IP
 *  multicasting in a real multicast environment. That's down to the board,
 *  but even with no multicast programs running a multicast IP kernel is
 *  in group 224.0.0.1 and you will therefore be listening to all multicasts.
 *  One nv conference running over that ethernet and you can give up.
 *
 */

#include "pmdrvr.h"
#include "3c501.h"

/* Prototypes
 */

static int   el_open       (struct device *dev);
static int   el_start_xmit (struct device *dev, const void *buf, int len);
static void  el_receive    (struct device *dev);
static void  el_reset      (struct device *dev);
static void  el1_close     (struct device *dev);
static void *el1_get_stats (struct device *dev);
static void  el_interrupt  (int irq);
static void  set_multicast_list(struct device *dev);

#ifndef EL_DEBUG
#define EL_DEBUG  0  /* use 0 for production, 1 for devel., >2 for debug */
#endif               /* Anything above 5 is wordy death! */

static int el_debug = EL_DEBUG;

/*
 * Board-specific info in dev->priv.
 */
struct net_local {
       struct net_device_stats stats;
       int    tx_pkt_start;    /* The length of the current Tx packet. */
       int    collisions;      /* Tx collisions this packet */
     };

#define RX_STATUS    (ioaddr + 0x06)
#define RX_CMD       RX_STATUS
#define TX_STATUS    (ioaddr + 0x07)
#define TX_CMD       TX_STATUS
#define GP_LOW       (ioaddr + 0x08)
#define GP_HIGH      (ioaddr + 0x09)
#define RX_BUF_CLR   (ioaddr + 0x0A)
#define RX_LOW       (ioaddr + 0x0A)
#define RX_HIGH      (ioaddr + 0x0B)
#define SAPROM       (ioaddr + 0x0C)
#define AX_STATUS    (ioaddr + 0x0E)
#define AX_CMD       AX_STATUS
#define DATAPORT     (ioaddr + 0x0F)
#define TX_RDY       0x08    /* In TX_STATUS */

#define EL1_DATAPTR  0x08
#define EL1_RXPTR    0x0A
#define EL1_SAPROM   0x0C
#define EL1_DATAPORT 0x0f

/*
 * Writes to the AX command register.
 */

#define AX_OFF   0x00      /* Irq off, buffer access on */
#define AX_SYS   0x40      /* Load the buffer */
#define AX_XMIT  0x44      /* Transmit a packet */
#define AX_RX    0x48      /* Receive a packet */
#define AX_LOOP  0x0C      /* Loopback mode */
#define AX_RESET 0x80

/*
 * Normal receive mode written to RX_STATUS. We must intr on short packets
 * to avoid bogus rx lockups.
 */

#define RX_NORM 0xA8    /* 0x68 == all addrs, 0xA8 only to me. */
#define RX_PROM 0x68    /* Senior Prom, uhmm promiscuous mode. */
#define RX_MULT 0xE8    /* Accept multicast packets. */
#define TX_NORM 0x0A    /* Interrupt on everything that might hang the chip */

/*
 * TX_STATUS register.
 */

#define TX_COLLISION    0x02
#define TX_16COLLISIONS 0x04
#define TX_READY        0x08

#define RX_RUNT         0x08
#define RX_MISSED       0x01    /* Missed a packet due to 3c501 braindamage. */
#define RX_GOOD         0x30    /* Good packet 0x20, or simple overflow 0x10. */


/*
 * The probe routine
 */
int el1_probe (struct device *dev)
{
  const char *mname;    /* Vendor name */
  ETHER eth;
  int   autoirq = 0;
  int   ioaddr  = dev->base_addr;
  int   i;

  /* Read the station address PROM data from the special port.
   */
  for (i = 0; i < ETH_ALEN; i++)
  {
    outw (i, ioaddr + EL1_DATAPTR);
    eth[i] = inb (ioaddr + EL1_SAPROM);
  }
  /* Check the first three octets of the S.A. for 3Com's prefix, or
   * for the Sager NP943 prefix.
   */
  if (eth[0] == 0x02 && eth[1] == 0x60 && eth[2] == 0x8C)
       mname = "3c501";
  else if (eth[0] == 0x00 && eth[1] == 0x80 && eth[2] == 0xC8)
       mname = "NP943";
  else return (0);

  /* We auto-IRQ by shutting off the interrupt line and letting it float
   * high.
   */
  if (dev->irq < 2)
  {
    autoirq_setup (2);
    inb (RX_STATUS);    /* Clear pending interrupts */
    inb (TX_STATUS);
    outb (AX_LOOP+1,AX_CMD);
    outb (0,AX_CMD);

    autoirq = autoirq_report (1);
    if (autoirq == 0)
    {
      printk ("%s probe at %X failed to detect IRQ line.\n",
              mname, ioaddr);
      return (0);
    }
  }

  outb (AX_RESET+AX_LOOP,AX_CMD);      /* Loopback mode */
  dev->base_addr = ioaddr;
  memcpy (&dev->dev_addr, &eth, ETH_ALEN);

  if (autoirq)
     dev->irq = autoirq;

  printk ("%s: %s EtherLink at %08lX, using %sIRQ %d.\n",
          dev->name, mname, dev->base_addr,
          autoirq ? "auto" : "assigned ", dev->irq);

  /* Initialize the device structure.
   */
  dev->priv = calloc (sizeof(struct net_local),1);
  if (!dev->priv)
     return (0);

  /* The EL1-specific entries in the device structure.
   */   
  dev->open      = el_open;
  dev->xmit      = el_start_xmit;
  dev->close     = el1_close;
  dev->get_stats = el1_get_stats;
  dev->set_multicast_list = set_multicast_list;
  return (1);
}

/*
 * Open/initialize the board.
 */
static int el_open (struct device *dev)
{
  int ioaddr = dev->base_addr;

  if (el_debug > 2)
     printk ("%s: Doing el_open()...", dev->name);

  irq2dev_map[dev->irq] = dev;
  if (!request_irq(dev->irq,&el_interrupt))
  {
    irq2dev_map[dev->irq] = NULL;
    return (0);
  }

  el_reset (dev);
  dev->start = 1;

  outb (AX_RX,AX_CMD);  /* Aux control, irq and receive enabled */
  return (1);
}

static int el_start_xmit (struct device *dev, const void *buf, int len)
{
  struct net_local *lp = (struct net_local*)dev->priv;
  int    ioaddr = dev->base_addr;
  int    gp_start;

  if (dev->reentry)    /* May be unloading, don't stamp on */
     return (0);       /* the packet buffer this time      */

  if (dev->tx_busy)
  {
    if (jiffies - dev->tx_start < 20)
    {
      if (el_debug > 2)
         printk (" transmitter busy, deferred.\n");
      return (0);
    }
    if (el_debug)
       printk ("%s: transmit timed out, txsr %2x axsr=%02x rxsr=%02x.\n",
               dev->name, inb(TX_STATUS), inb(AX_STATUS), inb(RX_STATUS));

    lp->stats.tx_errors++;
    outb (TX_NORM,TX_CMD);
    outb (RX_NORM,RX_CMD);
    outb (AX_OFF, AX_CMD);  /* Just trigger a false interrupt. */
    outb (AX_RX,  AX_CMD);  /* Aux control, irq and receive enabled */
    dev->tx_busy  = 0;
    dev->tx_start = jiffies;
  }

  /* Avoid incoming interrupts between us flipping tx_busy and flipping
   * mode as the driver assumes tx_busy is a faithful indicator of card
   * state
   */
  DISABLE();
  dev->tx_busy = 1;
  gp_start = 0x800 - (ETH_MIN < len ? len : ETH_MIN);

  lp->tx_pkt_start    = gp_start;
  lp->collisions      = 0;
  lp->stats.tx_bytes += len;

  /* Command mode with status cleared should [in theory]
   * mean no more interrupts can be pending on the card.
   */
  disable_irq (dev->irq);
  outb (AX_SYS,AX_CMD);
  inb (RX_STATUS);
  inb (TX_STATUS);

  /* Turn interrupts back on while we spend a pleasant afternoon
   * loading bytes into the board
   */
  ENABLE();
  outw (0, RX_BUF_CLR);         /* Set rx packet area to 0. */
  outw (gp_start,GP_LOW);       /* aim - packet will be loaded into buffer start */
  rep_outsb (DATAPORT,buf,len); /* load buffer into ddataport */
  outw (gp_start,GP_LOW);       /* the board reuses the same register */
  outb (AX_XMIT,AX_CMD);        /* fire ... Trigger xmit */
  enable_irq (dev->irq);
  dev->tx_start = jiffies;

  if (el_debug > 2)
     printk (" queued xmit.\n");
  return (1);
}


/*
 * Handle the ether interface interrupts.
 */
static void el_interrupt (int irq)
{
  struct device *dev = (struct device*) &irq2dev_map[irq];
  struct net_local *lp;
  int    ioaddr, axsr;      /* Aux. status reg. */

  if (!dev || dev->irq != irq)
  {
    printk ("3c501: irq %d for unknown device.\n", irq);
    return;
  }

  ioaddr = dev->base_addr;
  lp = (struct net_local *)dev->priv;

  /*
   *  What happened ?
   */
  axsr = inb (AX_STATUS);

  if (el_debug > 3)
  {
    beep();
    printk("%s: el_interrupt() aux=%02x", dev->name, axsr);
  }

  if (dev->reentry)
     printk ("%s: Reentering the interrupt driver!\n", dev->name);

  dev->reentry = 1;

  if (dev->tx_busy)
  {
   /* Board in transmit mode. May be loading. If we are
    * loading we shouldn't have got this.
    */
    int txsr = inb (TX_STATUS);
    if (el_debug > 6)
       printk (" txsr=%02x gp=%04x rp=%04x", txsr,
               inw(GP_LOW),inw(RX_LOW));

    if ((axsr & 0x80) && (txsr & TX_READY) == 0)
    {
      /* FIXME: is there a logic to whether to keep on trying or
       * reset immediately ?
       */
      if (el_debug>1)
         printk ("%s: Unusual interrupt during Tx, txsr=%02x axsr=%02x"
                 " gp=%03x rp=%03x.\n", dev->name, txsr, axsr,
                 inw (ioaddr + EL1_DATAPTR),
                 inw (ioaddr + EL1_RXPTR));
      dev->tx_busy = 0;
    }
    else if (txsr & TX_16COLLISIONS)
    {
      /* Timed out
       */
      if (el_debug)
         printk ("%s: Transmit failed 16 times, ethernet jammed?\n",dev->name);
      outb (AX_SYS,AX_CMD);
      lp->stats.tx_aborted_errors++;
    }
    else if (txsr & TX_COLLISION)
    {
      /* Retrigger xmit.
       */
      if (el_debug > 6)
         printk (" retransmitting after a collision.\n");

      /* Poor little chip can't reset its own start pointer
       */
      outb (AX_SYS,AX_CMD);
      outw (lp->tx_pkt_start,GP_LOW);
      outb (AX_XMIT,AX_CMD);
      lp->stats.tx_collisions++;
      dev->reentry = 0;
      return;
    }
    else
    {
      /* It worked.. we will now fall through and receive
       */
      lp->stats.tx_packets++;
      if (el_debug > 6)
         printk (" Tx succeeded %s\n",
                 (txsr & TX_RDY) ? "." : "but tx is busy!");
      dev->tx_busy = 0;
    }
  }
  else
  {
   /* In receive mode.
    */
    int rxsr = inb(RX_STATUS);
    if (el_debug > 5)
       printk (" rxsr=%02x txsr=%02x rp=%04x", rxsr,
               inb(TX_STATUS),inw(RX_LOW));
    /* Just reading rx_status fixes most errors.
     */
    if (rxsr & RX_MISSED)
       lp->stats.rx_missed_errors++;

    else if (rxsr & RX_RUNT)
    {  /* Handled to avoid board lock-up. */
      lp->stats.rx_length_errors++;
      if (el_debug > 5)
         printk (" runt.\n");
    }
    else if (rxsr & RX_GOOD)
    {
      /* Receive worked.
       */
      el_receive (dev);
    }
    else
    {
      /* Nothing?  Something is broken!
       */
      if (el_debug > 2)
         printk ("%s: No packet seen, rxsr=%02x **resetting 3c501***\n",
                 dev->name, rxsr);
      el_reset (dev);
    }
    if (el_debug > 3)
       printk (".\n");
  }

  /* Move into receive mode
   */
  outb (AX_RX,AX_CMD);
  outw (0,RX_BUF_CLR);
  inb (RX_STATUS);    /* Be certain that interrupts are cleared. */
  inb (TX_STATUS);
  dev->reentry = 0;
}


/*
 * We have a good packet. Well, not really "good", just mostly not broken.
 * We must check everything to see if it is good.
 */
static void el_receive (struct device *dev)
{
  struct net_local *lp = (struct net_local *)dev->priv;
  int    ioaddr = dev->base_addr;
  int    len    = inw (RX_LOW);

  if (el_debug > 4)
     printk (" el_receive %d.\n", len);

  if (len < 60 || len > 1536)
  {
    if (el_debug)
       printk ("%s: bogus packet, length=%d\n", dev->name, len);
    lp->stats.rx_over_errors++;
    return;
  }

  /* Command mode so we can empty the buffer
   */
  outb (AX_SYS,AX_CMD);

  /* Start of frame
   */
  outw (0,GP_LOW);

  /* The read increments through the bytes. The interrupt
   * handler will fix the pointer when it returns to
   * receive mode.
   */
  if (dev->get_rx_buf)
  {
    char *buf = (*dev->get_rx_buf) (len);
    if (buf)
         rep_insb (DATAPORT, buf, len);
    else lp->stats.rx_dropped++;
  }
  lp->stats.rx_packets++;
}

static void el_reset (struct device *dev)
{
  int ioaddr = dev->base_addr;

  if (el_debug > 2)
     printk ("3c501 reset...");

  outb (AX_RESET,AX_CMD);     /* Reset the chip */
  outb (AX_LOOP, AX_CMD);     /* Aux control, irq and loopback enabled */
  {
    int i;
    for (i = 0; i < ETH_ALEN; i++)  /* Set the station address. */
      outb (dev->dev_addr[i],ioaddr+i);
  }

  outw (0,RX_BUF_CLR);        /* Set rx packet area to 0. */
  DISABLE();                  /* Avoid glitch on writes to CMD regs */
  outb (TX_NORM,TX_CMD);      /* tx irq on done, collision */
  outb (RX_NORM,RX_CMD);      /* Set Rx commands. */
  inb (RX_STATUS);            /* Clear status. */
  inb (TX_STATUS);
  dev->reentry = 0;
  dev->tx_busy = 0;
  ENABLE();
}

static void el1_close (struct device *dev)
{
  int ioaddr = dev->base_addr;

  if (el_debug > 2)
     printk ("%s: Shutting down ethercard at %x.\n", dev->name, ioaddr);

  dev->tx_busy = 1;
  dev->start   = 0;

  /* Free and disable the IRQ.
   */
  free_irq (dev->irq);
  irq2dev_map[dev->irq] = NULL;
  outb (AX_RESET,AX_CMD);    /* Reset the chip */
}

static void *el1_get_stats (struct device *dev)
{
  struct net_local *lp = (struct net_local *)dev->priv;
  return (void*)&lp->stats;
}

/*
 * Set or clear the multicast filter for this adaptor.
 * best-effort filtering.
 */  
static void set_multicast_list (struct device *dev)
{
  int ioaddr = dev->base_addr;

  if (dev->flags & IFF_PROMISC)
       outb (RX_PROM,RX_CMD);
  else if (dev->mc_count || dev->flags & IFF_ALLMULTI)
       outb (RX_MULT,RX_CMD);  /* Multicast or all multicast is the same */
  else outb (RX_NORM,RX_CMD);

  inb (RX_STATUS);         /* Clear status. */
}

#ifdef __DLX__

#define SYSTEM_ID  ASCII ('_','W','A','T','T','3','2','_')
#define DRIVER_ID  ASCII ('3','C','5','0','1', 0,0,0)
#define VERSION_ID ASCII ('v','1','.','0',     0,0,0,0)

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

