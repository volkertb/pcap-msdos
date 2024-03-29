/* smc-ultra.c: A SMC Ultra ethernet driver for linux. */
/*
 * This is a driver for the SMC Ultra and SMC EtherEZ ISA ethercards.
 * 
 * Written 1993-1996 by Donald Becker.
 * 
 * Copyright 1993 United States Government as represented by the
 * Director, National Security Agency.
 * 
 * This software may be used and distributed according to the terms
 * of the GNU Public License, incorporated herein by reference.
 * 
 * The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
 * Center of Excellence in Space Data and Information Sciences
 * Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
 * 
 * This driver uses the cards in the 8390-compatible, shared memory mode.
 * Most of the run-time complexity is handled by the generic code in
 * 8390.c.  The code in this file is responsible for
 * 
 * ultra_probe()     Detecting and initializing the card.
 * ultra_probe1()
 * 
 * ultra_open()    The card-specific details of starting, stopping
 * ultra_reset_8390()  and resetting the 8390 NIC core.
 * ultra_close()
 * 
 * ultra_block_input()    Routines for reading and writing blocks of
 * ultra_block_output()  packet buffer memory.
 * 
 * This driver enables the shared memory only when doing the actual data
 * transfers to avoid a bug in early version of the card that corrupted
 * data transferred by a AHA1542.
 * 
 * This driver now supports the programmed-I/O (PIO) data transfer mode of
 * the EtherEZ. It does not use the non-8390-compatible "Altego" mode.
 * That support (if available) is smc-ez.c.
 * 
 * Changelog:
 * 
 * Paul Gortmaker  : multiple card support for module users.
 * Donald Becker  : 4/17/96 PIO support, minor potential problems avoided.
 * Donald Becker  : 6/6/96 correctly set auto-wrap bit.
 */

static const char *version = "smc-ultra.c:v2.00 6/6/96 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include "pmdrvr.h"
#include "8390.h"

/* A zero-terminated list of I/O addresses to be probed. */
static DWORD ultra_portlist[] = { 0x200, 0x220, 0x240, 0x280, 0x300, 0x340, 0x380, 0 };

static int  ultra_probe1 (struct device *dev, int ioaddr);
static int  ultra_open (struct device *dev);
static void ultra_reset_8390 (struct device *dev);
static void ultra_get_8390_hdr (struct device *dev, struct e8390_pkt_hdr *hdr, int ring_page);
static void ultra_block_input (struct device *dev, int count, struct sk_buff *skb, int ring_offset);
static void ultra_block_output (struct device *dev, int count, BYTE * buf, start_page);
static void ultra_pio_get_hdr (struct device *dev, struct e8390_pkt_hdr *hdr, int ring_page);
static void ultra_pio_input (struct device *dev, int count, struct sk_buff *skb, int ring_offset);
static void ultra_pio_output (struct device *dev, int count, BYTE * buf, start_page);
static void ultra_close_card (struct device *dev);


#define START_PG         0x00    /* First page of TX buffer */

#define ULTRA_CMDREG     0       /* Offset to ASIC command register. */
#define ULTRA_RESET      0x80    /* Board reset, in ULTRA_CMDREG. */
#define ULTRA_MEMENB     0x40    /* Enable the shared memory. */
#define IOPD             0x02    /* I/O Pipe Data (16 bits), PIO operation. */
#define IOPA             0x07    /* I/O Pipe Address for PIO operation. */
#define ULTRA_NIC_OFFSET 16      /* NIC register offset from the base_addr. */
#define ULTRA_IO_EXTENT  32
#define EN0_ERWCNT       0x08    /* Early receive warning count. */

/*
 * Probe for the Ultra.  This looks like a 8013 with the station
 * address PROM at I/O ports <base>+8 to <base>+13, with a checksum
 * following.
 */
