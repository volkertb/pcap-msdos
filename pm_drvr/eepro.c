/*
 * eepro.c: Intel EtherExpress Pro/10 device driver for Linux.
 *
 * Written 1994, 1995,1996 by Bao C. Ha.
 * 
 * Copyright (C) 1994, 1995,1996 by Bao C. Ha.
 * 
 * This software may be used and distributed according to the terms of
 * the GNU Public License, incorporated herein by reference.
 * 
 * The author may be reached at bao.ha@srs.gov or 418 Hastings Place,
 * Martinez, GA 30907.
 * 
 * Things remaining to do:
 *   Better record keeping of errors.
 *   Eliminate transmit interrupt to reduce overhead.
 *   Implement "concurrent processing". I won't be doing it!
 * 
 * Bugs:
 * 
 * If you have a problem of not detecting the 82595 during a reboot
 * (warm reset), disable the FLASH memory should fix it.
 * This is a compatibility hardware problem.
 * 
 * Versions:
 * 
 * 0.09   Fixed a race condition in the transmit algorithm, which causes
 *        crashes under heavy load with fast pentium computers.  The
 *        performance should also improve a bit.
 *        The size of RX buffer, and hence TX buffer, can also be changed
 *        via lilo or insmod. (BCH, 7/31/96)
 * 
 * 0.08   Implement 32-bit I/O for the 82595TX and 82595FX based lan cards.
 *        Disable full-duplex mode if TPE is not used.  (BCH, 4/8/96)
 * 
 * 0.07a  Fix a stat report which counts every packet as a heart-beat
 *        failure. (BCH, 6/3/95)
 * 
 * 0.07   Modified to support all other 82595-based lan cards. The IRQ
 *        vector of the EtherExpress Pro will be set according to the
 *        value saved in the EEPROM.  For other cards, I will do
 *        autoirq_request() to grab the next available interrupt vector.
 *        (BCH, 3/17/95)
 * 
 * 0.06a,b  Interim released.  Minor changes in the comments and print
          out format. (BCH, 3/9/95 and 3/14/95)
 * 
 * 0.06   First stable release that I am comfortable with. (BCH, 3/2/95)
 * 
 * 0.05   Complete testing of multicast. (BCH, 2/23/95)
 * 
 * 0.04   Adding multicast support. (BCH, 2/14/95)
 * 
 * 0.03   First widely alpha release for public testing. (BCH, 2/14/95)
 * 
 */

#ifdef VERSION
static const char *version = "eepro.c: v0.09 7/31/96 Bao C. Ha (bao.ha@srs.gov)\n";
#endif

#include "pmdrvr.h"

/* 
 * Sources:
 * 
 * This driver wouldn't have been written without the availability of the
 * Crynwr's Lan595 driver source code.  It helps me to familiarize with
 * the 82595 chipset while waiting for the Intel documentation.  I also
 * learned how to detect the 82595 using the packet driver's technique.
 * 
 * This driver is written by cutting and pasting the skeleton.c driver
 * provided by Donald Becker.  I also borrowed the EEPROM routine from
 * Donald Becker's 82586 driver.
 * 
 * Datasheet for the Intel 82595 (including the TX and FX version).
 * It provides just enough info that the casual reader might think
 * that it documents the i82595.
 * 
 * The User Manual for the 82595.  It provides a lot of the missing
 * information.
 * 
 */


/*
 * First, a few definitions that the brave might change.
 * A zero-terminated list of I/O addresses to be probed.
 */
static DWORD eepro_portlist[] = { 0x200, 0x240, 0x280, 0x2C0,
                                  0x300, 0x320, 0x340, 0x360,
                                  0
                                };
static const char *ifmap[] = { "AUI", "10Base2", "10BaseT" };
enum iftype {
     AUI = 0,
     BNC = 1,
     TPE = 2
   };

/*
 * use 0 for production, 1 for verification, >2 for debug
 */
#ifndef NET_DEBUG
#define NET_DEBUG 3
#endif
static DWORD net_debug = NET_DEBUG;

/* The number of low I/O ports used by the ethercard.
 */
#define EEPRO_IO_EXTENT  16

/* Different 82595 chips
 */
#define  LAN595    0
#define  LAN595TX  1
#define  LAN595FX  2

/* Information that need to be kept for each board.
 */
struct eepro_local {
       struct   net_device_stats stats;
       unsigned rx_start;
       unsigned tx_start;     /* start of the transmit chain */
       int      tx_last;      /* pointer to last packet in the transmit chain */
       unsigned tx_end;       /* end of the transmit chain (plus 1) */
       int      eepro;        /* a flag, TRUE=1 for the EtherExpress Pro/10,
                               * FALSE = 0 for other 82595-based lan cards. */
       int      version;      /* a flag to indicate if this is a TX or FX
                               * version of the 82595 chip. */
       int      stepping;
     };

/* The station (ethernet) address prefix, used for IDing the board.
 */
#define SA_ADDR0 0x00
#define SA_ADDR1 0xaa
#define SA_ADDR2 0x00

extern int eepro_probe (struct device *dev);

static int   eepro_probe1      (struct device *dev, short ioaddr);
static int   eepro_open        (struct device *dev);
static int   eepro_send_packet (struct device *dev, const void *buf, int len);
static void  eepro_interrupt   (int irq);
static void  eepro_rx          (struct device *dev);
static void  eepro_tx_interrupt(struct device *dev);
static void  eepro_close       (struct device *dev);
static void *eepro_get_stats   (struct device *dev);
static void  set_multicast_list(struct device *dev);

static int  read_eeprom   (int ioaddr, int location);
static void hardware_send (struct device *dev, const void *buf, short length);
static int  eepro_grab_irq(struct device *dev);

