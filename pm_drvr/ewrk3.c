/*  ewrk3.c: A DIGITAL EtherWORKS 3 ethernet driver for Linux.
 * 
 * Written 1994 by David C. Davies.
 * 
 * Copyright 1994 Digital Equipment Corporation.
 * 
 * This software may be used and distributed according to the terms of
 * the GNU Public License, incorporated herein by reference.
 * 
 * This driver is written for the Digital Equipment Corporation series
 * of EtherWORKS ethernet cards:
 * 
 * DE203 Turbo (BNC)
 * DE204 Turbo (TP)
 * DE205 Turbo (TP BNC)
 * 
 * The driver has been tested on a relatively busy  network using the DE205
 * card and benchmarked with 'ttcp': it transferred 16M  of data at 975kB/s
 * (7.8Mb/s) to a DECstation 5000/200.
 * 
 * The author may be reached at davies@maniac.ultranet.com.
 * 
 * =========================================================================
 * This driver has been written  substantially  from scratch, although  its
 * inheritance of style and stack interface from 'depca.c' and in turn from
 * Donald Becker's 'lance.c' should be obvious.
 * 
 * The  DE203/4/5 boards  all  use a new proprietary   chip in place of the
 * LANCE chip used in prior cards  (DEPCA, DE100, DE200/1/2, DE210, DE422).
 * Use the depca.c driver in the standard distribution  for the LANCE based
 * cards from DIGITAL; this driver will not work with them.
 * 
 * The DE203/4/5 cards have 2  main modes: shared memory  and I/O only. I/O
 * only makes  all the card accesses through  I/O transactions and  no high
 * (shared)  memory is used. This  mode provides a >48% performance penalty
 * and  is deprecated in this  driver,  although allowed to provide initial
 * setup when hardstrapped.
 * 
 * The shared memory mode comes in 3 flavours: 2kB, 32kB and 64kB. There is
 * no point in using any mode other than the 2kB  mode - their performances
 * are virtually identical, although the driver has  been tested in the 2kB
 * and 32kB modes. I would suggest you uncomment the line:
 * 
 * FORCE_2K_MODE;
 * 
 * to allow the driver to configure the card as a  2kB card at your current
 * base  address, thus leaving more  room to clutter  your  system box with
 * other memory hungry boards.
 * 
 * As many ISA  and EISA cards  can be supported  under this driver  as you
 * wish, limited primarily  by the available IRQ lines,  rather than by the
 * available I/O addresses  (24 ISA,  16 EISA).   I have  checked different
 * configurations of  multiple  depca cards and  ewrk3 cards  and have  not
 * found a problem yet (provided you have at least depca.c v0.38) ...
 * 
 * The board IRQ setting   must be at  an unused  IRQ which is  auto-probed
 * using  Donald  Becker's autoprobe  routines.   All  these cards   are at
 * {5,10,11,15}.
 * 
 * No 16MB memory  limitation should exist with this  driver as DMA is  not
 * used and the common memory area is in low memory on the network card (my
 * current system has 20MB and I've not had problems yet).
 * 
 * The ability to load  this driver as a  loadable module has been included
 * and used  extensively during the  driver development (to save those long
 * reboot sequences). To utilise this ability, you have to do 8 things:
 * 
 * 0) have a copy of the loadable modules code installed on your system.
 * 1) copy ewrk3.c from the  /linux/drivers/net directory to your favourite
 * temporary directory.
 * 2) edit the  source code near  line 1898 to reflect  the I/O address and
 * IRQ you're using.
 * 3) compile  ewrk3.c, but include -DMODULE in  the command line to ensure
 * that the correct bits are compiled (see end of source code).
 * 4) if you are wanting to add a new  card, goto 5. Otherwise, recompile a
 * kernel with the ewrk3 configuration turned off and reboot.
 * 5) insmod ewrk3.o
 * [Alan Cox: Changed this so you can insmod ewrk3.o irq=x io=y]
 * 6) run the net startup bits for your new eth?? interface manually
 * (usually /etc/rc.inet[12] at boot time).
 * 7) enjoy!
 * 
 * Note that autoprobing is not allowed in loadable modules - the system is
 * already up and running and you're messing with interrupts.
 * 
 * To unload a module, turn off the associated interface
 * 'ifconfig eth?? down' then 'rmmod ewrk3'.
 * 
 * Promiscuous   mode has been  turned  off  in this driver,   but  all the
 * multicast  address bits  have been   turned on. This  improved the  send
 * performance on a busy network by about 13%.
 * 
 * Ioctl's have now been provided (primarily because  I wanted to grab some
 * packet size statistics). They  are patterned after 'plipconfig.c' from a
 * suggestion by Alan Cox.  Using these  ioctls, you can enable promiscuous
 * mode, add/delete multicast  addresses, change the hardware address,  get
 * packet size distribution statistics and muck around with the control and
 * status register. I'll add others if and when the need arises.
 * 
 * TO DO:
 * ------
 * 
 * 
 * Revision History
 * ----------------
 * 
 * Version   Date        Description
 * 
 * 0.1     26-aug-94   Initial writing. ALPHA code release.
 * 0.11    31-aug-94   Fixed: 2k mode memory base calc.,
 * LeMAC version calc.,
 * IRQ vector assignments during autoprobe.
 * 0.12    31-aug-94   Tested working on LeMAC2 (DE20[345]-AC) card.
 * Fixed up MCA hash table algorithm.
 * 0.20     4-sep-94   Added IOCTL functionality.
 * 0.21    14-sep-94   Added I/O mode.
 * 0.21axp 15-sep-94   Special version for ALPHA AXP Linux V1.0.
 * 0.22    16-sep-94   Added more IOCTLs & tidied up.
 * 0.23    21-sep-94   Added transmit cut through.
 * 0.24    31-oct-94   Added uid checks in some ioctls.
 * 0.30     1-nov-94   BETA code release.
 * 0.31     5-dec-94   Added check/allocate region code.
 * 0.32    16-jan-95   Broadcast packet fix.
 * 0.33    10-Feb-95   Fix recognition bug reported by <bkm@star.rl.ac.uk>.
 * 0.40    27-Dec-95   Rationalise MODULE and autoprobe code.
 * Rewrite for portability & updated.
 * ALPHA support from <jestabro@amt.tay1.dec.com>
 * Added verify_area() calls in ewrk3_ioctl() from
 * suggestion by <heiko@colossus.escape.de>.
 * Add new multicasting code.
 * 0.41    20-Jan-96   Fix IRQ set up problem reported by
 * <kenneth@bbs.sas.ntu.ac.sg>.
 * 0.42    22-Apr-96    Fix alloc_device() bug <jari@markkus2.fimr.fi>
 * 0.43    16-Aug-96    Update alloc_device() to conform to de4x5.c
 * 
 * =========================================================================
 */

static const char *version = "ewrk3.c:v0.43 96/8/16 davies@maniac.ultranet.com\n";

#include "pmdrvr.h"
#include "ewrk3.h"

#ifndef EWRK3_DEBUG
#define EWRK3_DEBUG 1
#endif

int ewrk3_debug = EWRK3_DEBUG;

#define EWRK3_NDA 0xffe0	/* No Device Address */

#define PROBE_LENGTH    32
#define ETH_PROM_SIG    0xAA5500FFUL

#ifndef EWRK3_SIGNATURE
#define EWRK3_SIGNATURE {"DE203","DE204","DE205",""}
#define EWRK3_STRLEN 8
#endif

#ifndef EWRK3_RAM_BASE_ADDRESSES
#define EWRK3_RAM_BASE_ADDRESSES {0xc0000,0xd0000,0x00000}
#endif

/*
 * ** Sets up the I/O area for the autoprobe.
 */
#define EWRK3_IO_BASE 0x100	/* Start address for probe search */
#define EWRK3_IOP_INC 0x20	/* I/O address increment */
#define EWRK3_TOTAL_SIZE 0x20	/* required I/O address length */

#ifndef MAX_NUM_EWRK3S
#define MAX_NUM_EWRK3S 21
#endif

#ifndef EWRK3_EISA_IO_PORTS
#define EWRK3_EISA_IO_PORTS 0x0c00	/* I/O port base address, slot 0 */
#endif

#ifndef MAX_EISA_SLOTS
#define MAX_EISA_SLOTS 16
#define EISA_SLOT_INC 0x1000
#endif

#define CRC_POLYNOMIAL_BE 0x04c11db7UL	/* Ethernet CRC, big endian */
#define CRC_POLYNOMIAL_LE 0xedb88320UL	/* Ethernet CRC, little endian */