int ultra_probe (struct device *dev)
{
  int i, base_addr = dev ? dev->base_addr : 0;

  if (base_addr > 0x1ff)	/* Check a single specified location. */
     return ultra_probe1 (dev, base_addr);

  if (base_addr)		/* Don't probe at all. */
     return (0);

  for (i = 0; ultra_portlist[i]; i++)
  {
    int ioaddr = ultra_portlist[i];
    if (ultra_probe1 (dev, ioaddr))
      return (1);
  }
  return (0);
}

static int ultra_probe1 (struct device *dev, int ioaddr)
{
  static int version_printed = 0;
  int    i, checksum = 0;
  char  *model_name;
  BYTE   eeprom_irq = 0;

  /* Values from various config regs.
   */
  BYTE num_pages, irqreg, addr, piomode;
  BYTE idreg = inb (ioaddr + 7);
  BYTE reg4 = inb (ioaddr + 4) & 0x7f;

  /* Check the ID nibble.
   */
  if ((idreg & 0xF0) != 0x20 &&   /* SMC Ultra */
      (idreg & 0xF0) != 0x40)     /* SMC EtherEZ */
    return 0;

  /* Select the station address register set.
   */
  outb (reg4, ioaddr + 4);

  for (i = 0; i < 8; i++)
     checksum += inb (ioaddr + 8 + i);

  if ((checksum & 0xff) != 0xFF)
     return 0;

  /* We should have a "dev" from Space.c or the static module table.
   */
  if (!dev)
  {
    printk ("smc-ultra.c: Passed a NULL device.\n");
    dev = init_etherdev (0, 0);
  }

  if (ei_debug && version_printed++ == 0)
     printk (version);

  model_name = (idreg & 0xF0) == 0x20 ? "SMC Ultra" : "SMC EtherEZ";

  printk ("%s: %s at %#3x,", dev->name, model_name, ioaddr);

  for (i = 0; i < 6; i++)
    printk (" %2.2X", dev->dev_addr[i] = inb (ioaddr + 8 + i));

  /* Switch from the station address to the alternate register set and
   * read the useful registers there.
   */
  outb (0x80 | reg4, ioaddr + 4);

  /* Enabled FINE16 mode to avoid BIOS ROM width mismatches @ reboot.
   */
  outb (0x80 | inb (ioaddr + 0x0c), ioaddr + 0x0c);
  piomode = inb (ioaddr + 0x8);
  addr = inb (ioaddr + 0xb);
  irqreg = inb (ioaddr + 0xd);

  /* Switch back to the station address register set so that the MS-DOS driver
   * can find the card after a warm boot.
   */
  outb (reg4, ioaddr + 4);

  if (dev->irq < 2)
  {
    BYTE irqmap[] = { 0, 9, 3, 5, 7, 10, 11, 15 };
    int  irq;

    /* The IRQ bits are split.
     */
    irq = irqmap[((irqreg & 0x40) >> 4) + ((irqreg & 0x0c) >> 2)];
    if (irq == 0)
    {
      printk (", failed to detect IRQ line.\n");
      return 0;
    }
    dev->irq   = irq;
    eeprom_irq = 1;
  }

  /* Allocate dev->priv and fill in 8390 specific dev fields.
   */
  if (ethdev_init (dev))
  {
    printk (", no memory for dev->priv.\n");
    return 0;
  }

  /* The 8390 isn't at the base address, so fake the offset
   */
  dev->base_addr = ioaddr + ULTRA_NIC_OFFSET;

  {
    int   addr_tbl[4]      = { 0x0C0000, 0x0E0000, 0xFC0000, 0xFE0000 };
    short num_pages_tbl[4] = { 0x20, 0x40, 0x80, 0xff };

    dev->mem_start = ((addr & 0x0f) << 13) + addr_tbl[(addr >> 6) & 3];
    num_pages = num_pages_tbl[(addr >> 4) & 3];
  }

  ei_status.name          = model_name;
  ei_status.word16        = 1;
  ei_status.tx_start_page = START_PG;
  ei_status.rx_start_page = START_PG + TX_PAGES;
  ei_status.stop_page     = num_pages;

  dev->rmem_start = dev->mem_start + TX_PAGES * 256;
  dev->mem_end    = dev->rmem_end = dev->mem_start +
                    (ei_status.stop_page - START_PG) * 256;

  if (piomode)
  {
    printk (",%s IRQ %d programmed-I/O mode.\n",
            eeprom_irq ? "EEPROM" : "assigned ", dev->irq);
    ei_status.block_input  = ultra_pio_input;
    ei_status.block_output = ultra_pio_output;
    ei_status.get_8390_hdr = ultra_pio_get_hdr;
  }
  else
  {
    printk (",%s IRQ %d memory %lx-%lx.\n", eeprom_irq ? "" : "assigned ",
	    dev->irq, dev->mem_start, dev->mem_end - 1);
    ei_status.block_input  = ultra_block_input;
    ei_status.block_output = ultra_block_output;
    ei_status.get_8390_hdr = ultra_get_8390_hdr;
  }
  ei_status.reset_8390 = ultra_reset_8390;
  dev->open  = ultra_open;
  dev->close = ultra_close_card;
  NS8390_init (dev, 0);
  return (1);
}

