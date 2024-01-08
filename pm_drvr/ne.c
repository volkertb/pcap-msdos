/* ne.c: A general non-shared-memory NS8390 ethernet driver for linux. */
/*
 * Written 1992-94 by Donald Becker.
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
 * This driver should work with many programmed-I/O 8390-based ethernet
 * boards.  Currently it supports the NE1000, NE2000, many clones,
 * and some Cabletron products.
 * 
 * Changelog:
 * 
 * Paul Gortmaker  : use ENISR_RDC to monitor Tx PIO uploads, made
 * sanity checks and bad clone support optional.
 * Paul Gortmaker  : new reset code, reset card after probe at boot.
 * Paul Gortmaker  : multiple card support for module users.
 * Paul Gortmaker  : Support for PCI ne2k clones, similar to lance.c
 * Paul Gortmaker  : Allow users with bad cards to avoid full probe.
 * Paul Gortmaker  : PCI probe changes, more PCI cards supported.
 * 
 */

/* Routines for the NatSemi-based designs (NE[12]000). */

static const char *version = "ne.c:v1.10 9/23/94 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include "pmdrvr.h"
#include "pci.h"
#include "bios32.h"
#include "8390.h"
#include "ne.h"

/* A zero-terminated list of I/O addresses to be probed at boot.
 */
static DWORD netcard_portlist[] = { 0x300, 0x280, 0x320, 0x340, 0x360, 0 };

#ifdef CONFIG_PCI
  /* Ack! People are making PCI ne2000 clones! Oh the horror, the horror...
   */
  static struct {
         WORD vendor;
         WORD dev_id;
       } pci_clone_list[] = {
         { PCI_VENDOR_ID_REALTEK,  PCI_DEVICE_ID_REALTEK_8029    } ,
         { PCI_VENDOR_ID_WINBOND2, PCI_DEVICE_ID_WINBOND2_89C940 } ,
         { PCI_VENDOR_ID_COMPEX,   PCI_DEVICE_ID_COMPEX_RL2000   } ,
         { 0, }
       };
#endif

#ifdef SUPPORT_NE_BAD_CLONES
/* A list of bad clones that we none-the-less recognize.
 */
static struct {
       const char *name8, *name16;
       BYTE  SAprefix[4];
     } bad_clone_list[] = {

{ "DE100",      "DE200",         { 0x00, 0xDE, 0x01, } },
{ "DE120",      "DE220",         { 0x00, 0x80, 0xc8, } },
{ "DFI1000",    "DFI2000",       { 'D',  'F',  'I',  } }, /* Original, eh?  */
{ "EtNext UTP8","EthNext UTP16", { 0x00, 0x00, 0x79, } },
{ "NE1000",     "NE2000-invalid",{ 0x00, 0x00, 0xd8, } }, /* Ancient real NE1000. */
{ "NN1000",     "NN2000",        { 0x08, 0x03, 0x08, } }, /* Outlaw no-name clone. */
{ "4-DIM8",     "4-DIM16",       { 0x00, 0x00, 0x4d, } }, /* Outlaw 4-Dimension cards. */
{ "Con-Intl_8", "Con-Intl_16",   { 0x00, 0x00, 0x24, } }, /* Connect Int'nl */
{ "ET-100",     "ET-200",        { 0x00, 0x45, 0x54, } }, /* YANG and YA clone */
{ "COMPEX",     "COMPEX16",      { 0x00, 0x80, 0x48, } }, /* Broken ISA Compex cards */
{ "E-LAN100",   "E-LAN200",      { 0x00, 0x00, 0x5d, } }, /* Broken ne1000 clones */
{ NULL, }
};
#endif

/* ---- No user-serviceable parts below ---- */

#define NE_BASE         (dev->base_addr)
#define NE_CMD          0x00
#define NE_DATAPORT     0x10     /* NatSemi-defined port window offset. */
#define NE_RESET        0x1f     /* Issue a read to reset, a write to clear. */
#define NE_IO_EXTENT    0x20

