/*------------------------------------------------------------------------
 . smc9194.c
 . This is a driver for SMC's 9000 series of Ethernet cards.
 .
 . Copyright (C) 1996 by Erik Stahlman
 . This software may be used and distributed according to the terms
 . of the GNU Public License, incorporated herein by reference.
 .
 . "Features" of the SMC chip:
 .   4608 byte packet memory. ( for the 91C92.  Others have more )
 .   EEPROM for configuration
 .   AUI/TP selection  ( mine has 10Base2/10BaseT select )
 .
 . Arguments:
 .   io     = for the base address
 .  irq   = for the IRQ
 .  ifport = 0 for autodetect, 1 for TP, 2 for AUI ( or 10base2 )
 .
 . author:
 .   Erik Stahlman        ( erik@vt.edu )
 .
 . Hardware multicast code from Peter Cammaert ( pc@denkart.be )
 .
 . Sources:
 .    o   SMC databook
 .    o   skeleton.c by Donald Becker ( becker@cesdis.gsfc.nasa.gov )
 .    o   ( a LOT of advice from Becker as well )
 .
 . History:
 .  12/07/95  Erik Stahlman  written, got receive/xmit handled
 .  01/03/96  Erik Stahlman  worked out some bugs, actually usable!!! :-)
 .  01/06/96  Erik Stahlman  cleaned up some, better testing, etc
 .  01/29/96  Erik Stahlman  fixed autoirq, added multicast
 .  02/01/96  Erik Stahlman  1. disabled all interrupts in smc_reset
 .                           2. got rid of post-decrementing bug -- UGH.
 .  02/13/96  Erik Stahlman  Tried to fix autoirq failure.  Added more
 .                           descriptive error messages.
 .  02/15/96  Erik Stahlman  Fixed typo that caused detection failure
 .  02/23/96  Erik Stahlman  Modified it to fit into kernel tree
 .                           Added support to change hardware address
 .                           Cleared stats on opens
 .  02/26/96  Erik Stahlman  Trial support for Kernel 1.2.13
 .                           Kludge for automatic IRQ detection
 .  03/04/96  Erik Stahlman  Fixed kernel 1.3.70 +
 .                           Fixed bug reported by Gardner Buchanan in
 .                           smc_enable, with outw instead of outb
 .  03/06/96  Erik Stahlman  Added hardware multicast from Peter Cammaert
 ----------------------------------------------------------------------------*/

static const char *version = "smc9194.c:v0.12 03/06/96 by Erik Stahlman (erik@vt.edu)\n";

#include "pmdrvr.h"
#include "smc9194.h"

/*
 * Configuration options, for the experienced user to change.
 */

/*
 * Do you want to use 32 bit xfers?  This should work on all chips, as
 * the chipset is designed to accommodate them.
 */
#define USE_32_BIT 1

/*
 * The SMC9194 can be at any of the following port addresses.  To change,
 * for a slightly different card, you can add it to the array.  Keep in
 * mind that the array must end in zero.
 */
static DWORD smc_portlist[] = { 0x200, 0x220, 0x240, 0x260, 0x280,
                                0x2A0, 0x2C0, 0x2E0, 0x300, 0x320,
                                0x340, 0x360, 0x380, 0x3A0, 0x3C0,
                                0x3E0, 0
                              };

static char *chip_ids[15] = {
             NULL, NULL, NULL,
             /* 3 */ "SMC91C90/91C92",
             /* 4 */ "SMC91C94",
             /* 5 */ "SMC91C95",
             NULL,
             /* 7 */ "SMC91C100",
             NULL, NULL, NULL, NULL,
             NULL, NULL, NULL
           };

static char *interfaces[2] = { "TP", "AUI" };

/*
 * Wait time for memory to be free.  This probably shouldn't be
 * tuned that much, as waiting for this means nothing else happens
 * in the system
 */
#define MEMORY_WAIT_TIME 16

/*
 * DEBUGGING LEVELS
 *
 * 0 for normal operation
 * 1 for slightly more details
 * >2 for various levels of increasingly useless information
 *    2 for interrupt tracking, status flags
 *    3 for packet dumps, etc.
 */
#ifndef SMC_DEBUG
#define SMC_DEBUG 0
#endif

int smc_debug = SMC_DEBUG;

#define PRINTK(lvl,fmt) do {                    \
                          if (smc_debug >= lvl) \
                             printk fmt ;       \
                        } while (0)

/*------------------------------------------------------------------------
 .
 . The internal workings of the driver.  If you are changing anything
 . here with the SMC stuff, you should have the datasheet and known
 . what you are doing.
 .
 -------------------------------------------------------------------------*/
#define CARDNAME "SMC9194"

/* store this information for the driver.
 */
struct smc_local {
  /* these are things that the kernel wants me to keep, so users
   * can find out semi-useless statistics of how well the card is
   * performing
   */
  struct net_device_stats stats;

  /* This keeps track of how many packets that I have
   * sent out.  When an TX_EMPTY interrupt comes, I know
   * that all of these have been sent.
   */
  int packets_waiting;
};


/*-----------------------------------------------------------------
 .
 .  The driver can be entered at any of the following entry points.
 .
 .------------------------------------------------------------------  */

/* This is called by  register_netdev().  It is responsible for
 * checking the portlist for the SMC9000 series chipset.  If it finds
 * one, then it will initialize the device, find the hardware information,
 * and sets up the appropriate device parameters.
 * NOTE: Interrupts are *OFF* when this procedure is called.
 *
 * NB:This shouldn't be static since it is referred to externally.
 */
int smc_probe (struct device *dev);

/* The kernel calls this function when someone wants to use the device,
 * typically 'ifconfig ethX up'.
 */
