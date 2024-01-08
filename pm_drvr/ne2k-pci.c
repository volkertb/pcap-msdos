/* ne2k-pci.c: A NE2000 clone on PCI bus driver for Linux. */
/*
 * A Linux device driver for PCI NE2000 clones.
 * 
 * Authors and other copyright holders:
 * 1992-2000 by Donald Becker, NE2000 core and various modifications.
 * 1995-1998 by Paul Gortmaker, core modifications and PCI support.
 * Copyright 1993 assigned to the United States Government as represented
 * by the Director, National Security Agency.
 * 
 * This software may be used and distributed according to the terms of
 * the GNU General Public License (GPL), incorporated herein by reference.
 * Drivers based on or derived from this code fall under the GPL and must
 * retain the authorship, copyright and license notice.  This file is not
 * a complete program and may only be used when the entire operating
 * system is licensed under the GPL.
 * 
 * The author may be reached as becker@scyld.com, or C/O
 * Scyld Computing Corporation
 * 410 Severn Ave., Suite 210
 * Annapolis MD 21403
 * 
 * Issues remaining:
 * People are making PCI ne2000 clones! Oh the horror, the horror...
 * Limited full-duplex support.
 */

/* These identify the driver base version and may not be removed.
 */
static const char version1[] = "ne2k-pci.c:v1.04 1/31/2001 D. Becker/P. Gortmaker\n";
static const char version2[] = "  http://www.scyld.com/network/ne2k-pci.html\n";

#include "pmdrvr.h"
#include "bios32.h"
#include "pci.h"
#include "module.h"
#include "8390.h"
#include "pci-scan.h"

/* The user-configurable values.
 */
int ne2k_pci_debug = 1;         /* 1 normal messages, 0 quiet .. 7 verbose. */

#define MAX_UNITS 8             /* More are supported, limit only on options */

/* Used to pass the full-duplex flag, etc.
 */
static int full_duplex[MAX_UNITS] = { 0, };
static int options[MAX_UNITS]     = { 0, };

/* Force a non std. amount of memory.  Units are 256 byte pages.
 */
/* #define PACKETBUF_MEMSIZE 0x40 */

static void *ne2k_pci_probe1 (struct pci_dev *pdev, void *dev, long ioaddr, int irq, int chip_idx, int fnd_cnt);

/* Flags.  We rename an existing ei_status field to store flags!
 * Thus only the low 8 bits are usable for non-init-time flags.
 */
#define ne2k_flags reg0

enum {
  ONLY_16BIT_IO = 8, ONLY_32BIT_IO = 4, /* Chip can do only 16/32-bit xfers. */
  FORCE_FDX = 0x20,                     /* User override. */
  REALTEK_FDX = 0x40,
  HOLTEK_FDX = 0x80,
  STOP_PG_0x60 = 0x100,
};

#define NE_IO_EXTENT  0x20
#define PCI_IOTYPE   (PCI_USES_IO | PCI_ADDR0)

static struct pci_id_info pci_id_tbl[] = {
  { "RealTek RTL-8029", { 0x802910ec, 0xffffffff }, PCI_IOTYPE,
    NE_IO_EXTENT, REALTEK_FDX
  },
  { "Winbond 89C940", { 0x09401050, 0xffffffff }, PCI_IOTYPE,
    NE_IO_EXTENT, 0
  },
  { "Winbond w89c940", { 0x5a5a1050, 0xffffffff }, PCI_IOTYPE,
    NE_IO_EXTENT, 0
  },
  { "KTI ET32P2", { 0x30008e2e, 0xffffffff }, PCI_IOTYPE,
    NE_IO_EXTENT, 0
  },
  { "NetVin NV5000SC", { 0x50004a14, 0xffffffff }, PCI_IOTYPE,
    NE_IO_EXTENT, 0
  },
  { "Via 86C926", { 0x09261106, 0xffffffff }, PCI_IOTYPE,
    NE_IO_EXTENT, ONLY_16BIT_IO
  },
  { "SureCom NE34", { 0x0e3410bd, 0xffffffff }, PCI_IOTYPE,
    NE_IO_EXTENT, 0
  },
  { "Holtek HT80232", { 0x005812c3, 0xffffffff }, PCI_IOTYPE,
    NE_IO_EXTENT, ONLY_16BIT_IO | HOLTEK_FDX
  },
  { "Holtek HT80229", { 0x559812c3, 0xffffffff }, PCI_IOTYPE,
    NE_IO_EXTENT, ONLY_32BIT_IO | HOLTEK_FDX | STOP_PG_0x60
  },
  { "Compex RL2000", { 0x140111f6, 0xffffffff }, PCI_IOTYPE,
    NE_IO_EXTENT, 0
  },
  { 0, }              /* 0 terminated list. */
};