#define NE1SM_START_PG  0x20     /* First page of TX buffer */
#define NE1SM_STOP_PG   0x40     /* Last page +1 of RX ring */
#define NESM_START_PG   0x40     /* First page of TX buffer */
#define NESM_STOP_PG    0x80     /* Last page +1 of RX ring */

/* Non-zero only if the current card is a PCI with BIOS-set IRQ.
 */
static BYTE pci_irq_line = 0;

static int ne_probe1 (struct device *dev, int ioaddr);

#ifdef CONFIG_PCI
static int ne_probe_pci (struct device *dev);
#endif

static int  ne_open         (struct device *dev);
static void ne_close        (struct device *dev);
static void ne_reset_8390   (struct device *dev);
static void ne_get_8390_hdr (struct device *dev, struct e8390_pkt_hdr *hdr, int ring_page);
static void ne_block_input  (struct device *dev, int count, char *, int ring_offset);
static void ne_block_output (struct device *dev, int count, const char *, int start_page);


/*
 * Probe for various non-shared-memory ethercards.
 * 
 * NEx000-clone boards have a Station Address PROM (SAPROM) in the packet
 * buffer memory space.  NE2000 clones have 0x57,0x57 in bytes 0x0e,0x0f of
 * the SAPROM, while other supposed NE2000 clones must be detected by their
 * SA prefix.
 * 
 * Reading the SAPROM from a word-wide card with the 8390 set in byte-wide
 * mode results in doubled values, which can be detected and compensated for.
 * 
 * The probe is also responsible for initializing the card and filling
 * in the 'dev' and 'ei_status' structures.
 * 
 * We use the minimum memory size for some ethercard product lines, iff we can't
 * distinguish models.  You can increase the packet buffer size by setting
 * PACKETBUF_MEMSIZE.  Reported Cabletron packet buffer locations are:
 * E1010   starts at 0x100 and ends at 0x2000.
 * E1010-x starts at 0x100 and ends at 0x8000. ("-x" means "more memory")
 * E2010   starts at 0x100 and ends at 0x4000.
 * E2010-x starts at 0x100 and ends at 0xffff.
 */
  
/*
 * Note that at boot, this probe only picks up one card at a time, even for
 * multiple PCI ne2k cards. Use "ether=0,0,eth1" if you have a second PCI
 * ne2k card.  This keeps things consistent regardless of the bus type of
 * the card.
 */

int ne_probe (struct device *dev)
{
  int base_addr = dev ? dev->base_addr : 0;

  /* First check any supplied i/o locations. User knows best. <cough>
   */
  if (base_addr > 0x1ff)   /* Check a single specified location. */
     return ne_probe1 (dev, base_addr);

  if (base_addr)           /* Don't probe at all. */
     return (0);

#ifdef CONFIG_PCI
  /* Then look for any installed PCI clones
   */
  if (pcibios_present() && !ne_probe_pci(dev))
     return (0);
#endif

  /* Last resort. The semi-risky ISA auto-probe.
   */
  for (base_addr = 0; netcard_portlist[base_addr]; base_addr++)
      if (ne_probe1(dev, netcard_portlist[base_addr]))
         return (1);

  return (0);
}

