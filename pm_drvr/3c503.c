/* 3c503.c: A shared-memory NS8390 ethernet driver for linux. */
/*
 *  Written 1992-94 by Donald Becker.
 *
 *  Copyright 1993 United States Government as represented by the
 *  Director, National Security Agency.  This software may be used and
 *  distributed according to the terms of the GNU Public License,
 *  incorporated herein by reference.
 *
 *  The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
 *  Center of Excellence in Space Data and Information Sciences
 *     Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
 *
 *  This driver should work with the 3c503 and 3c503/16.  It should be used
 *  in shared memory mode for best performance, although it may also work
 *  in programmed-I/O mode.
 *
 *  Sources:
 *  EtherLink II Technical Reference Manual,
 *  EtherLink II/16 Technical Reference Manual Supplement,
 *  3Com Corporation, 5400 Bayfront Plaza, Santa Clara CA 95052-8145
 *
 *  The Crynwr 3c503 packet driver.
 *
 *  Changelog:
 *
 *  Paul Gortmaker  : add support for the 2nd 8kB of RAM on 16 bit cards.
 *  Paul Gortmaker  : multiple card support for module users.
 *  rjohnson@analogic.com : Fix up PIO interface for efficient operation.
 *
 */

#include "pmdrvr.h"
#include "8390.h"
#include "3c503.h"

/* A zero-terminated list of I/O addresses to be probed in PIO mode.
 */
static WORD netcard_portlist[] = { 0x300, 0x310, 0x330, 0x350,
                                   0x250, 0x280, 0x2a0, 0x2e0, 0
                                 };
#define EL2_IO_EXTENT  16
#define BNC_AUI()      (ei_status.interface_num ? ECNTRL_AUI : ECNTRL_THIN)

static int  el2_probe1      (struct device *dev, int ioaddr);
static int  el2_open        (struct device *dev);
static void el2_close       (struct device *dev);
static void el2_reset_8390  (struct device *dev);
static void el2_init_card   (struct device *dev);
static void el2_block_output(struct device *dev, int count, const char *buf, int start_page);
static void el2_block_input (struct device *dev, int count, char *data,int ring_offset);
static void el2_get_8390_hdr(struct device *dev, struct e8390_pkt_hdr *hdr, int ring_page);

/*
 * This routine probes for a memory-mapped 3c503 board by looking for
 * the "location register" at the end of the jumpered boot PROM space.
 * This works even if a PROM isn't there.
 *
 * If the ethercard isn't found there is an optional probe for
 * ethercard jumpered to programmed-I/O mode.
 */
int el2_probe (struct device *dev)
{
  DWORD *addr, addrs[] = { 0xDDFFE, 0xD9FFE, 0xCDFFE, 0xC9FFE, 0 };
  DWORD  base_addr = dev->base_addr;

  if (base_addr > 0x1ff)  /* Check a single specified location. */
     return el2_probe1 (dev, base_addr);

  if (base_addr != 0)     /* Don't probe at all. */
     return (0);

  for (addr = addrs; *addr; addr++)
  {
    int   i;
    DWORD base_bits = readb (*addr);

    /* Find first set bit. */
    for (i = 7; i >= 0; i--, base_bits >>= 1)
        if (base_bits & 1)
           break;
    if (base_bits != 1)
       continue;
    if (el2_probe1(dev, netcard_portlist[i]))
       return (1);
  }
  return (0);
}


/*
 * Probe for the Etherlink II card at I/O port base IOADDR,
 * returning non-zero on success.  If found, set the station
 * address and memory parameters in DEVICE.
 */