struct drv_id_info ne2k_pci_drv_id = {
                   "ne2k-pci", 0, PCI_CLASS_NETWORK_ETHERNET << 8,
                   pci_id_tbl, ne2k_pci_probe1
                 };

/* ---- No user-serviceable parts below ---- */

#define NE_BASE         (dev->base_addr)
#define NE_CMD	 	0x00
#define NE_DATAPORT     0x10  /* NatSemi-defined port window offset. */
#define NE_RESET        0x1f  /* Issue a read to reset, a write to clear. */

#define NESM_START_PG   0x40  /* First page of TX buffer */
#define NESM_STOP_PG    0x80  /* Last page +1 of RX ring */

static int  ne2k_pci_open (struct device *dev);
static void ne2k_pci_close (struct device *dev);

static void ne2k_pci_reset_8390 (struct device *dev);
static void ne2k_pci_get_8390_hdr (struct device *dev, struct e8390_pkt_hdr *hdr, int ring_page);
static void ne2k_pci_block_input (struct device *dev, int count, char *buf, int ring_offset);
static void ne2k_pci_block_output (struct device *dev, int count,
                                   const char *buf, int start_page);

/* There is no room in the standard 8390 structure for extra info we need,
 * so we build a meta/outer-wrapper structure..
 */
struct ne2k_pci_card {
       struct ne2k_pci_card *next;
       struct device        *dev;
       struct pci_dev       *pci_dev;
     };

/* A list of all installed devices, for removing the driver module.
 */
static struct ne2k_pci_card *ne2k_card_list = NULL;

int ne2k_pci_probe (struct device *dev)
{
  int found_cnt = pci_drv_register (&ne2k_pci_drv_id, NULL);

  if (found_cnt >= 0 && ne2k_pci_debug)
     printk ("%s%s", version1, version2);
  return (found_cnt < 0 ? 0 : found_cnt);
}