#ifdef CONFIG_PCI
static int ne_probe_pci (struct device *dev)
{
  int i;

  for (i = 0; pci_clone_list[i].vendor != 0; i++)
  {
    BYTE  pci_bus, pci_device_fn;
    DWORD pci_ioaddr;
    int   pci_index;

    for (pci_index = 0; pci_index < 8; pci_index++)
    {
      if (pcibios_find_device (pci_clone_list[i].vendor,
                               pci_clone_list[i].dev_id,
                               pci_index, &pci_bus, &pci_device_fn))
         break;   /* No more of these type of cards */

      pcibios_read_config_dword (pci_bus, pci_device_fn, PCI_BASE_ADDRESS_0,
                                 &pci_ioaddr);

      /* Strip the I/O address out of the returned value
       */
      pci_ioaddr &= PCI_BASE_ADDRESS_IO_MASK;

      pcibios_read_config_byte (pci_bus, pci_device_fn, PCI_INTERRUPT_LINE,
                                &pci_irq_line);
      break;   /* Beauty -- got a valid card. */
    }
    if (pci_irq_line == 0) /* Try next PCI ID */
       continue;   

    printk ("ne.c: PCI BIOS reports %s %s at i/o %3x, irq %d.\n",
	    pci_strvendor (pci_clone_list[i].vendor),
            pci_strdev (pci_clone_list[i].vendor, pci_clone_list[i].dev_id),
            (unsigned)pci_ioaddr, pci_irq_line);

    if (!ne_probe1 (dev, pci_ioaddr))  /* Shouldn't happen. */
    {                     
      printk ("ne.c: Probe of PCI card at %3x failed.\n",
              (unsigned)pci_ioaddr);
      pci_irq_line = 0;
      return (0);
    }
    pci_irq_line = 0;
    return (1);
  }
  return (0);
}
#endif	   /* CONFIG_PCI */