static int smc_open (struct device *dev);

/* This is called by the kernel to send a packet out into the net.  it's
 * responsible for doing a best-effort send, but if it's simply not possible
 * to send it, the packet gets dropped.
 */
static int smc_send_packet (struct device *dev, void *buf, int len);

/* This is called by the kernel in response to 'ifconfig ethX down'.  It
 * is responsible for cleaning up everything that the open routine
 * does, and maybe putting the card into a powerdown state.
 */
static void smc_close (struct device *dev);

/* This routine allows the proc file system to query the driver's
 * statistics.
 */
static void *smc_query_statistics (struct device *dev);

#ifdef HAVE_MULTICAST
/* Finally, a call to set promiscuous mode (for TCPDUMP and related
 * programs) and multicast modes.
 */
static void smc_set_multicast_list (struct device *dev);
#endif


/* Handles the actual interrupt
 */
static void smc_interrupt (int irq);

/* This is a separate procedure to handle the receipt of a packet, to
 * leave the interrupt code looking slightly cleaner
 */
static void smc_rcv (struct device *dev);

/* This handles a TX interrupt, which is only called when an error
 * relating to a packet is sent.
 */
static void smc_tx (struct device *dev);

/* Test if a given location contains a chip, trying to cause as
 * little damage as possible if it's not a SMC chip.
 */
static int smc_probe1 (int ioaddr);

/* This routine initializes the cards hardware, prints out the configuration
 * to the system log as well as the vanity message, and handles the setup
 * of a device parameter.
 * It will give an error if it can't initialize the card.
 */
static int smc_initcard (struct device *, int ioaddr);

#define tx_done(dev) 1

/* this is called to actually send the packet to the chip
 */
static void smc_hardware_send_packet (struct device *dev, void *buf, int len);

/* Since I am not sure if I will have enough room in the chip's ram
 * to store the packet, I call this routine, which either sends it
 * now, or generates an interrupt when the card is ready for the packet
 */
static int smc_wait_to_send_packet (struct device *dev, void *buf, int len);

/* this does a soft reset on the device
 */
static void smc_reset (int ioaddr);

/* Enable Interrupts, Receive, and Transmit
 */
static void smc_enable (int ioaddr);

/* this puts the device in an inactive state
 */
static void smc_shutdown (int ioaddr);

#ifndef NO_AUTOPROBE
/* This routine will find the IRQ of the driver if one is not
 * specified in the input to the device.
 */
static int smc_findirq (int ioaddr);
#endif

/*
 * Function: smc_reset (int ioaddr)
 * Purpose:
 *    This sets the SMC91xx chip to its normal state, hopefully from whatever
 *   mess that any other DOS driver has put it in.
 *
 * Maybe I should reset more registers to defaults in here?  SOFTRESET  should
 * do that for me.
 *
 * Method:
 *  1.  send a SOFT RESET
 *  2.  wait for it to finish
 *  3.  enable autorelease mode
 *  4.  reset the memory management unit
 *  5.  clear all interrupts
 *
 */
static void smc_reset (int ioaddr)
{
  /* This resets the registers mostly to defaults, but doesn't
   * affect EEPROM.  That seems unnecessary
   */
  SMC_SELECT_BANK (0);
  outw (RCR_SOFTRESET, ioaddr + RCR);

  /* this should pause enough for the chip to be happy
   */
  SMC_DELAY();

  /* Set the transmit and receive configuration registers to
   * default values
    */
  outw (RCR_CLEAR, ioaddr + RCR);
  outw (TCR_CLEAR, ioaddr + TCR);

  /* set the control register to automatically
   * release successfully transmitted packets, to make the best
   * use out of our limited memory
    */
  SMC_SELECT_BANK (1);
  outw (inw (ioaddr + CONTROL) | CTL_AUTO_RELEASE, ioaddr + CONTROL);

  /* Reset the MMU
   */
  SMC_SELECT_BANK (2);
  outw (MC_RESET, ioaddr + MMU_CMD);

  /* Note:  It doesn't seem that waiting for the MMU busy is needed here,
   * but this is a place where future chipsets _COULD_ break.  Be wary
   * of issuing another MMU command right after this
   */
  outb (0, ioaddr + INT_MASK);
}

/*
 * Function: smc_enable
 * Purpose: let the chip talk to the outside work
 * Method:
 *  1.  Enable the transmitter
 *  2.  Enable the receiver
 *  3.  Enable interrupts
 */
static void smc_enable (int ioaddr)
{
  SMC_SELECT_BANK (0);
  /* see the header file for options in TCR/RCR NORMAL
   */
  outw (TCR_NORMAL, ioaddr + TCR);
  outw (RCR_NORMAL, ioaddr + RCR);

  /* now, enable interrupts
   */
  SMC_SELECT_BANK (2);
  outb (SMC_INTERRUPT_MASK, ioaddr + INT_MASK);
}

/*
 * Function: smc_shutdown
 * Purpose:  closes down the SMC91xxx chip.
 * Method:
 *  1. zero the interrupt mask
 *  2. clear the enable receive flag
 *  3. clear the enable xmit flags
 *
 * TODO:
 *   (1) maybe utilize power down mode.
 *  Why not yet?  Because while the chip will go into power down mode,
 *  the manual says that it will wake up in response to any I/O requests
 *  in the register space.   Empirical results do not show this working.
 */
static void smc_shutdown (int ioaddr)
{
  /* no more interrupts for me
   */
  SMC_SELECT_BANK (2);
  outb (0, ioaddr + INT_MASK);

  /* and tell the card to stay away from that nasty outside world
   */
  SMC_SELECT_BANK (0);
  outb (RCR_CLEAR, ioaddr + RCR);
  outb (TCR_CLEAR, ioaddr + TCR);
#if 0
  /* finally, shut the chip down */
  SMC_SELECT_BANK (1);
  outw (inw (ioaddr + CONTROL), CTL_POWERDOWN, ioaddr + CONTROL);
#endif
}