/* 
 * Details of the i82595.
 * 
 * You will need either the datasheet or the user manual to understand
 * what is going on here.  The 82595 is very different from the 82586, 82593.
 * 
 * The receive algorithm in eepro_rx() is just an implementation of the RCV
 * ring structure that the Intel 82595 imposes at the hardware level.
 * The receive buffer is set at 24K, and the transmit buffer is 8K.
 * I am assuming that the total buffer memory is 32K, which is true for
 * the Intel EtherExpress Pro/10.  If it is less than that on a generic
 * card, the driver will be broken.
 * 
 * The transmit algorithm in the hardware_send() is similar to the one
 * in the eepro_rx().  The transmit buffer is a ring linked list. I just
 * queue the next available packet to the end of the list.  In my system,
 * the 82595 is so fast that the list seems to always contain a single packet.
 * In other systems with faster computers and more congested network traffics,
 * the ring linked list should improve performance by allowing up to 8K
 * worth of packets to be queued.
 * 
 * The sizes of the receive and transmit buffers can now be changed via
 * lilo or insmod.  Lilo uses the appended line
 * "ether=io,irq,debug,rx-buffer,eth0" where rx-buffer is in KB unit.
 * Modules uses the parameter mem which is also in* KB unit, for example
 * "insmod io=io-address irq=0 mem=rx-buffer." The receive buffer has to
 * be more than 3K or less than 29K.  Otherwise, it is reset to the default
 * of 24K, and, hence, 8K for the trasmit buffer (transmit-buffer =
 * 32K - receive-buffer).
 */
#define RAM_SIZE        0x8000
#define RCV_HEADER      8
#define RCV_RAM         0x6000	/* 24KB default for RCV buffer */
#define RCV_LOWER_LIMIT 0x00	/* 0x0000 */

/* #define RCV_UPPER_LIMIT ((RCV_RAM - 2) >> 8) *//* 0x5ffe */
#define RCV_UPPER_LIMIT    (((rcv_ram) - 2) >> 8)

/* #define XMT_RAM         (RAM_SIZE - RCV_RAM) */ /* 8KB for XMT buffer */
#define XMT_RAM            (RAM_SIZE - (rcv_ram))  /* 8KB for XMT buffer */

/* #define XMT_LOWER_LIMIT (RCV_RAM >> 8) *//* 0x6000 */
#define XMT_LOWER_LIMIT    ((rcv_ram) >> 8)

#define XMT_UPPER_LIMIT    ((RAM_SIZE - 2) >> 8)   /* 0x7ffe */
#define XMT_HEADER         8

#define RCV_DONE      0x0008
#define RX_OK         0x2000
#define RX_ERROR      0x0d81

#define TX_DONE_BIT   0x0080
#define CHAIN_BIT     0x8000
#define XMT_STATUS    0x02
#define XMT_CHAIN     0x04
#define XMT_COUNT     0x06

#define BANK0_SELECT  0x00
#define BANK1_SELECT  0x40
#define BANK2_SELECT  0x80

/* Bank 0 registers
 */
#define COMMAND_REG     0x00  /* Register 0 */
#define MC_SETUP        0x03
#define XMT_CMD         0x04
#define DIAGNOSE_CMD    0x07
#define RCV_ENABLE_CMD  0x08
#define RCV_DISABLE_CMD 0x0a
#define STOP_RCV_CMD    0x0b
#define RESET_CMD       0x0e
#define POWER_DOWN_CMD  0x18
#define RESUME_XMT_CMD  0x1c
#define SEL_RESET_CMD   0x1e
#define STATUS_REG      0x01  /* Register 1 */
#define RX_INT          0x02
#define TX_INT          0x04
#define EXEC_STATUS     0x30
#define ID_REG          0x02  /* Register 2  */
#define R_ROBIN_BITS    0xc0  /* round robin counter */
#define ID_REG_MASK     0x2c
#define ID_REG_SIG      0x24
#define AUTO_ENABLE     0x10
#define INT_MASK_REG    0x03  /* Register 3  */
#define RX_STOP_MASK    0x01
#define RX_MASK         0x02
#define TX_MASK         0x04
#define EXEC_MASK       0x08
#define ALL_MASK        0x0f
#define IO_32_BIT       0x10
#define RCV_BAR         0x04  /* The following are word (16-bit) registers */
#define RCV_STOP        0x06
#define XMT_BAR         0x0a
#define HOST_ADDRESS_REG 0x0c
#define IO_PORT         0x0e
#define IO_PORT_32_BIT  0x0c

/* Bank 1 registers
 */
#define REG1                 0x01
#define WORD_WIDTH           0x02
#define INT_ENABLE           0x80
#define INT_NO_REG           0x02
#define RCV_LOWER_LIMIT_REG  0x08
#define RCV_UPPER_LIMIT_REG  0x09
#define XMT_LOWER_LIMIT_REG  0x0a
#define XMT_UPPER_LIMIT_REG  0x0b

/* Bank 2 registers
 */
#define XMT_Chain_Int        0x20 /* Interrupt at the end of the transmit chain */
#define XMT_Chain_ErrStop    0x40 /* Interrupt at the end of the chain even if there are errors */
#define RCV_Discard_BadFrame 0x80 /* Throw bad frames away, and continue to receive others */
#define REG2                 0x02
#define PRMSC_Mode           0x01
#define Multi_IA             0x20
#define REG3                 0x03
#define TPE_BIT              0x04
#define BNC_BIT              0x20
#define REG13                0x0d
#define FDX                  0x00
#define A_N_ENABLE           0x02

#define I_ADD_REG0           0x04
#define I_ADD_REG1           0x05
#define I_ADD_REG2           0x06
#define I_ADD_REG3           0x07
#define I_ADD_REG4           0x08
#define I_ADD_REG5           0x09

#define EEPROM_REG           0x0a
#define EESK                 0x01
#define EECS                 0x02
#define EEDI                 0x04
#define EEDO                 0x08


/*
 * Check for a network adaptor of this type, and return '0' iff one
 * exists. If dev->base_addr == 0, probe all likely locations. If
 * dev->base_addr == 1, always return failure. If dev->base_addr == 2,
 * allocate space for the device and return success.
 */