static int ne_probe1 (struct device *dev, int ioaddr)
{
  static unsigned version_printed = 0;
  BYTE   SA_prom[32];
  int    wordlength = 2;
  const  char *name = NULL;
  int    start_page, stop_page;
  int    neX000, ctron, bad_card;
  int    regd, i;
  int    reg0 = inb (ioaddr);

  if (reg0 == 0xFF)
     return (0);

  /* Do a preliminary verification that we have a 8390.
   */
  outb (E8390_NODMA + E8390_PAGE1 + E8390_STOP, ioaddr + E8390_CMD);
  regd = inb (ioaddr + 0x0d);
  outb (0xff, ioaddr + 0x0d);
  outb (E8390_NODMA + E8390_PAGE0, ioaddr + E8390_CMD);
  inb (ioaddr + EN0_COUNTER0);      /* Clear the counter by reading. */
  if (inb(ioaddr + EN0_COUNTER0) != 0)
  {
    outb (reg0, ioaddr);
    outb (regd, ioaddr + 0x0d);     /* Restore the old values. */
    return (0);
  }

  if (!dev)
  {
    printk ("ne.c: Passed a NULL device.\n");
    dev = init_etherdev (0, 0);
  }

  if (ei_debug && version_printed++ == 0)
     printk (version);

  printk ("NE*000 ethercard probe at %3x:", ioaddr);

  /* A user with a poor card that fails to ack the reset, or that
   * does not have a valid 0x57,0x57 signature can still use this
   * without having to recompile. Specifying an i/o address along
   * with an otherwise unused dev->mem_end value of "0xBAD" will
   * cause the driver to skip these parts of the probe.
   */
  bad_card = ((dev->base_addr != 0) && (dev->mem_end == 0xbad));

  /* Reset card. Who knows what dain-bramaged state it was left in.
   */
  {
    DWORD reset_start_time = jiffies;

    /* DON'T change these to inb_p/outb_p or reset will fail on clones.
     */
    outb (inb (ioaddr + NE_RESET), ioaddr + NE_RESET);

    while ((inb (ioaddr + EN0_ISR) & ENISR_RESET) == 0)
    {
      if (jiffies - reset_start_time > 2 * HZ / 100)
      {
	if (bad_card)
	{
	  printk (" (warning: no reset ack)");
	  break;
	}
        printk (" not found (no reset ack).\n");
        return (0);
      }
    }

    outb (0xff, ioaddr + EN0_ISR);    /* Ack all intr. */
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
      { E8390_NODMA+E8390_PAGE0+E8390_STOP, E8390_CMD }, /* Select page 0*/
      { 0x48,  EN0_DCFG },       /* Set byte-wide (0x48) access. */
      { 0x00,  EN0_RCNTLO },     /* Clear the count regs. */
      { 0x00,  EN0_RCNTHI },
      { 0x00,  EN0_IMR },        /* Mask completion irq. */
      { 0xFF,  EN0_ISR },
      { E8390_RXOFF, EN0_RXCR }, /* 0x20  Set to monitor */
      { E8390_TXOFF, EN0_TXCR }, /* 0x02  and loopback mode. */
      { 32,  EN0_RCNTLO },
      { 0x00,  EN0_RCNTHI },
      { 0x00,  EN0_RSARLO },     /* DMA starting at 0x0000. */
      { 0x00,  EN0_RSARHI },
      { E8390_RREAD+E8390_START, E8390_CMD }
    };

    for (i = 0; i < DIM(program_seq); i++)
        outb (program_seq[i].value, ioaddr + program_seq[i].offset);
  }
  for (i = 0; i < sizeof(SA_prom); i += 2)
  {
    SA_prom[i]   = inb (ioaddr + NE_DATAPORT);
    SA_prom[i+1] = inb (ioaddr + NE_DATAPORT);
    if (SA_prom[i] != SA_prom[i+1])
       wordlength = 1;
  }

  /* At this point, wordlength *only* tells us if the SA_prom is doubled
   * up or not because some broken PCI cards don't respect the byte-wide
   * request in program_seq above, and hence don't have doubled up values.
   * These broken cards would otherwise be detected as an ne1000.
   */
  if (wordlength == 2)
     for (i = 0; i < sizeof(SA_prom)/2; i++)
         SA_prom[i] = SA_prom[i+i];

  if (pci_irq_line || ioaddr >= 0x400)
     wordlength = 2;   /* Catch broken PCI cards mentioned above. */

  if (wordlength == 2)
  {
    /* We must set the 8390 for word mode.
     */
    outb (0x49, ioaddr + EN0_DCFG);
    start_page = NESM_START_PG;
    stop_page  = NESM_STOP_PG;
  }
  else
  {
    start_page = NE1SM_START_PG;
    stop_page  = NE1SM_STOP_PG;
  }

  neX000 = (SA_prom[14] == 0x57 && SA_prom[15] == 0x57);
  ctron  = (SA_prom[0]  == 0x00 && SA_prom[1]  == 0x00 && SA_prom[2] == 0x1d);

  /* Set up the rest of the parameters.
   */
  if (neX000 || bad_card)
  {
    name = (wordlength == 2) ? "NE2000" : "NE1000";
  }
  else if (ctron)
  {
    name = (wordlength == 2) ? "Ctron-8" : "Ctron-16";
    start_page = 0x01;
    stop_page  = (wordlength == 2) ? 0x40 : 0x20;
  }
  else
  {
#ifdef SUPPORT_NE_BAD_CLONES
    /* Ack!  Well, there might be a *bad* NE*000 clone there.
     * Check for total bogus addresses.
     */
    for (i = 0; bad_clone_list[i].name8; i++)
    {
      if (SA_prom[0] == bad_clone_list[i].SAprefix[0] &&
          SA_prom[1] == bad_clone_list[i].SAprefix[1] &&
          SA_prom[2] == bad_clone_list[i].SAprefix[2])
      {
	if (wordlength == 2)
             name = bad_clone_list[i].name16;
        else name = bad_clone_list[i].name8;
	break;
      }
    }
    if (bad_clone_list[i].name8 == NULL)
    {
      printk (" not found (invalid signature %2.2x %2.2x).\n",
              SA_prom[14], SA_prom[15]);
      return (0);
    }
#else
    printk (" not found.\n");
    return (0);
#endif
  }

  if (pci_irq_line)
     dev->irq = pci_irq_line;

  if (dev->irq < 2)
  {
    autoirq_setup (0);
    outb (0x50, ioaddr + EN0_IMR);    /* Enable one interrupt. */
    outb (0x00, ioaddr + EN0_RCNTLO);
    outb (0x00, ioaddr + EN0_RCNTHI);
    outb (E8390_RREAD + E8390_START, ioaddr); /* Trigger it... */
    outb (0x00, ioaddr + EN0_IMR);    /* Mask it again. */
    dev->irq = autoirq_report (0);
    if (ei_debug > 2)
       printk (" autoirq is %d\n", dev->irq);
  }
  else if (dev->irq == 2)
  {
    /* Fixup for users that don't know that IRQ 2 is really IRQ 9,
     * or don't know which one to set.
     */
    dev->irq = 9;
  }

  if (!dev->irq)
  {
    printk (" failed to detect IRQ line.\n");
    return (0);
  }

  /* Snarf the interrupt now.  There's no point in waiting since we cannot
   * share and the board will usually be enabled.
   *
   * MUST set irq2dev_map first, cause IRQ may come immediately
   * before request_irq() returns!
   */
  irq2dev_map[dev->irq] = dev;

  if (!request_irq (dev->irq, ei_interrupt))
  {
    irq2dev_map[dev->irq] = NULL;
    printk (" unable to hook IRQ %d\n", dev->irq);
    return (0);
  }

  dev->base_addr = ioaddr;

  /* Allocate dev->priv and fill in 8390 specific dev fields.
   */
  if (ethdev_init(dev))
  {
    printk (" unable to get memory for dev->priv.\n");
    free_irq (dev->irq);
    return (0);
  }

  for (i = 0; i < sizeof(ETHER); i++)
  {
    printk (" %02x", SA_prom[i]);
    dev->dev_addr[i] = SA_prom[i];
  }

  printk ("\n%s: %s found at %3x, using IRQ %d.\n",
          dev->name, name, ioaddr, dev->irq);

  ei_status.name          = name;
  ei_status.tx_start_page = start_page;
  ei_status.stop_page     = stop_page;
  ei_status.word16        = (wordlength == 2);
  ei_status.rx_start_page = start_page + TX_PAGES;

