/* atp.c: Attached (pocket) ethernet adapter driver for linux. */
/*
 * This is a driver for a commonly OEMed pocket (parallel port)
 * ethernet adapter.  
 * 
 * Written 1993,1994,1995 by Donald Becker.
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
 * The timer-based reset code was written by Bill Carlson, wwc@super.org.
 */

static const char *version = "atp.c:v1.01 1/18/95 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

/*
 * This file is a device driver for the RealTek (aka AT-Lan-Tec) pocket
 * ethernet adapter.  This is a common low-cost OEM pocket ethernet
 * adapter, sold under many names.
 * 
 * Sources:
 * This driver was written from the packet driver assembly code provided by
 * Vincent Bono of AT-Lan-Tec.   Ever try to figure out how a complicated
 * device works just from the assembly code?  It ain't pretty.  The following
 * description is written based on guesses and writing lots of special-purpose
 * code to test my theorized operation.
 * 
 * Theory of Operation
 * 
 * The RTL8002 adapter seems to be built around a custom spin of the SEEQ
 * controller core.  It probably has a 16K or 64K internal packet buffer, of
 * which the first 4K is devoted to transmit and the rest to receive.
 * The controller maintains the queue of received packet and the packet buffer
 * access pointer internally, with only 'reset to beginning' and 'skip to next
 * packet' commands visible.  The transmit packet queue holds two (or more?)
 * packets: both 'retransmit this packet' (due to collision) and 'transmit next
 * packet' commands must be started by hand.
 * 
 * The station address is stored in a standard bit-serial EEPROM which must be
 * read (ughh) by the device driver.  (Provisions have been made for
 * substituting a 74S288 PROM, but I haven't gotten reports of any models
 * using it.)  Unlike built-in devices, a pocket adapter can temporarily lose
 * power without indication to the device driver.  The major effect is that
 * the station address, receive filter (promiscuous, etc.) and transceiver
 * must be reset.
 * 
 * The controller itself has 16 registers, some of which use only the lower
 * bits.  The registers are read and written 4 bits at a time.  The four bit
 * register address is presented on the data lines along with a few additional
 * timing and control bits.  The data is then read from status port or written
 * to the data port.
 * 
 * Since the bulk data transfer of the actual packets through the slow
 * parallel port dominates the driver's running time, four distinct data
 * (non-register) transfer modes are provided by the adapter, two in each
 * direction.  In the first mode timing for the nibble transfers is
 * provided through the data port.  In the second mode the same timing is
 * provided through the control port.  In either case the data is read from
 * the status port and written to the data port, just as it is accessing
 * registers.
 * 
 * In addition to the basic data transfer methods, several more are modes are
 * created by adding some delay by doing multiple reads of the data to allow
 * it to stabilize.  This delay seems to be needed on most machines.
 * 
 * The data transfer mode is stored in the 'dev->if_port' field.  Its default
 * value is '4'.  It may be overridden at boot-time using the third parameter
 * to the "ether=..." initialization.
 * 
 * The header file <atp.h> provides inline functions that encapsulate the
 * register and data access methods.  These functions are hand-tuned to
 * generate reasonable object code.  This header file also documents my
 * interpretations of the device registers.
 */

#include "pmdrvr.h"
#include "atp.h"

/* use 0 for production, 1 for verification, >2 for debug
 */
#ifndef NET_DEBUG
#define NET_DEBUG 1
#endif
int atp_debug = NET_DEBUG;

/* The number of low I/O ports used by the ethercard.
 */
#define ETHERCARD_TOTAL_SIZE  3

/* This code, written by wwc@super.org, resets the adapter every
 * TIMED_CHECKER ticks.  This recovers from an unknown error which
 * hangs the device.
 */
#define TIMED_CHECKER (HZ/4)
#ifdef TIMED_CHECKER
  static void atp_timed_checker (DWORD ignored);
  static struct device *atp_timed_dev;
  static struct timer_list atp_timer = { NULL, 0, 0, atp_timed_checker };
#endif