/*
 * Function: smc_wait_to_send_packet()
 * Purpose:
 *    Attempt to allocate memory for a packet, if chip-memory is not
 *    available, then tell the card to generate an interrupt when it
 *    is available.
 *
 * Algorithm:
 *
 * o  if the saved_skb is null, then replace it with the current packet,
 * o  See if I can sending it now.
 * o   (NO): Enable interrupts and let the interrupt handler deal with it.
 * o  (YES):Send it now.
 */
static int smc_wait_to_send_packet (struct device *dev, void *buf, int len)
{
  struct smc_local *lp = (struct smc_local*) dev->priv;
  WORD   ioaddr = dev->base_addr;
  WORD   length, numPages, time_out;

  length = len < ETH_MIN ? ETH_MIN : len;

  /* the MMU wants the number of pages to be the number of 256 bytes
   * 'pages', minus 1 ( since a packet can't ever have 0 pages :) )
   */
  numPages = length / 256;

  if (numPages > 7)
  {
    printk (CARDNAME ": Far too big packet error. \n");
    return (0);
  }
  /* either way, a packet is waiting now
   */
  lp->packets_waiting++;

  /* now, try to allocate the memory
   */
  SMC_SELECT_BANK (2);
  outw (MC_ALLOC | numPages, ioaddr + MMU_CMD);

  /* Performance Hack
   *
   * wait a short amount of time.. if I can send a packet now, I send
   * it now.  Otherwise, I enable an interrupt and wait for one to be
   * available.
   *
   * I could have handled this a slightly different way, by checking to
   * see if any memory was available in the FREE MEMORY register.  However,
   * either way, I need to generate an allocation, and the allocation works
   * no matter what, so I saw no point in checking free memory.
   */
  time_out = MEMORY_WAIT_TIME;
  do
  {
    WORD status = inb (ioaddr + INTERRUPT);
    if (status & IM_ALLOC_INT)
    {
      /* acknowledge the interrupt
       */
      outb (IM_ALLOC_INT, ioaddr + INTERRUPT);
      break;
    }
  }
  while (--time_out);

  if (!time_out)
  {
    /* oh well, wait until the chip finds memory later
     */
    SMC_ENABLE_INT (IM_ALLOC_INT);
    PRINTK (2, (CARDNAME ": memory allocation deferred. \n"));
    /* it's deferred, but I'll handle it later
     */
    return (0);
  }
  /* or YES! I can send the packet now..
   */
  smc_hardware_send_packet (dev, buf, len);
  return (1);
}

/*
 * Function:  smc_hardware_send_packet()
 * Purpose:
 *  This sends the actual packet to the SMC9xxx chip.
 *
 * Algorithm:
 *  Find the packet number that the chip allocated
 *  Point the data pointers at it in memory
 *  Set the length word in the chip's memory
 *  Dump the packet to chip memory
 *  Check if a last byte is needed ( odd length packet )
 *    if so, set the control flag right
 *   Tell the card to send it
 *  Enable the transmit interrupt, so I know if it failed
 *   Free the kernel data if I actually sent it.
 */
static void smc_hardware_send_packet (struct device *dev, void *tx_buf, int len)
{
  BYTE   packet_no;
  BYTE  *buf    = (BYTE*)tx_buf;
  WORD   ioaddr = dev->base_addr;
  WORD   length = len < ETH_MIN ? ETH_MIN : len;

  /* If I get here, I _know_ there is a packet slot waiting for me
   */
  packet_no = inb (ioaddr + PNR_ARR + 1);
  if (packet_no & 0x80)
  {
    /* or isn't there?  BAD CHIP!
     */
    printk (CARDNAME ": Memory allocation failed. \n");
    dev->tx_busy = 0;
    return;
  }

  /* we have a packet address, so tell the card to use it
   */
  outb (packet_no, ioaddr + PNR_ARR);

  /* point to the beginning of the packet
   */
  outw (PTR_AUTOINC, ioaddr + POINTER);

  PRINTK (3, (CARDNAME ": Trying to xmit packet of length %x\n", length));

  /* send the packet length ( +6 for status, length and ctl byte)
   * and the status word (set to zeros)
   */
#ifdef USE_32_BIT
  outl ((length + 6) << 16, ioaddr + DATA_1);
#else
  outw (0, ioaddr + DATA_1);
  /* send the packet length (+6 for status words, length, and ctl
   */
  outb ((length + 6) & 0xFF, ioaddr + DATA_1);
  outb ((length + 6) >> 8, ioaddr + DATA_1);
#endif

  /* send the actual data
   * I _think_ it's faster to send the longs first, and then
   * mop up by sending the last word.  It depends heavily
   * on alignment, at least on the 486.  Maybe it would be
   * a good idea to check which is optimal?  But that could take
   * almost as much time as is saved?
   */
#ifdef USE_32_BIT
  if (length & 0x2)
  {
    outsl (ioaddr + DATA_1, buf, length >> 2);
    outw (*((WORD*)(buf + (length & 0xFFFFFFFC))), ioaddr + DATA_1);
  }
  else
    outsl (ioaddr + DATA_1, buf, length >> 2);
#else
  outsw (ioaddr + DATA_1, buf, (length) >> 1);
#endif

  /* Send the last byte, if there is one.
   */
  if ((length & 1) == 0)
    outw (0, ioaddr + DATA_1);
  else
  {
    outb (buf[length-1], ioaddr + DATA_1);
    outb (0x20, ioaddr + DATA_1);
  }

  /* enable the interrupts
   */
  SMC_ENABLE_INT ((IM_TX_INT | IM_TX_EMPTY_INT));

  /* and let the chipset deal with it
   */
  outw (MC_ENQUEUE, ioaddr + MMU_CMD);

  PRINTK (2, (CARDNAME ": Sent packet of length %d \n", length));

  dev->tx_start = jiffies;

  /* we can send another packet
   */
  dev->tx_busy = 0;
}