static int ultra_open (struct device *dev)
{
  int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET;	/* ASIC addr */

  if (!request_irq (dev->irq, ei_interrupt))
     return (0);

  outb (0x00, ioaddr);		/* Disable shared memory for safety. */
  outb (0x80, ioaddr + 5);
  if (ei_status.block_input == &ultra_pio_input)
  {
    outb (0x11, ioaddr + 6);	/* Enable interrupts and PIO. */
    outb (0x01, ioaddr + 0x19);	/* Enable ring read auto-wrap. */
  }
  else
    outb (0x01, ioaddr + 6);	/* Enable interrupts and memory. */

  /* Set the early receive warning level in window 0 high enough not
   * to receive ERW interrupts.
   */
  outb_p (E8390_NODMA + E8390_PAGE0, dev->base_addr);
  outb (0xff, dev->base_addr + EN0_ERWCNT);
  ei_open (dev);
  return (1);
}

static void ultra_reset_8390 (struct device *dev)
{
  int cmd_port = dev->base_addr - ULTRA_NIC_OFFSET;	/* ASIC base addr */

  outb (ULTRA_RESET, cmd_port);
  if (ei_debug > 1)
     printk ("resetting Ultra, t=%ld...", jiffies);

  ei_status.txing = 0;
  outb (0x00, cmd_port);           /* Disable shared memory for safety. */
  outb (0x80, cmd_port + 5);
  if (ei_status.block_input == &ultra_pio_input)
       outb (0x11, cmd_port + 6);  /* Enable interrupts and PIO. */
  else outb (0x01, cmd_port + 6);  /* Enable interrupts and memory. */

  if (ei_debug > 1)
    printk ("reset done\n");
}

/*
 * Grab the 8390 specific header. Similar to the block_input routine, but
 * we don't need to be concerned with ring wrap as the header will be at
 * the start of a page, so we optimize accordingly.
 */
