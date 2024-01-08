/* 
 * ethpknic.c: A general NS8390 ethernet driver core for linux.
 *
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
 * This is the chip-specific code for many 8390-based ethernet adaptors.
 * This is not a complete driver, it must be combined with board-specific
 * code such as ne.c, wd.c, 3c503.c, etc.
 */

#ifdef version
static char *version = "8390.c:v1.10 9/23/94 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";
#endif

#include "pmdrvr.h"
#include "ethpknic.h"
#include "accton.h"

/* 
 * Braindamage remaining:
 *  Much of this code should have been cleaned up, but every attempt
 *  has broken some clone part.
 *
 * Sources:
 *  The National Semiconductor LAN Databook, and the 3Com 3c503 databook.
 *
 *
 * These are the operational function interfaces to board-specific
 * routines.
 *
 * void reset_8390()
 *   Resets the board associated with DEV, including a hardware reset of
 *   the 8390.  This is only called when there is a transmit timeout, and
 *   it is always followed by 8390_init().
 *
 * void block_output()
 *   Write the COUNT bytes of BUF to the packet buffer at START_PAGE.  The
 *   "page" value uses the 8390's 256-byte pages.
 *
 * int block_input()
 *   Read COUNT bytes from the packet buffer into BUF. Start reading from
 *   RING_OFFSET, the address as the 8390 sees it. The first read will
 *   always be the 4 byte, page aligned 8390 header.  *If* there is a
 *   subsequent read, it will be of the rest of the packet.
 */
#define ei_reset_8390   (ei_local->reset_8390)
#define ei_block_output (ei_local->block_output)
#define ei_block_input  (ei_local->block_input)

/* use 0 for production, 1 for verification, >2 for debug
 */
#ifndef EI_DEBUG
#define EI_DEBUG 0
#endif

int ei_debug = EI_DEBUG;

/* Max number of packets received at one Intr.
 * Current this may only be examined by a kernel debugger
 */
static int high_water_mark = 0;

static void ei_tx_intr    (struct device *dev);
static void ei_receive    (struct device *dev);
static void ei_rx_overrun (struct device *dev);

/* Routines generic to NS8390-based boards
 */
static void NS8390_trigger_send (struct device *dev, unsigned int length,
				 int start_page);

/*
 * Open/initialize the board.  This routine goes all-out, setting everything
 * up anew at each open, even though many of these registers should only
 * need to be set once at boot.
 */
int ei_open (struct device *dev)
{
  struct ei_device *ei_local = (struct ei_device *) dev->priv;

  if (ei_debug == 3)
     printk ("ei_open called..");
  if (!ei_local)
  {
    printk ("%s: Opening a non-existent physical device\n", dev->name);
    return (0);
  }

  irq2dev_map[dev->irq] = dev;
  NS8390_init (dev, 1);
  dev->start = 1;
  ei_local->irqlock = 0;
  if (ei_debug == 3)
     printk ("ei_open done\n");
  return (1);
}