/*
 * smc_probe()
 *   Input parameters:
 *  dev->base_addr == 0, try to find all possible locations
 *  dev->base_addr == 1, return failure code
 *  dev->base_addr == 2, always allocate space,  and return success
 *  dev->base_addr == <anything else>   this is the address to check
 *
 *   Output:
 *  0 --> there is a device
 *  anything else, error
 *
 */
int smc_probe (struct device *dev)
{
  int i, base_addr = dev ? dev->base_addr : 0;

  /* try a specific location
   */
  if (base_addr > 0x1ff)
  {
    int error = smc_probe1 (base_addr);
    if (!error)
       return smc_initcard (dev, base_addr);
    return (error);
  }

  if (base_addr)
     return (0);

  /* check every ethernet address
   */
  for (i = 0; smc_portlist[i]; i++)
  {
    int ioaddr = smc_portlist[i];
    if (smc_probe1 (ioaddr))
       return smc_initcard (dev, ioaddr);
  }
  return (0);
}

#ifndef NO_AUTOPROBE
/*
 * smc_findirq()
 *
 * This routine has a simple purpose -- make the SMC chip generate an
 * interrupt, so an auto-detect routine can detect it, and find the IRQ,
 */
int smc_findirq (int ioaddr)
{
  int timeout = 20;

  /* I have to do a STI() here, because this is called from
   * a routine that does an CLI during this process, making it
   * rather difficult to get interrupts for auto detection
   */
  ENABLE();
  autoirq_setup (0);

  /* What I try to do here is trigger an ALLOC_INT. This is done
   * by allocating a small chunk of memory, which will give an interrupt
   * when done.
   */
  SMC_SELECT_BANK (2);

  /* enable ALLOCation interrupts ONLY
   */
  outb (IM_ALLOC_INT, ioaddr + INT_MASK);

  /* Allocate 512 bytes of memory.  Note that the chip was just
   * reset so all the memory is available
   */
  outw (MC_ALLOC | 1, ioaddr + MMU_CMD);

  /* Wait until positive that the interrupt has been generated
   */
  while (timeout)
  {
    BYTE int_status = inb (ioaddr + INTERRUPT);

    if (int_status & IM_ALLOC_INT)
       break;                    /* got the interrupt */
    timeout--;
  }
  /* there is really nothing that I can do here if timeout fails,
   * as autoirq_report will return a 0 anyway, which is what I
   * want in this case.   Plus, the clean up is needed in both
   * cases.
   */

  /* DELAY HERE!
   * On a fast machine, the status might change before the interrupt
   * is given to the processor.  This means that the interrupt was
   * never detected, and autoirq_report fails to report anything.
   * This should fix autoirq_* problems.
   */
  SMC_DELAY();
  SMC_DELAY();

  /* and disable all interrupts again
   */
  outb (0, ioaddr + INT_MASK);

  /* clear hardware interrupts again, because that's how it
   * was when I was called...
   */
  DISABLE();

  /* and return what I found
   */
  return autoirq_report (0);
}
#endif

/*
 * Function: smc_probe1()
 *
 * Purpose:
 *  Tests to see if a given ioaddr points to an SMC9xxx chip.
 *  Returns a 0 on success
 *
 * Algorithm:
 *  (1) see if the high byte of BANK_SELECT is 0x33
 *   (2) compare the ioaddr with the base register's address
 *  (3) see if I recognize the chip ID in the appropriate register
 */
static int smc_probe1 (int ioaddr)
{
  DWORD bank;
  WORD  revision_register;
  WORD  base_address_register;

  /* First, see if the high byte is 0x33
   */
  bank = inw (ioaddr + BANK_SELECT);
  if ((bank & 0xFF00) != 0x3300)
     return (0);

  /* The above MIGHT indicate a device, but I need to write to further
   * test this.
   */
  outw (0x0, ioaddr + BANK_SELECT);
  bank = inw (ioaddr + BANK_SELECT);
  if ((bank & 0xFF00) != 0x3300)
     return (0);

  /* well, we've already written once, so hopefully another time won't
   * hurt.  This time, I need to switch the bank register to bank 1,
   * so I can access the base address register
   */
  SMC_SELECT_BANK (1);
  base_address_register = inw (ioaddr + BASE);
  if (ioaddr != (base_address_register >> 3 & 0x3E0))
  {
    printk (CARDNAME ": IOADDR %x doesn't match configuration (%x)."
            "Probably not a SMC chip\n",
            ioaddr, base_address_register >> 3 & 0x3E0);
    /* well, the base address register didn't match.  Must not have
     * been a SMC chip after all.
     */
    return (0);
  }

  /* Check if the revision register is something that I recognize.
   * These might need to be added to later, as future revisions
   * could be added.
   */
  SMC_SELECT_BANK (3);
  revision_register = inw (ioaddr + REVISION);
  if (!chip_ids[(revision_register >> 4) & 0xF])
  {
    /* I don't recognize this chip, so...
     */
    printk (CARDNAME ": IO %x: Unrecognized revision register:"
            " %x, Contact author. \n", ioaddr, revision_register);
    return (0);
  }

  /* at this point I'll assume that the chip is an SMC9xxx.
   * It might be prudent to check a listing of MAC addresses
   * against the hardware address, or do some other tests.
   */
  return (1);
}