#define QUEUE_PKT_TIMEOUT (1*HZ)	/* Jiffies */

/*
 * ** EtherWORKS 3 shared memory window sizes
 */
#define IO_ONLY         0x00
#define SHMEM_2K        0x800
#define SHMEM_32K       0x8000
#define SHMEM_64K       0x10000

/*
 * ** EtherWORKS 3 IRQ ENABLE/DISABLE
 */
#define ENABLE_IRQs { \
        icr |= lp->irq_mask;\
        outb(icr, EWRK3_ICR);         /* Enable the IRQs */\
      }

#define DISABLE_IRQs { \
        icr = inb(EWRK3_ICR);\
        icr &= ~lp->irq_mask;\
        outb(icr, EWRK3_ICR);         /* Disable the IRQs */\
      }

/*
 * ** EtherWORKS 3 START/STOP
 */
#define START_EWRK3 { \
        csr = inb(EWRK3_CSR);\
        csr &= ~(CSR_TXD|CSR_RXD);\
        outb(csr, EWRK3_CSR);         /* Enable the TX and/or RX */\
      }

#define STOP_EWRK3 { \
        csr = (CSR_TXD|CSR_RXD);\
        outb(csr, EWRK3_CSR);         /* Disable the TX and/or RX */\
      }

/*
 * ** The EtherWORKS 3 private structure
 */
#define EWRK3_PKT_STAT_SZ 16
#define EWRK3_PKT_BIN_SZ  128	/* Should be >=100 unless you
				 * increase EWRK3_PKT_STAT_SZ */

struct ewrk3_private {
  char   adapter_name[80];          /* Name exported to /proc/ioports */
  DWORD  shmem_base;                /* Shared memory start address */
  DWORD  shmem_length;              /* Shared memory window length */
  struct net_device_stats stats;    /* Public stats */
  struct {
    DWORD bins[EWRK3_PKT_STAT_SZ];  /* Private stats counters */
    DWORD unicast;
    DWORD multicast;
    DWORD broadcast;
    DWORD excessive_collisions;
    DWORD tx_underruns;
    DWORD excessive_underruns;
  } pktStats;
  BYTE irq_mask;		/* Adapter IRQ mask bits */
  BYTE mPage;			/* Maximum 2kB Page number */
  BYTE lemac;			/* Chip rev. level */
  BYTE hard_strapped;		/* Don't allow a full open */
  BYTE lock;			/* Lock the page register */
  BYTE txc;			/* Transmit cut through */
  BYTE *mctbl;			/* Pointer to the multicast table */
};

/*
 * ** Force the EtherWORKS 3 card to be in 2kB MODE
 */
#define FORCE_2K_MODE { \
        shmem_length = SHMEM_2K;\
        outb(((mem_start - 0x80000) >> 11), EWRK3_MBR);\
      }

/*
 * ** Public Functions
 */
static int   ewrk3_open (struct device *dev);
static int   ewrk3_queue_pkt (struct sk_buff *skb, struct device *dev);
static void  ewrk3_interrupt (int irq);
static int   ewrk3_close (struct device *dev);
static void *ewrk3_get_stats (struct device *dev);
static void  set_multicast_list (struct device *dev);
static int   ewrk3_ioctl (struct device *dev, struct ifreq *rq, int cmd);

/*
 * ** Private functions
 */
static void ewrk3_init (struct device *dev);
static int  ewrk3_rx (struct device *dev);
static int  ewrk3_tx (struct device *dev);

static void EthwrkSignature (char *name, char *eeprom_image);
static int  DevicePresent (DWORD iobase);
static void SetMulticastFilter (struct device *dev);
static int  EISA_signature (char *name, s32 eisa_id);

static int  Read_EEPROM (DWORD iobase, BYTE eaddr);
static int  Write_EEPROM (short data, DWORD iobase, BYTE eaddr);
static int  get_hw_addr (struct device *dev, BYTE *eeprom_image, char chipType);

static void isa_probe (struct device *dev, DWORD iobase);
static void eisa_probe (struct device *dev, DWORD iobase);
static struct device *alloc_device (struct device *dev, DWORD iobase);
static int ewrk3_dev_index (char *s);
static struct device *insert_device (struct device *dev, DWORD iobase, int (*init) (struct device *));

static BYTE irq[] = { 5, 0, 10, 3, 11, 9, 15, 12 };
static int  autoprobed = 0;

static char name[EWRK3_STRLEN + 1];
static int  num_ewrk3s = 0, num_eth = 0;

/*
 * ** Miscellaneous defines...
 */
#define INIT_EWRK3() {                 \
        outb(EEPROM_INIT, EWRK3_IOPR); \
        udelay(1000);                  \
      }


int ewrk3_probe (struct device *dev)
{
  int   tmp = num_ewrk3s, status = -ENODEV;
  DWORD iobase = dev->base_addr;

  if (iobase == 0)
  {
    printk ("Autoprobing is not supported when loading a module based driver.\n");
    status = 0;
  }
  else
  {				/* First probe for the Ethernet */
    /* Address PROM pattern */
    isa_probe (dev, iobase);
    eisa_probe (dev, iobase);

    if (tmp == num_ewrk3s && iobase)
       printk ("%s: ewrk3_probe() cannot find device at 0x%04lx.\n",
               dev->name, iobase);

    /* Walk the device list to check that at least one device
     * initialised OK
     */
    for ( ; dev->priv == NULL && dev->next; dev = dev->next)
        ;

    if (dev->priv)
      status = 1;
    if (iobase == 0)
      autoprobed = 1;
  }
  return (status);
}