#ifdef PACKETBUF_MEMSIZE
  /* Allow the packet buffer size to be overridden by know-it-alls. */
  ei_status.stop_page = ei_status.tx_start_page + PACKETBUF_MEMSIZE;
#endif

  ei_status.reset_8390   = ne_reset_8390;
  ei_status.block_input  = ne_block_input;
  ei_status.block_output = ne_block_output;
  ei_status.get_8390_hdr = ne_get_8390_hdr;
  dev->open  = ne_open;
  dev->close = ne_close;
  NS8390_init (dev, 0);
  return (1);
}

static int ne_open (struct device *dev)
{
  return ei_open (dev);
}

static void ne_close (struct device *dev)
{
  if (ei_debug > 1)
     printk ("%s: Shutting down ethercard.\n", dev->name);
  ei_close (dev);
}

/*
 * Hard reset the card.  This used to pause for the same period that a
 * 8390 reset command required, but that shouldn't be necessary.
 */
static void ne_reset_8390 (struct device *dev)
{
  DWORD reset_start_time = jiffies;

  if (ei_debug > 1)
     printk ("resetting the 8390 t=%u...", (unsigned)jiffies);

  /* DON'T change these to inb_p/outb_p or reset will fail on clones.
   */
  outb (inb (NE_BASE + NE_RESET), NE_BASE + NE_RESET);

  ei_status.txing  = 0;
  ei_status.dmaing = 0;

  /* This check _should_not_ be necessary, omit eventually.
   */
  while ((inb (NE_BASE + EN0_ISR) & ENISR_RESET) == 0)
    if (jiffies - reset_start_time > 2 * HZ / 100)
    {
      printk ("%s: ne_reset_8390() did not complete.\n", dev->name);
      break;
    }
  outb (ENISR_RESET, NE_BASE + EN0_ISR);      /* Ack intr. */
}

