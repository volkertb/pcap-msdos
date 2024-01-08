/*
 *  de620.c $Revision: 1.40 $ BETA
 *
 *
 *  Linux driver for the D-Link DE-620 Ethernet pocket adapter.
 *
 *  Portions (C) Copyright 1993, 1994 by Bjorn Ekwall <bj0rn@blox.se>
 *
 *  Based on adapter information gathered from DOS packetdriver
 *  sources from D-Link Inc:  (Special thanks to Henry Ngai of D-Link.)
 *    Portions (C) Copyright D-Link SYSTEM Inc. 1991, 1992
 *    Copyright, 1988, Russell Nelson, Crynwr Software
 *
 *  Adapted to the sample network driver core for linux,
 *  written by: Donald Becker <becker@super.org>
 *    (Now at <becker@cesdis.gsfc.nasa.gov>
 *
 *  Valuable assistance from:
 *    J. Joshua Kopper <kopper@rtsg.mot.com>
 *    Olav Kvittem <Olav.Kvittem@uninett.no>
 *    Germano Caronni <caronni@nessie.cs.id.ethz.ch>
 *    Jeremy Fitzhardinge <jeremy@suite.sw.oz.au>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

static const char *version = "de620.c: $Revision: 1.40 $,  Bjorn Ekwall <bj0rn@blox.se>\n";

/*
 * "Tuning" section.
 *
 * Compile-time options: (see below for descriptions)
 * -DDE620_IO=0x378  (lpt1)
 * -DDE620_IRQ=7  (lpt1)
 * -DDE602_DEBUG=...
 * -DSHUTDOWN_WHEN_LOST
 * -DCOUNT_LOOPS
 * -DLOWSPEED
 * -DREAD_DELAY
 * -DWRITE_DELAY
 */

/*
 * This driver assumes that the printer port is a "normal",
 * dumb, uni-directional port!
 * If your port is "fancy" in any way, please try to set it to "normal"
 * with your BIOS setup.  I have no access to machines with bi-directional
 * ports, so I can't test such a driver :-(
 * (Yes, I _know_ it is possible to use DE620 with bidirectional ports...)
 *
 * There are some clones of DE620 out there, with different names.
 * If the current driver does not recognize a clone, try to change
 * the following #define to:
 *
 * #define DE620_CLONE 1
 */
#define DE620_CLONE 0

/*
 * If the adapter has problems with high speeds, enable this #define
 * otherwise full printerport speed will be attempted.
 *
 * You can tune the READ_DELAY/WRITE_DELAY below if you enable LOWSPEED
 *
 #define LOWSPEED
 */

#ifndef READ_DELAY
#define READ_DELAY 100		/* adapter internal read delay in 100ns units */
#endif

#ifndef WRITE_DELAY
#define WRITE_DELAY 100		/* adapter internal write delay in 100ns units */
#endif

/*
 * Enable this #define if you want the adapter to do a "ifconfig down" on
 * itself when we have detected that something is possibly wrong with it.
 * The default behaviour is to retry with "adapter_init()" until success.
 * This should be used for debugging purposes only.
 *
 #define SHUTDOWN_WHEN_LOST
 */

/* use 0 for production, 1 for verification, >2 for debug
 */
#ifndef DE620_DEBUG
#define DE620_DEBUG 0
#endif

#ifdef LOWSPEED
/*
 * Enable this #define if you want to see debugging output that show how long
 * we have to wait before the DE-620 is ready for the next read/write/command.
 *
 #define COUNT_LOOPS
 */
#endif

#include "pmdrvr.h"
#include "de620.h"


/*
 * Definition of D-Link DE-620 Ethernet Pocket adapter
 * See also "de620.h"
 */
#ifndef DE620_IO		/* Compile-time configurable */
#define DE620_IO   0x378
#endif

#ifndef DE620_IRQ		/* Compile-time configurable */
#define DE620_IRQ  7
#endif

#define DATA_PORT    (dev->base_addr)
#define STATUS_PORT  (dev->base_addr + 1)
#define COMMAND_PORT (dev->base_addr + 2)

#if DE620_DEBUG			/* Compile-time configurable */
  #define PRINTK(x) if (de620_debug >= 2) printk x