/*
 * Here I do typical initialization tasks.
 *
 * o  Initialize the structure if needed
 * o  print out my vanity message if not done so already
 * o  print out what type of hardware is detected
 * o  print out the ethernet address
 * o  find the IRQ
 * o  set up my private data
 * o  configure the dev structure with my subroutines
 * o  actually GRAB the irq.
 * o  GRAB the region
 *
 */
static int smc_initcard (struct device *dev, int ioaddr)
{
  static int version_printed = 0;
  int    i;
  WORD   revision_register;
  WORD   configuration_register;
  WORD   memory_info_register;
  WORD   memory_cfg_register;
  char  *version_string;
  char  *if_string;
  int    memory;

  /* see if I need to initialize the ethernet card structure
   */
  if (dev == NULL)
  {
    dev = init_etherdev (0, 0);
    if (!dev)
      return (0);
  }

  if (version_printed++ == 0)
     printk ("%s", version);

  dev->base_addr = ioaddr;

  /* Get the MAC address ( bank 1, regs 4 - 9 )
   */
  SMC_SELECT_BANK (1);
  for (i = 0; i < 6; i += 2)
  {
    WORD address = inw (ioaddr + ADDR0 + i);
    dev->dev_addr[i + 1] = address >> 8;
    dev->dev_addr[i] = address & 0xFF;
  }

  /* get the memory information
   */
  SMC_SELECT_BANK (0);
  memory_info_register = inw (ioaddr + MIR);
  memory_cfg_register  = inw (ioaddr + MCR);
  memory  = (memory_cfg_register >> 9) & 0x7;    /* multiplier */
  memory *= (memory_info_register & 0xFF) << 8;

  /* Now, I want to find out more about the chip.  This is sort of
   * redundant, but it's cleaner to have it in both, rather than having
   * one VERY long probe procedure.
   */
  SMC_SELECT_BANK (3);
  revision_register = inw (ioaddr + REVISION);
  version_string    = chip_ids [(revision_register >> 4) & 0xF];
  if (!version_string)
  {
    /* I shouldn't get here because this call was done before....
     */
    return (0);
  }

  /* is it using AUI or 10BaseT ?
   */
  if (dev->if_port == 0)
  {
    SMC_SELECT_BANK (1);
    configuration_register = inw (ioaddr + CONFIG);
    if (configuration_register & CFG_AUI_SELECT)
         dev->if_port = 2;
    else dev->if_port = 1;
  }
  if_string = interfaces[dev->if_port - 1];

  /* now, reset the chip, and put it into a known state
   */
  smc_reset (ioaddr);

  /*
   * If dev->irq is 0, then the device has to be banged on to see
   * what the IRQ is.
   *
   * This banging doesn't always detect the IRQ, for unknown reasons.
   * a workaround is to reset the chip and try again.
   *
   * Interestingly, the DOS packet driver *SETS* the IRQ on the card to
   * be what is requested on the command line.   I don't do that, mostly
   * because the card that I have uses a non-standard method of accessing
   * the IRQs, and because this _should_ work in most configurations.
   *
   * Specifying an IRQ is done with the assumption that the user knows
   * what (s)he is doing.  No checking is done!!!!
   *
   */
#ifndef NO_AUTOPROBE
  if (dev->irq < 2)
  {
    int trials = 3;
    while (trials--)
    {
      dev->irq = smc_findirq (ioaddr);
      if (dev->irq)
         break;
      /* kick the card and try again
       */
      smc_reset (ioaddr);
    }
  }
#endif
  if (dev->irq == 0)
  {
    printk (CARDNAME ": Couldn't autodetect your IRQ. Use irq=xx.\n");
    return (0);
  }
  if (dev->irq == 2)
  {
    /* Fixup for users that don't know that IRQ 2 is really IRQ 9,
     * or don't know which one to set.
     */
    dev->irq = 9;
  }

  /* now, print out the card info, in a short format..
   */
  printk (CARDNAME ": %s(r:%d) at %#3x IRQ:%d INTF:%s MEM:%db ",
          version_string, revision_register & 0xF, ioaddr, dev->irq,
          if_string, memory);
  /* Print the Ethernet address
   */
  printk ("ADDR: ");
  for (i = 0; i < 5; i++)
     printk ("%2.2x:", dev->dev_addr[i]);
  printk ("%2.2x \n", dev->dev_addr[5]);


  /* Initialize the private structure.
   */
  if (dev->priv == NULL)
  {
    dev->priv = k_calloc (sizeof (struct smc_local), 1);
    if (dev->priv == NULL)
      return (0);
  }

  /* Fill in the fields of the device structure with ethernet values.
   */
  ether_setup (dev);

  /* Grab the IRQ
   */
  if (!request_irq (dev->irq, smc_interrupt))
  {
    printk (CARDNAME ": unable to get IRQ %d.\n", dev->irq);
    return (0);
  }
  irq2dev_map[dev->irq] = dev;

  dev->open  = smc_open;
  dev->close = smc_close;
  dev->xmit  = smc_send_packet;
  dev->get_stats = smc_query_statistics;
#ifdef HAVE_MULTICAST
  dev->set_multicast_list = smc_set_multicast_list;
#endif
  return (1);
}

/*
 * Open and Initialize the board
 *
 * Set up everything, reset the card, etc ..
 *
 */