static int el2_probe1 (struct device *dev, int ioaddr)
{
  int   i, iobase_reg, membase_reg, saved_406, wordlength;
  DWORD vendor_id;

  /* Reset and/or avoid any lurking NE2000
   */
  if (inb(ioaddr + 0x408) == 0xFF)
  {
    delay (1);
    return (0);
  }

  /* We verify that it's a 3C503 board by checking the first three octets
   * of its ethernet address.
   */
  iobase_reg  = inb (ioaddr+0x403);
  membase_reg = inb (ioaddr+0x404);

  /* ASIC location registers should be 0 or have only a single bit set.
   */
  if ((iobase_reg & (iobase_reg - 1)) || (membase_reg & (membase_reg - 1)))
     return (0);

  saved_406 = inb (ioaddr + 0x406);
  outb (ECNTRL_RESET|ECNTRL_THIN, ioaddr+0x406); /* Reset it... */
  outb (ECNTRL_THIN,              ioaddr+0x406);

  /* Map the station addr PROM into the lower I/O ports. We now check
   * for both the old and new 3Com prefix
   */
  outb (ECNTRL_SAPROM | ECNTRL_THIN, ioaddr + 0x406);
  vendor_id = 0x10000 * inb (ioaddr)   +
                0x100 * inb (ioaddr+1) +
                        inb (ioaddr+2);

  if ((vendor_id != OLD_3COM_ID) && (vendor_id != NEW_3COM_ID))
  {
    /* Restore the register we frobbed. */
    outb (saved_406, ioaddr + 0x406);
    return (0);
  }

  dev->base_addr = ioaddr;

  /* Allocate dev->priv and fill in 8390 specific dev fields
   */
  ethdev_init (dev);
  printk ("%s: 3c503 at i/o base %3x, node ", dev->name, ioaddr);

  /* Retrieve and print the ethernet address
   */
  for (i = 0; i < ETH_ALEN; i++)
      printk (" %02x", dev->dev_addr[i] = inb(ioaddr + i));

  /* Map the 8390 back into the window
   */
  outb (ECNTRL_THIN, ioaddr + 0x406);

  /* Check for EL2/16 as described in tech. man
   */
  outb (E8390_PAGE0, ioaddr + E8390_CMD);
  outb (0,           ioaddr + EN0_DCFG);
  outb (E8390_PAGE2, ioaddr + E8390_CMD);
  wordlength = inb (ioaddr + EN0_DCFG) & ENDCFG_WTS;
  outb (E8390_PAGE0, ioaddr + E8390_CMD);

  /* Probe for, turn on and clear the board's shared memory
   */
  if (ei_debug > 2)
     printk (" memory jumpers %02x ", membase_reg);
  outb (EGACFR_NORM, ioaddr + 0x405);  /* Enable RAM */

  /*
   * This should be probed for (or set via an ioctl()) at run-time.
   * Right now we use a sleazy hack to pass in the interface number
   * at boot-time via the low bits of the mem_end field.  That value is
   * unused, and the low bits would be discarded even if it was used.
   */
  ei_status.interface_num = 1;
  printk (", using %sternal tranceiver.\n",
          ei_status.interface_num ? "ex" : "in");

  if ((membase_reg & 0xf0) == 0)
  {
    dev->mem_start = 0;
    ei_status.name = "3c503-PIO";
  }
  else
  {
    dev->mem_start = ((membase_reg & 0xC0) ? 0xD8000 : 0xC8000) +
                     ((membase_reg & 0xA0) ? 0x4000 : 0);

#define EL2_MEMSIZE (EL2_MB1_STOP_PG - EL2_MB1_START_PG)*256
#ifdef EL2MEMTEST
  /*
   * Check the card's memory.
   * This has never found an error, but someone might care.
   * Note that it only tests the 2nd 8kB on 16kB 3c503/16
   * cards between card addr. 0x2000 and 0x3fff.
   */
    { 
      DWORD mem_base = dev->mem_start;
      DWORD test_val = 0xbbadf00d;

      writel (0xBA5EBA5E, mem_base);
      for (i = sizeof(test_val); i < EL2_MEMSIZE; i+=sizeof(test_val))
      {
        writel (test_val, mem_base + i);
        if (readl(mem_base) != 0xba5eba5e || readl(mem_base + i) != test_val)
        {
          printk ("3c503: memory failure or memory address conflict.\n");
          dev->mem_start = 0;
          ei_status.name = "3c503-PIO";
          break;
        }
        test_val += 0x55555555;
        writel (0, mem_base + i);
      }
    }
#endif  /* EL2MEMTEST */

    dev->mem_end = dev->rmem_end = dev->mem_start + EL2_MEMSIZE;

    if (wordlength)   /* No Tx pages to skip over to get to Rx */
    {
      dev->rmem_start = dev->mem_start;
      ei_status.name = "3c503/16";
    }
    else
    {
      dev->rmem_start = TX_PAGES*256 + dev->mem_start;
      ei_status.name = "3c503";
    }
  }

  /*
   * Divide up the memory on the card. This is the same regardless of
   * whether shared-mem or PIO is used. For 16 bit cards (16kB RAM),
   * we use the entire 8k of bank1 for an Rx ring. We only use 3k
   * of the bank0 for 2 full size Tx packet slots. For 8 bit cards,
   * (8kB RAM) we use 3kB of bank1 for two Tx slots, and the remaining
   * 5kB for an Rx ring.
   */

  if (wordlength)
  {
    ei_status.tx_start_page = EL2_MB0_START_PG;
    ei_status.rx_start_page = EL2_MB1_START_PG;
  }
  else
  {
    ei_status.tx_start_page = EL2_MB1_START_PG;
    ei_status.rx_start_page = EL2_MB1_START_PG + TX_PAGES;
  }

  /* Finish setting the board's parameters
   */
  ei_status.stop_page    = EL2_MB1_STOP_PG;
  ei_status.word16       = wordlength;
  ei_status.reset_8390   = el2_reset_8390;
  ei_status.get_8390_hdr = el2_get_8390_hdr;
  ei_status.block_input  = el2_block_input;
  ei_status.block_output = el2_block_output;

  if (dev->irq == 2)
      dev->irq = 9;
  else if (dev->irq > 5 && dev->irq != 9)
  {
    printk ("3c503: configured interrupt %d invalid, will use autoIRQ.\n",
            dev->irq);
    dev->irq = 0;
  }

  ei_status.saved_irq = dev->irq;

  dev->open  = el2_open;
  dev->close = el2_close;

  if (dev->mem_start)
     printk ("%s: %s - %dkB RAM, 8kB shared mem window at %06x-%06x.\n",
             dev->name, ei_status.name, (wordlength+1)<<3,
             (unsigned)dev->mem_start, (unsigned)dev->mem_end-1);
  else
  {
    ei_status.tx_start_page = EL2_MB1_START_PG;
    ei_status.rx_start_page = EL2_MB1_START_PG + TX_PAGES;
    printk ("\n%s: %s, %dkB RAM, using programmed I/O (REJUMPER for SHARED MEMORY).\n",
            dev->name, ei_status.name, (wordlength+1)<<3);
  }
  return (1);
}