int eepro_probe (struct device *dev)
{
  int i, base_addr = dev ? dev->base_addr : 0;

  if (base_addr > 0x1ff)      /* Check a single specified location. */
    return eepro_probe1 (dev, base_addr);

  if (base_addr)              /* Don't probe at all. */
    return (0);

  for (i = 0; eepro_portlist[i]; i++)
    if (eepro_probe1 (dev, eepro_portlist[i]))
       return (1);

  return (0);
}

/*
 * This is the real probe routine.  Linux has a history of friendly
 * device probes on the ISA bus.  A good device probes avoids doing
 * writes, and verifies that the correct device exists and functions.
 */
int eepro_probe1 (struct device *dev, short ioaddr)
{
  WORD station_addr[6], id, counter;
  int  i, eepro;         /* a flag, TRUE=1 for the EtherExpress Pro/10,
                          * FALSE = 0 for other 82595-based lan cards.
                          */

  /* Now, we are going to check for the signature of the
   * ID_REG (register 2 of bank 0)
   */
  id = inb (ioaddr + ID_REG);
  if ((id & ID_REG_MASK) == ID_REG_SIG)
  {
    /* We seem to have the 82595 signature, let's
     * play with its counter (last 2 bits of
     * register 2 of bank 0) to be sure.
     */
    counter = (id & R_ROBIN_BITS);
    id = inb (ioaddr + ID_REG);
    if ((id & R_ROBIN_BITS) == (counter + 0x40))
    {
      /* Yes, the 82595 has been found
       * Now, get the ethernet hardware address from
       * the EEPROM
       */
      station_addr[0] = read_eeprom (ioaddr, 2);
      station_addr[1] = read_eeprom (ioaddr, 3);
      station_addr[2] = read_eeprom (ioaddr, 4);

      /* Check the station address for the manufacturer's code
       */
      if (station_addr[2] != 0x00aa || (station_addr[1] & 0xff00) != 0x0000)
      {
	eepro = 0;
	printk ("%s: Intel 82595-based lan card at %#x,",
		dev->name, ioaddr);
      }
      else
      {
	eepro = 1;
	printk ("%s: Intel EtherExpress Pro/10 at %#x,",
		dev->name, ioaddr);
      }

      /* Fill in the 'dev' fields. */
      dev->base_addr = ioaddr;

      for (i = 0; i < 6; i++)
      {
	dev->dev_addr[i] = ((BYTE *) station_addr)[5 - i];
	printk ("%c%02x", i ? ':' : ' ', dev->dev_addr[i]);
      }

      if ((dev->mem_end & 0x3f) < 3 ||        /* RX buffer must be > 3K */
          (dev->mem_end & 0x3f) > 29)         /* and less < 29K */
           dev->mem_end = RCV_RAM;            /* or it will be set to 24K */
      else dev->mem_end = 1024 * dev->mem_end;

      /* From now on, dev->mem_end contains the actual size of rx buffer
       */
      if (net_debug > 3)
         printk (", %dK RCV buffer", (int) (dev->mem_end) / 1024);

      outb (BANK2_SELECT, ioaddr);	/* be CAREFUL, BANK 2 now */
      id = inb (ioaddr + REG3);
      if (id & TPE_BIT)
           dev->if_port = TPE;
      else dev->if_port = BNC;

      if (dev->irq < 2 && eepro)
      {
	i = read_eeprom (ioaddr, 1);
	switch (i & 0x07)
        {
          case 0:
	       dev->irq = 9;
	       break;
          case 1:
	       dev->irq = 3;
	       break;
          case 2:
	       dev->irq = 5;
	       break;
          case 3:
	       dev->irq = 10;
	       break;
          case 4:
	       dev->irq = 11;
	       break;
          default:           /* should never get here !!!!! */
	       printk (" illegal interrupt vector stored in EEPROM.\n");
	       return (0);
        }
      }
      else if (dev->irq == 2)
	dev->irq = 9;

      if (dev->irq > 2)
      {
	printk (", IRQ %d, %s.\n", dev->irq,
		ifmap[dev->if_port]);
        if (!request_irq (dev->irq, &eepro_interrupt))
	{
	  printk ("%s: unable to get IRQ %d.\n", dev->name, dev->irq);
	  return (0);
	}
      }
      else
	printk (", %s.\n", ifmap[dev->if_port]);

      if ((dev->mem_start & 0xf) > 0)      /* I don't know if this is */
         net_debug = dev->mem_start & 7;   /* still useful or not */

      if (net_debug > 3)
      {
	i = read_eeprom (ioaddr, 5);
        if (i & 0x2000)                    /* bit 13 of EEPROM word 5 */
	  printk ("%s: Concurrent Processing is enabled but not used!\n",
		  dev->name);
      }
      /* Initialize the device structure
       */
      dev->priv = calloc (sizeof(struct eepro_local),1);
      if (!dev->priv)
	return (0);

      dev->open      = eepro_open;
      dev->close     = eepro_close;
      dev->xmit      = eepro_send_packet;
      dev->get_stats = eepro_get_stats;
      dev->set_multicast_list = set_multicast_list;

      outb (RESET_CMD, ioaddr);	/* RESET the 82595 */
      return (1);
    }
    return (1);
  }
  if (net_debug > 3)
     printk ("EtherExpress Pro probed failed!\n");
  return (0);
}

/*
 * Open/initialize the board.  This is called (in the current kernel)
 * sometime after booting when the 'ifconfig' program is run.
 * 
 * This routine should set everything up anew at each open, even registers
 * that "should" only need to be set once at boot, so that there is non-
 * reboot way to recover if something goes wrong.
 */
static char irqrmap[] = { -1,-1, 0, 1, -1, 2, -1, -1,
                          -1, 0, 3, 4, -1,-1, -1, -1
                        };