static int smc_open (struct device *dev)
{
  int i, ioaddr = dev->base_addr;

  memset (dev->priv, 0, sizeof (struct smc_local));
  dev->tx_busy = 0;
  dev->reentry = 0;
  dev->start = 1;

  /* reset the hardware
   */
  smc_reset (ioaddr);
  smc_enable (ioaddr);

  /* Select which interface to use
   */
  SMC_SELECT_BANK (1);
  if (dev->if_port == 1)
     outw (inw (ioaddr + CONFIG) & ~CFG_AUI_SELECT, ioaddr + CONFIG);
  else if (dev->if_port == 2)
     outw (inw (ioaddr + CONFIG) | CFG_AUI_SELECT, ioaddr + CONFIG);

  /* According to Becker, I have to set the hardware address
   * at this point, because the user can set it with an
   * ioctl.  Easily done...
   */
  SMC_SELECT_BANK (1);
  for (i = 0; i < 6; i += 2)
  {
    WORD address = dev->dev_addr[i + 1] << 8;
    address |= dev->dev_addr[i];
    outw (address, ioaddr + ADDR0 + i);
  }
  return (1);
}

/*
 * Called by the kernel to send a packet out into the void
 * of the net.  This routine is largely based on
 * skeleton.c, from Becker.
 */
static int smc_send_packet (struct device *dev, void *buf, int len)
{
  if (dev->tx_busy)
  {
    /* If we get here, some higher level has decided we are broken.
     * There should really be a "kick me" function call instead.
     */
    int tickssofar = jiffies - dev->tx_start;

    if (tickssofar < 5)
       return (0);

    printk (CARDNAME ": transmit timed out, %s?\n", tx_done (dev) ?
            "IRQ conflict" : "network cable problem");
    /* "kick" the adapter
     */
    smc_reset (dev->base_addr);
    smc_enable (dev->base_addr);
    dev->tx_busy  = 0;
    dev->tx_start = jiffies;
  }

  if (set_bit (0, (void *) &dev->tx_busy))
     printk (CARDNAME ": Transmitter access conflict.\n");
  else
  {
    /* Well, I want to send the packet.. but I don't know
     * if I can send it right now...
     */
    return smc_wait_to_send_packet (dev, buf, len);
  }
  return (0);
}

/*
 * This is the main routine of the driver, to handle the device when
 * it needs some attention.
 *
 * So:
 *   first, save state of the chipset
 *   branch off into routines to handle each case, and acknowledge
 *      each to the interrupt register
 *   and finally restore state.
 */
static void smc_interrupt (int irq)
{
  struct device    *dev = irq2dev_map[irq];
  struct smc_local *lp  = (struct smc_local*) dev->priv;
  BYTE   status, mask;
  WORD   card_stats;
  WORD   saved_bank;
  WORD   saved_pointer;
  int    timeout, ioaddr;

  PRINTK (3, (CARDNAME ": SMC interrupt started \n"));

  if (!dev)
  {
    printk (CARDNAME ": irq %d for unknown device.\n", irq);
    return;
  }

  if (dev->reentry)
  {
    printk (CARDNAME ": interrupt inside interrupt.\n");
    return;
  }
  ioaddr = dev->base_addr;
  dev->reentry = 1;

  saved_bank = inw (ioaddr + BANK_SELECT);

  SMC_SELECT_BANK (2);
  saved_pointer = inw (ioaddr + POINTER);

  mask = inb (ioaddr + INT_MASK);
  /* clear all interrupts
   */
  outb (0, ioaddr + INT_MASK);

  /* set a timeout value, so I don't stay here forever
   */
  timeout = 4;

  PRINTK (2, (CARDNAME ": MASK IS %x \n", mask));
  do
  {
    /* read the status flag, and mask it
     */
    status = inb (ioaddr + INTERRUPT) & mask;
    if (!status)
       break;

    PRINTK (3, (CARDNAME ": Handling interrupt status %x \n", status));

    if (status & IM_RCV_INT)
    {
      /* Got a packet(s).
       */
      PRINTK (2, (CARDNAME ": Receive Interrupt\n"));
      smc_rcv (dev);
    }
    else if (status & IM_TX_INT)
    {
      PRINTK (2, (CARDNAME ": TX ERROR handled\n"));
      smc_tx (dev);
      outb (IM_TX_INT, ioaddr + INTERRUPT);
    }
    else if (status & IM_TX_EMPTY_INT)
    {
      /* update stats
       */
      SMC_SELECT_BANK (0);
      card_stats = inw (ioaddr + COUNTER);

      /* single collisions
       */
      lp->stats.tx_collisions += card_stats & 0xF;
      card_stats >>= 4;

      /* multiple collisions
       */
      lp->stats.tx_collisions += card_stats & 0xF;

      SMC_SELECT_BANK (2);
      PRINTK (2, (CARDNAME ": TX_BUFFER_EMPTY handled\n"));
      outb (IM_TX_EMPTY_INT, ioaddr + INTERRUPT);
      mask &= ~IM_TX_EMPTY_INT;
      lp->stats.tx_packets += lp->packets_waiting;
      lp->packets_waiting = 0;
    }
    else if (status & IM_ALLOC_INT)
    {
      PRINTK (2, (CARDNAME ": Allocation interrupt \n"));

      /* clear this interrupt so it doesn't happen again
       */
      mask &= ~IM_ALLOC_INT;
      smc_hardware_send_packet (dev, NULL, 0);

      /* enable xmit interrupts based on this
       */
      mask |= (IM_TX_EMPTY_INT | IM_TX_INT);
    }
    else if (status & IM_RX_OVRN_INT)
    {
      lp->stats.rx_errors++;
      lp->stats.rx_fifo_errors++;
      outb (IM_RX_OVRN_INT, ioaddr + INTERRUPT);
    }
    else if (status & IM_EPH_INT)
    {
      PRINTK (1, (CARDNAME ": UNSUPPORTED: EPH INTERRUPT \n"));
    }
    else if (status & IM_ERCV_INT)
    {
      PRINTK (1, (CARDNAME ": UNSUPPORTED: ERCV INTERRUPT \n"));
      outb (IM_ERCV_INT, ioaddr + INTERRUPT);
    }
  }
  while (timeout--);

  /* restore state register
   */
  SMC_SELECT_BANK (2);
  outb (mask, ioaddr + INT_MASK);

  PRINTK (3, (CARDNAME ": MASK is now %x \n", mask));
  outw (saved_pointer, ioaddr + POINTER);

  SMC_SELECT_BANK (saved_bank);

  dev->reentry = 0;
  PRINTK (3, (CARDNAME ": Interrupt done\n"));
}