int ei_start_xmit (struct device *dev, const void *buf, int len)
{
  struct ei_device *ei_local = (struct ei_device *) dev->priv;
  int    send_length;

  if (ei_debug == 3)
     printk ("ei_start_xmit..");

  /* We normally shouldn't be called if dev->tx_busy is set, but the
   * existing code does anyway.
   * If it has been too long (> 100 or 150ms.) since the last Tx we assume
   * the board has died and kick it.
   */                                           
  if (dev->tx_busy)
  {                  /* Do timeouts, just like the 8003 driver. */
    int txsr = Read_Register_Value (EN0_TSR), isr;
    int tickssofar = jiffies - dev->tx_start;

    if (tickssofar < 100 || (tickssofar < 150 && !(txsr & ENTSR_PTX)))
    {
      if (ei_debug >= 3)
         printk ("ei_start_xmit done (tbusy)\n");
      en_prn_int();
      return (0);
    }
    isr = Read_Register_Value (EN0_ISR);
    printk ("%s: transmit timed out, TX status %#2x, ISR %#2x.\n",
	    dev->name, txsr, isr);

    /* Does the 8390 thinks it has posted an interrupt?
     */
    if (isr)
      printk ("%s: Possible IRQ conflict on IRQ%d?\n",
	      dev->name, dev->irq);
    else
    {
      /* The 8390 probably hasn't gotten on the cable yet
       */
      printk ("%s: Possible network cable problem?\n", dev->name);
      if (ei_local->stat.tx_packets == 0)
	ei_local->interface_num ^= 1;	/* Try a different xcvr.  */
    }

    /* Try to restart the card. Perhaps the user has fixed something
     */
    ei_reset_8390 (dev);
    NS8390_init (dev, 1);

    /* en_prn_int(); */
    dev->tx_start = jiffies;
  }

  if (len <= 0)
  {
    if (ei_debug >= 3)
       printk ("ei_start_xmit done (len <= 0)\n");
    return (1);
  }

  /* Block a timer-based transmit from overlapping
   */
  if (dev->tx_busy)
  {
    printk ("%s: ei_start_xmit: Transmitter access conflict.\n", dev->name);
    return (0);
  }

  /* Mask interrupts from the ethercard
   */
  Set_Register_Value (EN0_IMR, 0x00);
  ei_local->irqlock = 1;

  send_length = ETH_MIN < len ? len : ETH_MIN;

  ei_block_output (dev, len, buf, TXPAGE_8p);
  NS8390_trigger_send (dev, send_length, TXPAGE_8p);
  dev->tx_start = jiffies;
  dev->tx_busy  = 1;

  /* Turn 8390 interrupts back on
   */
  ei_local->irqlock = 0;
  Set_Register_Value (EN0_IMR, 0xff);	/*0xff used to be ENISR_ALL */

  if (ei_debug >= 3)
     printk ("ei_start_xmit done\n");
  en_prn_int();
  return (1);
}

/* 
 * Handle the ether interface interrupts.
 */
void ei_interrupt (int irq)
{
  struct device    *dev;
  struct ei_device *ei_local;
  int    interrupts, boguscount = 0;

#ifdef POLLING
  irq = ethpk_irq;
  if (ei_debug > 4)
     printk ("ei_interrupt %d (reg_ptr %d)..", irq, reg_ptr);

  if (ei_debug > 3)
     printk ("called from timer, not from interrupt handler\n");

  dev = irq2dev_map[ethpk_irq];
  if (ei_debug > 3)
     printk ("dev found:%s\n", dev->name);

#else
  dev = irq2dev_map[irq];
  if (ei_debug > 4)
     printk ("ei_interrupt %d..", irq);
#endif

  if (!dev)
  {
    printk ("ei_interrupt(): irq %d for unknown device.\n", irq);
    return;
  }

  ei_local = (struct ei_device *) dev->priv;
  if (dev->reentry || ei_local->irqlock)
  {
    if (ei_debug > 3)
       printk ("%s: %s\n", dev->name, ei_local->irqlock
               ? "interrupted while interrupts are masked"
               : "rentering the interrupt handler");
    return;
  }

  dev->reentry = 1;

  /* Change to page 0 and read the intr status reg
   */
  Set_Register_Value (E8390_CMD, E8390_NODMA + E8390_PAGE0);

  /* !!Assumption!! -- we stay in page 0. Don't break this.
   */
  interrupts = Read_Register_Value (EN0_ISR);
  while (interrupts)
  {
    if (ei_debug > 4)
      printk ("isr = %#2.2x.\n", interrupts);

    if (interrupts & ENISR_RDC)
    {
      /* Ack meaningless DMA complete
       */
      if (ei_debug > 4)
	printk ("Acknowledge DMA\n");
      Set_Register_Value (EN0_ISR, ENISR_RDC);
    }

    if (interrupts & ENISR_OVER)
    {
      if (ei_debug > 4)
	printk ("rx overrun\n");
      ei_rx_overrun (dev);
    }
    else if (interrupts & (ENISR_RX + ENISR_RX_ERR))
    {
      /* Got a good (?) packet. */
      if (ei_debug > 4)
	printk ("Receive one\n");
      ei_receive (dev);
    }

    /* Push the next to-transmit packet through
     */
    if (interrupts & ENISR_TX)
    {
      if (ei_debug > 4)
	printk ("Next to transmit\n");
      ei_tx_intr (dev);
    }
    else if (interrupts & ENISR_COUNTERS)
    {
      if (ei_debug > 4)
	printk ("Counters\n");
      ei_local->stat.rx_frame_errors  += Read_Register_Value (EN0_COUNTER0);
      ei_local->stat.rx_crc_errors    += Read_Register_Value (EN0_COUNTER1);
      ei_local->stat.rx_missed_errors += Read_Register_Value (EN0_COUNTER2);
      Set_Register_Value (EN0_ISR, ENISR_COUNTERS);	/* Ack intr. */
    }

    /* Ignore the transmit errs and reset intr for now
     */
    if (interrupts & ENISR_TX_ERR)
    {
      if (ei_debug > 4)
	printk ("Ignoring transmit and interrupt errors\n");
      Set_Register_Value (EN0_ISR, ENISR_TX_ERR);	/* Ack intr. */
    }
    interrupts = Read_Register_Value (EN0_ISR);
  }
  Set_Register_Value (E8390_CMD, E8390_NODMA + E8390_PAGE0 + E8390_START);

  interrupts = Read_Register_Value (EN0_ISR);
  if (interrupts && ei_debug)
  {
    if (boguscount == 9)
      printk ("%s: Too much work at interrupt, status %#2.2x\n",
	      dev->name, interrupts);
    else
      printk ("%s: unknown interrupt %#2x\n", dev->name, interrupts);
    Set_Register_Value (E8390_CMD, E8390_NODMA + E8390_PAGE0 + E8390_START);
    Set_Register_Value (EN0_ISR, 0x00);		/* Ack. all intrs. */
  }
  dev->reentry = 0;

  Set_Register_Value (EN0_RXCR, E8390_RXCONFIG);
  if (ei_debug > 4)
     printk ("ei_interrupt done\n");
  en_prn_int();
}