static int irqlist[]  = { 5, 9, 10, 11, 4, 3, 0 };

static int eepro_grab_irq (struct device *dev)
{
  int  temp_reg;
  int *irqp   = irqlist;
  int  ioaddr = dev->base_addr;

  outb (BANK1_SELECT, ioaddr);	/* be CAREFUL, BANK 1 now */

  /* Enable the interrupt line.
   */
  temp_reg = inb (ioaddr + REG1);
  outb (temp_reg | INT_ENABLE, ioaddr + REG1);

  outb (BANK0_SELECT, ioaddr);	/* be CAREFUL, BANK 0 now */

  /* clear all interrupts
   */
  outb (ALL_MASK, ioaddr + STATUS_REG);

  /* Let EXEC event to interrupt
   */
  outb (ALL_MASK & ~(EXEC_MASK), ioaddr + INT_MASK_REG);

  do
  {
    outb (BANK1_SELECT, ioaddr);	/* be CAREFUL, BANK 1 now */

    temp_reg = inb (ioaddr + INT_NO_REG);
    outb ((temp_reg & 0xf8) | irqrmap[*irqp], ioaddr + INT_NO_REG);

    outb (BANK0_SELECT, ioaddr);	/* Switch back to Bank 0 */

    if (!request_irq (*irqp, NULL))
    {
      /* Twinkle the interrupt, and check if it's seen
       */
      autoirq_setup (0);
      outb (DIAGNOSE_CMD, ioaddr);	/* RESET the 82595 */

      if (*irqp == autoirq_report(2) && /* It's a good IRQ line */
          (request_irq(dev->irq = *irqp, &eepro_interrupt)))
	break;

      /* clear all interrupts */
      outb (ALL_MASK, ioaddr + STATUS_REG);
    }
  }
  while (*++irqp);

  outb (BANK1_SELECT, ioaddr);	/* Switch back to Bank 1 */

  /* Disable the physical interrupt line
   */
  temp_reg = inb (ioaddr + REG1);
  outb (temp_reg & 0x7f, ioaddr + REG1);

  outb (BANK0_SELECT, ioaddr);	/* Switch back to Bank 0 */

  /* Mask all the interrupts
   */
  outb (ALL_MASK, ioaddr + INT_MASK_REG);

  /* clear all interrupts
   */
  outb (ALL_MASK, ioaddr + STATUS_REG);

  return (dev->irq);
}

static int eepro_open (struct device *dev)
{
  WORD   temp_reg, old8, old9;
  int    i, ioaddr = dev->base_addr;
  int    rcv_ram   = dev->mem_end;
  struct eepro_local *lp = (struct eepro_local *) dev->priv;

  if (net_debug > 3)
     printk ("eepro: entering eepro_open routine.\n");

  if (dev->dev_addr[0] == SA_ADDR0 &&
      dev->dev_addr[1] == SA_ADDR1 &&
      dev->dev_addr[2] == SA_ADDR2)
       lp->eepro = 1;              /* Yes, an Intel EtherExpress Pro/10 */
  else lp->eepro = 0;              /* No, it is a generic 82585 lan card */

  /* Get the interrupt vector for the 82595
   */
  if (dev->irq < 2 && eepro_grab_irq(dev) == 0)
  {
    printk ("%s: unable to get IRQ %d.\n", dev->name, dev->irq);
    return (0);
  }

  if (irq2dev_map[dev->irq] || (irq2dev_map[dev->irq] = dev) == 0)
    return (0);

  /* Initialize the 82595
   */
  outb (BANK2_SELECT, ioaddr);	/* be CAREFUL, BANK 2 now */
  temp_reg = inb (ioaddr + EEPROM_REG);

  lp->stepping = temp_reg >> 5;	/* Get the stepping number of the 595 */

  if (net_debug > 3)
    printk ("The stepping of the 82595 is %d\n", lp->stepping);

  if (temp_reg & 0x10)		/* Check the TurnOff Enable bit */
    outb (temp_reg & 0xef, ioaddr + EEPROM_REG);

  for (i = 0; i < 6; i++)
    outb (dev->dev_addr[i], ioaddr + I_ADD_REG0 + i);

  temp_reg = inb (ioaddr + REG1);                /* Setup Transmit Chaining */
  outb (temp_reg|XMT_Chain_Int|XMT_Chain_ErrStop /* and discard bad RCV frames */
	| RCV_Discard_BadFrame, ioaddr + REG1);

  temp_reg = inb (ioaddr + REG2);          /* Match broadcast */
  outb (temp_reg | 0x14, ioaddr + REG2);

  temp_reg = inb (ioaddr + REG3);
  outb (temp_reg & 0x3f, ioaddr + REG3);   /* clear test mode */

  /* Set the receiving mode
   */
  outb (BANK1_SELECT, ioaddr);	/* be CAREFUL, BANK 1 now */

  temp_reg = inb (ioaddr + INT_NO_REG);
  outb ((temp_reg & 0xf8) | irqrmap[dev->irq], ioaddr + INT_NO_REG);

  /* Initialize the RCV and XMT upper and lower limits
   */
  outb (RCV_LOWER_LIMIT, ioaddr + RCV_LOWER_LIMIT_REG);
  outb (RCV_UPPER_LIMIT, ioaddr + RCV_UPPER_LIMIT_REG);
  outb (XMT_LOWER_LIMIT, ioaddr + XMT_LOWER_LIMIT_REG);
  outb (XMT_UPPER_LIMIT, ioaddr + XMT_UPPER_LIMIT_REG);

  /* Enable the interrupt line
   */
  temp_reg = inb (ioaddr + REG1);
  outb (temp_reg | INT_ENABLE, ioaddr + REG1);

  outb (BANK0_SELECT, ioaddr);	/* Switch back to Bank 0 */

  /* Let RX and TX events to interrupt
   */
  outb (ALL_MASK & ~(RX_MASK | TX_MASK), ioaddr + INT_MASK_REG);

  /* clear all interrupts
   */
  outb (ALL_MASK, ioaddr + STATUS_REG);

  /* Initialize RCV
   */
  outw (RCV_LOWER_LIMIT << 8, ioaddr + RCV_BAR);
  lp->rx_start = (RCV_LOWER_LIMIT << 8);
  outw ((RCV_UPPER_LIMIT << 8) | 0xfe, ioaddr + RCV_STOP);

  /* Initialize XMT
   */
  outw (XMT_LOWER_LIMIT << 8, ioaddr + XMT_BAR);

  /* Check for the i82595TX and i82595FX
   */
  old8 = inb (ioaddr + 8);
  outb (~old8, ioaddr + 8);
  if ((temp_reg = inb (ioaddr + 8)) == old8)
  {
    if (net_debug > 3)
      printk ("i82595 detected!\n");
    lp->version = LAN595;
  }
  else
  {
    lp->version = LAN595TX;
    outb (old8, ioaddr + 8);
    old9 = inb (ioaddr + 9);
    outb (~old9, ioaddr + 9);
    if ((temp_reg = inb (ioaddr + 9)) == ~old9)
    {
      if (net_debug > 3)
         printk ("i82595FX detected!\n");

      lp->version = LAN595FX;
      outb (old9, ioaddr + 9);
      if (dev->if_port != TPE)
      {				/* Hopefully, this will fix the
				 * problem of using Pentiums and
                                 * pro/10 w/ BNC.
                                 */
	outb (BANK2_SELECT, ioaddr);	/* be CAREFUL, BANK 2 now */
	temp_reg = inb (ioaddr + REG13);

        /* disable the full duplex mode since it is not
         * applicable with the 10Base2 cable.
         */
	outb (temp_reg & ~(FDX | A_N_ENABLE), REG13);
	outb (BANK0_SELECT, ioaddr);	/* be CAREFUL, BANK 0 now */
      }
    }
    else if (net_debug > 3)
      printk ("i82595TX detected!\n");
  }

  outb (SEL_RESET_CMD, ioaddr);

  /* We are supposed to wait for 2 us after a SEL_RESET
   */
  SLOW_DOWN_IO();
  SLOW_DOWN_IO();

  lp->tx_start = lp->tx_end = XMT_LOWER_LIMIT << 8;	/* or = RCV_RAM */
  lp->tx_last = 0;

  dev->tx_busy = 0;
  dev->reentry = 0;
  dev->start   = 1;

  if (net_debug > 3)
    printk ("eepro: exiting eepro_open routine.\n");

  outb (RCV_ENABLE_CMD, ioaddr);
  return (1);
}