static int el2_open (struct device *dev)
{
  if (dev->irq < 2)
  {
    int  irqlist[] = { 5, 9, 3, 4, 0 };
    int *irqp      = irqlist;

    outb (EGACFR_NORM,E33G_GACFR);  /* Enable RAM and interrupts */
    do
    {
      irq2dev_map [dev->irq] = dev;
      if (request_irq (*irqp,NULL))
      {
        /* Twinkle the interrupt, and check if it's seen
         */
        autoirq_setup (0);
        outb (4 << ((*irqp == 9) ? 2 : *irqp), E33G_IDCFR);
        outb (0,E33G_IDCFR);
        dev->irq = *irqp;
        if (*irqp == autoirq_report(0) &&       /* It's a good IRQ line! */
           request_irq(dev->irq,&ei_interrupt)) /* set our IRQ handler */
          break;
        irq2dev_map [dev->irq] = NULL;
      }
    }
    while (*++irqp);

    if (*irqp == 0)
    {
      outb (EGACFR_IRQOFF, E33G_GACFR);  /* disable interrupts. */
      return (0);
    }
  }
  else
  {
    irq2dev_map[dev->irq] = dev;
    if (!request_irq(dev->irq,&ei_interrupt))
    {
      irq2dev_map[dev->irq] = NULL;
      return (0);
    }
  }
  el2_init_card (dev);
  ei_open (dev);
  return (1);
}

static void el2_close (struct device *dev)
{
  free_irq (dev->irq);
  dev->irq = ei_status.saved_irq;
  outb (EGACFR_IRQOFF, E33G_GACFR);  /* disable interrupts */

  ei_close (dev);
}

/*
 * This is called whenever we have a unrecoverable failure:
 *   transmit timeout
 *   Bad ring buffer packet header
 */