/*
 * We have finished a transmit: check for errors and then trigger the
 * next packet to be sent.
 */
static void ei_tx_intr (struct device *dev)
{
  struct ei_device *ei_local = (struct ei_device *) dev->priv;
  int    status = Read_Register_Value (EN0_TSR);

  if (ei_debug >= 3)
    printk ("ei_tx_intr..\n");
  Set_Register_Value (EN0_ISR, ENISR_TX);     /* Ack intr */

  ei_local->txing = 0;
  dev->tx_busy = 0;

  /* Minimize Tx latency: update the statistics after we restart TXing
   */
  if (status & ENTSR_COL)
    ei_local->stat.tx_collisions++;
  if (status & ENTSR_PTX)
    ei_local->stat.tx_packets++;
  else
  {
    ei_local->stat.tx_errors++;
    if (status & ENTSR_ABT)
      ei_local->stat.tx_aborted_errors++;
    if (status & ENTSR_CRS)
      ei_local->stat.tx_carrier_errors++;
    if (status & ENTSR_FU)
      ei_local->stat.tx_fifo_errors++;
    if (status & ENTSR_CDH)
      ei_local->stat.tx_heartbeat_errors++;
    if (status & ENTSR_OWC)
      ei_local->stat.tx_window_errors++;
  }
  /* en_prn_int(); */
  if (ei_debug == 3)
    printk ("ei_tx_intr done\n");
}

/*
 * We have a good packet(s), get it/them out of the buffer(s).
 */