static void *ne2k_pci_probe1 (struct pci_dev *pdev, void *init_dev, long ioaddr,
                              int irq, int chip_idx, int fnd_cnt)
{
  struct device    *dev;
  struct ne2k_pci_card *ne2k_card;
  BYTE   SA_prom[32];
  int    i, start_page, stop_page;
  int    reg0  = inb (ioaddr);
  int    flags = pci_id_tbl[chip_idx].drv_flags;

  if (reg0 == 0xFF)
     return (0);

  /* Do a preliminary verification that we have a 8390. */
  {
    int regd;

    outb (E8390_NODMA + E8390_PAGE1 + E8390_STOP, ioaddr + E8390_CMD);
    regd = inb (ioaddr + 0x0d);
    outb (0xff, ioaddr + 0x0d);
    outb (E8390_NODMA + E8390_PAGE0, ioaddr + E8390_CMD);
    inb (ioaddr + EN0_COUNTER0); /* Clear the counter by reading. */
    if (inb (ioaddr + EN0_COUNTER0) != 0)
    {
      outb (reg0, ioaddr);
      outb (regd, ioaddr + 0x0d); /* Restore the old values. */
      return (0);
    }
  }

  dev = init_etherdev (init_dev, 0);
  if (!dev)
     return (0);

  ne2k_card = k_malloc (sizeof(struct ne2k_pci_card));
  if (!ne2k_card)
     return (0);

  ne2k_card->next    = ne2k_card_list;
  ne2k_card_list     = ne2k_card;
  ne2k_card->dev     = dev;
  ne2k_card->pci_dev = pdev;

  /* Reset card. Who knows what dain-bramaged state it was left in. */
  {
    DWORD reset_start_time = jiffies;

    outb (inb (ioaddr + NE_RESET), ioaddr + NE_RESET);

    /* This looks like a horrible timing loop, but it should never take
     * more than a few cycles.
     */
    while ((inb (ioaddr + EN0_ISR) & ENISR_RESET) == 0)
    {
      /* Limit wait: '2' avoids jiffy roll-over. */
      if (jiffies - reset_start_time > 2)
      {
        printk ("ne2k-pci: Card failure (no reset ack).\n");
        return (0);
      }
    }
    outb (0xff, ioaddr + EN0_ISR); /* Ack all intr. */
  }

  /* Read the 16 bytes of station address PROM.
   * We must first initialize registers, similar to NS8390_init(eifdev, 0).
   * We can't reliably read the SAPROM address without this.
   * (I learned the hard way!).
   */
  {
    struct {
      BYTE value;
      BYTE offset;
    } program_seq[] = {
      { E8390_NODMA + E8390_PAGE0 + E8390_STOP, E8390_CMD } ,                         /* Select page 0 */
      { 0x49, EN0_DCFG },        /* Set word-wide access. */
      { 0x00, EN0_RCNTLO },      /* Clear the count regs. */
      { 0x00, EN0_RCNTHI },
      { 0x00, EN0_IMR },         /* Mask completion irq. */
      { 0xFF, EN0_ISR },
      { E8390_RXOFF, EN0_RXCR }, /* 0x20  Set to monitor */
      { E8390_TXOFF, EN0_TXCR }, /* 0x02  and loopback mode. */
      { 32, EN0_RCNTLO },
      { 0x00, EN0_RCNTHI },
      { 0x00, EN0_RSARLO },      /* DMA starting at 0x0000. */
      { 0x00, EN0_RSARHI },
      { E8390_RREAD + E8390_START, E8390_CMD }
    };
    for (i = 0; i < DIM(program_seq); i++)
        outb (program_seq[i].value, ioaddr + program_seq[i].offset);
  }

  /* Note: all PCI cards have at least 16 bit access, so we don't have
   * to check for 8 bit cards.  Most cards permit 32 bit access.
   */
  if (flags & ONLY_32BIT_IO)
  {
    for (i = 0; i < 8; i++)
       ((DWORD*)SA_prom)[i] = inl (ioaddr + NE_DATAPORT);
  }
  else
  {
    for (i = 0; i < 32 /*sizeof(SA_prom) */ ; i++)
        SA_prom[i] = inb (ioaddr + NE_DATAPORT);
  }

  /* We always set the 8390 registers for word mode.
   */
  outb (0x49, ioaddr + EN0_DCFG);
  start_page = NESM_START_PG;

  stop_page = flags & STOP_PG_0x60 ? 0x60 : NESM_STOP_PG;

  /* Set up the rest of the parameters.
   */
  dev->irq       = irq;
  dev->base_addr = ioaddr;

  /* Allocate dev->priv and fill in 8390 specific dev fields.
   */
  if (ethdev_init (dev))
  {
    printk ("%s: unable to get memory for dev->priv.\n", dev->name);
    return (0);
  }

  if (ne2k_pci_debug)
     printk ("%s: %s found at %x, IRQ %d, ",
             dev->name, pci_id_tbl[chip_idx].name, ioaddr, dev->irq);

  for (i = 0; i < 6; i++)
  {
    if (ne2k_pci_debug)
       printk ("%02X%s", SA_prom[i], i == 5 ? ".\n" : ":");
    dev->dev_addr[i] = SA_prom[i];
  }

  ei_status.name          = pci_id_tbl[chip_idx].name;
  ei_status.tx_start_page = start_page;
  ei_status.stop_page     = stop_page;
  ei_status.word16        = 1;
  ei_status.ne2k_flags    = flags;
  if (fnd_cnt < MAX_UNITS)
  {
    if (full_duplex[fnd_cnt] > 0 || (options[fnd_cnt] & FORCE_FDX))
    {
      printk ("%s:  Full duplex set by user option.\n", dev->name);
      ei_status.ne2k_flags |= FORCE_FDX;
    }
  }

  ei_status.rx_start_page = start_page + TX_PAGES;

#ifdef PACKETBUF_MEMSIZE
  /* Allow the packet buffer size to be overridden by know-it-alls.
   */
  ei_status.stop_page = ei_status.tx_start_page + PACKETBUF_MEMSIZE;
#endif

  ei_status.reset_8390   = ne2k_pci_reset_8390;
  ei_status.block_input  = ne2k_pci_block_input;
  ei_status.block_output = ne2k_pci_block_output;
  ei_status.get_8390_hdr = ne2k_pci_get_8390_hdr;
  dev->open  = ne2k_pci_open;
  dev->close = ne2k_pci_close;
  NS8390_init (dev, 0);
  return (dev);
}

