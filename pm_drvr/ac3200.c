/* ac3200.c: A driver for the Ansel Communications EISA ethernet adaptor. */
/*
 * Written 1993, 1994 by Donald Becker.
 * Copyright 1993 United States Government as represented by the Director,
 * National Security Agency.  This software may only be used and distributed
 * according to the terms of the GNU Public License as modified by SRC,
 * incorporated herein by reference.
 * 
 * The author may be reached as becker@cesdis.gsfc.nasa.gov, or
 * C/O Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
 * 
 * This is driver for the Ansel Communications Model 3200 EISA Ethernet LAN
 * Adapter.  The programming information is from the users manual, as related
 * by glee@ardnassak.math.clemson.edu.
 */

static const char *version = "ac3200.c:v1.01 7/1/94 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include "pmdrvr.h"
#include "8390.h"

/* Offsets from the base address.
 */
#define AC_NIC_BASE    0x00
#define AC_SA_PROM     0x16     /* The station address PROM. */
#define   AC_ADDR0     0x00     /* Prefix station address values. */
#define   AC_ADDR1     0x40     /* !!!!These are just guesses!!!! */
#define   AC_ADDR2     0x90
#define AC_ID_PORT     0xC80
#define AC_EISA_ID     0x0110d305
#define AC_RESET_PORT  0xC84
#define   AC_RESET     0x00
#define   AC_ENABLE    0x01
#define AC_CONFIG      0xC90    /* The configuration port. */

#define AC_IO_EXTENT 0x10       /* IS THIS REALLY TRUE ??? */
				/* Actually accessed is:
				 * * AC_NIC_BASE (0-15)
				 * * AC_SA_PROM (0-5)
				 * * AC_ID_PORT (0-3)
				 * * AC_RESET_PORT
				 * * AC_CONFIG
				 */

/* Decoding of the configuration register.
 */
static BYTE  config2irqmap[8] = { 15, 12, 11, 10, 9, 7, 5, 3 };
static int   addrmap[8]       = { 0xFF0000, 0xFE0000, 0xFD0000, 0xFFF0000,
                                  0xFFE0000, 0xFFC0000, 0xD0000, 0
                                };
static char *port_name[4] = { "10baseT", "invalid", "AUI", "10base2" };

#define config2irq(configval)   config2irqmap[((configval) >> 3) & 7]
#define config2mem(configval)   addrmap[(configval) & 7]
#define config2name(configval)  port_name[((configval) >> 6) & 3]

/* First and last 8390 pages.
 */
#define AC_START_PG  0x00    /* First page of 8390 TX buffer */
#define AC_STOP_PG   0x80    /* Last page +1 of the 8390 RX ring */

static int  ac_probe1 (int ioaddr, struct device *dev);
static int  ac_open (struct device *dev);
static void ac_reset_8390 (struct device *dev);
static void ac_block_input (struct device *dev, int count, char *buf, int ring_offset);
static void ac_block_output (struct device *dev, int count, char *buf, int start_page);
static void ac_get_8390_hdr (struct device *dev, struct e8390_pkt_hdr *hdr, int ring_page);
static void ac_close_card (struct device *dev);


/*  Probe for the AC3200.
 * 
 * The AC3200 can be identified by either the EISA configuration registers,
 * or the unique value in the station address PROM.
 */
int ac3200_probe (struct device *dev)
{
  WORD ioaddr = dev->base_addr;

  if (ioaddr > 0x1ff)		/* Check a single specified location. */
     return ac_probe1 (ioaddr, dev);

  if (ioaddr > 0)		/* Don't probe at all. */
     return (0);

  /* If you have a pre 0.99pl15 machine you should delete this line.
   */
  if (!EISA_bus)
     return (0);

  for (ioaddr = 0x1000; ioaddr < 0x9000; ioaddr += 0x1000)
    if (ac_probe1 (ioaddr, dev))
       return (1);
  return (0);
}