static int   atp_probe1 (struct device *dev, short ioaddr);
static void  get_node_ID (struct device *dev);
static WORD  eeprom_op (short ioaddr, DWORD cmd);
static int   net_open (struct device *dev);
static void  hardware_init (struct device *dev);
static void  trigger_send (short ioaddr, int length);
static int   net_send_packet (struct device *dev, void *buf, int len);
static void  net_interrupt (int irq);
static void  net_rx (struct device *dev);
static void  read_block (short ioaddr, int length, BYTE *buffer, int data_mode);
static void  net_close (struct device *dev);
static void *net_get_stats (struct device *dev);
static void  set_multicast_list (struct device *dev);


/*
 * Check for a network adapter of this type, and return '0' iff one exists.
 * If dev->base_addr == 0, probe all likely locations.
 * If dev->base_addr == 1, always return failure.
 * If dev->base_addr == 2, allocate space for the device and return success
 * (detachable devices only).
 */
int atp_probe (struct device *dev)
{
  int *port, ports[] = { 0x378, 0x278, 0x3bc, 0 };
  int  base_addr = dev->base_addr;

  if (base_addr > 0x1ff)	/* Check a single specified location. */
     return atp_probe1 (dev, base_addr);

  if (base_addr == 1)           /* Don't probe at all. */
     return (0);

  for (port = ports; *port; port++)
  {
    int ioaddr = *port;

    outb (0x57, ioaddr + PAR_DATA);
    if (inb (ioaddr + PAR_DATA) != 0x57)
       continue;
    if (atp_probe1 (dev, ioaddr))
       return (1);
  }
  return (0);
}