static void ei_receive (struct device *dev)
{
  struct ei_device     *ei_local = (struct ei_device*) dev->priv;
  struct e8390_pkt_hdr  rx_frame;
  int    rxing_page, this_frame, next_frame, current_offset;
  int    rx_pkt_count = 0;
  int    num_rx_pages = ei_local->stop_page - ei_local->rx_start_page;

  if (ei_debug == 3)
    printk ("ei_receive..");

  while (++rx_pkt_count < 10)
  {
    int len;

    /* Get the rx page (incoming packet pointer)
     */
    Set_Register_Value (E8390_CMD, E8390_NODMA + E8390_PAGE1);
    rxing_page = Read_Register_Value (EN1_CURPAG);
    Set_Register_Value (E8390_CMD, E8390_NODMA + E8390_PAGE0);

    /* Remove one frame from the ring. Boundary is alway a page behind
     */
    this_frame = Read_Register_Value (EN0_BOUNDARY);
    this_frame += 1;
    if (this_frame >= ei_local->stop_page)
      this_frame = ei_local->rx_start_page;

    /* Someday we'll omit the previous, if we never get this message.
     * (There is at least one clone claimed to have a problem.)
     */
    if (ei_debug > 0 && this_frame != ei_local->current_page)
      printk ("%s: mismatched read page pointers %2x vs %2x.\n",
	      dev->name, this_frame, ei_local->current_page);

    if (this_frame == rxing_page)   /* Read all the frames? */
      break;                        /* Done for now */

    current_offset = this_frame << 8;
    ei_block_input (dev, sizeof (rx_frame), (char *) &rx_frame,
		    current_offset);

    len = rx_frame.count - sizeof (rx_frame);

    next_frame = this_frame + 1 + ((len + 4) >> 8);

    /* Check for bogosity warned by 3c503 book: the status byte is never
     * written.  This happened a lot during testing! This code should be
     * cleaned up someday.
     */
    if (rx_frame.next != next_frame                &&
        rx_frame.next != next_frame + 1            &&
        rx_frame.next != next_frame - num_rx_pages &&
        rx_frame.next != next_frame + 1 - num_rx_pages)
    {
      ei_local->current_page = rxing_page;
      Set_Register_Value (EN0_BOUNDARY, ei_local->current_page - 1);
      ei_local->stat.rx_errors++;
      continue;
    }

    if (len < 60 || len > 1518)
    {
      if (ei_debug)
         printk ("%s: bogus packet size: %d, status=%#2x nxpg=%#2x.\n",
                 dev->name, rx_frame.count, rx_frame.status,
                 rx_frame.next);
      ei_local->stat.rx_errors++;
    }
    else if ((rx_frame.status & 0x0F) == ENRSR_RXOK)
    {
      if (dev->get_rx_buf)
      {
        char *buf = (*dev->get_rx_buf)(len);
        if (buf)
             ei_block_input (dev, len, buf, current_offset + sizeof(rx_frame));
        else ei_local->stat.rx_dropped++;
      }
      ei_local->stat.rx_packets++;
    }
    else
    {
      int errs = rx_frame.status;

      if (ei_debug)
	printk ("%s: bogus packet: status=%#2x nxpg=%#2x size=%d\n",
		dev->name, rx_frame.status, rx_frame.next,
		rx_frame.count);
      if (errs & ENRSR_FO)
	ei_local->stat.rx_fifo_errors++;
    }
    next_frame = rx_frame.next;

    /* This _should_ never happen: it's here for avoiding bad clones
     */
    if (next_frame >= ei_local->stop_page)
    {
      printk ("%s: next frame inconsistency, %#2x..", dev->name,
	      next_frame);
      next_frame = ei_local->rx_start_page;
    }
    ei_local->current_page = next_frame;
    Set_Register_Value (EN0_BOUNDARY, next_frame - 1);
  }

  /* If any worth-while packets have been received, dev_rint()
   * has done a mark_bh(NET_BH) for us and will work on them
   * when we get to the bottom-half routine
   */

  /* Record the maximum Rx packet queue. */
  if (rx_pkt_count > high_water_mark)
    high_water_mark = rx_pkt_count;

  /* Bug alert!  Reset ENISR_OVER to avoid spurious overruns!
   */
  Set_Register_Value (EN0_ISR, ENISR_RX + ENISR_RX_ERR + ENISR_OVER);
  /* en_prn_int(); */
  if (ei_debug == 3)
    printk ("ei_receive done\n");
}

/*
 * We have a receiver overrun: we have to kick the 8390 to get it
 * started again.
 */