static int eepro_send_packet (struct device *dev, const void *buf, int len)
{
  struct eepro_local *lp = (struct eepro_local*) dev->priv;
  int    ioaddr  = dev->base_addr;
  int    rcv_ram = dev->mem_end;

  if (net_debug > 5)
     printk ("eepro: entering eepro_send_packet routine.\n");

  if (dev->tx_busy)
  {
    /* If we get here, some higher level has decided we are broken.
     * There should really be a "kick me" function call instead.
     */
    int tickssofar = jiffies - dev->tx_start;

    if (tickssofar < 40)
       return (1);

    if (net_debug > 1)
       printk ("%s: transmit timed out, %s?\n", dev->name,
               "network cable problem");
    lp->stats.tx_errors++;

    /* Try to restart the adaptor.
     */
    outb (SEL_RESET_CMD, ioaddr);
    /* We are supposed to wait for 2 us after a SEL_RESET
     */
    SLOW_DOWN_IO();
    SLOW_DOWN_IO();

    /* Do I also need to flush the transmit buffers here? YES?
     */
    lp->tx_start = lp->tx_end = rcv_ram;
    lp->tx_last  = 0;

    dev->tx_busy  = 0;
    dev->tx_start = jiffies;

    outb (RCV_ENABLE_CMD, ioaddr);
  }

  /* If some higher layer thinks we've missed an tx-done interrupt
   * we are passed NULL.
   */
  if (buf == NULL)
    return (0);

  /* Block a timer-based transmit from overlapping
   */
  if (dev->tx_busy)
    printk ("%s: Transmitter access conflict.\n", dev->name);
  else
  {
    short length = ETH_MIN < len ? len : ETH_MIN;
    hardware_send (dev, buf, length);
    dev->tx_start = jiffies;
  }

  if (net_debug > 5)
    printk ("eepro: exiting eepro_send_packet routine.\n");

  return (0);
}


/*
 * The typical workload of the driver: Handle the network interface
 * interrupts.
 */
static void eepro_interrupt (int irq)
{
  struct device *dev = (struct device *) irq2dev_map[irq];
  int    ioaddr, status, boguscount = 20;

  if (net_debug > 5)
    printk ("eepro: entering eepro_interrupt routine.\n");

  if (!dev || !dev->irq)
  {
    printk ("eepro_interrupt(): irq %d for unknown device.\n", irq);
    return;
  }
  dev->reentry = 1;

  ioaddr = dev->base_addr;

  do
  {
    status = inb (ioaddr + STATUS_REG);

    if (status & RX_INT)
    {
      if (net_debug > 4)
	printk ("eepro: packet received interrupt.\n");

      /* Acknowledge the RX_INT */
      outb (RX_INT, ioaddr + STATUS_REG);

      /* Get the received packets */
      eepro_rx (dev);
    }

    else if (status & TX_INT)
    {
      if (net_debug > 4)
	printk ("eepro: packet transmit interrupt.\n");

      /* Acknowledge the TX_INT */
      outb (TX_INT, ioaddr + STATUS_REG);

      /* Process the status of transmitted packets */
      eepro_tx_interrupt (dev);
    }
  }
  while ((boguscount-- > 0) && (status & 0x06));

  dev->reentry = 0;
  if (net_debug > 5)
    printk ("eepro: exiting eepro_interrupt routine.\n");
}