#else
  #define PRINTK(x) ((void)0)
#endif

/*
 * Force media with insmod:
 *  insmod de620.o bnc=1
 * or
 *  insmod de620.o utp=1
 *
 * Force io and/or irq with insmod:
 *  insmod de620.o io=0x378 irq=7
 *
 * Make a clone skip the Ethernet-address range check:
 *  insmod de620.o clone=1
 */
static int bnc   = 0;
static int utp   = 0;
static int io    = DE620_IO;
static int irq   = DE620_IRQ;
static int clone = DE620_CLONE;

int de620_debug = DE620_DEBUG;

static int   de620_open (struct device *);
static void  de620_close (struct device *);
static void *get_stats (struct device *);
static void  de620_set_multicast_list (struct device *);
static int   de620_start_xmit (struct device *, const void *, int);
static void  de620_interrupt (int irq);
static int   de620_rx_intr (struct device *);

static int   adapter_init (struct device *);
static int   read_eeprom (struct device *);

/*
 * D-Link driver variables:
 */
#define SCR_DEF             NIBBLEMODE |INTON | SLEEP | AUTOTX
#define TCR_DEF             RXPB  /* not used: | TXSUCINT | T16INT */
#define DE620_RX_START_PAGE 12    /* 12 pages (=3k) reserved for tx */
#define DEF_NIC_CMD         IRQEN | ICEN | DS1

static volatile BYTE NIC_Cmd;
static volatile BYTE next_rx_page;

static BYTE first_rx_page;
static BYTE last_rx_page;
static BYTE EIPRegister;

static struct nic {
       BYTE  NodeID[6];
       BYTE  RAM_Size;
       BYTE  Model;
       BYTE  Media;
       BYTE  SCR;
     } nic_data;


/*
 * Convenience macros/functions for D-Link DE-620 adapter
 */
#define de620_tx_buffs(dd) (inb(STATUS_PORT) & (TXBF0 | TXBF1))
#define de620_flip_ds(dd)  NIC_Cmd ^= DS0 | DS1; outb(NIC_Cmd, COMMAND_PORT);

/* Check for ready-status, and return a nibble (high 4 bits) for data input
 */
#ifdef COUNT_LOOPS
static int tot_cnt;
#endif

static inline BYTE de620_ready (struct device *dev)
{
  BYTE  value;
  short int cnt = 0;

  while ((((value = inb (STATUS_PORT)) & READY) == 0) && (cnt <= 1000))
     ++cnt;

#ifdef COUNT_LOOPS
  tot_cnt += cnt;
#endif
  return (value & 0xf0);     /* nibble */
}

static inline void de620_send_command (struct device *dev, BYTE cmd)
{
  de620_ready (dev);
  if (cmd == W_DUMMY)
     outb (NIC_Cmd, COMMAND_PORT);

  outb (cmd, DATA_PORT);
  outb (NIC_Cmd ^ CS0, COMMAND_PORT);
  de620_ready (dev);
  outb (NIC_Cmd, COMMAND_PORT);
}

static inline void de620_put_byte (struct device *dev, BYTE value)
{
  /* The de620_ready() makes 7 loops, on the average, on a DX2/66
   */
  de620_ready (dev);
  outb (value, DATA_PORT);
  de620_flip_ds (dev);
}

static inline BYTE de620_read_byte (struct device *dev)
{
  BYTE value;

  /* The de620_ready() makes 7 loops, on the average, on a DX2/66
   */
  value = de620_ready (dev);       /* High nibble */
  de620_flip_ds (dev);
  value |= de620_ready (dev) >> 4; /* Low nibble */
  return (value);
}