static int ewrk3_hw_init (struct device *dev, DWORD iobase)
{
  struct ewrk3_private *lp;
  int    i, hw_valid;
  DWORD  mem_start, shmem_length;
  BYTE   cr, cmr, icr, nicsr;
  BYTE   lemac, hard_strapped = 0;
  BYTE   chksum, eisa_cr = 0;
  BYTE   eeprom_image[EEPROM_MAX];

  /* Stop the EWRK3. Enable the DBR ROM. Disable interrupts and remote boot.
   * This also disables the EISA_ENABLE bit in the EISA Control Register.
   */
  if (iobase > 0x400)
     eisa_cr = inb (EISA_CR);

  INIT_EWRK3;
  nicsr = inb (EWRK3_CSR);

  icr  = inb (EWRK3_ICR);
  icr &= 0x70;
  outb (icr, EWRK3_ICR);	/* Disable all the IRQs */

  if (nicsr != (CSR_TXD | CSR_RXD))
     return (0);

  /* Check that the EEPROM is alive and well and not living on Pluto...
   */
  for (chksum = 0, i = 0; i < EEPROM_MAX; i += 2)
  {
    union {
      short val;
      char  c[2];
    } tmp;

    tmp.val = (short)Read_EEPROM (iobase, (i >> 1));
    eeprom_image[i]   = tmp.c[0];
    eeprom_image[i+1] = tmp.c[1];
    chksum += eeprom_image[i] + eeprom_image[i + 1];
  }

  if (chksum)
  {                           /* Bad EEPROM Data! */
    printk ("%s: Device has a bad on-board EEPROM.\n", dev->name);
    return (0);
  }

  EthwrkSignature (name, eeprom_image);
  if (*name == '\0')
     return (0);
                          /* found a EWRK3 device */
  dev->base_addr = iobase;

  if (iobase > 0x400)
     outb (eisa_cr, EISA_CR);      /* Rewrite the EISA CR */

  lemac = eeprom_image[EEPROM_CHIPVER];
  cmr = inb (EWRK3_CMR);

  if ((lemac == LeMAC && ((cmr & CMR_NO_EEPROM) != CMR_NO_EEPROM) ||
      (lemac == LeMAC2 && !(cmr & CMR_HS)))
  {
    printk ("%s: %s at %4lx", dev->name, name, iobase);
    hard_strapped = 1;
  }
  else if ((iobase & 0x0fff) == EWRK3_EISA_IO_PORTS)
  {
    /* EISA slot address */
    printk ("%s: %s at %4lx (EISA slot %ld)", dev->name, name, iobase,
            ((iobase >> 12) & 0x0f));
  }
  else            /* ISA port address */
    printk ("%s: %s at %#4lx", dev->name, name, iobase);

  printk (", h/w address ");
  if (lemac != LeMAC2)
     DevicePresent (iobase);     /* need after EWRK3_INIT */

  hw_valid = get_hw_addr (dev, eeprom_image, lemac);

  for (i = 0; i < ETH_ALEN - 1; i++)
     printk ("%2.2x:", dev->dev_addr[i]);
  printk ("%2.2x,\n", dev->dev_addr[i]);

  if (!hw_valid)
  {
    printk ("      which has an EEPROM CRC error.\n");
    return (0);
  }

  if (lemac == LeMAC2)
  {                   /* Special LeMAC2 CMR things */
    cmr &= ~(CMR_RA | CMR_WB | CMR_LINK | CMR_POLARITY | CMR_0WS);
    if (eeprom_image[EEPROM_MISC0] & READ_AHEAD)
      cmr |= CMR_RA;
    if (eeprom_image[EEPROM_MISC0] & WRITE_BEHIND)
      cmr |= CMR_WB;
    if (eeprom_image[EEPROM_NETMAN0] & NETMAN_POL)
      cmr |= CMR_POLARITY;
    if (eeprom_image[EEPROM_NETMAN0] & NETMAN_LINK)
      cmr |= CMR_LINK;
    if (eeprom_image[EEPROM_MISC0] & _0WS_ENA)
      cmr |= CMR_0WS;
  }
  if (eeprom_image[EEPROM_SETUP] & SETUP_DRAM)
     cmr |= CMR_DRAM;
  outb (cmr, EWRK3_CMR);

  cr = inb (EWRK3_CR);        /* Set up the Control Register */
  cr |= eeprom_image[EEPROM_SETUP] & SETUP_APD;
  if (cr & SETUP_APD)
    cr |= eeprom_image[EEPROM_SETUP] & SETUP_PS;
  cr |= eeprom_image[EEPROM_MISC0] & FAST_BUS;
  cr |= eeprom_image[EEPROM_MISC0] & ENA_16;
  outb (cr, EWRK3_CR);

  /* Determine the base address and window length for the EWRK3
   * RAM from the memory base register.
   */
  mem_start = inb (EWRK3_MBR);
  shmem_length = 0;
  if (mem_start != 0)
  {
    if ((mem_start >= 0x0a) && (mem_start <= 0x0f))
    {
      mem_start *= SHMEM_64K;
      shmem_length = SHMEM_64K;
    }
    else if ((mem_start >= 0x14) && (mem_start <= 0x1f))
    {
      mem_start *= SHMEM_32K;
      shmem_length = SHMEM_32K;
    }
    else if ((mem_start >= 0x40) && (mem_start <= 0xff))
    {
      mem_start = mem_start * SHMEM_2K + 0x80000;
      shmem_length = SHMEM_2K;
    }
    else
      return (0);
  }

  /* See the top of this source code for comments about
   * uncommenting this line.
   */
  /* FORCE_2K_MODE; */

  if (hard_strapped)
     printk ("      is hard strapped.\n");
  else if (mem_start)
  {
    printk ("      has a %dk RAM window", (int)(shmem_length >> 10));
    printk (" at 0x%.5lx", mem_start);
  }
  else
    printk ("      is in I/O only mode");

  /* private area & initialise
   */
  dev->priv = k_calloc (sizeof(struct ewrk3_private), 1);
  if (!dev->priv)
     return (0);

  lp = (struct ewrk3_private*) dev->priv;
  lp->shmem_base    = mem_start;
  lp->shmem_length  = shmem_length;
  lp->lemac         = lemac;
  lp->hard_strapped = hard_strapped;

  lp->mPage = 64;
  if (cmr & CMR_DRAM)
     lp->mPage <<= 1;        /* 2 DRAMS on module */

  sprintf (lp->adapter_name, "%s (%s)", name, dev->name);
  request_region (iobase, EWRK3_TOTAL_SIZE, lp->adapter_name);

  lp->irq_mask = ICR_TNEM | ICR_TXDM | ICR_RNEM | ICR_RXDM;

  if (!hard_strapped)
  {
    /* Enable EWRK3 board interrupts for autoprobing
     */
    icr |= ICR_IE;  /* Enable interrupts */
    outb (icr, EWRK3_ICR);

    /* The DMA channel may be passed in on this parameter.
     */
    dev->dma = 0;

    /* To auto-IRQ we enable the initialization-done and DMA err,
     * interrupts. For now we will always get a DMA error.
     */
    if (dev->irq < 2)
    {
      BYTE irqnum;

      autoirq_setup (0);

      /* Trigger a TNE interrupt.
       */
      icr |= ICR_TNEM;
      outb (1, EWRK3_TDQ);    /* Write to the TX done queue */
      outb (icr, EWRK3_ICR);  /* Unmask the TXD interrupt */

      irqnum = irq [(icr & IRQ_SEL) >> 4];

      dev->irq = autoirq_report (1);
      DISABLE_IRQs;                     /* Mask all interrupts */
      if (dev->irq && irqnum == dev->irq)
         printk (" and uses IRQ%d.\n", dev->irq);
      else
      {
        if (!dev->irq)
             printk (" and failed to detect IRQ line.\n");
        else if ((irqnum == 1) && (lemac == LeMAC2))
             printk (" and an illegal IRQ line detected.\n");
        else printk (", but incorrect IRQ line detected.\n");
        return (0);
      }
    }
    else
      printk (" and requires IRQ%d.\n", dev->irq);
  }

  if (ewrk3_debug > 1)
     printk (version);

  /* The EWRK3-specific entries in the device structure.
   */
  dev->open  = ewrk3_open;
  dev->xmit  = ewrk3_queue_pkt;
  dev->close = ewrk3_close;
  dev->get_stats = ewrk3_get_stats;
  dev->set_multicast_list = set_multicast_list;
  dev->mem_start = 0;

  /* Fill in the generic field of the device structure.
   */
  ether_setup (dev);
  return (1);
}


static int ewrk3_open (struct device *dev)
{
  struct ewrk3_private *lp = (struct ewrk3_private *) dev->priv;
  DWORD  iobase = dev->base_addr;
  int    i;
  BYTE   icr, csr;

  /* Stop the TX and RX...
   */
  STOP_EWRK3;

  if (!lp->hard_strapped)
  {
    irq2dev_map[dev->irq] = dev;	/* For latched interrupts */

    if (!request_irq (dev->irq, ewrk3_interrupt))
    {
      printk ("ewrk3_open(): Requested IRQ%d is busy\n", dev->irq);
      return (0);
    }

    /* Re-initialize the EWRK3...
     */
    ewrk3_init (dev);

    if (ewrk3_debug > 1)
    {
      printk ("%s: ewrk3 open with irq %d\n", dev->name, dev->irq);
      printk ("  physical address: ");
      for (i = 0; i < 5; i++)
         printk ("%2.2x:", (BYTE)dev->dev_addr[i]);
      printk ("%2.2x\n", (BYTE)dev->dev_addr[i]);
      if (lp->shmem_length == 0)
        printk ("  no shared memory, I/O only mode\n");
      else
      {
        printk ("  start of shared memory: 0x%08lx\n", lp->shmem_base);
        printk ("  window length: 0x%04lx\n", lp->shmem_length);
      }
      printk ("  # of DRAMS: %d\n", ((inb (EWRK3_CMR) & 0x02) ? 2 : 1));
      printk ("  csr:  0x%02x\n", inb (EWRK3_CSR));
      printk ("  cr:   0x%02x\n", inb (EWRK3_CR));
      printk ("  icr:  0x%02x\n", inb (EWRK3_ICR));
      printk ("  cmr:  0x%02x\n", inb (EWRK3_CMR));
      printk ("  fmqc: 0x%02x\n", inb (EWRK3_FMQC));
    }

    dev->tx_busy = 0;
    dev->start   = 1;
    dev->reentry = UNMASK_INTERRUPTS;

    /* Unmask EWRK3 board interrupts
     */
    icr = inb (EWRK3_ICR);
    ENABLE_IRQs;
  }
  else
  {
    dev->start   = 0;
    dev->tx_busy = 1;
    printk ("%s: ewrk3 available for hard strapped set up only.\n", dev->name);
  }
  return (1);
}


/*
 * Initialize the EtherWORKS 3 operating conditions
 */
static void ewrk3_init (struct device *dev)
{
  struct ewrk3_private *lp = (struct ewrk3_private *) dev->priv;
  BYTE   csr, page;
  DWORD  iobase = dev->base_addr;

  /* Enable any multicasts
   */
  set_multicast_list (dev);

  /* Clean out any remaining entries in all the queues here
   */
  while (inb (EWRK3_TQ))  ;
  while (inb (EWRK3_TDQ)) ;
  while (inb (EWRK3_RQ))  ;
  while (inb (EWRK3_FMQ)) ;

  /* Write a clean free memory queue
   */
  for (page = 1; page < lp->mPage; page++)
  {				/* Write the free page numbers */
    outb (page, EWRK3_FMQ);	/* to the Free Memory Queue */
  }

  lp->lock = 0;			/* Ensure there are no locks */
  START_EWRK3;			/* Enable the TX and/or RX */
}

/*
 * Writes a socket buffer to the free page queue
 */
static int ewrk3_queue_pkt (struct device *dev, void *tx_buf, int len)
{
  struct ewrk3_private *lp = (struct ewrk3_private *) dev->priv;
  DWORD  iobase = dev->base_addr;
  int    status = 0;
  BYTE   icr, csr;

  /* Transmitter timeout, serious problems.
   */
  if (dev->tx_busy || lp->lock)
  {
    int tickssofar = jiffies - dev->trans_start;

    if (tickssofar < QUEUE_PKT_TIMEOUT)
       status = -1;

    else if (!lp->hard_strapped)
    {
      printk ("%s: transmit timed/locked out, status %04x, resetting.\n",
              dev->name, inb (EWRK3_CSR));

      /* Mask all board interrupts
       */
      DISABLE_IRQs;

      /* Stop the TX and RX...
       */
      STOP_EWRK3;
      ewrk3_init (dev);

      /* Unmask EWRK3 board interrupts
       */
      ENABLE_IRQs;

      dev->tx_busy = 0;
      dev->tx_start = jiffies;
    }
  }
  else if (len > 0)
  {
    /* Block a timer-based transmit from overlapping.  This could better be
     * done with atomic_swap(1, dev->tbusy), but set_bit() works as well.
     */
    if (set_bit (0, (void*)&dev->tx_busy))
      printk ("%s: Transmitter access conflict.\n", dev->name);

    DISABLE_IRQs;		/* So that the page # remains correct */

    /*
     * ** Get a free page from the FMQ when resources are available
     */
    if (inb (EWRK3_FMQC) > 0)
    {
      DWORD buf = 0;
      BYTE  page;

      if ((page = inb (EWRK3_FMQ)) < lp->mPage)
      {
        /* Set up shared memory window and pointer into the window
	 */
        while (set_bit (0, (void*)&lp->lock))   /* Wait for lock to free */
              ;

	if (lp->shmem_length == IO_ONLY)
	{
	  outb (page, EWRK3_IOPR);
	}
	else if (lp->shmem_length == SHMEM_2K)
	{
	  buf = lp->shmem_base;
	  outb (page, EWRK3_MPR);
	}
	else if (lp->shmem_length == SHMEM_32K)
	{
	  buf = ((((short) page << 11) & 0x7800) + lp->shmem_base);
	  outb ((page >> 4), EWRK3_MPR);
	}
	else if (lp->shmem_length == SHMEM_64K)
	{
	  buf = ((((short) page << 11) & 0xf800) + lp->shmem_base);
	  outb ((page >> 5), EWRK3_MPR);
	}
	else
	{
	  status = -1;
	  printk ("%s: Oops - your private data area is hosed!\n", dev->name);
	}

	if (!status)
	{
          /* Set up the buffer control structures and copy the data from
           * the socket buffer to the shared memory .
	   */
	  if (lp->shmem_length == IO_ONLY)
	  {
            int   i;
            BYTE *p = (BYTE*)tx_buf;

	    outb ((char) (TCR_QMODE | TCR_PAD | TCR_IFC), EWRK3_DATA);
            outb ((char) (len & 0xff), EWRK3_DATA);
            outb ((char) ((len >> 8) & 0xff), EWRK3_DATA);
	    outb ((char) 0x04, EWRK3_DATA);
            for (i = 0; i < len; i++)
              outb (*p++, EWRK3_DATA);
	    outb (page, EWRK3_TQ);	/* Start sending pkt */
	  }
	  else
	  {
            writeb ((char)(TCR_QMODE | TCR_PAD | TCR_IFC), (char*)buf); /* ctrl byte */
            buf++;
            writeb ((char) (skb->len & 0xff), (char*)buf);   /* length (16 bit xfer) */
            buf++;
	    if (lp->txc)
	    {
              writeb ((char) (((len >> 8) & 0xff) | XCT), (char*)buf);
              buf++;
              writeb (0x04, (char*)buf);      /* index byte */
              buf++;
              writeb (0x00, (char*)(tx_buf + len)); /* Write the XCT flag */
              memcpy_toio (buf, tx_buf, PRELOAD);   /* Write PRELOAD bytes */
              outb (page, EWRK3_TQ);                /* Start sending pkt */
              memcpy_toio (buf + PRELOAD, (char*)tx_buf + PRELOAD, len - PRELOAD);
              writeb (0xff, (char*)(buf + len));    /* Write the XCT flag */
	    }
	    else
	    {
	      writeb ((char) ((skb->len >> 8) & 0xff), (char *) buf);
	      buf += 1;
              writeb (0x04, (char*)buf);      /* index byte */
	      buf += 1;
              memcpy_toio ((char*)buf, skb->data, skb->len);  /* Write data bytes */
	      outb (page, EWRK3_TQ);	/* Start sending pkt */
	    }
	  }

	  dev->trans_start = jiffies;
	  dev_kfree_skb (skb, FREE_WRITE);

	}
        else           /* return unused page to the free memory queue */
	  outb (page, EWRK3_FMQ);

        lp->lock = 0;  /* unlock the page register */
      }
      else
        printk ("ewrk3_queue_pkt(): Invalid free memory page (%d).\n",
                (BYTE)page);
    }
    else
    {
      printk ("ewrk3_queue_pkt(): No free resources...\n");
      printk ("ewrk3_queue_pkt(): CSR: %02x ICR: %02x FMQC: %02x\n",
              inb(EWRK3_CSR), inb(EWRK3_ICR), inb(EWRK3_FMQC));
    }

    /* Check for free resources: clear 'tbusy' if there are some
     */
    if (inb (EWRK3_FMQC) > 0)
      dev->tx_busy = 0;
    ENABLE_IRQs;
  }
  return (status);
}

/*
 * ** The EWRK3 interrupt handler.
 */
static void ewrk3_interrupt (int irq)
{
  struct device        *dev = irq2dev_map[irq];
  struct ewrk3_private *lp;
  DWORD  iobase;
  BYTE   icr, cr, csr;

  if (!dev)
     printk ("ewrk3_interrupt(): irq %d for unknown device.\n", irq);
  else
  {
    lp = (struct ewrk3_private *) dev->priv;
    iobase = dev->base_addr;

    if (dev->interrupt)
      printk ("%s: Re-entering the interrupt handler.\n", dev->name);

    dev->interrupt = MASK_INTERRUPTS;

    /* get the interrupt information */
    csr = inb (EWRK3_CSR);

    /* Mask the EWRK3 board interrupts and turn on the LED
     */
    DISABLE_IRQs;

    cr = inb (EWRK3_CR);
    cr |= CR_LED;
    outb (cr, EWRK3_CR);

    if (csr & CSR_RNE)		/* Rx interrupt (packet[s] arrived) */
      ewrk3_rx (dev);

    if (csr & CSR_TNE)		/* Tx interrupt (packet sent) */
      ewrk3_tx (dev);

    /* Now deal with the TX/RX disable flags. These are set when there
     * are no more resources. If resources free up then enable these
     * interrupts, otherwise mask them - failure to do this will result
     * in the system hanging in an interrupt loop.
     */
    if (inb (EWRK3_FMQC))
    {				/* any resources available? */
      lp->irq_mask |= ICR_TXDM | ICR_RXDM;	/* enable the interrupt source */
      csr &= ~(CSR_TXD | CSR_RXD);	/* ensure restart of a stalled TX or RX */
      outb (csr, EWRK3_CSR);
      dev->tbusy = 0;		/* clear TX busy flag */
      mark_bh (NET_BH);
    }
    else
      lp->irq_mask &= ~(ICR_TXDM | ICR_RXDM);   /* disable the interrupt source */

    /* Unmask the EWRK3 board interrupts and turn off the LED
     */
    cr &= ~CR_LED;
    outb (cr, EWRK3_CR);

    dev->interrupt = UNMASK_INTERRUPTS;
    ENABLE_IRQs;
  }
}

static int ewrk3_rx (struct device *dev)
{
  struct ewrk3_private *lp = (struct ewrk3_private *) dev->priv;
  DWORD  iobase = dev->base_addr;
  DWORD  buf = 0;
  BYTE   page, tmpPage = 0, tmpLock = 0;
  int    i, status = 0;

  while (inb (EWRK3_RQC) && !status)
  {				/* Whilst there's incoming data */
    if ((page = inb (EWRK3_RQ)) < lp->mPage)
    {				/* Get next entry's buffer page */
      /*
       * ** Preempt any process using the current page register. Check for
       * ** an existing lock to reduce time taken in I/O transactions.
       */
      if ((tmpLock = set_bit (0,(void*)&lp->lock)) == 1) /* Assert lock */
      {                         
        if (lp->shmem_length == IO_ONLY)    /* Get existing page */
             tmpPage = inb (EWRK3_IOPR);
        else tmpPage = inb (EWRK3_MPR);
      }

      /* Set up shared memory window and pointer into the window
       */
      if (lp->shmem_length == IO_ONLY)
      {
	outb (page, EWRK3_IOPR);
      }
      else if (lp->shmem_length == SHMEM_2K)
      {
	buf = lp->shmem_base;
	outb (page, EWRK3_MPR);
      }
      else if (lp->shmem_length == SHMEM_32K)
      {
	buf = ((((short) page << 11) & 0x7800) + lp->shmem_base);
	outb ((page >> 4), EWRK3_MPR);
      }
      else if (lp->shmem_length == SHMEM_64K)
      {
	buf = ((((short) page << 11) & 0xf800) + lp->shmem_base);
	outb ((page >> 5), EWRK3_MPR);
      }
      else
      {
	status = -1;
	printk ("%s: Oops - your private data area is hosed!\n", dev->name);
      }

      if (!status)
      {
	char rx_status;
	int pkt_len;

	if (lp->shmem_length == IO_ONLY)
	{
	  rx_status = inb (EWRK3_DATA);
	  pkt_len = inb (EWRK3_DATA);
	  pkt_len |= ((WORD) inb (EWRK3_DATA) << 8);
	}
	else
	{
	  rx_status = readb (buf);
	  buf += 1;
	  pkt_len = readw (buf);
	  buf += 3;
	}

	if (!(rx_status & R_ROK))
	{			/* There was an error. */
	  lp->stats.rx_errors++;	/* Update the error stats. */
	  if (rx_status & R_DBE)
	    lp->stats.rx_frame_errors++;
	  if (rx_status & R_CRC)
	    lp->stats.rx_crc_errors++;
	  if (rx_status & R_PLL)
	    lp->stats.rx_fifo_errors++;
	}
	else
	{
	  struct sk_buff *skb;

	  if ((skb = dev_alloc_skb (pkt_len + 2)) != NULL)
	  {
	    BYTE *p;

	    skb->dev = dev;
	    skb_reserve (skb, 2);	/* Align to 16 bytes */
	    p = skb_put (skb, pkt_len);

	    if (lp->shmem_length == IO_ONLY)
	    {
	      *p = inb (EWRK3_DATA);	/* dummy read */
	      for (i = 0; i < pkt_len; i++)
	      {
		*p++ = inb (EWRK3_DATA);
	      }
	    }
	    else
	    {
	      memcpy_fromio (p, buf, pkt_len);
	    }

	    /*
	     * ** Notify the upper protocol layers that there is another
	     * ** packet to handle
	     */
	    skb->protocol = eth_type_trans (skb, dev);
	    netif_rx (skb);

	    /*
	     * ** Update stats
	     */
	    lp->stats.rx_packets++;
	    for (i = 1; i < EWRK3_PKT_STAT_SZ - 1; i++)
	    {
	      if (pkt_len < i * EWRK3_PKT_BIN_SZ)
	      {
		lp->pktStats.bins[i]++;
		i = EWRK3_PKT_STAT_SZ;
	      }
	    }
	    p = skb->data;	/* Look at the dest addr */
	    if (p[0] & 0x01)
	    {			/* Multicast/Broadcast */
	      if ((*(s32 *) & p[0] == -1) && (*(s16 *) & p[4] == -1))
	      {
		lp->pktStats.broadcast++;
	      }
	      else
	      {
		lp->pktStats.multicast++;
	      }
	    }
	    else if ((*(s32 *) & p[0] == *(s32 *) & dev->dev_addr[0]) &&
		     (*(s16 *) & p[4] == *(s16 *) & dev->dev_addr[4]))
	    {
	      lp->pktStats.unicast++;
	    }

	    lp->pktStats.bins[0]++;	/* Duplicates stats.rx_packets */
	    if (lp->pktStats.bins[0] == 0)
	    {			/* Reset counters */
	      memset (&lp->pktStats, 0, sizeof (lp->pktStats));
	    }
	  }
	  else
	  {
	    printk ("%s: Insufficient memory; nuking packet.\n", dev->name);
	    lp->stats.rx_dropped++;	/* Really, deferred. */
	    break;
	  }
	}
      }
      /*
       * ** Return the received buffer to the free memory queue
       */
      outb (page, EWRK3_FMQ);

      if (tmpLock)
      {				/* If a lock was preempted */
	if (lp->shmem_length == IO_ONLY)
	{			/* Replace old page */
	  outb (tmpPage, EWRK3_IOPR);
	}
	else
	{
	  outb (tmpPage, EWRK3_MPR);
	}
      }
      lp->lock = 0;		/* Unlock the page register */
    }
    else
    {
      printk ("ewrk3_rx(): Illegal page number, page %d\n", page);
      printk ("ewrk3_rx(): CSR: %02x ICR: %02x FMQC: %02x\n", inb (EWRK3_CSR), inb (EWRK3_ICR), inb (EWRK3_FMQC));
    }
  }
  return status;
}

/*
 * ** Buffer sent - check for TX buffer errors.
 */
static int ewrk3_tx (struct device *dev)
{
  struct ewrk3_private *lp = (struct ewrk3_private *) dev->priv;
  DWORD iobase = dev->base_addr;
  BYTE tx_status;

  while ((tx_status = inb (EWRK3_TDQ)) > 0)
  {				/* Whilst there's old buffers */
    if (tx_status & T_VSTS)
    {				/* The status is valid */
      if (tx_status & T_TXE)
      {
	lp->stats.tx_errors++;
	if (tx_status & T_NCL)
	  lp->stats.tx_carrier_errors++;
	if (tx_status & T_LCL)
	  lp->stats.tx_window_errors++;
	if (tx_status & T_CTU)
	{
	  if ((tx_status & T_COLL) ^ T_XUR)
	  {
	    lp->pktStats.tx_underruns++;
	  }
	  else
	  {
	    lp->pktStats.excessive_underruns++;
	  }
	}
	else if (tx_status & T_COLL)
	{
	  if ((tx_status & T_COLL) ^ T_XCOLL)
	  {
            lp->stats.tx_collisions++;
	  }
	  else
	  {
	    lp->pktStats.excessive_collisions++;
	  }
	}
      }
      else
      {
	lp->stats.tx_packets++;
      }
    }
  }

  return 0;
}

static int ewrk3_close (struct device *dev)
{
  struct ewrk3_private *lp = (struct ewrk3_private *) dev->priv;
  DWORD iobase = dev->base_addr;
  BYTE icr, csr;

  dev->start = 0;
  dev->tbusy = 1;

  if (ewrk3_debug > 1)
  {
    printk ("%s: Shutting down ethercard, status was %2.2x.\n", dev->name, inb (EWRK3_CSR));
  }

  /*
   * ** We stop the EWRK3 here... mask interrupts and stop TX & RX
   */
  DISABLE_IRQs;

  STOP_EWRK3;

  /*
   * ** Clean out the TX and RX queues here (note that one entry
   * ** may get added to either the TXD or RX queues if the the TX or RX
   * ** just starts processing a packet before the STOP_EWRK3 command
   * ** is received. This will be flushed in the ewrk3_open() call).
   */
  while (inb (EWRK3_TQ)) ;
  while (inb (EWRK3_TDQ)) ;
  while (inb (EWRK3_RQ)) ;

  if (!lp->hard_strapped)
  {
    free_irq (dev->irq, NULL);

    irq2dev_map[dev->irq] = 0;
  }

  MOD_DEC_USE_COUNT;

  return 0;
}

static struct net_device_stats *ewrk3_get_stats (struct device *dev)
{
  struct ewrk3_private *lp = (struct ewrk3_private *) dev->priv;

  /* Null body since there is no framing error counter */
  return &lp->stats;
}

/*
 * ** Set or clear the multicast filter for this adapter.
 */
static void set_multicast_list (struct device *dev)
{
  struct ewrk3_private *lp = (struct ewrk3_private *) dev->priv;
  DWORD iobase = dev->base_addr;
  BYTE csr;

  if (irq2dev_map[dev->irq] != NULL)
  {
    csr = inb (EWRK3_CSR);

    if (lp->shmem_length == IO_ONLY)
    {
      lp->mctbl = (char *) PAGE0_HTE;
    }
    else
    {
      lp->mctbl = (char *) (lp->shmem_base + PAGE0_HTE);
    }

    csr &= ~(CSR_PME | CSR_MCE);
    if (dev->flags & IFF_PROMISC)
    {				/* set promiscuous mode */
      csr |= CSR_PME;
      outb (csr, EWRK3_CSR);
    }
    else
    {
      SetMulticastFilter (dev);
      csr |= CSR_MCE;
      outb (csr, EWRK3_CSR);
    }
  }
}

/*
 * ** Calculate the hash code and update the logical address filter
 * ** from a list of ethernet multicast addresses.
 * ** Little endian crc one liner from Matt Thomas, DEC.
 * **
 * ** Note that when clearing the table, the broadcast bit must remain asserted
 * ** to receive broadcast messages.
 */
static void SetMulticastFilter (struct device *dev)
{
  struct ewrk3_private *lp = (struct ewrk3_private *) dev->priv;
  struct dev_mc_list *dmi = dev->mc_list;
  DWORD iobase = dev->base_addr;
  int i;
  char *addrs, j, bit, byte;
  short *p = (short *) lp->mctbl;
  u16 hashcode;
  s32 crc, poly = CRC_POLYNOMIAL_LE;

  while (set_bit (0, (void *) &lp->lock) != 0) ;	/* Wait for lock to free */

  if (lp->shmem_length == IO_ONLY)
  {
    outb (0, EWRK3_IOPR);
    outw (EEPROM_OFFSET (lp->mctbl), EWRK3_PIR1);
  }
  else
  {
    outb (0, EWRK3_MPR);
  }

  if (dev->flags & IFF_ALLMULTI)
  {
    for (i = 0; i < (HASH_TABLE_LEN >> 3); i++)
    {
      if (lp->shmem_length == IO_ONLY)
      {
	outb (0xff, EWRK3_DATA);
      }
      else
      {				/* memset didn't work here */
	writew (0xffff, p);
	p++;
	i++;
      }
    }
  }
  else
  {
    /* Clear table except for broadcast bit */
    if (lp->shmem_length == IO_ONLY)
    {
      for (i = 0; i < (HASH_TABLE_LEN >> 4) - 1; i++)
      {
	outb (0x00, EWRK3_DATA);
      }
      outb (0x80, EWRK3_DATA);
      i++;			/* insert the broadcast bit */
      for (; i < (HASH_TABLE_LEN >> 3); i++)
      {
	outb (0x00, EWRK3_DATA);
      }
    }
    else
    {
      memset_io (lp->mctbl, 0, (HASH_TABLE_LEN >> 3));
      writeb (0x80, (char *) (lp->mctbl + (HASH_TABLE_LEN >> 4) - 1));
    }

    /* Update table */
    for (i = 0; i < dev->mc_count; i++)
    {				/* for each address in the list */
      addrs = dmi->dmi_addr;
      dmi = dmi->next;
      if ((*addrs & 0x01) == 1)
      {				/* multicast address? */
	crc = 0xffffffff;	/* init CRC for each address */
	for (byte = 0; byte < ETH_ALEN; byte++)
	{			/* for each address byte */
	  /* process each address bit */
	  for (bit = *addrs++, j = 0; j < 8; j++, bit >>= 1)
	  {
	    crc = (crc >> 1) ^ (((crc ^ bit) & 0x01) ? poly : 0);
	  }
	}
	hashcode = crc & ((1 << 9) - 1);	/* hashcode is 9 LSb of CRC */

	byte = hashcode >> 3;	/* bit[3-8] -> byte in filter */
	bit = 1 << (hashcode & 0x07);	/* bit[0-2] -> bit in byte */

	if (lp->shmem_length == IO_ONLY)
	{
	  BYTE tmp;

	  outw ((short) ((long) lp->mctbl) + byte, EWRK3_PIR1);
	  tmp = inb (EWRK3_DATA);
	  tmp |= bit;
	  outw ((short) ((long) lp->mctbl) + byte, EWRK3_PIR1);
	  outb (tmp, EWRK3_DATA);
	}
	else
	{
	  writeb (readb (lp->mctbl + byte) | bit, lp->mctbl + byte);
	}
      }
    }
  }

  lp->lock = 0;			/* Unlock the page register */

  return;
}

/*
 * ** ISA bus I/O device probe
 */
static void isa_probe (struct device *dev, DWORD ioaddr)
{
  DWORD iobase;
  int   i = num_ewrk3s, maxSlots;

  if (!ioaddr && autoprobed)
     return;                    /* Been here before ! */

  if (ioaddr >= 0x400)
    return;			/* Not ISA */

  if (ioaddr == 0)
  {				/* Autoprobing */
    iobase = EWRK3_IO_BASE;	/* Get the first slot address */
    maxSlots = 24;
  }
  else
  {				/* Probe a specific location */
    iobase = ioaddr;
    maxSlots = i + 1;
  }

  for (; (i < maxSlots) && (dev != NULL); iobase += EWRK3_IOP_INC, i++)
  {
    if (DevicePresent(iobase))
    {
      dev = alloc_device (dev, iobase);
      if (dev)
      {
        if (ewrk3_hw_init (dev, iobase))
           num_ewrk3s++;
        num_eth++;
      }
    }
  }
}

/*
 * ** EISA bus I/O device probe. Probe from slot 1 since slot 0 is usually
 * ** the motherboard.
 */
static void eisa_probe (struct device *dev, DWORD ioaddr)
{
  int i, maxSlots;
  DWORD iobase;
  char name[EWRK3_STRLEN];

  if (!ioaddr && autoprobed)
     return;                    /* Been here before ! */

  if (ioaddr < 0x1000)
     return;                    /* Not EISA */

  if (ioaddr == 0)
  {				/* Autoprobing */
    iobase = EISA_SLOT_INC;	/* Get the first slot address */
    i = 1;
    maxSlots = MAX_EISA_SLOTS;
  }
  else
  {				/* Probe a specific location */
    iobase = ioaddr;
    i = (ioaddr >> 12);
    maxSlots = i + 1;
  }

  for (i = 1; i < maxSlots && dev; i++, iobase += EISA_SLOT_INC)
  {
    if (EISA_signature (name, EISA_ID) == 0)
    {
      if (DevicePresent (iobase))
      {
        dev = alloc_device (dev, iobase);
        if (dev)
        {
          if (ewrk3_hw_init (dev, iobase))
             num_ewrk3s++;
          num_eth++;
        }
      }
    }
  }
}

/*
 * ** Search the entire 'eth' device list for a fixed probe. If a match isn't
 * ** found then check for an autoprobe or unused device location. If they
 * ** are not available then insert a new device structure at the end of
 * ** the current list.
 */
static struct device *alloc_device (struct device *dev, DWORD iobase)
{
  struct device *adev = NULL;
  int fixed = 0, new_dev = 0;

  num_eth = ewrk3_dev_index (dev->name);

  while (1)
  {
    if (((dev->base_addr == EWRK3_NDA) || (dev->base_addr == 0)) && !adev)
    {
      adev = dev;
    }
    else if ((dev->priv == NULL) && (dev->base_addr == iobase))
    {
      fixed = 1;
    }
    else
    {
      if (dev->next == NULL)
      {
	new_dev = 1;
      }
      else if (strncmp (dev->next->name, "eth", 3) != 0)
      {
	new_dev = 1;
      }
    }
    if ((dev->next == NULL) || new_dev || fixed)
      break;
    dev = dev->next;
    num_eth++;
  }
  if (adev && !fixed)
  {
    dev = adev;
    num_eth = ewrk3_dev_index (dev->name);
    new_dev = 0;
  }

  if (((dev->next == NULL) && ((dev->base_addr != EWRK3_NDA) && (dev->base_addr != 0)) && !fixed) || new_dev)
  {
    num_eth++;			/* New device */
    dev = insert_device (dev, iobase, ewrk3_probe);
  }

  return dev;
}

/*
 * ** If at end of eth device list and can't use current entry, malloc
 * ** one up. If memory could not be allocated, print an error message.
 */
static struct device *insert_device (struct device *dev, DWORD iobase, int (*init) (struct device *))
{
  struct device *new;

  new = (struct device *) kmalloc (sizeof (struct device) + 8, GFP_KERNEL);

  if (new == NULL)
  {
    printk ("eth%d: Device not initialised, insufficient memory\n", num_eth);
    return NULL;
  }
  else
  {
    new->next = dev->next;
    dev->next = new;
    dev = dev->next;		/* point to the new device */
    dev->name = (char *) (dev + 1);
    if (num_eth > 9999)
    {
      sprintf (dev->name, "eth????");	/* New device name */
    }
    else
    {
      sprintf (dev->name, "eth%d", num_eth);	/* New device name */
    }
    dev->base_addr = iobase;	/* assign the io address */
    dev->init = init;		/* initialisation routine */
  }

  return dev;
}

static int ewrk3_dev_index (char *s)
{
  int i = 0, j = 0;

  for (; *s; s++)
  {
    if (isdigit (*s))
    {
      j = 1;
      i = (i * 10) + (*s - '0');
    }
    else if (j)
      break;
  }

  return i;
}

/*
 * ** Read the EWRK3 EEPROM using this routine
 */
static int Read_EEPROM (DWORD iobase, BYTE eaddr)
{
  int i;

  outb ((eaddr & 0x3f), EWRK3_PIR1);	/* set up 6 bits of address info */
  outb (EEPROM_RD, EWRK3_IOPR);	/* issue read command */
  for (i = 0; i < 5000; i++)
    inb (EWRK3_CSR);		/* wait 1msec */

  return inw (EWRK3_EPROM1);	/* 16 bits data return */
}

/*
 * ** Write the EWRK3 EEPROM using this routine
 */
static int Write_EEPROM (short data, DWORD iobase, BYTE eaddr)
{
  int i;

  outb (EEPROM_WR_EN, EWRK3_IOPR);	/* issue write enable command */
  for (i = 0; i < 5000; i++)
    inb (EWRK3_CSR);		/* wait 1msec */
  outw (data, EWRK3_EPROM1);	/* write data to register */
  outb ((eaddr & 0x3f), EWRK3_PIR1);	/* set up 6 bits of address info */
  outb (EEPROM_WR, EWRK3_IOPR);	/* issue write command */
  for (i = 0; i < 75000; i++)
    inb (EWRK3_CSR);		/* wait 15msec */
  outb (EEPROM_WR_DIS, EWRK3_IOPR);	/* issue write disable command */
  for (i = 0; i < 5000; i++)
    inb (EWRK3_CSR);		/* wait 1msec */

  return 0;
}

/*
 * ** Look for a particular board name in the on-board EEPROM.
 */
static void EthwrkSignature (char *name, char *eeprom_image)
{
  DWORD i, j, k;
  char *signatures[] = EWRK3_SIGNATURE;

  strcpy (name, "");
  for (i = 0; *signatures[i] != '\0' && *name == '\0'; i++)
  {
    for (j = EEPROM_PNAME7, k = 0; j <= EEPROM_PNAME0 && k < strlen (signatures[i]); j++)
    {
      if (signatures[i][k] == eeprom_image[j])
      {				/* track signature */
	k++;
      }
      else
      {				/* lost signature; begin search again */
	k = 0;
      }
    }
    if (k == strlen (signatures[i]))
    {
      for (k = 0; k < EWRK3_STRLEN; k++)
      {
	name[k] = eeprom_image[EEPROM_PNAME7 + k];
	name[EWRK3_STRLEN] = '\0';
      }
    }
  }

  return;			/* return the device name string */
}

/*
 * ** Look for a special sequence in the Ethernet station address PROM that
 * ** is common across all EWRK3 products.
 * **
 * ** Search the Ethernet address ROM for the signature. Since the ROM address
 * ** counter can start at an arbitrary point, the search must include the entire
 * ** probe sequence length plus the (length_of_the_signature - 1).
 * ** Stop the search IMMEDIATELY after the signature is found so that the
 * ** PROM address counter is correctly positioned at the start of the
 * ** ethernet address for later read out.
 */

static int DevicePresent (DWORD iobase)
{
  union {
    struct {
      DWORD a;
      DWORD b;
    } llsig;
    char Sig[sizeof(DWORD) << 1];
  } dev;

  short sigLength;
  char  data;
  int   i, j;

  dev.llsig.a = ETH_PROM_SIG;
  dev.llsig.b = ETH_PROM_SIG;
  sigLength = sizeof (DWORD) << 1;

  for (i = 0, j = 0; j < sigLength && i < PROBE_LENGTH + sigLength - 1; i++)
  {
    data = inb (EWRK3_APROM);
    if (dev.Sig[j] == data)       /* track signature */
      j++;
    else
    {				/* lost signature; begin search again */
      if (data == dev.Sig[0])
           j = 1;
      else j = 0;
    }
  }

  if (j != sigLength)
    return (0);
  return (1);
}

static int get_hw_addr (struct device *dev, BYTE * eeprom_image, char chipType)
{
  int   i, j, k;
  WORD  chksum;
  BYTE  crc, lfsr, sd;
  DWORD iobase = dev->base_addr;
  WORD  tmp;

  if (chipType == LeMAC2)
  {
    for (crc = 0x6a, j = 0; j < ETH_ALEN; j++)
    {
      sd = dev->dev_addr[j] = eeprom_image[EEPROM_PADDR0 + j];
      outb (dev->dev_addr[j], EWRK3_PAR0 + j);
      for (k = 0; k < 8; k++, sd >>= 1)
      {
	lfsr = ((((crc & 0x02) >> 1) ^ (crc & 0x01)) ^ (sd & 0x01)) << 7;
	crc = (crc >> 1) + lfsr;
      }
    }
    if (crc != eeprom_image[EEPROM_PA_CRC])
       return (0);
  }
  else
  {
    for (i = 0, k = 0; i < ETH_ALEN;)
    {
      k <<= 1;
      if (k > 0xffff)
         k -= 0xffff;

      k += (BYTE) (tmp = inb (EWRK3_APROM));
      dev->dev_addr[i] = (BYTE) tmp;
      outb (dev->dev_addr[i], EWRK3_PAR0 + i);
      i++;
      k += (WORD) ((tmp = inb (EWRK3_APROM)) << 8);
      dev->dev_addr[i] = (BYTE) tmp;
      outb (dev->dev_addr[i], EWRK3_PAR0 + i);
      i++;
      if (k > 0xffff)
	k -= 0xffff;
    }
    if (k == 0xffff)
       k = 0;
    chksum = inb (EWRK3_APROM);
    chksum |= (inb (EWRK3_APROM) << 8);
    if (k != chksum)
       return (0);
  }
  return (1);
}

/*
 * ** Look for a particular board name in the EISA configuration space
 */
static int EISA_signature (char *name, s32 eisa_id)
{
  DWORD i;
  char *signatures[] = EWRK3_SIGNATURE;
  char ManCode[EWRK3_STRLEN];
  union
  {
    s32 ID;
    char Id[4];
  }
  Eisa;
  int status = 0;

  *name = '\0';
  for (i = 0; i < 4; i++)
  {
    Eisa.Id[i] = inb (eisa_id + i);
  }

  ManCode[0] = (((Eisa.Id[0] >> 2) & 0x1f) + 0x40);
  ManCode[1] = (((Eisa.Id[1] & 0xe0) >> 5) + ((Eisa.Id[0] & 0x03) << 3) + 0x40);
  ManCode[2] = (((Eisa.Id[2] >> 4) & 0x0f) + 0x30);
  ManCode[3] = ((Eisa.Id[2] & 0x0f) + 0x30);
  ManCode[4] = (((Eisa.Id[3] >> 4) & 0x0f) + 0x30);
  ManCode[5] = '\0';

  for (i = 0; (*signatures[i] != '\0') && (*name == '\0'); i++)
  {
    if (strstr (ManCode, signatures[i]) != NULL)
    {
      strcpy (name, ManCode);
      status = 1;
    }
  }

  return status;		/* return the device name string */
}

#if 0
/*
 * ** Perform IOCTL call functions here. Some are privileged operations and the
 * ** effective uid is checked in those cases.
 */
static int ewrk3_ioctl (struct device *dev, struct ifreq *rq, int cmd)
{
  struct ewrk3_private *lp = (struct ewrk3_private *) dev->priv;
  struct ewrk3_ioctl *ioc = (struct ewrk3_ioctl *) &rq->ifr_data;
  DWORD iobase = dev->base_addr;
  int i, j, status = 0;
  BYTE csr;
  union
  {
    BYTE addr[HASH_TABLE_LEN * ETH_ALEN];
    WORD val[(HASH_TABLE_LEN * ETH_ALEN) >> 1];
  }
  tmp;

  switch (ioc->cmd)
  {
       case EWRK3_GET_HWADDR:	/* Get the hardware address */
	 for (i = 0; i < ETH_ALEN; i++)
	 {
	   tmp.addr[i] = dev->dev_addr[i];
	 }
	 ioc->len = ETH_ALEN;
	 if (!(status = verify_area (VERIFY_WRITE, (void *) ioc->data, ioc->len)))
	 {
	   copy_to_user (ioc->data, tmp.addr, ioc->len);
	 }

	 break;
       case EWRK3_SET_HWADDR:	/* Set the hardware address */
	 if (suser ())
	 {
	   if (!(status = verify_area (VERIFY_READ, (void *) ioc->data, ETH_ALEN)))
	   {
	     csr = inb (EWRK3_CSR);
	     csr |= (CSR_TXD | CSR_RXD);
	     outb (csr, EWRK3_CSR);	/* Disable the TX and RX */

	     copy_from_user (tmp.addr, ioc->data, ETH_ALEN);
	     for (i = 0; i < ETH_ALEN; i++)
	     {
	       dev->dev_addr[i] = tmp.addr[i];
	       outb (tmp.addr[i], EWRK3_PAR0 + i);
	     }

	     csr &= ~(CSR_TXD | CSR_RXD);	/* Enable the TX and RX */
	     outb (csr, EWRK3_CSR);
	   }
	 }
	 else
	 {
	   status = -EPERM;
	 }

	 break;
       case EWRK3_SET_PROM:	/* Set Promiscuous Mode */
	 if (suser ())
	 {
	   csr = inb (EWRK3_CSR);
	   csr |= CSR_PME;
	   csr &= ~CSR_MCE;
	   outb (csr, EWRK3_CSR);
	 }
	 else
	 {
	   status = -EPERM;
	 }

	 break;
       case EWRK3_CLR_PROM:	/* Clear Promiscuous Mode */
	 if (suser ())
	 {
	   csr = inb (EWRK3_CSR);
	   csr &= ~CSR_PME;
	   outb (csr, EWRK3_CSR);
	 }
	 else
	 {
	   status = -EPERM;
	 }

	 break;
       case EWRK3_SAY_BOO:	/* Say "Boo!" to the kernel log file */
	 printk ("%s: Boo!\n", dev->name);

	 break;
       case EWRK3_GET_MCA:	/* Get the multicast address table */
	 if (!(status = verify_area (VERIFY_WRITE, ioc->data, ioc->len)))
	 {
	   while (set_bit (0, (void *) &lp->lock) != 0) ;	/* Wait for lock to free */
	   if (lp->shmem_length == IO_ONLY)
	   {
	     outb (0, EWRK3_IOPR);
	     outw (PAGE0_HTE, EWRK3_PIR1);
	     for (i = 0; i < (HASH_TABLE_LEN >> 3); i++)
	     {
	       tmp.addr[i] = inb (EWRK3_DATA);
	     }
	   }
	   else
	   {
	     outb (0, EWRK3_MPR);
	     memcpy_fromio (tmp.addr, (char *) (lp->shmem_base + PAGE0_HTE), (HASH_TABLE_LEN >> 3));
	   }
	   ioc->len = (HASH_TABLE_LEN >> 3);
	   copy_to_user (ioc->data, tmp.addr, ioc->len);
	 }
	 lp->lock = 0;		/* Unlock the page register */

	 break;
       case EWRK3_SET_MCA:	/* Set a multicast address */
	 if (suser ())
	 {
	   if (!(status = verify_area (VERIFY_READ, ioc->data, ETH_ALEN * ioc->len)))
	   {
	     copy_from_user (tmp.addr, ioc->data, ETH_ALEN * ioc->len);
	     set_multicast_list (dev);
	   }
	 }
	 else
	 {
	   status = -EPERM;
	 }

	 break;
       case EWRK3_CLR_MCA:	/* Clear all multicast addresses */
	 if (suser ())
	 {
	   set_multicast_list (dev);
	 }
	 else
	 {
	   status = -EPERM;
	 }

	 break;
       case EWRK3_MCA_EN:	/* Enable multicast addressing */
	 if (suser ())
	 {
	   csr = inb (EWRK3_CSR);
	   csr |= CSR_MCE;
	   csr &= ~CSR_PME;
	   outb (csr, EWRK3_CSR);
	 }
	 else
	 {
	   status = -EPERM;
	 }

	 break;
       case EWRK3_GET_STATS:	/* Get the driver statistics */
	 cli ();
	 ioc->len = sizeof (lp->pktStats);
	 if (!(status = verify_area (VERIFY_WRITE, ioc->data, ioc->len)))
	 {
	   copy_to_user (ioc->data, &lp->pktStats, ioc->len);
	 }
	 sti ();

	 break;
       case EWRK3_CLR_STATS:	/* Zero out the driver statistics */
	 if (suser ())
	 {
	   cli ();
	   memset (&lp->pktStats, 0, sizeof (lp->pktStats));
	   sti ();
	 }
	 else
	 {
	   status = -EPERM;
	 }

	 break;
       case EWRK3_GET_CSR:	/* Get the CSR Register contents */
	 tmp.addr[0] = inb (EWRK3_CSR);
	 ioc->len = 1;
	 if (!(status = verify_area (VERIFY_WRITE, ioc->data, ioc->len)))
	 {
	   copy_to_user (ioc->data, tmp.addr, ioc->len);
	 }

	 break;
       case EWRK3_SET_CSR:	/* Set the CSR Register contents */
	 if (suser ())
	 {
	   if (!(status = verify_area (VERIFY_READ, ioc->data, 1)))
	   {
	     copy_from_user (tmp.addr, ioc->data, 1);
	     outb (tmp.addr[0], EWRK3_CSR);
	   }
	 }
	 else
	 {
	   status = -EPERM;
	 }

	 break;
       case EWRK3_GET_EEPROM:	/* Get the EEPROM contents */
	 if (suser ())
	 {
	   for (i = 0; i < (EEPROM_MAX >> 1); i++)
	   {
	     tmp.val[i] = (short) Read_EEPROM (iobase, i);
	   }
	   i = EEPROM_MAX;
	   tmp.addr[i++] = inb (EWRK3_CMR);	/* Config/Management Reg. */
	   for (j = 0; j < ETH_ALEN; j++)
	   {
	     tmp.addr[i++] = inb (EWRK3_PAR0 + j);
	   }
	   ioc->len = EEPROM_MAX + 1 + ETH_ALEN;
	   if (!(status = verify_area (VERIFY_WRITE, ioc->data, ioc->len)))
	   {
	     copy_to_user (ioc->data, tmp.addr, ioc->len);
	   }
	 }
	 else
	 {
	   status = -EPERM;
	 }

	 break;
       case EWRK3_SET_EEPROM:	/* Set the EEPROM contents */
	 if (suser ())
	 {
	   if (!(status = verify_area (VERIFY_READ, ioc->data, EEPROM_MAX)))
	   {
	     copy_from_user (tmp.addr, ioc->data, EEPROM_MAX);
	     for (i = 0; i < (EEPROM_MAX >> 1); i++)
	     {
	       Write_EEPROM (tmp.val[i], iobase, i);
	     }
	   }
	 }
	 else
	 {
	   status = -EPERM;
	 }

	 break;
       case EWRK3_GET_CMR:	/* Get the CMR Register contents */
	 tmp.addr[0] = inb (EWRK3_CMR);
	 ioc->len = 1;
	 if (!(status = verify_area (VERIFY_WRITE, ioc->data, ioc->len)))
	 {
	   copy_to_user (ioc->data, tmp.addr, ioc->len);
	 }

	 break;
       case EWRK3_SET_TX_CUT_THRU:	/* Set TX cut through mode */
	 if (suser ())
	 {
	   lp->txc = 1;
	 }
	 else
	 {
	   status = -EPERM;
	 }

	 break;
       case EWRK3_CLR_TX_CUT_THRU:	/* Clear TX cut through mode */
	 if (suser ())
	 {
	   lp->txc = 0;
	 }
	 else
	 {
	   status = -EPERM;
	 }

	 break;
       default:
	 status = -EOPNOTSUPP;
  }

  return status;
}
#endif