static void eepro_close (struct device *dev)
{
  struct eepro_local *lp = (struct eepro_local*) dev->priv;
  int    ioaddr  = dev->base_addr;
  int    rcv_ram = dev->mem_end;
  short  temp_reg;

  dev->tx_busy = 1;
  dev->start   = 0;

  outb (BANK1_SELECT, ioaddr);	/* Switch back to Bank 1 */

  /* Disable the physical interrupt line
   */
  temp_reg = inb (ioaddr + REG1);
  outb (temp_reg & 0x7f, ioaddr + REG1);

  outb (BANK0_SELECT, ioaddr);	/* Switch back to Bank 0 */

  /* Flush the Tx and disable Rx
   */
  outb (STOP_RCV_CMD, ioaddr);
  lp->tx_start = lp->tx_end = rcv_ram;
  lp->tx_last  = 0;

  /* Mask all the interrupts
   */
  outb (ALL_MASK, ioaddr + INT_MASK_REG);

  /* clear all interrupts
   */
  outb (ALL_MASK, ioaddr + STATUS_REG);

  /* Reset the 82595
   */
  outb (RESET_CMD, ioaddr);

  free_irq (dev->irq);
  irq2dev_map[dev->irq] = 0;

  /* Update the statistics here. What statistics? */

  /* We are supposed to wait for 200 us after a RESET */
  SLOW_DOWN_IO();
  SLOW_DOWN_IO();
}

/*
 * Get the current statistics.  This may be called with the card
 * open or closed.
 */
static void*eepro_get_stats (struct device *dev)
{
  struct eepro_local *lp = (struct eepro_local*) dev->priv;
  return (void*)&lp->stats;
}

/*
 * Set or clear the multicast filter for this adaptor.
 */
static void set_multicast_list (struct device *dev)
{
  struct eepro_local *lp  = (struct eepro_local *) dev->priv;
  short  ioaddr = dev->base_addr;
  WORD   mode;

  if (dev->flags & (IFF_ALLMULTI|IFF_PROMISC) || dev->mc_count > 63)
  {
    /*
     * We must make the kernel realise we had to move
     * into promisc mode or we start all out war on
     * the cable. If it was a promisc request the
     * flag is already set. If not we assert it.
     */
    dev->flags |= IFF_PROMISC;

    outb (BANK2_SELECT, ioaddr);    /* be CAREFUL, BANK 2 now */
    mode = inb (ioaddr + REG2);
    outb (mode | PRMSC_Mode, ioaddr + REG2);
    mode = inb (ioaddr + REG3);
    outb (mode, ioaddr + REG3);     /* writing reg. 3 to complete the update */
    outb (BANK0_SELECT, ioaddr);    /* Return to BANK 0 now */
    printk ("%s: promiscuous mode enabled.\n", dev->name);
  }
  else if (dev->mc_count == 0)
  {
    outb (BANK2_SELECT, ioaddr);    /* be CAREFUL, BANK 2 now */
    mode = inb (ioaddr + REG2);
    outb (mode & 0xd6, ioaddr+REG2);/* Turn off Multi-IA and PRMSC_Mode bits */
    mode = inb (ioaddr + REG3);
    outb (mode, ioaddr + REG3);     /* writing reg. 3 to complete the update */
    outb (BANK0_SELECT, ioaddr);    /* Return to BANK 0 now */
  }
  else
  {
    WORD status, *eaddrs;
    int  i, boguscount = 0;

    /* Disable RX and TX interrupts.  Necessary to avoid
     * corruption of the HOST_ADDRESS_REG by interrupt
     * service routines
     */
    outb (ALL_MASK, ioaddr + INT_MASK_REG);

    outb (BANK2_SELECT, ioaddr);      /* be CAREFUL, BANK 2 now */
    mode = inb (ioaddr + REG2);
    outb (mode | Multi_IA, ioaddr + REG2);
    mode = inb (ioaddr + REG3);
    outb (mode, ioaddr + REG3);       /* writing reg. 3 to complete the update */
    outb (BANK0_SELECT, ioaddr);      /* Return to BANK 0 now */
    outw (lp->tx_end, ioaddr + HOST_ADDRESS_REG);
    outw (MC_SETUP, ioaddr + IO_PORT);
    outw (0, ioaddr + IO_PORT);
    outw (0, ioaddr + IO_PORT);
    outw (6 * (dev->mc_count + 1), ioaddr + IO_PORT);

    for (i = 0; i < dev->mc_count; i++)
    {
      eaddrs = (WORD*) &dev->mc_list[i];
      outw (*eaddrs++, ioaddr + IO_PORT);
      outw (*eaddrs++, ioaddr + IO_PORT);
      outw (*eaddrs++, ioaddr + IO_PORT);
    }
    eaddrs = (WORD*) dev->dev_addr;
    outw (eaddrs[0], ioaddr + IO_PORT);
    outw (eaddrs[1], ioaddr + IO_PORT);
    outw (eaddrs[2], ioaddr + IO_PORT);
    outw (lp->tx_end, ioaddr + XMT_BAR);
    outb (MC_SETUP, ioaddr);

    /* Update the transmit queue
     */
    i = lp->tx_end + XMT_HEADER + 6 * (dev->mc_count + 1);
    if (lp->tx_start != lp->tx_end)
    {
      /* update the next address and the chain bit in the
       * last packet
       */
      outw (lp->tx_last + XMT_CHAIN, ioaddr + HOST_ADDRESS_REG);
      outw (i, ioaddr + IO_PORT);
      outw (lp->tx_last + XMT_COUNT, ioaddr + HOST_ADDRESS_REG);
      status = inw (ioaddr + IO_PORT);
      outw (status | CHAIN_BIT, ioaddr + IO_PORT);
      lp->tx_end = i;
    }
    else
      lp->tx_start = lp->tx_end = i;

    /* Acknowledge that the MC setup is done
     */
    do
    {     /* We should be doing this in the eepro_interrupt()! */
      SLOW_DOWN_IO();
      SLOW_DOWN_IO();
      if (inb (ioaddr + STATUS_REG) & 0x08)
      {
	i = inb (ioaddr);
	outb (0x08, ioaddr + STATUS_REG);
        if (i & 0x20)                    /* command ABORTed */
        {                     
          printk ("%s: multicast setup failed.\n", dev->name);
	  break;
	}
	else if ((i & 0x0f) == 0x03)
	{			/* MC-Done */
	  printk ("%s: set Rx mode to %d addresses.\n",
		  dev->name, dev->mc_count);
	  break;
	}
      }
    }
    while (++boguscount < 100);

    /* Re-enable RX and TX interrupts
     */
    outb (ALL_MASK & ~(RX_MASK | TX_MASK), ioaddr + INT_MASK_REG);
  }
  outb (RCV_ENABLE_CMD, ioaddr);
}