static inline void de620_write_block (struct device *dev, BYTE *buffer, int count)
{
#ifndef LOWSPEED
  BYTE uflip = NIC_Cmd ^ (DS0 | DS1);
  BYTE dflip = NIC_Cmd;
#else
#ifdef COUNT_LOOPS
  int bytes = count;
#endif
#endif

#ifdef LOWSPEED
#ifdef COUNT_LOOPS
  tot_cnt = 0;
#endif

  /* No further optimization useful, the limit is in the adapter.
   */
  for ( ; count > 0; --count, ++buffer)
    de620_put_byte (dev, *buffer);

  de620_send_command (dev, W_DUMMY);

#ifdef COUNT_LOOPS
  /* trial debug output: loops per byte in de620_ready()
   */
  printk ("WRITE(%d)\n", tot_cnt / ((bytes ? bytes : 1)));
#endif
#else
  for ( ; count > 0; count -= 2)
  {
    outb (*buffer++, DATA_PORT);
    outb (uflip, COMMAND_PORT);
    outb (*buffer++, DATA_PORT);
    outb (dflip, COMMAND_PORT);
  }
  de620_send_command (dev, W_DUMMY);
#endif
}

static inline void de620_read_block (struct device *dev, BYTE *data, int count)
{
#ifndef LOWSPEED
  BYTE value;
  BYTE uflip = NIC_Cmd ^ (DS0 | DS1);
  BYTE dflip = NIC_Cmd;
#else
#ifdef COUNT_LOOPS
  int bytes = count;

  tot_cnt = 0;
#endif
#endif

#ifdef LOWSPEED
  /* No further optimization useful, the limit is in the adapter.
   */
  while (count-- > 0)
  {
    *data++ = de620_read_byte (dev);
    de620_flip_ds (dev);
  }

#ifdef COUNT_LOOPS
  /* trial debug output: loops per byte in de620_ready()
   */
  printk ("READ(%d)\n", tot_cnt / (2 * (bytes ? bytes : 1)));
#endif
#else
  while (count-- > 0)
  {
    value = inb (STATUS_PORT) & 0xf0;	/* High nibble */
    outb (uflip, COMMAND_PORT);
    *data++ = value | inb (STATUS_PORT) >> 4;	/* Low nibble */
    outb (dflip, COMMAND_PORT);
  }
#endif
}

static inline void de620_set_delay (struct device *dev)
{
  de620_ready (dev);
  outb (W_DFR, DATA_PORT);
  outb (NIC_Cmd ^ CS0, COMMAND_PORT);

  de620_ready (dev);
#ifdef LOWSPEED
  outb (WRITE_DELAY, DATA_PORT);
#else
  outb (0, DATA_PORT);
#endif
  de620_flip_ds (dev);

  de620_ready (dev);
#ifdef LOWSPEED
  outb (READ_DELAY, DATA_PORT);
#else
  outb (0, DATA_PORT);
#endif
  de620_flip_ds (dev);
}

static inline void de620_set_register (struct device *dev, BYTE reg, BYTE value)
{
  de620_ready (dev);
  outb (reg, DATA_PORT);
  outb (NIC_Cmd ^ CS0, COMMAND_PORT);
  de620_put_byte (dev, value);
}

static inline BYTE de620_get_register (struct device *dev, BYTE reg)
{
  BYTE value;

  de620_send_command (dev, reg);
  value = de620_read_byte (dev);
  de620_send_command (dev, W_DUMMY);
  return (value);
}

/*
 * Open/initialize the board.
 *
 * This routine should set everything up anew at each open, even
 * registers that "should" only need to be set once at boot, so that
 * there is a non-reboot way to recover if something goes wrong.
 */
static int de620_open (struct device *dev)
{
  if (!request_irq(dev->irq, de620_interrupt))
  {
    printk ("%s: unable to get IRQ %d\n", dev->name, dev->irq);
    return (0);
  }
  irq2dev_map[dev->irq] = dev;

  if (!adapter_init (dev))
     return (0);
  dev->start = 1;
  return (1);
}

/*
 * The inverse routine to de620_open().
 */
static void de620_close (struct device *dev)
{
  /* disable recv */
  de620_set_register (dev, W_TCR, RXOFF);

  free_irq (dev->irq);
  irq2dev_map[dev->irq] = NULL;
  dev->start = 0;
}

/*
 * Return current statistics
 */
static void *get_stats (struct device *dev)
{
  return (void*) dev->priv;
}

/*
 * Set or clear the multicast filter for this adaptor.
 * (no real multicast implemented for the DE-620, but she can be promiscuous...)
 */