/*
 * smc_rcv -  receive a packet from the card
 *
 * There is (at least) a packet waiting to be read from
 * chip-memory.
 *
 * o Read the status
 * o If an error, record it
 * o otherwise, read in the packet
 */
static void smc_rcv (struct device *dev)
{
  struct smc_local *lp = (struct smc_local*) dev->priv;
  int    ioaddr        = dev->base_addr;
  int    packet_number;
  WORD   status;
  WORD   len;

  /* assume bank 2
   */
  packet_number = inw (ioaddr + FIFO_PORTS);

  if (packet_number & FP_RXEMPTY)
  {
    /* we got called , but nothing was on the FIFO
     */
    PRINTK (1, (CARDNAME ": WARNING: smc_rcv with nothing on FIFO. \n"));
    /* don't need to restore anything
     */
    return;
  }

  /* start reading from the start of the packet
   */
  outw (PTR_READ | PTR_RCV | PTR_AUTOINC, ioaddr + POINTER);

  /* First two words are status and packet_length
   */
  status = inw (ioaddr + DATA_1);
  len    = inw (ioaddr + DATA_1) & 0x07ff;  /* mask off top bits */

  PRINTK (2, ("RCV: STATUS %4x LENGTH %4x\n", status, len));

  /* The packet length contains 3 extra words :
   * status, length, and an extra word with an odd byte.
   */
  len -= 6;

  if (!(status & RS_ERRORS))
  {
    BYTE *buf;

    /* read one extra byte
     */
    if (status & RS_ODDFRAME)
       len++;

    /* set multicast stats
     */
    if (status & RS_MULTICAST)
       lp->stats.multicast++;

    if (!dev->get_rx_buf)
       goto get_out;

    buf = (*dev->get_rx_buf) (len);
    if (!buf)
    {
      lp->stats.rx_dropped++;
      goto get_out;
    }

#ifdef USE_32_BIT
    /* QUESTION:  Like in the TX routine, do I want
     * to send the dwords or the bytes first, or some
     * mixture.  A mixture might improve already slow PIO
     * performance
     */
    PRINTK (3, (" Reading %d dwords (and %d bytes) \n", len >> 2, len & 3));
    rep_insl (ioaddr + DATA_1, (DWORD*)buf, len >> 2);

    /* read the left over bytes
     */
    rep_insb (ioaddr + DATA_1, buf + (len & 0xFFFFFC), len & 3);

#else
    /* Use slower (?) 16-bit IO copy
     */
    PRINTK (3, (" Reading %d words and %d byte(s) \n", len >> 1, len & 1));
    if (len & 1)
       *buf++ = inb (ioaddr + DATA_1);
    rep_insw (ioaddr + DATA_1, (WORD*)buf, (len+1) >> 1);
    if (len & 1)
    {
      buf += len & ~1;
      *buf++ = inb (ioaddr + DATA_1);
    }
#endif

    lp->stats.rx_packets++;
  }
  else
  {
    /* error ...
     */
    lp->stats.rx_errors++;
    if (status & RS_ALGNERR)
       lp->stats.rx_frame_errors++;
    if (status & (RS_TOOSHORT | RS_TOOLONG))
       lp->stats.rx_length_errors++;
    if (status & RS_BADCRC)
       lp->stats.rx_crc_errors++;
  }

get_out:
  /* error or good, tell the card to get rid of this packet
   */
  outw (MC_RELEASE, ioaddr + MMU_CMD);
}

/*
 * smc_tx
 *
 * Purpose:  Handle a transmit error message.   This will only be called
 *   when an error, because of the AUTO_RELEASE mode.
 *
 * Algorithm:
 *  Save pointer and packet no
 *  Get the packet no from the top of the queue
 *  check if it's valid ( if not, is this an error??? )
 *  read the status word
 *  record the error
 *  ( resend?  Not really, since we don't want old packets around )
 *  Restore saved values
 */
static void smc_tx (struct device *dev)
{
  struct smc_local *lp = (struct smc_local *) dev->priv;
  int    ioaddr = dev->base_addr;
  BYTE   saved_packet;
  BYTE   packet_no;
  WORD   tx_status;

  /* assume bank 2
   */
  saved_packet = inb (ioaddr + PNR_ARR);
  packet_no = inw (ioaddr + FIFO_PORTS);
  packet_no &= 0x7F;

  /* select this as the packet to read from
   */
  outb (packet_no, ioaddr + PNR_ARR);

  /* read the first word from this packet
   */
  outw (PTR_AUTOINC | PTR_READ, ioaddr + POINTER);

  tx_status = inw (ioaddr + DATA_1);
  PRINTK (3, (CARDNAME ": TX DONE STATUS: %4x \n", tx_status));

  lp->stats.tx_errors++;
  if (tx_status & TS_LOSTCAR)
     lp->stats.tx_carrier_errors++;
  if (tx_status & TS_LATCOL)
  {
    printk (CARDNAME ": Late collision occurred on last xmit.\n");
    lp->stats.tx_window_errors++;
  }

  if (tx_status & TS_SUCCESS)
     printk (CARDNAME ": Successful packet caused interrupt \n");

  /* re-enable transmit
   */
  SMC_SELECT_BANK (0);
  outw (inw (ioaddr + TCR) | TCR_ENABLE, ioaddr + TCR);

  /* kill the packet
   */
  SMC_SELECT_BANK (2);
  outw (MC_FREEPKT, ioaddr + MMU_CMD);

  /* one less packet waiting for me
   */
  lp->packets_waiting--;
  outb (saved_packet, ioaddr + PNR_ARR);
}