/*
 * The horrible routine to read a word from the serial EEPROM.
 *
 * IMPORTANT - the 82595 will be set to Bank 0 after the eeprom is read
 *
 * The delay between EEPROM clock transitions.
 */
#define eeprom_delay()  { int _i=40; while (--_i>0) __SLOW_DOWN_IO(); }
#define EE_READ_CMD     (6 << 6)

int read_eeprom (int ioaddr, int location)
{
  int   i;
  WORD  retval   = 0;
  short ee_addr  = ioaddr + EEPROM_REG;
  short ctrl_val = EECS;
  int   read_cmd = location | EE_READ_CMD;

  outb (BANK2_SELECT, ioaddr);
  outb (ctrl_val, ee_addr);

  /* Shift the read command bits out
   */
  for (i = 8; i >= 0; i--)
  {
    short outval = (read_cmd & (1 << i)) ? ctrl_val | EEDI : ctrl_val;

    outb (outval, ee_addr);
    outb (outval | EESK, ee_addr);  /* EEPROM clock tick. */
    eeprom_delay();
    outb (outval, ee_addr);         /* Finish EEPROM a clock tick. */
    eeprom_delay();
  }
  outb (ctrl_val, ee_addr);

  for (i = 16; i > 0; i--)
  {
    outb (ctrl_val | EESK, ee_addr);
    eeprom_delay();
    retval = (retval << 1) | ((inb (ee_addr) & EEDO) ? 1 : 0);
    outb (ctrl_val, ee_addr);
    eeprom_delay();
  }

  /* Terminate the EEPROM access
   */
  ctrl_val &= ~EECS;
  outb (ctrl_val | EESK, ee_addr);
  eeprom_delay();
  outb (ctrl_val, ee_addr);
  eeprom_delay();
  outb (BANK0_SELECT, ioaddr);
  return (retval);
}

static void hardware_send (struct device *dev, const void *buf, short length)
{
  struct eepro_local *lp = (struct eepro_local *) dev->priv;
  short    ioaddr  = dev->base_addr;
  int      rcv_ram = dev->mem_end;
  unsigned status, tx_available, last, end;
  unsigned boguscount = 100;

  if (net_debug > 5)
     printk ("eepro: entering hardware_send().\n");

  while (boguscount-- > 0)
  {
    /* Disable RX and TX interrupts.  Necessary to avoid corruption
     * of the HOST_ADDRESS_REG by interrupt service routines.
     */
    outb (ALL_MASK, ioaddr + INT_MASK_REG);

    if (dev->reentry)
    {
      /* Enable RX and TX interrupts */
      outb (ALL_MASK & ~(RX_MASK | TX_MASK), ioaddr + INT_MASK_REG);
      continue;
    }

    /* determine how much of the transmit buffer space is available */
    if (lp->tx_end > lp->tx_start)
         tx_available = XMT_RAM - (lp->tx_end - lp->tx_start);
    else if (lp->tx_end < lp->tx_start)
         tx_available = lp->tx_start - lp->tx_end;
    else tx_available = XMT_RAM;

    /* space available? */
    if (((((length + 3) >> 1) << 1) + 2 * XMT_HEADER) >= tx_available)       
    {
      eepro_tx_interrupt (dev);   /* Clean up the transmiting queue */

      /* Enable RX and TX interrupts */
      outb (ALL_MASK & ~(RX_MASK | TX_MASK), ioaddr + INT_MASK_REG);
      continue;
    }

    last = lp->tx_end;
    end = last + (((length + 3) >> 1) << 1) + XMT_HEADER;

    if (end >= RAM_SIZE)     /* the transmit buffer is wrapped around */
    {                      
      if ((RAM_SIZE - last) <= XMT_HEADER)
      {
	/* Arrrr!!!, must keep the xmt header together,
         * several days were lost to chase this one down.
         */
	last = rcv_ram;
	end = last + (((length + 3) >> 1) << 1) + XMT_HEADER;
      }
      else
	end = rcv_ram + (end - RAM_SIZE);
    }

    outw (last, ioaddr + HOST_ADDRESS_REG);
    outw (XMT_CMD, ioaddr + IO_PORT);
    outw (0, ioaddr + IO_PORT);
    outw (end, ioaddr + IO_PORT);
    outw (length, ioaddr + IO_PORT);

    if (lp->version == LAN595)
      rep_outsw (ioaddr + IO_PORT, buf, (length+3) >> 1);
    else
    {           /* LAN595TX or LAN595FX, capable of 32-bit I/O processing */
      WORD temp = inb (ioaddr + INT_MASK_REG);

      outb (temp | IO_32_BIT, ioaddr + INT_MASK_REG);
      rep_outsl (ioaddr + IO_PORT_32_BIT, buf, (length+3) >> 2);
      outb (temp & ~(IO_32_BIT), ioaddr + INT_MASK_REG);
    }

    /* A dummy read to flush the DRAM write pipeline
     */
    status = inw (ioaddr + IO_PORT);

    if (lp->tx_start == lp->tx_end)
    {
      outw (last, ioaddr + XMT_BAR);
      outb (XMT_CMD, ioaddr);
      lp->tx_start = last;	/* I don't like to change tx_start here */
    }
    else
    {
      /* update the next address and the chain bit in the last packet
       */
      if (lp->tx_end != last)
      {
	outw (lp->tx_last + XMT_CHAIN, ioaddr + HOST_ADDRESS_REG);
	outw (last, ioaddr + IO_PORT);
      }
      outw (lp->tx_last + XMT_COUNT, ioaddr + HOST_ADDRESS_REG);
      status = inw (ioaddr + IO_PORT);
      outw (status | CHAIN_BIT, ioaddr + IO_PORT);

      /* Continue the transmit command
       */
      outb (RESUME_XMT_CMD, ioaddr);
    }

    lp->tx_last = last;
    lp->tx_end = end;

    /* Enable RX and TX interrupts
     */
    outb (ALL_MASK & ~(RX_MASK | TX_MASK), ioaddr + INT_MASK_REG);

    dev->tx_busy = 0;

    if (net_debug > 5)
      printk ("eepro: exiting hardware_send().\n");
    return;
  }

  dev->tx_busy = 1;
  if (net_debug > 5)
    printk ("eepro: exiting hardware_send().\n");
}