static void el2_reset_8390 (struct device *dev)
{
  if (ei_debug > 1)
  {
    printk ("%s: Resetting the 3c503 board...", dev->name);
    printk ("%lx=%02x %lx=%02x %lx=%02x...",
            E33G_IDCFR, inb(E33G_IDCFR),
            E33G_CNTRL, inb(E33G_CNTRL),
            E33G_GACFR, inb(E33G_GACFR));
  }
  outb (ECNTRL_RESET|ECNTRL_THIN, E33G_CNTRL);
  ei_status.txing = 0;
  outb (ei_status.interface_num ? ECNTRL_AUI : ECNTRL_THIN, E33G_CNTRL);
  el2_init_card (dev);
  if (ei_debug > 1)
     printk ("done\n");
}

/*
 * Initialize the 3c503 GA registers after a reset.
 */
static void el2_init_card (struct device *dev)
{
  /* Unmap the station PROM and select the DIX or BNC connector
   */
  outb (ei_status.interface_num == 0 ? ECNTRL_THIN : ECNTRL_AUI, E33G_CNTRL);

  /* Set ASIC copy of rx's first and last+1 buffer pages
   * These must be the same as in the 8390
   */
  outb (ei_status.rx_start_page,E33G_STARTPG);
  outb (ei_status.stop_page,    E33G_STOPPG);

  /* Point the vector pointer registers somewhere ?harmless?
   */
  outb (0xff,E33G_VP2);  /* Point at the ROM restart location 0xffff0 */
  outb (0xff,E33G_VP1);
  outb (0x00,E33G_VP0);

  /* Turn off all interrupts until we're opened
   */
  outb (0, dev->base_addr + EN0_IMR);

  /* Enable IRQs iff started
   */
  outb (EGACFR_NORM,E33G_GACFR);

  /* Set the interrupt line
   */
  outb ((0x04 << (dev->irq == 9 ? 2 : dev->irq)), E33G_IDCFR);
  outb (WRD_COUNT << 1, E33G_DRQCNT);  /* Set burst size to 8 */
  outb (0x20, E33G_DMAAH);             /* Put a valid addr in the GA DMA */
  outb (0x00, E33G_DMAAL);
}

/*
 * Either use the shared memory (if enabled on the board) or put the packet
 * out through the ASIC FIFO.
 */
static void el2_block_output (struct device *dev, int count,
                              const char *buf, const int start_page)
{
  WORD *wrd;
  WORD  word;         /* temporary for better machine code */
  int   boguscount;   /* timeout counter */

  if (ei_status.word16)    /* Tx packets go into bank 0 on EL2/16 card */
       outb (EGACFR_RSEL | EGACFR_TCM, E33G_GACFR);
  else outb (EGACFR_NORM, E33G_GACFR);

  if (dev->mem_start)   /* Shared memory transfer */
  {
    DWORD dest_addr = dev->mem_start +
                     ((start_page - ei_status.tx_start_page) << 8);
    memcpy_toio (dest_addr, buf, count);
    outb (EGACFR_NORM,E33G_GACFR);  /* Back to bank1 in case on bank0 */
    return;
  }

 /* No shared memory, put the packet out the other way.
  * Set up then start the internal memory transfer to Tx Start Page
  */
  word = (WORD)start_page;
  outb (loBYTE(word),E33G_DMAAH);
  outb (hiBYTE(word),E33G_DMAAL);

  outb (BNC_AUI() | ECNTRL_OUTPUT | ECNTRL_START, E33G_CNTRL);

 /* Here I am going to write data to the FIFO as quickly as possible.
  * Note that E33G_FIFOH is defined incorrectly. It is really
  * E33G_FIFOL, the lowest port address for both the byte and
  * word write. Variable 'count' is NOT checked. Caller must supply a
  * valid count. Note that I may write a harmless extra byte to the
  * 8390 if the byte-count was not even.
  */
  wrd = (WORD*)buf;
  count  = (count + 1) >> 1;
  for (;;)
  {
    boguscount = 0x1000;
    while ((inb(E33G_STATUS) & ESTAT_DPRDY) == 0)
    {
      if (!boguscount--)
      {
        printk ("%s: FIFO blocked in el2_block_output.\n", dev->name);
        el2_reset_8390 (dev);
        goto blocked;
      }
    }
    if (count > WRD_COUNT)
    {
      rep_outsw (E33G_FIFOH, wrd, WRD_COUNT);
      wrd   += WRD_COUNT;
      count -= WRD_COUNT;
    }
    else
    {
      rep_outsw (E33G_FIFOH, wrd, count);
      break;
    }
  }
blocked:
  outb (BNC_AUI(),E33G_CNTRL);
}