/*
 * smc_close()
 *
 * this makes the board clean up everything that it can
 * and not talk to the outside world.   Caused by
 * an 'ifconfig ethX down'
 */
static void smc_close (struct device *dev)
{
  dev->tx_busy = 1;
  dev->start = 0;
  smc_shutdown (dev->base_addr);
}

/*
 * Get the current statistics.
 * This may be called with the card open or closed.
 */
static void *smc_query_statistics (struct device *dev)
{
  struct smc_local *lp = (struct smc_local *) dev->priv;
  return (void*)&lp->stats;
}

#ifdef HAVE_MULTICAST
/*
 * Finds the CRC32 of a set of bytes.
 * Again, from Peter Cammaert's code.
 */
static int crc32 (char *s, int length)
{
  int perByte, perBit;

  /* crc polynomial for Ethernet
   */
  const DWORD poly = 0xEDB88320;

  /* crc value - preinitialized to all 1's
   */
  DWORD crc_value = 0xFFFFFFFF;

  for (perByte = 0; perByte < length; perByte++)
  {
    BYTE c = *s++;
    for (perBit = 0; perBit < 8; perBit++)
    {
      crc_value = (crc_value >> 1) ^ (((crc_value ^ c) & 1) ? poly : 0);
      c >>= 1;
    }
  }
  return (crc_value);
}

/*
 * Function: smc_setmulticast()
 * Purpose:
 *    This sets the internal hardware table to filter out unwanted multicast
 *    packets before they take up memory.
 *
 *    The SMC chip uses a hash table where the high 6 bits of the CRC of
 *    address are the offset into the table.  If that bit is 1, then the
 *    multicast packet is accepted.  Otherwise, it's dropped silently.
 *
 *    To use the 6 bits as an offset into the table, the high 3 bits are the
 *    number of the 8 bit register, while the low 3 bits are the bit within
 *    that register.
 *
 * This routine is based very heavily on the one provided by Peter Cammaert.
 */
static void smc_setmulticast (int ioaddr, int count, struct dev_mc_list *addrs)
{
  struct dev_mc_list *cur_addr;
  BYTE   multicast_table[8];
  int    i;

  /* table for flipping the order of 3 bits
   */
  BYTE invert3[] = { 0, 4, 2, 6, 1, 5, 3, 7 };

  /* start with a table of all zeros: reject all
   */
  memset (multicast_table, 0, sizeof(multicast_table));

  cur_addr = addrs;
  for (i = 0; i < count; i++, cur_addr = cur_addr->next)
  {
    int position;  

    if (!cur_addr)     /* do we have a pointer here? */
       break;

    /* make sure this is a multicast address - shouldn't this
     * be a given if we have it here ?
     */
    if (!(*cur_addr->dmi_addr & 1))
       continue;

    /* only use the low order bits
     */
    position = crc32 (cur_addr->dmi_addr, 6) & 0x3f;

    /* do some messy swapping to put the bit in the right spot
     */
    multicast_table [invert3[position & 7]] |=
      (1 << invert3[(position >> 3) & 7]);

  }
  /* now, the table can be loaded into the chipset
   */
  SMC_SELECT_BANK (3);

  for (i = 0; i < 8; i++)
     outb (multicast_table[i], ioaddr + MULTICAST1 + i);
}

/*
 * smc_set_multicast_list()
 *
 * This routine will, depending on the values passed to it,
 * either make it accept multicast packets, go into
 * promiscuous mode ( for TCPDUMP and cousins ) or accept
 * a select set of multicast packets
 */
static void smc_set_multicast_list (struct device *dev)
{
  short ioaddr = dev->base_addr;

  SMC_SELECT_BANK (0);
  if (dev->flags & IFF_PROMISC)
     outw (inw (ioaddr + RCR) | RCR_PROMISC, ioaddr + RCR);

  else if (dev->flags & IFF_ALLMULTI)
  {
    outw (inw (ioaddr + RCR) | RCR_ALMUL, ioaddr + RCR);

    /* We just get all multicast packets even if we only want them
     * from one source.  This will be changed at some future point.
     */
  }
  else if (dev->mc_count)
  {
    /* Support hardware multicasting
     *
     * Be sure I get rid of flags I might have set
     */
    outw (inw (ioaddr + RCR) & ~(RCR_PROMISC | RCR_ALMUL), ioaddr + RCR);

    /* NOTE: this has to set the bank, so make sure it is the
     * last thing called.  The bank is set to zero at the top
     */
    smc_setmulticast (ioaddr, dev->mc_count, dev->mc_list);
  }
  else
  {
    outw (inw (ioaddr + RCR) & ~(RCR_PROMISC | RCR_ALMUL), ioaddr + RCR);

    /* Since I'm disabling all multicast entirely, I need to
     * clear the multicast list
     */
    SMC_SELECT_BANK (3);
    outw (0, ioaddr + MULTICAST1);
    outw (0, ioaddr + MULTICAST2);
    outw (0, ioaddr + MULTICAST3);
    outw (0, ioaddr + MULTICAST4);
  }
}
#endif