static int ac_probe1 (int ioaddr, struct device *dev)
{
  int i;

  printk ("AC3200 ethercard probe at %#3x:", ioaddr);

  for (i = 0; i < ETH_ALEN; i++)
     printk (" %02x", inb (ioaddr + AC_SA_PROM + i));

  /* !!!!The values of AC_ADDRn (see above) should be corrected when we
   * find out the correct station address prefix!!!!
   */
  if (inb (ioaddr + AC_SA_PROM + 0) != AC_ADDR0 ||
      inb (ioaddr + AC_SA_PROM + 1) != AC_ADDR1 ||
      inb (ioaddr + AC_SA_PROM + 2) != AC_ADDR2)
  {
    printk (" not found (invalid prefix).\n");
    return (0);
  }

  /* The correct probe method is to check the EISA ID.
   */
  for (i = 0; i < 4; i++)
    if (inl (ioaddr + AC_ID_PORT) != AC_EISA_ID)
    {
      printk ("EISA ID mismatch, %8x vs %8x.\n",
              (unsigned) inl(ioaddr+AC_ID_PORT), AC_EISA_ID);
      return (0);
    }

  /* We should have a "dev" from Space.c or the static module table.
   */
  if (!dev)
  {
    printk ("ac3200.c: Passed a NULL device.\n");
    dev = init_etherdev (0, 0);
  }

  for (i = 0; i < ETH_ALEN; i++)
     dev->dev_addr[i] = inb (ioaddr + AC_SA_PROM + i);

  printk ("\nAC3200 ethercard configuration register is %#02x,"
	  " EISA ID %02x %02x %02x %02x.\n", inb (ioaddr + AC_CONFIG),
	  inb (ioaddr + AC_ID_PORT + 0), inb (ioaddr + AC_ID_PORT + 1),
	  inb (ioaddr + AC_ID_PORT + 2), inb (ioaddr + AC_ID_PORT + 3));

  /* Assign and allocate the interrupt now.
   */
  if (dev->irq == 0)
       dev->irq = config2irq (inb (ioaddr + AC_CONFIG));
  else if (dev->irq == 2)
       dev->irq = 9;

  if (!request_irq (dev->irq, ei_interrupt))
  {
    printk (" unable to get IRQ %d.\n", dev->irq);
    return (0);
  }

  /* Allocate dev->priv and fill in 8390 specific dev fields.
   */
  if (ethdev_init (dev))
  {
    printk (" unable to allocate memory for dev->priv.\n");
    free_irq (dev->irq);
    return (0);
  }

  dev->base_addr = ioaddr;

#ifdef notyet
  if (dev->mem_start)
  {				/* Override the value from the board. */
    for (i = 0; i < 7; i++)
       if (addrmap[i] == dev->mem_start)
          break;
    if (i >= 7)
        i = 0;
    outb ((inb (ioaddr + AC_CONFIG) & ~7) | i, ioaddr + AC_CONFIG);
  }
#endif

  dev->if_port    = inb (ioaddr + AC_CONFIG) >> 6;
  dev->mem_start  = config2mem (inb (ioaddr + AC_CONFIG));
  dev->rmem_start = dev->mem_start + TX_PAGES * 256;
  dev->mem_end    = dev->rmem_end = dev->mem_start + (AC_STOP_PG - AC_START_PG) * 256;

  ei_status.name = "AC3200";
  ei_status.tx_start_page = AC_START_PG;
  ei_status.rx_start_page = AC_START_PG + TX_PAGES;
  ei_status.stop_page = AC_STOP_PG;
  ei_status.word16    = 1;

  printk ("\n%s: AC3200 at %3x, IRQ %d, %s port, shared memory %08x-%08x.\n",
          dev->name, ioaddr, dev->irq, port_name[dev->if_port],
          (unsigned)dev->mem_start, (unsigned)dev->mem_end - 1);

  if (ei_debug > 0)
     printk (version);

  ei_status.reset_8390   = ac_reset_8390;
  ei_status.block_input  = ac_block_input;
  ei_status.block_output = ac_block_output;
  ei_status.get_8390_hdr = ac_get_8390_hdr;

  dev->open  = ac_open;
  dev->close = ac_close_card;
  NS8390_init (dev, 0);
  return (1);
}

static int ac_open (struct device *dev)
{
#ifdef notyet
  /* Someday we may enable the IRQ and shared memory here.
   */
  int ioaddr = dev->base_addr;

  if (!request_irq (dev->irq, ei_interrupt))
     return (0);
#endif

  ei_open (dev);
  return (1);
}

static void ac_reset_8390 (struct device *dev)
{
  WORD ioaddr = dev->base_addr;

  outb (AC_RESET, ioaddr + AC_RESET_PORT);
  if (ei_debug > 1)
     printk ("resetting AC3200, t=%u...", (unsigned)jiffies);

  ei_status.txing = 0;
  outb (AC_ENABLE, ioaddr + AC_RESET_PORT);
  if (ei_debug > 1)
     printk ("reset done\n");
}

/*
 * Grab the 8390 specific header. Similar to the block_input routine, but
 * we don't need to be concerned with ring wrap as the header will be at
 * the start of a page, so we optimize accordingly.
 */
static void ac_get_8390_hdr (struct device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{
  DWORD hdr_start = dev->mem_start + ((ring_page - AC_START_PG) << 8);
  memcpy_fromio (hdr, hdr_start, sizeof (struct e8390_pkt_hdr));
}

/*
 * Block input and output are easy on shared memory ethercards, the only
 * complication is when the ring buffer wraps.
 */
static void ac_block_input (struct device *dev, int count, char *buf, int ring_offset)
{
  DWORD xfer_start = dev->mem_start + ring_offset - (AC_START_PG << 8);

  if (xfer_start + count > dev->rmem_end)
  {
    /* We must wrap the input move. */
    int semi_count = dev->rmem_end - xfer_start;

    memcpy_fromio (buf, xfer_start, semi_count);
    count -= semi_count;
    memcpy_fromio (buf + semi_count, dev->rmem_start, count);
  }
  else
  {
    /* Packet is in one chunk -- we can copy + cksum. */
#if 0
    eth_io_copy_and_sum (buf, xfer_start, count, 0);
#else
    memcpy_fromio (buf, xfer_start, count);
#endif
  }
}

static void ac_block_output (struct device *dev, int count, char *buf, int start_page)
{
  DWORD shmem = dev->mem_start + ((start_page - AC_START_PG) << 8);

  memcpy_toio (shmem, buf, count);
}

static void ac_close_card (struct device *dev)
{
  dev->start = 0;
  dev->tx_busy = 1;

  if (ei_debug > 1)
    printk ("%s: Shutting down ethercard.\n", dev->name);

#ifdef notyet
  /* We should someday disable shared memory and interrupts. */
  outb (0x00, ioaddr + 6);	/* Disable interrupts. */
#endif

  free_irq (dev->irq);
  irq2dev_map[dev->irq] = NULL;
  ei_close (dev);
}