static int ne2k_pci_open (struct device *dev)
{
  /* MUST set irq2dev_map first, because IRQ may come
   * before request_irq() returns.
   */
  irq2dev_map [dev->irq] = dev;
  if (!request_irq (dev->irq, ei_interrupt))
  {
    irq2dev_map [dev->irq] = NULL;
    return (0);
  }

  /* Set full duplex for the chips that we know about.
   */
  if (ei_status.ne2k_flags & FORCE_FDX)
  {
    long ioaddr = dev->base_addr;

    if (ei_status.ne2k_flags & REALTEK_FDX)
    {
      outb (0xC0 + E8390_NODMA, ioaddr + NE_CMD); /* Page 3 */
      outb (inb (ioaddr + 0x20) | 0x80, ioaddr + 0x20);
    }
    else if (ei_status.ne2k_flags & HOLTEK_FDX)
      outb (inb (ioaddr + 0x20) | 0x80, ioaddr + 0x20);
  }
  ei_open (dev);
  return (1);
}

static void ne2k_pci_close (struct device *dev)
{
  ei_close (dev);
  free_irq (dev->irq);
  irq2dev_map [dev->irq] = NULL;
}

/*
 * Hard reset the card.  This used to pause for the same period that a
 * 8390 reset command required, but that shouldn't be necessary.
 */
static void ne2k_pci_reset_8390 (struct device *dev)
{
  DWORD reset_start_time = jiffies;

  if (ne2k_pci_debug > 1)
     printk ("%s: Resetting the 8390 t=%ld...", dev->name, jiffies);

  outb (inb (NE_BASE + NE_RESET), NE_BASE + NE_RESET);

  ei_status.txing = 0;
  ei_status.dmaing = 0;

  /* This check _should_not_ be necessary, omit eventually.
   */
  while ((inb (NE_BASE + EN0_ISR) & ENISR_RESET) == 0)
    if (jiffies - reset_start_time > 2)
    {
      printk ("%s: ne2k_pci_reset_8390() did not complete.\n", dev->name);
      break;
    }
  outb (ENISR_RESET, NE_BASE + EN0_ISR); /* Ack intr. */
}

/*
 * Grab the 8390 specific header. Similar to the block_input routine, but
 * we don't need to be concerned with ring wrap as the header will be at
 * the start of a page, so we optimize accordingly.
 */
static void ne2k_pci_get_8390_hdr (struct device *dev,
                                   struct e8390_pkt_hdr *hdr,
                                   int ring_page)
{
  long nic_base = dev->base_addr;

  /* This *shouldn't* happen. If it does, it's the last thing you'll see
   */
  if (ei_status.dmaing)
  {
    printk ("%s: DMAing conflict in ne2k_pci_get_8390_hdr "
            "[DMAstat:%d][irqlock:%d][intr:%d].\n",
            dev->name, ei_status.dmaing, ei_status.irqlock, dev->reentry);
    return;
  }

  ei_status.dmaing |= 0x01;
  outb (E8390_NODMA + E8390_PAGE0 + E8390_START, nic_base + NE_CMD);
  outb (sizeof (struct e8390_pkt_hdr), nic_base + EN0_RCNTLO);

  outb (0, nic_base + EN0_RCNTHI);
  outb (0, nic_base + EN0_RSARLO); /* On page boundary */
  outb (ring_page, nic_base + EN0_RSARHI);
  outb (E8390_RREAD + E8390_START, nic_base + NE_CMD);

  if (ei_status.ne2k_flags & ONLY_16BIT_IO)
       rep_insw (NE_BASE + NE_DATAPORT, (WORD*)hdr, sizeof(*hdr) >> 1);
  else *(DWORD*)hdr = inl (NE_BASE + NE_DATAPORT);

  outb (ENISR_RDC, nic_base + EN0_ISR); /* Ack intr. */
  ei_status.dmaing &= ~0x01;
}

/*
 * Block input and output, similar to the Crynwr packet driver.  If you
 * are porting to a new ethercard, look at the packet driver source for hints.
 * The NEx000 doesn't share the on-board packet memory -- you have to put
 * the packet out through the "remote DMA" dataport using outb.
 */