static void de620_set_multicast_list (struct device *dev)
{
  if (dev->mc_count || dev->flags & (IFF_ALLMULTI | IFF_PROMISC))
  {				/* Enable promiscuous mode */
    /* We must make the kernel realise we had to move
     * into promisc mode or we start all out war on
     * the cable. - AC
     */
    dev->flags |= IFF_PROMISC;
    de620_set_register (dev, W_TCR, (TCR_DEF & ~RXPBM) | RXALL);
  }
  else    /* Disable promiscuous mode, use normal mode */
    de620_set_register (dev, W_TCR, TCR_DEF);
}

/*
 * Copy a buffer to the adapter transmit page memory.
 * Start sending.
 */
static int de620_start_xmit (struct device *dev, const void *buf, int len)
{
  int   tickssofar;
  BYTE *buffer = (BYTE*) buf;
  BYTE   using_txbuf = de620_tx_buffs (dev);   /* Peek at the adapter */

  dev->tx_busy = (using_txbuf == (TXBF0 | TXBF1));

  if (dev->tx_busy)
  {				/* Do timeouts, to avoid hangs. */
    tickssofar = jiffies - dev->tx_start;

    if (tickssofar < 5)
       return (1);

    printk ("%s: transmit timed out (%d), %s?\n",
            dev->name, tickssofar, "network cable problem");
    /* Restart the adapter.
     */
    if (!adapter_init (dev))	/* maybe close it */
       return (0);
  }

  if (len < ETH_MIN)
      len = ETH_MIN;
  if (len & 1)			/* send an even number of bytes */
    ++len;

  /* Start real output
   */
  DISABLE();
  PRINTK (("de620_start_xmit: len=%d, bufs 0x%02x\n", len, using_txbuf));

  /* select a free tx buffer. if there is one...
   */
  switch (using_txbuf)
  {
    default:         /* both are free: use TXBF0 */
    case TXBF1:              /* use TXBF0 */
	 de620_send_command (dev, W_CR | RW0);
	 using_txbuf |= TXBF0;
	 break;

    case TXBF0:              /* use TXBF1 */
	 de620_send_command (dev, W_CR | RW1);
	 using_txbuf |= TXBF1;
	 break;

    case (TXBF0 | TXBF1):    /* NONE!!! */
	 printk ("de620: Ouch! No tx-buffer available!\n");
         ENABLE();
	 return (0);

  }
  de620_write_block (dev, buffer, len);

  dev->tx_start = jiffies;
  dev->tx_busy  = (using_txbuf == (TXBF0 | TXBF1));

  ((struct net_device_stats *) (dev->priv))->tx_packets++;
  ENABLE();
  return (1);
}

/*
 * Handle the network interface interrupts.
 */
static void de620_interrupt (int irq_in)
{
  struct device *dev = irq2dev_map[irq_in];
  BYTE   irq_status;
  int    bogus_count = 0;
  int    again = 0;

  /* This might be deleted now, no crummy drivers present :-) Or..?
   */
  if (!dev || irq != irq_in)
  {
    printk ("%s: bogus interrupt %d\n", dev ? dev->name : "de620", irq_in);
    return;
  }

  dev->reentry = 1;

  /* Read the status register (_not_ the status port)
   */
  irq_status = de620_get_register (dev, R_STS);

  PRINTK (("de620_interrupt (%2.2X)\n", irq_status));

  if (irq_status & RXGOOD)
  {
    do
    {
      again = de620_rx_intr (dev);
      PRINTK (("again=%d\n", again));
    }
    while (again && (++bogus_count < 100));
  }

  dev->tx_busy = (de620_tx_buffs (dev) == (TXBF0 | TXBF1));	/* Boolean! */
  dev->reentry = 0;
}

/*
 * Get a packet from the adapter
 *
 * Send it "upstairs"
 */