static void ei_rx_overrun (struct device *dev)
{
  struct ei_device *ei_local = (struct ei_device*) dev->priv;
  int    reset_start_time = jiffies;

  if (ei_debug > 4)
    printk ("ei_rx_overrun..");

  /* We should already be stopped and in page0.  Remove after testing.
   */
  Set_Register_Value (E8390_CMD, E8390_NODMA + E8390_PAGE0 + E8390_STOP);

  if (ei_debug > 1)
    printk ("%s: Receiver overrun.\n", dev->name);

  ei_local->stat.rx_over_errors++;

  /* The old Biro driver does dummy = Read_Register_Value(RBCR[01]);
   * at this point. It might mean something -- magic to speed up a
   * reset?  A 8390 bug?
   */

  /* Wait for the reset to complete. This should happen almost instantly,
   * but could take up to 1.5msec in certain rare instances.  There is no
   * easy way of timing something in that range, so we use 'jiffies' as
   * a sanity check
   */
  while ((Read_Register_Value (EN0_ISR) & ENISR_RESET) == 0)
    if (jiffies - reset_start_time > 1)
    {
      printk ("%s: reset did not complete at ei_rx_overrun.\n",
	      dev->name);
      NS8390_init (dev, 1);
      if (ei_debug > 4)
	printk ("ei_rx_overrun done\n");
      return;
    }

  /* Remove packets right away
   */
  ei_receive (dev);

  Set_Register_Value (EN0_ISR, 0xff);

  /* Generic 8390 insns to start up again, same as in open_8390()
   */
  Set_Register_Value (E8390_CMD, E8390_NODMA + E8390_PAGE0 + E8390_START);

  Set_Register_Value (EN0_TXCR, E8390_TXCONFIG);        /* xmit on */
  if (ei_debug > 4)
    printk ("ei_rx_overrun done\n");
}

static void *get_stats (struct device *dev)
{
  struct ei_device *ei_local = (struct ei_device *) dev->priv;

  if (ei_debug > 4)
    printk ("enet_statistics..\n");

  /* Read the counter registers, assuming we are in page 0
   */
  ei_local->stat.rx_frame_errors  += Read_Register_Value (EN0_COUNTER0);
  ei_local->stat.rx_crc_errors    += Read_Register_Value (EN0_COUNTER1);
  ei_local->stat.rx_missed_errors += Read_Register_Value (EN0_COUNTER2);
  return (void*)&ei_local->stat;
}

/*
 * Set or clear the multicast filter for this adaptor.
 * num_addrs == -1 => Promiscuous mode, receive all packets
 * num_addrs == 0  => Normal mode, clear multicast list
 * num_addrs > 0   => Multicast mode, receive normal and MC packets, and do
 *                    best-effort filtering.
 */
static void set_multicast_list (struct device *dev, int num_addrs, void *addrs)
{
  if (ei_debug > 4)
    printk ("set_multicast_list..");

  if (num_addrs > 0)
  {
    /* The multicast-accept list is initialized to accept-all, and we
     * rely on higher-level filtering for now.
     */
    Set_Register_Value (EN0_RXCR, E8390_RXCONFIG | 0x08);
  }
  else if (num_addrs < 0)
       Set_Register_Value (EN0_RXCR, E8390_RXCONFIG | 0x18);
  else Set_Register_Value (EN0_RXCR, E8390_RXCONFIG);

  en_prn_int();
}

/*
 * Initialize the rest of the 8390 device structure.
 */
int ethdev_init (struct device *dev)
{
  if (ei_debug > 4)
    printk ("ethdev_init..");

  if (dev->priv == NULL)
  {
    struct ei_device *ei_local;

    dev->priv = calloc (sizeof(struct ei_device), 1);
    ei_local  = (struct ei_device*) dev->priv;
#ifndef NO_PINGPONG
    ei_local->pingpong = 1;
#endif
  }

  /* The open call may be overridden by the card-specific code
   */
  if (dev->open == NULL)
      dev->open = ei_open;

  /* We should have a dev->close entry also. */
  dev->xmit      = ei_start_xmit;
  dev->get_stats = get_stats;
  dev->set_multicast_list = (void(*)(struct device*)) set_multicast_list;
  return (1);
}


/*
 * This page of functions should be 8390 generic
 * Follow National Semi's recommendations for initializing the "NIC".
 */