static void ne2k_pci_block_input (struct device *dev, int count,
                                  char *buf, int ring_offset)
{
  long nic_base = dev->base_addr;

  /* This *shouldn't* happen. If it does, it's the last thing you'll see
   */
  if (ei_status.dmaing)
  {
    printk ("%s: DMAing conflict in ne2k_pci_block_input "
            "[DMAstat:%d][irqlock:%d][intr:%d].\n",
            dev->name, ei_status.dmaing, ei_status.irqlock, dev->reentry);
    return;
  }

  ei_status.dmaing |= 0x01;
  if (ei_status.ne2k_flags & ONLY_32BIT_IO)
     count = (count + 3) & 0xFFFC;

  outb (E8390_NODMA + E8390_PAGE0 + E8390_START, nic_base + NE_CMD);
  outb (count & 0xff, nic_base + EN0_RCNTLO);
  outb (count >> 8, nic_base + EN0_RCNTHI);
  outb (ring_offset & 0xff, nic_base + EN0_RSARLO);
  outb (ring_offset >> 8, nic_base + EN0_RSARHI);
  outb (E8390_RREAD + E8390_START, nic_base + NE_CMD);

  if (ei_status.ne2k_flags & ONLY_16BIT_IO)
  {
    rep_insw (NE_BASE + NE_DATAPORT, (WORD*)buf, count >> 1);
    if (count & 1)
       buf[count-1] = inb (NE_BASE + NE_DATAPORT);
  }
  else
  {
    rep_insl (NE_BASE + NE_DATAPORT, (DWORD*)buf, count >> 2);
    if (count & 3)
    {
      buf += count & ~3;
      if (count & 2)
         *((WORD*)buf)++ = inw (NE_BASE + NE_DATAPORT);
      if (count & 1)
         *buf = inb (NE_BASE + NE_DATAPORT);
    }
  }
  outb (ENISR_RDC, nic_base + EN0_ISR); /* Ack intr. */
  ei_status.dmaing &= ~0x01;
}

static void ne2k_pci_block_output (struct device *dev, int count,
                                   const char *buf, int start_page)
{
  int   nic_base = NE_BASE;
  DWORD dma_start;

  /* On little-endian it's always safe to round the count up for
   * word writes.
   */
  if (ei_status.ne2k_flags & ONLY_32BIT_IO)
     count = (count + 3) & 0xFFFC;
  else if (count & 0x01)
     count++;

  /* This *shouldn't* happen. If it does, it's the last thing you'll see
   */
  if (ei_status.dmaing)
  {
    printk ("%s: DMAing conflict in ne2k_pci_block_output."
            "[DMAstat:%d][irqlock:%d][intr:%d]\n",
            dev->name, ei_status.dmaing, ei_status.irqlock, dev->reentry);
    return;
  }

  ei_status.dmaing |= 0x01;

  /* We should already be in page 0, but to be safe...
   */
  outb (E8390_PAGE0 + E8390_START + E8390_NODMA, nic_base + NE_CMD);

#ifdef NE8390_RW_BUGFIX
  /* Handle the read-before-write bug the same way as the
   * Crynwr packet driver -- the NatSemi method doesn't work.
   * Actually this doesn't always work either, but if you have
   * problems with your NEx000 this is better than nothing!
   */
  outb (0x42, nic_base + EN0_RCNTLO);
  outb (0x00, nic_base + EN0_RCNTHI);
  outb (0x42, nic_base + EN0_RSARLO);
  outb (0x00, nic_base + EN0_RSARHI);
  outb (E8390_RREAD + E8390_START, nic_base + NE_CMD);
#endif
  outb (ENISR_RDC, nic_base + EN0_ISR);

  /* Now the normal output.
   */
  outb (count & 0xff, nic_base + EN0_RCNTLO);
  outb (count >> 8, nic_base + EN0_RCNTHI);
  outb (0x00, nic_base + EN0_RSARLO);
  outb (start_page, nic_base + EN0_RSARHI);
  outb (E8390_RWRITE + E8390_START, nic_base + NE_CMD);

  if (ei_status.ne2k_flags & ONLY_16BIT_IO)
  {
    rep_outsw (NE_BASE + NE_DATAPORT, (WORD*)buf, count >> 1);
  }
  else
  {
    rep_outsl (NE_BASE + NE_DATAPORT, (DWORD*)buf, count >> 2);
    if (count & 3)
    {
      buf += count & ~3;
      if (count & 2)
         outw (*((WORD*)buf)++, NE_BASE + NE_DATAPORT);
    }
  }

  dma_start = jiffies;

  while ((inb (nic_base + EN0_ISR) & ENISR_RDC) == 0)
    if (jiffies - dma_start > 2)
    {                           /* Avoid clock roll-over. */
      printk ("%s: timeout waiting for Tx RDC.\n", dev->name);
      ne2k_pci_reset_8390 (dev);
      NS8390_init (dev, 1);
      break;
    }

  outb (ENISR_RDC, nic_base + EN0_ISR); /* Ack intr. */
  ei_status.dmaing &= ~0x01;
}