static int de620_rx_intr (struct device *dev)
{
  struct header_buf {
         BYTE  status;
         BYTE  Rx_NextPage;
         WORD  Rx_ByteCount;
       } header_buf;
  BYTE  pagelink;
  BYTE  curr_page;
  short len;

  PRINTK (("de620_rx_intr: next_rx_page = %d\n", next_rx_page));

  /* Tell the adapter that we are going to read data, and from where
   */
  de620_send_command (dev, W_CR | RRN);
  de620_set_register (dev, W_RSA1, next_rx_page);
  de620_set_register (dev, W_RSA0, 0);

  /* Deep breath, and away we goooooo
   */
  de620_read_block (dev, (BYTE*)&header_buf, sizeof(struct header_buf));

  PRINTK (("page status=0x%02x, nextpage=%d, packetsize=%d\n",
           header_buf.status, header_buf.Rx_NextPage,
           header_buf.Rx_ByteCount));

  /* Plausible page header?
   */
  pagelink = header_buf.Rx_NextPage;
  if ((pagelink < first_rx_page) || (last_rx_page < pagelink))
  {
    /* Ouch... Forget it! Skip all and start afresh...
     */
    printk ("%s: Ring overrun? Restoring...\n", dev->name);

    /* You win some, you loose some. And sometimes plenty...
     */
    adapter_init (dev);
    ((struct net_device_stats *) (dev->priv))->rx_over_errors++;
    return (0);
  }

  /* OK, this look good, so far. Let's see if it's consistent...
   * Let's compute the start of the next packet, based on where we are
   */
  pagelink = next_rx_page + ((header_buf.Rx_ByteCount + (4 - 1 + 0x100)) >> 8);

  /* Are we going to wrap around the page counter?
   */
  if (pagelink > last_rx_page)
      pagelink -= (last_rx_page - first_rx_page + 1);

  /* Is the _computed_ next page number equal to what the adapter says?
   */
  if (pagelink != header_buf.Rx_NextPage)
  {
    /* Naah, we'll skip this packet. Probably bogus data as well
     */
    printk ("%s: Page link out of sync! Restoring...\n", dev->name);
    next_rx_page = header_buf.Rx_NextPage;	/* at least a try... */
    de620_send_command (dev, W_DUMMY);
    de620_set_register (dev, W_NPRF, next_rx_page);
    ((struct net_device_stats *) (dev->priv))->rx_over_errors++;
    return (0);
  }
  next_rx_page = pagelink;

  len = header_buf.Rx_ByteCount - 4;
  if (len < ETH_MIN || len > ETH_MAX)
     printk ("%s: Illegal packet size: %d!\n", dev->name, len);
  else
  {
    if (dev->get_rx_buf)
    {
      char *buf = (*dev->get_rx_buf) (len);
      if (buf)
      {
       /* copy the packet into the buffer
        */
        de620_read_block (dev, buf, len);
        PRINTK (("Read %d bytes\n", len));

        ((struct net_device_stats *) (dev->priv))->rx_packets++;
      }
      else
        ((struct net_device_stats *) (dev->priv))->rx_dropped++;
    }
  }

  /* Let's peek ahead to see if we have read the last current packet
   * NOTE! We're _not_ checking the 'EMPTY'-flag! This seems better...
   */
  curr_page = de620_get_register (dev, R_CPR);
  de620_set_register (dev, W_NPRF, next_rx_page);
  PRINTK (("next_rx_page=%d CPR=%d\n", next_rx_page, curr_page));

  return (next_rx_page != curr_page);	/* That was slightly tricky... */
}


/*
 * Reset the adapter to a known state
 */