static void ultra_get_8390_hdr (struct device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{
  DWORD hdr_start = dev->mem_start + ((ring_page - START_PG) << 8);

  outb (ULTRA_MEMENB, dev->base_addr - ULTRA_NIC_OFFSET);	/* shmem on */

#if 0
  /* Officially this is what we are doing, but the readl() is faster
   */
  memcpy_fromio (hdr, hdr_start, sizeof (struct e8390_pkt_hdr));
#else
  ((DWORD*)hdr)[0] = readl (hdr_start);
#endif
  outb (0x00, dev->base_addr - ULTRA_NIC_OFFSET);	/* shmem off */
}

/*
 * Block input and output are easy on shared memory ethercards, the only
 * complication is when the ring buffer wraps.
 */
static void ultra_block_input (struct device *dev, int count, struct sk_buff *skb, int ring_offset)
{
  DWORD xfer_start = dev->mem_start + ring_offset - (START_PG << 8);

  /* Enable shared memory.
   */
  outb (ULTRA_MEMENB, dev->base_addr - ULTRA_NIC_OFFSET);

  if (xfer_start + count > dev->rmem_end)
  {
    /* We must wrap the input move.
     */
    int semi_count = dev->rmem_end - xfer_start;

    memcpy_fromio (skb->data, xfer_start, semi_count);
    count -= semi_count;
    memcpy_fromio (skb->data + semi_count, dev->rmem_start, count);
  }
  else
  {
    /* Packet is in one chunk -- we can copy + cksum.
     */
    eth_io_copy_and_sum (skb, xfer_start, count, 0);
  }
  outb (0x00, dev->base_addr - ULTRA_NIC_OFFSET);	/* Disable memory. */
}

static void ultra_block_output (struct device *dev, int count, BYTE *buf, int start_page)
{
  DWORD shmem = dev->mem_start + ((start_page - START_PG) << 8);

  /* Enable shared memory.
   */
  outb (ULTRA_MEMENB, dev->base_addr - ULTRA_NIC_OFFSET);

  memcpy_toio (shmem, buf, count);
  outb (0x00, dev->base_addr - ULTRA_NIC_OFFSET);       /* Disable memory. */
}

/*
 * The identical operations for programmed I/O cards.
 * The PIO model is trivial to use: the 16 bit start address is written
 * byte-sequentially to IOPA, with no intervening I/O operations, and the
 * data is read or written to the IOPD data port.
 * The only potential complication is that the address register is shared
 * must be always be rewritten between each read/write direction change.
 * This is no problem for us, as the 8390 code ensures that we are single
 * threaded.
 */
static void ultra_pio_get_hdr (struct device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{
  int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET;	/* ASIC addr */

  outb (0x00, ioaddr + IOPA);	/* Set the address, LSB first. */
  outb (ring_page, ioaddr + IOPA);
  insw (ioaddr + IOPD, hdr, sizeof (struct e8390_pkt_hdr) >> 1);
}

static void ultra_pio_input (struct device *dev, int count, struct sk_buff *skb, int ring_offset)
{
  int   ioaddr = dev->base_addr - ULTRA_NIC_OFFSET;       /* ASIC addr */
  char *buf = skb->data;

  /* For now set the address again, although it should already be correct.
   */
  outb (ring_offset, ioaddr + IOPA);	/* Set the address, LSB first. */
  outb (ring_offset >> 8, ioaddr + IOPA);
  insw (ioaddr + IOPD, buf, (count + 1) >> 1);
#if 0
  /* We don't need this -- skbuffs are padded to at least word alignment.
   */
  if (count & 0x01)
     buf[count-1] = inb (ioaddr + IOPD);
#endif
}

static void ultra_pio_output (struct device *dev, int count, BYTE *buf, start_page)
{
  int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET;	/* ASIC addr */

  outb (0x00, ioaddr + IOPA);	/* Set the address, LSB first. */
  outb (start_page, ioaddr + IOPA);
  outsw (ioaddr + IOPD, buf, (count + 1) >> 1);
}

static void ultra_close_card (struct device *dev)
{
  int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET;	/* CMDREG */

  dev->start   = 0;
  dev->tx_busy = 1;

  if (ei_debug > 1)
     printk ("%s: Shutting down ethercard.\n", dev->name);

  outb (0x00, ioaddr + 6);	/* Disable interrupts. */
  free_irq (dev->irq);
  irq2dev_map[dev->irq] = NULL;

  NS8390_init (dev, 0);

  /* We should someday disable shared memory and change to 8-bit mode
   * "just in case"...
   */
}