/*
 * Grab the 8390 specific header. Similar to the block_input routine, but
 * we don't need to be concerned with ring wrap as the header will be at
 * the start of a page, so we optimize accordingly.
 */
static void ne_get_8390_hdr (struct device *dev, struct e8390_pkt_hdr *hdr,
                             int ring_page)
{
  int nic_base = dev->base_addr;

  /* This *shouldn't* happen. If it does, it's the last thing you'll see
   */
  if (ei_status.dmaing)
  {
    printk ("%s: DMAing conflict in ne_get_8390_hdr "
            "[DMAstat:%d][irqlock:%d][intr:%d].\n",
            dev->name, ei_status.dmaing, ei_status.irqlock, dev->reentry);
    return;
  }

  ei_status.dmaing |= 0x01;
  outb (E8390_NODMA + E8390_PAGE0 + E8390_START, nic_base + NE_CMD);
  outb (sizeof (struct e8390_pkt_hdr), nic_base + EN0_RCNTLO);

  outb (0, nic_base + EN0_RCNTHI);
  outb (0, nic_base + EN0_RSARLO);    /* On page boundary */
  outb (ring_page, nic_base + EN0_RSARHI);
  outb (E8390_RREAD + E8390_START, nic_base + NE_CMD);

  if (ei_status.word16)
       rep_insw (NE_BASE + NE_DATAPORT, (WORD*)hdr, sizeof(*hdr)/2);
  else rep_insb (NE_BASE + NE_DATAPORT, (BYTE*)hdr, sizeof(*hdr));

  outb (ENISR_RDC, nic_base + EN0_ISR);       /* Ack intr. */
  ei_status.dmaing &= ~0x01;
}

/*
 * Block input and output, similar to the Crynwr packet driver.  If you
 * are porting to a new ethercard, look at the packet driver source for hints.
 * The NEx000 doesn't share the on-board packet memory -- you have to put
 * the packet out through the "remote DMA" dataport using outb.
 */
static void ne_block_input (struct device *dev, int count,
                            char *buf, int ring_offset)
{
  int   xfer_count = count;
  int   nic_base   = dev->base_addr;

  /* This *shouldn't* happen. If it does, it's the last thing you'll see
   */
  if (ei_status.dmaing)
  {
    printk ("%s: DMAing conflict in ne_block_input "
            "[DMAstat:%d][irqlock:%d][intr:%d].\n",
            dev->name, ei_status.dmaing, ei_status.irqlock, dev->reentry);
    return;
  }

  ei_status.dmaing |= 0x01;
  outb (E8390_NODMA + E8390_PAGE0 + E8390_START, nic_base + NE_CMD);
  outb (count & 0xff, nic_base + EN0_RCNTLO);
  outb (count >> 8, nic_base + EN0_RCNTHI);
  outb (ring_offset & 0xff, nic_base + EN0_RSARLO);
  outb (ring_offset >> 8, nic_base + EN0_RSARHI);
  outb (E8390_RREAD + E8390_START, nic_base + NE_CMD);
  if (ei_status.word16)
  {
    rep_insw (NE_BASE + NE_DATAPORT, (WORD*)buf, count >> 1);
    if (count & 0x01)
    {
      buf[count - 1] = inb (NE_BASE + NE_DATAPORT);
      xfer_count++;
    }
  }
  else
    insb (NE_BASE + NE_DATAPORT, buf, count);

#ifdef NE_SANITY_CHECK
  /* This was for the ALPHA version only, but enough people have
   * been encountering problems so it is still here.  If you see
   * this message you either 1) have a slightly incompatible clone
   * or 2) have noise/speed problems with your bus.
   */
  if (ei_debug > 1)     /* DMA termination address check... */
  {             
    int addr, tries = 20;

    do
    {
      /* DON'T check for 'inb_p(EN0_ISR) & ENISR_RDC' here
       * -- it's broken for Rx on some cards!
       */
      int high = inb (nic_base + EN0_RSARHI);
      int low  = inb (nic_base + EN0_RSARLO);

      addr = (high << 8) + low;
      if (((ring_offset + xfer_count) & 0xff) == low)
         break;
    }
    while (--tries > 0);
    if (tries <= 0)
      printk ("%s: RX transfer address mismatch,"
              "%4x (expected) vs. %4x (actual).\n",
              dev->name, ring_offset + xfer_count, addr);
  }
#endif

  outb (ENISR_RDC, nic_base + EN0_ISR);       /* Ack intr. */
  ei_status.dmaing &= ~0x01;
}