static int adapter_init (struct device *dev)
{
  static int was_down = 0;
  int    i;

  if (nic_data.Model == 3 || nic_data.Model == 0)
  {                               /* CT */
    EIPRegister = NCTL0;
    if (nic_data.Media != 1)
       EIPRegister |= NIS0;       /* not BNC */
  }
  else if (nic_data.Model == 2)
    EIPRegister = NCTL0 | NIS0;   /* UTP */

  if (utp)
     EIPRegister = NCTL0 | NIS0;
  if (bnc)
     EIPRegister = NCTL0;

  de620_send_command (dev, W_CR | RNOP | CLEAR);
  de620_send_command (dev, W_CR | RNOP);

  de620_set_register (dev, W_SCR, SCR_DEF);

  /* disable recv to wait init
   */
  de620_set_register (dev, W_TCR, RXOFF);

  /* Set the node ID in the adapter
   */
  for (i = 0; i < ETH_ALEN; ++i)
    de620_set_register (dev, W_PAR0 + i, dev->dev_addr[i]);

  de620_set_register (dev, W_EIP, EIPRegister);

  next_rx_page = first_rx_page = DE620_RX_START_PAGE;
  if (nic_data.RAM_Size)
       last_rx_page = nic_data.RAM_Size - 1;
  else last_rx_page = 255; /* 64k RAM */

  de620_set_register (dev, W_SPR, first_rx_page);  /* Start Page Register */
  de620_set_register (dev, W_EPR, last_rx_page);   /* End Page Register */
  de620_set_register (dev, W_CPR, first_rx_page);  /* Current Page Register */
  de620_send_command (dev, W_NPR | first_rx_page); /* Next Page Register */
  de620_send_command (dev, W_DUMMY);
  de620_set_delay (dev);

  /* Final sanity check: Anybody out there?
   * Let's hope some bits from the statusregister make a good check
   */
#define CHECK_MASK (  0 | TXSUC |  T16  |  0  | RXCRC | RXSHORT |  0  |  0  )
#define CHECK_OK   (  0 |   0   |  0    |  0  |   0   |   0     |  0  |  0  )
  /* success:   X     0      0       X      0       0        X     X
   * ignore:   EEDI                RXGOOD                   COLS  LNKS
   */

  if (((i = de620_get_register (dev, R_STS)) & CHECK_MASK) != CHECK_OK)
  {
    printk ("Something has happened to the DE-620!  Please check it"
#ifdef SHUTDOWN_WHEN_LOST
	    " and do a new ifconfig"
#endif
	    "! (%02x)\n", i);
#ifdef SHUTDOWN_WHEN_LOST
    /* Goodbye, cruel world... */
    dev->flags &= ~IFF_UP;
    de620_close (dev);
#endif
    was_down = 1;
    return (0);
  }
  if (was_down)
  {
    printk ("Thanks, I feel much better now!\n");
    was_down = 0;
  }

  /* All OK, go ahead...
   */
  de620_set_register (dev, W_TCR, TCR_DEF);
  return (1);
}


/*
 * Only start-up code below
 *
 * Check if there is a DE-620 connected
 */
int de620_probe (struct device *dev)
{
  static struct net_device_stats de620_netstats;
  BYTE   checkbyte = 0xa5;
  int    i;

  /* This is where the base_addr and irq gets set.
   * Tunable at compile-time and insmod-time
   */
  dev->base_addr = io;
  dev->irq = irq;

  if (de620_debug)
     printk (version);

  printk ("D-Link DE-620 pocket adapter");

  /* Initially, configure basic nibble mode, so we can read the EEPROM
   */
  NIC_Cmd = DEF_NIC_CMD;
  de620_set_register (dev, W_EIP, EIPRegister);

  /* Anybody out there?
   */
  de620_set_register (dev, W_CPR, checkbyte);
  checkbyte = de620_get_register (dev, R_CPR);

  if (checkbyte != 0xA5 || !read_eeprom(dev))
  {
    printk (" not identified in the printer port\n");
    return (0);
  }

  printk (", Ethernet Address: %2.2X", dev->dev_addr[0] = nic_data.NodeID[0]);
  for (i = 1; i < ETH_ALEN; i++)
  {
    printk (":%2.2X", dev->dev_addr[i] = nic_data.NodeID[i]);
    dev->broadcast[i] = 0xff;
  }

  printk (" (%dk RAM,", (nic_data.RAM_Size) ? (nic_data.RAM_Size >> 2) : 64);

  if (nic_data.Media == 1)
       printk (" BNC)\n");
  else printk (" UTP)\n");

  /* Initialize the device structure.
   */
  dev->priv = &de620_netstats;
  memset (dev->priv, 0, sizeof (struct net_device_stats));

  dev->open  = de620_open;
  dev->close = de620_close;
  dev->xmit  = de620_start_xmit;
  dev->get_stats          = get_stats;
  dev->set_multicast_list = de620_set_multicast_list;
  ether_setup (dev);

  if (de620_debug)   /* dump eeprom */
  {
    printk ("\nEEPROM contents:\n");
    printk ("RAM_Size = 0x%02X\n", nic_data.RAM_Size);
    printk ("NodeID = %02X:%02X:%02X:%02X:%02X:%02X\n",
	    nic_data.NodeID[0], nic_data.NodeID[1],
	    nic_data.NodeID[2], nic_data.NodeID[3], nic_data.NodeID[4], nic_data.NodeID[5]);
    printk ("Model = %d\n", nic_data.Model);
    printk ("Media = %d\n", nic_data.Media);
    printk ("SCR = 0x%02x\n", nic_data.SCR);
  }
  return (1);
}