/*
 * Read the 4 byte, page aligned 8390 specific header.
 */
static void el2_get_8390_hdr (struct device        *dev,
                              struct e8390_pkt_hdr *hdr,
                              int ring_page)
{
  int   boguscount;
  DWORD hdr_start = dev->mem_start + ((ring_page - EL2_MB1_START_PG) << 8);
  WORD  word;

  if (dev->mem_start)        /* Use the shared memory. */
  {
    memcpy_fromio (hdr, hdr_start, sizeof(struct e8390_pkt_hdr));
    return;
  }

/*
 *  No shared memory, use programmed I/O.
 */

  word = (WORD)ring_page;
  outb (loBYTE(word),E33G_DMAAH);
  outb (hiBYTE(word),E33G_DMAAL);

  outb (BNC_AUI() | ECNTRL_INPUT | ECNTRL_START, E33G_CNTRL);
  boguscount = 0x1000;
  while ((inb(E33G_STATUS) & ESTAT_DPRDY) == 0)
  {
    if (!boguscount--)
    {
      printk ("%s: FIFO blocked in el2_get_8390_hdr.\n", dev->name);
      memset (hdr, 0x00, sizeof(struct e8390_pkt_hdr));
      el2_reset_8390 (dev);
      goto blocked;
    }
  }
  rep_insw (E33G_FIFOH, (WORD*)hdr, sizeof(*hdr)/2);
blocked:
  outb (BNC_AUI(),E33G_CNTRL);
}


static void el2_block_input (struct device *dev, int count,
                             char *data, int ring_offset)
{
  WORD *buf;
  WORD  word;
  int   boguscount = 0;
  int   end_of_ring = dev->rmem_end;

  if (dev->mem_start)   /* Use the shared memory. */
  {
    ring_offset -= (EL2_MB1_START_PG << 8);
    if (dev->mem_start + ring_offset + count > end_of_ring)
    {
      /* We must wrap the input move
       */
      int semi_count = end_of_ring - (dev->mem_start + ring_offset);

      memcpy_fromio (data, dev->mem_start + ring_offset, semi_count);
      count -= semi_count;
      memcpy_fromio (data + semi_count, dev->rmem_start, count);
    }
    else
    {
      /* Packet is in one chunk -- we can copy + cksum. */
      memcpy_fromio (dev->mem_start + ring_offset, data, count);
    }
    return;
  }

  /*
   *  No shared memory, use programmed I/O.
   */
  word = (WORD) ring_offset;
  outb (hiBYTE(word), E33G_DMAAH);
  outb (loBYTE(word), E33G_DMAAL);
  outb (BNC_AUI() | ECNTRL_INPUT | ECNTRL_START, E33G_CNTRL);

  /*
   *  Here I also try to get data as fast as possible. I am betting that I
   *  can read one extra byte without clobbering anything in the kernel because
   *  this would only occur on an odd byte-count and allocation of skb->data
   *  is word-aligned. Variable 'count' is NOT checked. Caller must check
   *  for a valid count.
   *  [This is currently quite safe.... but if one day the 3c503 explodes
   *  you know where to come looking ;)]
   */
  buf   = (WORD*) data;
  count = (count + 1) >> 1;
  for (;;)
  {
    boguscount = 0x1000;
    while ((inb(E33G_STATUS) & ESTAT_DPRDY) == 0)
    {
      if (!boguscount--)
      {
        printk ("%s: FIFO blocked in el2_block_input.\n", dev->name);
        el2_reset_8390 (dev);
        goto blocked;
      }
    }
    if (count > WRD_COUNT)
    {
      rep_insw (E33G_FIFOH, buf, WRD_COUNT);
      buf   += WRD_COUNT;
      count -= WRD_COUNT;
    }
    else
    {
      rep_insw (E33G_FIFOH, buf, count);
      break;
    }
  }
blocked:
  outb (BNC_AUI(),E33G_CNTRL);
}

#ifdef __DLX__

#define SYSTEM_ID   ASCII ('_','W','A','T','T','3','2','_')
#define DRIVER_ID   ASCII ('3','C','5','0','3', 0,0,0)
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