static void ne_block_output (struct device *dev, int count, const char *buf, int start_page)
{
  int   nic_base = NE_BASE;
  DWORD dma_start;
  int   retries = 0;

  /* Round the count up for word writes.  Do we need to do this?
   * What effect will an odd byte count have on the 8390?
   * I should check someday.
   */
  if (ei_status.word16 && (count & 0x01))
    count++;

  /* This *shouldn't* happen. If it does, it's the last thing you'll see
   */
  if (ei_status.dmaing)
  {
    printk ("%s: DMAing conflict in ne_block_output."
            "[DMAstat:%d][irqlock:%d][intr:%d]\n",
            dev->name, ei_status.dmaing, ei_status.irqlock, dev->reentry);
    return;
  }

  ei_status.dmaing |= 0x01;

  /* We should already be in page 0, but to be safe...
   */
  outb (E8390_PAGE0 + E8390_START + E8390_NODMA, nic_base + NE_CMD);

retry:

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

  /* Make certain that the dummy read has occurred.
   */
  SLOW_DOWN_IO();
  SLOW_DOWN_IO();
  SLOW_DOWN_IO();
#endif

  outb (ENISR_RDC, nic_base + EN0_ISR);

  /* Now the normal output.
   */
  outb (count & 0xff, nic_base + EN0_RCNTLO);
  outb (count >> 8, nic_base + EN0_RCNTHI);
  outb (0x00, nic_base + EN0_RSARLO);
  outb (start_page, nic_base + EN0_RSARHI);

  outb (E8390_RWRITE + E8390_START, nic_base + NE_CMD);
  if (ei_status.word16)
       rep_outsw (NE_BASE + NE_DATAPORT, (WORD*)buf, count >> 1);
  else rep_outsb (NE_BASE + NE_DATAPORT, (BYTE*)buf, count);

  dma_start = jiffies;

#ifdef NE_SANITY_CHECK
  /* This was for the ALPHA version only, but enough people have
   * been encountering problems so it is still here.
   */
  if (ei_debug > 1)    /* DMA termination address check... */
  {    
    int addr, tries = 20;

    do
    {
      int high = inb (nic_base + EN0_RSARHI);
      int low  = inb (nic_base + EN0_RSARLO);

      addr = (high << 8) + low;
      if ((start_page << 8) + count == addr)
         break;
    }
    while (--tries > 0);
    if (tries <= 0)
    {
      printk ("%s: Tx packet transfer address mismatch,"
              "%4x (expected) vs. %4x (actual).\n",
              dev->name, (start_page << 8) + count, addr);
      if (retries++ == 0)
         goto retry;
    }
  }
#endif

  while ((inb (nic_base + EN0_ISR) & ENISR_RDC) == 0)
    if (jiffies - dma_start > 2 * HZ / 100)
    {				/* 20ms */
      printk ("%s: timeout waiting for Tx RDC.\n", dev->name);
      ne_reset_8390 (dev);
      NS8390_init (dev, 1);
      break;
    }

  outb (ENISR_RDC, nic_base + EN0_ISR);       /* Ack intr. */
  ei_status.dmaing &= ~0x01;
}