/*
 * Read info from on-board EEPROM
 *
 * Note: Bitwise serial I/O to/from the EEPROM vi the status _register_!
 */
#define sendit(dev,data) de620_set_register(dev, W_EIP, data | EIPRegister);

static WORD ReadAWord (struct device *dev, int from)
{
  WORD data;
  int  nbits;

  /* cs   [__~~] SET SEND STATE */
  /* di   [____]                */
  /* sck  [_~~_]                */
  sendit (dev, 0);
  sendit (dev, 1);
  sendit (dev, 5);
  sendit (dev, 4);

  /* Send the 9-bit address from where we want to read the 16-bit word
   */
  for (nbits = 9; nbits > 0; --nbits, from <<= 1)
  {
    if (from & 0x0100)
    {				/* bit set? */
      /* cs    [~~~~] SEND 1 */
      /* di    [~~~~]        */
      /* sck   [_~~_]        */
      sendit (dev, 6);
      sendit (dev, 7);
      sendit (dev, 7);
      sendit (dev, 6);
    }
    else
    {
      /* cs    [~~~~] SEND 0 */
      /* di    [____]        */
      /* sck   [_~~_]        */
      sendit (dev, 4);
      sendit (dev, 5);
      sendit (dev, 5);
      sendit (dev, 4);
    }
  }

  /* Shift in the 16-bit word. The bits appear serially in EEDI (=0x80)
   */
  for (data = 0, nbits = 16; nbits > 0; --nbits)
  {
    /* cs    [~~~~] SEND 0 */
    /* di    [____]        */
    /* sck   [_~~_]        */
    sendit (dev, 4);
    sendit (dev, 5);
    sendit (dev, 5);
    sendit (dev, 4);
    data = (data << 1) | ((de620_get_register (dev, R_STS) & EEDI) >> 7);
  }
  /* cs    [____] RESET SEND STATE */
  /* di    [____]                  */
  /* sck   [_~~_]                  */
  sendit (dev, 0);
  sendit (dev, 1);
  sendit (dev, 1);
  sendit (dev, 0);

  return (data);
}

static int read_eeprom (struct device *dev)
{
  WORD wrd;

  /* D-Link Ethernet addresses are in the series  00:80:c8:7X:XX:XX:XX
   */
  wrd = ReadAWord (dev, 0x1aa);	/* bytes 0 + 1 of NodeID */
  if (!clone && (wrd != htons (0x0080))) /* Valid D-Link ether sequence? */
     return (0);                         /* Nope, not a DE-620 */

  nic_data.NodeID[0] = wrd & 0xff;
  nic_data.NodeID[1] = wrd >> 8;

  wrd = ReadAWord (dev, 0x1ab);	/* bytes 2 + 3 of NodeID */
  if (!clone && ((wrd & 0xff) != 0xc8))  /* Valid D-Link ether sequence? */
     return (0);                         /* Nope, not a DE-620 */

  nic_data.NodeID[2] = wrd & 0xff;
  nic_data.NodeID[3] = wrd >> 8;

  wrd = ReadAWord (dev, 0x1ac);   /* bytes 4 + 5 of NodeID */
  nic_data.NodeID[4] = wrd & 0xff;
  nic_data.NodeID[5] = wrd >> 8;

  wrd = ReadAWord (dev, 0x1ad);   /* RAM size in pages (256 bytes). 0 = 64k */
  nic_data.RAM_Size = (wrd >> 8);

  wrd = ReadAWord (dev, 0x1ae);   /* hardware model (CT = 3) */
  nic_data.Model = (wrd & 0xff);

  wrd = ReadAWord (dev, 0x1af);   /* media (indicates BNC/UTP) */
  nic_data.Media = (wrd & 0xff);

  wrd = ReadAWord (dev, 0x1a8);   /* System Configuration Register */
  nic_data.SCR = (wrd >> 8);
  return (1);
}