void NS8390_init (struct device *dev, int startp)
{
  struct ei_device *ei_local = (struct ei_device*) dev->priv;
  int    i;

  if (ei_debug > 4)
     printk ("NS8390_init..");

  /* Follow National Semi's recommendations for initing the DP83902
   */
  Set_Register_Value (E8390_CMD, E8390_NODMA + E8390_PAGE0);
  Set_Register_Value (EN0_DCFG, 0x48);	/* 0x48 or 0x49 */

  /* Clear the remote byte count registers
   */
  Set_Register_Value (EN0_RCNTLO, 0x00);
  Set_Register_Value (EN0_RCNTHI, 0x00);

  /* Set to monitor and loopback mode -- this is vital!
   */
  Set_Register_Value (EN0_RXCR, E8390_RXOFF);	/* 0x20 */
  Set_Register_Value (EN0_TXCR, 0x00);	/* normal */

  /* Set the transmit page and receive ring
   */
  ei_local->tx1 = ei_local->tx2 = 0;
  Set_Register_Value (EN0_STARTPG, PSTART_8p);
  Set_Register_Value (EN0_BOUNDARY, PSTART_8p);
  ei_local->current_page = ei_local->rx_start_page + 1; /* assert
                                                         * boundary+1 */
  ei_local->stop_page = PSTOP_8p;
  Set_Register_Value (EN0_STOPPG, ei_local->stop_page);

  /* Clear the pending interrupts and mask
   */
  Set_Register_Value (EN0_ISR, 0xFF);
  Set_Register_Value (EN0_IMR, 0x00);

  /* Copy the station address into the DS8390 registers,
   * and set the multicast hash bitmap to receive all multicasts
   */
  DISABLE();
  Set_Register_Value (E8390_CMD, E8390_NODMA + E8390_PAGE1);

  /* Initialize the multicast list to accept-all.  If we enable multicast
   * the higher levels can do the filtering.
   */
  for (i = 0; i < 8; i++)
    Set_Register_Value (EN1_MULT + i, 0xff);

  Set_Register_Value (EN1_CURPAG, ei_local->rx_start_page + 1);
  Set_Register_Value (E8390_CMD, E8390_NODMA + E8390_PAGE0);
  ENABLE();
  dev->tx_busy  = 0;
  dev->reentry  = 0;
  ei_local->tx1 = ei_local->tx2 = 0;
  ei_local->txing = 0;
  if (startp)
  {
    Set_Register_Value (EN0_IMR, 0x00);
    Set_Register_Value (EN0_ISR, 0xff);
    Set_Register_Value (E8390_CMD, E8390_NODMA + E8390_START);
 // Set_Register_Value(EN0_TXCR, E8390_TXCONFIG);  /* xmit on. */

    /* 3c503 TechMan says rxconfig only after the NIC is started
     */
    Set_Register_Value (EN0_RXCR, E8390_RXCONFIG);      /* rx on */
    Set_Register_Value (EN0_IMR, 0xff /*ENISR_ALL*/);
  }
}


/*
 * Trigger a transmit start, assuming the length is valid
 */
static void NS8390_trigger_send (struct device *dev, unsigned int length,
				 int start_page)
{
  int xx;

  if (ei_debug > 4)
     printk ("NS8390_trigger_send..");

  ei_status.txing = 1;
  xx = Read_Register_Value (E8390_CMD);
  if (ei_debug > 4)
     printk ("E8390_CMD = %#04x\n", xx);

  if (xx & E8390_TRANS)
  {
    printk ("%s: trigger_send() called with the transmitter busy.(%#04x)\n",
	    dev->name, xx);
    return;
  }
  Set_Register_Value (E8390_CMD, E8390_NODMA + E8390_PAGE0);
  Set_Register_Value (EN0_TCNTLO, length & 0xff);
  Set_Register_Value (EN0_TCNTHI, length >> 8);
  Set_Register_Value (EN0_TPSR, start_page);
  Set_Register_Value (E8390_CMD, E8390_NODMA + E8390_TRANS + E8390_START);

  /* en_prn_int(); */
}