static void eepro_rx (struct device *dev)
{
  struct eepro_local *lp = (struct eepro_local *) dev->priv;
  short  ioaddr     = dev->base_addr;
  short  rcv_ram    = dev->mem_end;
  short  boguscount = 20;
  short  rcv_car    = lp->rx_start;
  WORD   rcv_event, rcv_status, rcv_next_frame, rcv_size;

  if (net_debug > 5)
    printk ("eepro: entering eepro_rx routine.\n");

  /* Set the read pointer to the start of the RCV
   */
  outw (rcv_car, ioaddr + HOST_ADDRESS_REG);
  rcv_event = inw (ioaddr + IO_PORT);

  while (rcv_event == RCV_DONE)
  {
    rcv_status     = inw (ioaddr + IO_PORT);
    rcv_next_frame = inw (ioaddr + IO_PORT);
    rcv_size       = inw (ioaddr + IO_PORT);

    if ((rcv_status & (RX_OK | RX_ERROR)) == RX_OK)
    {
      if (dev->get_rx_buf)
      {
        short pkt_len = (rcv_size & 0x3fff);
        BYTE *pkt_buf = (*dev->get_rx_buf) (pkt_len);
        if (!pkt_buf)
           lp->stats.rx_dropped++;
        else
        {
          if (lp->version == LAN595)
          {
            pkt_len = (pkt_len+3) >> 1;
            rep_insw (ioaddr + IO_PORT, (WORD*)pkt_buf, pkt_len);
          }
          else  /* LAN595TX or LAN595FX, capable of 32-bit I/O processing */
          {   
            WORD temp = inb (ioaddr + INT_MASK_REG);

            pkt_len = (pkt_len+3) >> 2;
            outb (temp | IO_32_BIT, ioaddr + INT_MASK_REG);
            rep_insl (ioaddr + IO_PORT_32_BIT, (DWORD*)pkt_buf, pkt_len);
            outb (temp & ~(IO_32_BIT), ioaddr + INT_MASK_REG);
          }
        }
      }
      lp->stats.rx_packets++;
    }
    else
    {       /* Not sure will ever reach here,
             * I set the 595 to discard bad received frames
             */
      lp->stats.rx_errors++;
      if (rcv_status & 0x0100)
	lp->stats.rx_over_errors++;
      else if (rcv_status & 0x0400)
	lp->stats.rx_frame_errors++;
      else if (rcv_status & 0x0800)
	lp->stats.rx_crc_errors++;
      printk ("%s: event = %#x, status = %#x, next = %#x, size = %#x\n",
	      dev->name, rcv_event, rcv_status, rcv_next_frame, rcv_size);
    }
    if (rcv_status & 0x1000)
      lp->stats.rx_length_errors++;

    if (--boguscount == 0)
      break;

    rcv_car = lp->rx_start + RCV_HEADER + rcv_size;
    lp->rx_start = rcv_next_frame;
    outw (rcv_next_frame, ioaddr + HOST_ADDRESS_REG);
    rcv_event = inw (ioaddr + IO_PORT);

  }
  if (rcv_car == 0)
    rcv_car = (RCV_UPPER_LIMIT << 8) | 0xff;
  outw (rcv_car - 1, ioaddr + RCV_STOP);

  if (net_debug > 5)
    printk ("eepro: exiting eepro_rx routine.\n");
}

static void eepro_tx_interrupt (struct device *dev)
{
  struct eepro_local *lp = (struct eepro_local *) dev->priv;
  short  ioaddr          = dev->base_addr;
  short  boguscount      = 20;
  short  xmt_status;

  while (lp->tx_start != lp->tx_end)
  {
    outw (lp->tx_start, ioaddr + HOST_ADDRESS_REG);
    xmt_status = inw (ioaddr + IO_PORT);
    if ((xmt_status & TX_DONE_BIT) == 0)
      break;

    xmt_status   = inw (ioaddr + IO_PORT);
    lp->tx_start = inw (ioaddr + IO_PORT);
    dev->tx_busy = 0;

    if (xmt_status & 0x2000)
      lp->stats.tx_packets++;
    else
    {
      lp->stats.tx_errors++;
      if (xmt_status & 0x0400)
	lp->stats.tx_carrier_errors++;
      printk ("%s: XMT status = %#x\n", dev->name, xmt_status);
    }
    if (xmt_status & 0x000f)
      lp->stats.tx_collisions += (xmt_status & 0x000f);

    if ((xmt_status & 0x0040) == 0x0)
      lp->stats.tx_heartbeat_errors++;

    if (--boguscount == 0)
      break;
  }
}