static int atp_probe1 (struct device *dev, short ioaddr)
{
  struct net_local *lp;
  int    saved_ctrl_reg, status;

  outb (0xff, ioaddr + PAR_DATA);

  /* Save the original value of the Control register, in case we guessed
   * wrong.
   */
  saved_ctrl_reg = inb (ioaddr + PAR_CONTROL);

  /* IRQEN=0, SLCTB=high INITB=high, AUTOFDB=high, STBB=high.
   */
  outb (0x04, ioaddr + PAR_CONTROL);
  write_reg_high (ioaddr, CMR1, CMR1h_RESET);
  eeprom_delay (2048);
  status = read_nibble (ioaddr, CMR1);

  if ((status & 0x78) != 0x08)
  {
    /* The pocket adapter probe failed, restore the control register.
     */
    outb (saved_ctrl_reg, ioaddr + PAR_CONTROL);
    return (0);
  }
  status = read_nibble (ioaddr, CMR2_h);
  if ((status & 0x78) != 0x10)
  {
    outb (saved_ctrl_reg, ioaddr + PAR_CONTROL);
    return (0);
  }

  /* Find the IRQ used by triggering an interrupt.
   */

  /* No accept mode, IRQ out. */
  write_reg_byte (ioaddr, CMR2, 0x01);  

  /* Enable Tx and Rx. */
  write_reg_high (ioaddr, CMR1, CMR1h_RxENABLE|CMR1h_TxENABLE);       

  /* Omit autoIRQ routine for now. Use "table lookup" instead.  Uhgggh
   */
  if (ioaddr == 0x378)
       dev->irq = 7;
  else dev->irq = 5;
  write_reg_high (ioaddr, CMR1, CMR1h_TxRxOFF);	/* Disable Tx and Rx units. */
  write_reg (ioaddr, CMR2, CMR2_NULL);

  dev->base_addr = ioaddr;

  /* Read the station address PROM.
   */
  get_node_ID (dev);

  printk ("%s: Pocket adapter found at %#3lx, IRQ %d, SAPROM "
          "%02X:%02X:%02X:%02X:%02X:%02X.\n",
          dev->name, dev->base_addr, dev->irq,
          dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
	  dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

  /* Leave the hardware in a reset state.
   */
  write_reg_high (ioaddr, CMR1, CMR1h_RESET);

  if (atp_debug)
     printk (version);

  /* Initialize the device structure.
   */
  ether_setup (dev);
  dev->priv = k_calloc (sizeof (struct net_local), 1);
  if (!dev->priv)
     return (0);

  lp = (struct net_local *) dev->priv;

  lp->addr_mode = CMR2h_Normal;

  /* For the ATP adapter the "if_port" is really the data transfer mode.
   */
  dev->if_port = (dev->mem_start & 0xf) ? dev->mem_start & 0x7 : 4;
  if (dev->mem_end & 0xf)
     atp_debug = dev->mem_end & 7;

  dev->open      = net_open;
  dev->close     = net_close;
  dev->xmit      = net_send_packet;
  dev->get_stats = net_get_stats;
  dev->set_multicast_list = &set_multicast_list;

#ifdef TIMED_CHECKER
  del_timer (&atp_timer);
  atp_timer.expires = jiffies + TIMED_CHECKER;
  atp_timed_dev     = dev;
  add_timer (&atp_timer);
#endif
  return (1);
}

/*
 * Read the station address PROM, usually a word-wide EEPROM.
 */
static void get_node_ID (struct device *dev)
{
  short ioaddr = dev->base_addr;
  int   i, sa_offset = 0;

  /* Point to the EEPROM control registers.
   */
  write_reg (ioaddr, CMR2, CMR2_EEPROM);

  /* Some adapters have the station address at offset 15 instead of offset
   * zero.  Check for it, and fix it if needed.
   */
  if (eeprom_op (ioaddr, EE_READ (0)) == 0xffff)
     sa_offset = 15;

  for (i = 0; i < ETH_ALEN/2; i++)
     ((WORD*)dev->dev_addr)[i] = ntohs (eeprom_op(ioaddr, EE_READ(sa_offset+i)));

  write_reg (ioaddr, CMR2, CMR2_NULL);
}

/*
 * An EEPROM read command starts by shifting out 0x60+address, and then
 * shifting in the serial data. See the NatSemi databook for details.
 *           ________________
 *   CS : __|
 *               ____     ____
 *   CLK: ______|    |___|    |
 *       __ _______ _______
 *   DI :__X_______X_______X
 *   DO :_________X_______X
 */
static WORD eeprom_op (short ioaddr, DWORD cmd)
{
  unsigned eedata_out = 0;
  int      num_bits = EE_CMD_SIZE;

  while (--num_bits >= 0)
  {
    char outval = test_bit (num_bits, &cmd) ? EE_DATA_WRITE : 0;

    write_reg_high (ioaddr, PROM_CMD, outval | EE_CLK_LOW);
    eeprom_delay (5);
    write_reg_high (ioaddr, PROM_CMD, outval | EE_CLK_HIGH);
    eedata_out <<= 1;
    if (read_nibble (ioaddr, PROM_DATA) & EE_DATA_READ)
       eedata_out++;
    eeprom_delay (5);
  }
  write_reg_high (ioaddr, PROM_CMD, EE_CLK_LOW & ~EE_CS);
  return (eedata_out);
}


/*
 * Open/initialize the board.  This is called (in the current kernel)
 * sometime after booting when the 'ifconfig' program is run.
 * 
 * This routine sets everything up anew at each open, even
 * registers that "should" only need to be set once at boot, so that
 * there is non-reboot way to recover if something goes wrong.
 * 
 * This is an attachable device: if there is no dev->priv entry then it wasn't
 * probed for at boot-time, and we need to probe for it again.
 */
static int net_open (struct device *dev)
{
  /* The interrupt line is turned off (tri-stated) when the device isn't in
   * use.  That's especially important for "attached" interfaces where the
   * port or interrupt may be shared.
   */
  if (irq2dev_map[dev->irq] ||
      (irq2dev_map[dev->irq] = dev) == NULL ||
      !request_irq (dev->irq, &net_interrupt))
    return (0);

  hardware_init (dev);
  dev->start = 1;
  return (1);
}

/*
 * This routine resets the hardware.  We initialize everything, assuming that
 * the hardware may have been temporarily detached.
 */
static void hardware_init (struct device *dev)
{
  struct net_local *lp = (struct net_local*) dev->priv;
  int    i, ioaddr = dev->base_addr;

  write_reg_high (ioaddr, CMR1, CMR1h_RESET);

  for (i = 0; i < ETH_ALEN; i++)
     write_reg_byte (ioaddr, PAR0 + i, dev->dev_addr[i]);

  write_reg_high (ioaddr, CMR2, lp->addr_mode);

  if (atp_debug > 2)
     printk ("%s: Reset: current Rx mode %d.\n",
             dev->name, (read_nibble (ioaddr, CMR2_h) >> 3) & 0x0f);

  write_reg (ioaddr, CMR2, CMR2_IRQOUT);
  write_reg_high (ioaddr, CMR1, CMR1h_RxENABLE | CMR1h_TxENABLE);

  /* Enable the interrupt line from the serial port.
   */
  outb (Ctrl_SelData + Ctrl_IRQEN, ioaddr + PAR_CONTROL);

  /* Unmask the interesting interrupts.
   */
  write_reg (ioaddr, IMR, ISR_RxOK | ISR_TxErr | ISR_TxOK);
  write_reg_high (ioaddr, IMR, ISRh_RxErr);

  lp->tx_unit_busy      = 0;
  lp->pac_cnt_in_tx_buf = 0;
  lp->saved_tx_size     = 0;

  dev->tx_busy = 0;
  dev->reentry = 0;
}

static void trigger_send (short ioaddr, int length)
{
  write_reg_byte (ioaddr, TxCNT0, length & 0xff);
  write_reg (ioaddr, TxCNT1, length >> 8);
  write_reg (ioaddr, CMR1, CMR1_Xmit);
}

static void write_packet (short ioaddr, int length, BYTE *packet, int data_mode)
{
  length = (length + 1) & ~1;	/* Round up to word length. */
  outb (EOC + MAR, ioaddr + PAR_DATA);

  if ((data_mode & 1) == 0)
  {
    /* Write the packet out, starting with the write addr.
     */
    outb (WrAddr + MAR, ioaddr + PAR_DATA);
    do
      write_byte_mode0 (ioaddr, *packet++);
    while (--length > 0);
  }
  else
  {
    /* Write the packet out in slow mode.
     */
    BYTE outbyte = *packet++;

    outb (Ctrl_LNibWrite + Ctrl_IRQEN, ioaddr + PAR_CONTROL);
    outb (WrAddr + MAR, ioaddr + PAR_DATA);

    outb ((outbyte & 0x0f) | 0x40, ioaddr + PAR_DATA);
    outb (outbyte & 0x0f, ioaddr + PAR_DATA);
    outbyte >>= 4;
    outb (outbyte & 0x0f, ioaddr + PAR_DATA);
    outb (Ctrl_HNibWrite + Ctrl_IRQEN, ioaddr + PAR_CONTROL);
    while (--length > 0)
      write_byte_mode1 (ioaddr, *packet++);
  }
  /* Terminate the Tx frame.  End of write: ECB.
   */
  outb (0xff, ioaddr + PAR_DATA);
  outb (Ctrl_HNibWrite | Ctrl_SelData | Ctrl_IRQEN, ioaddr + PAR_CONTROL);
}

static int net_send_packet (struct device *dev, void *buf, int len)
{
  struct net_local *lp = (struct net_local *) dev->priv;
  int    ioaddr = dev->base_addr;

  if (dev->tx_busy)
  {
    /* If we get here, some higher level has decided we are broken.
     * There should really be a "kick me" function call instead.
     */
    int tickssofar = jiffies - dev->tx_start;

    if (tickssofar < 5)
       return (1);

    printk ("%s: transmit timed out, %s?\n", dev->name,
            inb (ioaddr + PAR_CONTROL) & 0x10 ? "network cable problem" :
                                                "IRQ conflict");
    lp->stats.tx_errors++;
    /* Try to restart the adapter.
     */
    hardware_init (dev);
    dev->tx_busy = 0;
    dev->tx_start = jiffies;
  }

  if (dev->tx_busy)
     printk ("%s: Transmitter access conflict.\n", dev->name);
  else
  {
    short length = len < ETH_MIN ? ETH_MIN : len;

    /* Disable interrupts by writing 0x00 to the Interrupt Mask Register.
     * This sequence must not be interrupted by an incoming packet.
     */
    DISABLE ();
    write_reg (ioaddr, IMR, 0);
    write_reg_high (ioaddr, IMR, 0);
    ENABLE ();

    write_packet (ioaddr, length, (BYTE *) buf, dev->if_port);

    lp->pac_cnt_in_tx_buf++;
    if (lp->tx_unit_busy == 0)
    {
      trigger_send (ioaddr, length);
      lp->saved_tx_size = 0;	/* Redundant */
      lp->re_tx = 0;
      lp->tx_unit_busy = 1;
    }
    else
      lp->saved_tx_size = length;

    dev->tx_start = jiffies;

    /* Re-enable the LPT interrupts.
     */
    write_reg (ioaddr, IMR, ISR_RxOK | ISR_TxErr | ISR_TxOK);
    write_reg_high (ioaddr, IMR, ISRh_RxErr);
  }
  return (1);
}

/*
 * The typical workhorse of the driver:
 * Handle the network interface interrupts.
 */
static void net_interrupt (int irq)
{
  static int num_tx_since_rx = 0;
  struct device    *dev = irq2dev_map[irq];
  struct net_local *lp;
  int    i, ioaddr, status, boguscount = 20;

  if (!dev)
  {
    printk ("ATP_interrupt(): irq %d for unknown device.\n", irq);
    return;
  }
  dev->reentry = 1;

  ioaddr = dev->base_addr;
  lp = (struct net_local*) dev->priv;

  /* Disable additional spurious interrupts.
   */
  outb (Ctrl_SelData, ioaddr + PAR_CONTROL);

  /* The adapter's output is currently the IRQ line, switch it to data.
   */
  write_reg (ioaddr, CMR2, CMR2_NULL);
  write_reg (ioaddr, IMR, 0);

  if (atp_debug > 5)
     printk ("%s: In interrupt ", dev->name);

  while (--boguscount > 0)
  {
    status = read_nibble (ioaddr, ISR);
    if (atp_debug > 5)
       printk ("loop status %02x..", status);

    if (status & (ISR_RxOK << 3))
    {
      write_reg (ioaddr, ISR, ISR_RxOK);	/* Clear the Rx interrupt. */
      do
      {
	int read_status = read_nibble (ioaddr, CMR1);

        if (atp_debug > 6)
           printk ("handling Rx packet %02x..", read_status);

        /* We acknowledged the normal Rx interrupt, so if the interrupt
         * is still outstanding we must have a Rx error.
         */
        if (read_status & (CMR1_IRQ << 3))   /* Overrun. */
        {  
	  lp->stats.rx_over_errors++;

          /* Set to no-accept mode long enough to remove a packet.
           */
	  write_reg_high (ioaddr, CMR2, CMR2h_OFF);
	  net_rx (dev);

          /* Clear the interrupt and return to normal Rx mode.
           */
	  write_reg_high (ioaddr, ISR, ISRh_RxErr);
	  write_reg_high (ioaddr, CMR2, lp->addr_mode);
	}
        else if ((read_status & (CMR1_BufEnb << 3)) == 0) /* Normal Rx */
	{
	  net_rx (dev);
	  dev->last_rx = jiffies;
	  num_tx_since_rx = 0;
	}
	else
	  break;
      }
      while (--boguscount > 0);
    }
    else if (status & ((ISR_TxErr + ISR_TxOK) << 3))
    {
      if (atp_debug > 6)
         printk ("handling Tx done..");

      /* Clear the Tx interrupt.  We should check for too many failures
       * and reinitialize the adapter.
       */
      write_reg (ioaddr, ISR, ISR_TxErr + ISR_TxOK);
      if (status & (ISR_TxErr << 3))
      {
        lp->stats.tx_collisions++;
	if (++lp->re_tx > 15)
	{
	  lp->stats.tx_aborted_errors++;
	  hardware_init (dev);
	  break;
	}
        /* Attempt to retransmit.
         */
        if (atp_debug > 6)
           printk ("attempting to ReTx");
	write_reg (ioaddr, CMR1, CMR1_ReXmit + CMR1_Xmit);
      }
      else
      {
        /* Finish up the transmit.
         */
	lp->stats.tx_packets++;
	lp->pac_cnt_in_tx_buf--;
	if (lp->saved_tx_size)
	{
	  trigger_send (ioaddr, lp->saved_tx_size);
	  lp->saved_tx_size = 0;
	  lp->re_tx = 0;
	}
	else
	  lp->tx_unit_busy = 0;
	dev->tx_busy = 0;
      }
      num_tx_since_rx++;
    }
    else if (num_tx_since_rx > 8 && jiffies > dev->last_rx + 100)
    {
      if (atp_debug > 2)
         printk ("%s: Missed packet? No Rx after %d Tx and %u jiffies"
                 " status %02x  CMR1 %02x.\n",
                 dev->name, num_tx_since_rx,
                 (unsigned)(jiffies - dev->last_rx), status,
                 (read_nibble (ioaddr, CMR1) >> 3) & 15);
      lp->stats.rx_missed_errors++;
      hardware_init (dev);
      num_tx_since_rx = 0;
      break;
    }
    else
      break;
  }

  /* This following code fixes a rare (and very difficult to track down)
   * problem where the adapter forgets its ethernet address.
   */
  for (i = 0; i < ETH_ALEN; i++)
      write_reg_byte (ioaddr, PAR0 + i, dev->dev_addr[i]);

#ifdef TIMED_CHECKER
  del_timer (&atp_timer);
  atp_timer.expires = jiffies + TIMED_CHECKER;
  add_timer (&atp_timer);
#endif

  /* Tell the adapter that it can go back to using the output line as IRQ.
   */
  write_reg (ioaddr, CMR2, CMR2_IRQOUT);

  /* Enable the physical interrupt line, which is sure to be low until..
   */
  outb (Ctrl_SelData + Ctrl_IRQEN, ioaddr + PAR_CONTROL);

  /* .. we enable the interrupt sources.
   */
  write_reg (ioaddr, IMR, ISR_RxOK | ISR_TxErr | ISR_TxOK);
  write_reg_high (ioaddr, IMR, ISRh_RxErr);	/* Hmmm, really needed? */

  if (atp_debug > 5)
     printk ("exiting interrupt.\n");
  dev->reentry = 0;
}

#ifdef TIMED_CHECKER
/*
 * This following code fixes a rare (and very difficult to track down)
 * problem where the adapter forgets its ethernet address.
 */
static void atp_timed_checker (DWORD ignored)
{
  int i, ioaddr = atp_timed_dev->base_addr;

  if (!atp_timed_dev->reentry)
  {
    for (i = 0; i < ETH_ALEN; i++)
       write_reg_byte (ioaddr, PAR0 + i, atp_timed_dev->dev_addr[i]);
  }
  del_timer (&atp_timer);
  atp_timer.expires = jiffies + TIMED_CHECKER;
  add_timer (&atp_timer);
}
#endif

/*
 * We have a good packet(s), get it/them out of the buffers.
 */
static void net_rx (struct device *dev)
{
  struct net_local *lp = (struct net_local *) dev->priv;
  struct rx_header  rx_head;
  int    ioaddr = dev->base_addr;

  /* Process the received packet.
   */
  outb (EOC + MAR, ioaddr + PAR_DATA);
  read_block (ioaddr, 8, (BYTE*)&rx_head, dev->if_port);

  if (atp_debug > 5)
     printk (" rx_count %04x %04x %04x %04x..",
             rx_head.pad, rx_head.rx_count, rx_head.rx_status,
             rx_head.cur_addr);

  if ((rx_head.rx_status & 0x77) != 0x01)
  {
    lp->stats.rx_errors++;
    /* Ackkk!  I don't have any documentation on what the error bits mean!
     * The best I can do is slap the device around a bit.
     */
    if (atp_debug > 3)
       printk ("%s: Unknown ATP Rx error %04x.\n",
               dev->name, rx_head.rx_status);
    hardware_init (dev);
    return;
  }
  else
  {
    if (dev->get_rx_buf)
    {
      int len = (rx_head.rx_count & 0x7ff)-4; /* The "-4" is omits the FCS (CRC). */

      char *buf = (*dev->get_rx_buf) (len);
      if (buf)
      {
        read_block (ioaddr, len, buf, dev->if_port);
        if (atp_debug > 6)
           printk (" head %02x:%02x:%02x:%02x:%02x:%02x "
                         "%02x:%02x:%02x:%02x:%02x:%02x",
                   buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
                   buf[6], buf[7], buf[8], buf[9], buf[10],buf[11]);
      }
      else
        lp->stats.rx_dropped++;
    }
    lp->stats.rx_packets++;
  }
  write_reg (ioaddr, CMR1, CMR1_NextPkt);
}

static void read_block (short ioaddr, int length, BYTE *p, int data_mode)
{
  if (data_mode <= 3)
  {				/* Mode 0 or 1 */
    outb (Ctrl_LNibRead, ioaddr + PAR_CONTROL);
    outb (length == 8 ? RdAddr | HNib | MAR : RdAddr | MAR, ioaddr + PAR_DATA);
    if (data_mode <= 1)
    {				/* Mode 0 or 1 */
      do
	*p++ = read_byte_mode0 (ioaddr);
      while (--length > 0);
    }
    else			/* Mode 2 or 3 */
    {
      do
	*p++ = read_byte_mode2 (ioaddr);
      while (--length > 0);
    }
  }
  else if (data_mode <= 5)
  {
    do
      *p++ = read_byte_mode4 (ioaddr);
    while (--length > 0);
  }
  else
  {
    do
      *p++ = read_byte_mode6 (ioaddr);
    while (--length > 0);
  }
  outb (EOC + HNib + MAR, ioaddr + PAR_DATA);
  outb (Ctrl_SelData, ioaddr + PAR_CONTROL);
}

/*
 * The inverse routine to net_open().
 */
static void net_close (struct device *dev)
{
  struct net_local *lp = (struct net_local *) dev->priv;
  int    ioaddr = dev->base_addr;

  dev->tx_busy = 1;
  dev->start   = 0;

  /* Flush the Tx and disable Rx here.
   */
  lp->addr_mode = CMR2h_OFF;
  write_reg_high (ioaddr, CMR2, CMR2h_OFF);

  /* Free the IRQ line.
   */
  outb (0x00, ioaddr + PAR_CONTROL);
  free_irq (dev->irq);
  irq2dev_map[dev->irq] = NULL;

  /* Leave the hardware in a reset state.
   */
  write_reg_high (ioaddr, CMR1, CMR1h_RESET);
}

/*
 * Get the current statistics.  This may be called with the card open or
 * closed.
 */
static void *net_get_stats (struct device *dev)
{
  struct net_local *lp = (struct net_local*) dev->priv;
  return (void*) &lp->stats;
}

/*
 * Set or clear the multicast filter for this adapter.
 */
static void set_multicast_list (struct device *dev)
{
  struct net_local *lp = (struct net_local *) dev->priv;
  short  ioaddr    = dev->base_addr;
  int    num_addrs = dev->mc_count;

  if (dev->flags & (IFF_ALLMULTI | IFF_PROMISC))
     num_addrs = 1;

  /* We must make the kernel realise we had to move
   * into promisc mode or we start all out war on
   * the cable. - AC
   */
  if (num_addrs)
     dev->flags |= IFF_PROMISC;
  lp->addr_mode = num_addrs ? CMR2h_PROMISC : CMR2h_Normal;
  write_reg_high (ioaddr, CMR2, lp->addr_mode);
}
