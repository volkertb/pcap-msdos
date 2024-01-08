/*
 * Linux ethernet device driver for the 3Com Etherlink Plus (3C505)
 *      By Craig Southeren, Juha Laiho and Philip Blundell
 *
 * 3c505.c      This module implements an interface to the 3Com
 *              Etherlink Plus (3c505) ethernet card. Linux device
 *              driver interface reverse engineered from the Linux 3C509
 *              device drivers. Some 3C505 information gleaned from
 *              the Crynwr packet driver. Still this driver would not
 *              be here without 3C505 technical reference provided by
 *              3Com.
 *
 * $Id: 3c505.c,v 1.10 1996/04/16 13:06:27 phil Exp $
 *
 * Authors:     Linux 3c505 device driver by
 *                      Craig Southeren, <craigs@ineluki.apana.org.au>
 *              Final debugging by
 *                      Andrew Tridgell, <tridge@nimbus.anu.edu.au>
 *              Auto irq/address, tuning, cleanup and v1.1.4+ kernel mods by
 *                      Juha Laiho, <jlaiho@ichaos.nullnet.fi>
 *              Linux 3C509 driver by
 *                      Donald Becker, <becker@super.org>
 *              Crynwr packet driver by
 *                      Krishnan Gopalan and Gregg Stefancik,
 *                      Clemson University Engineering Computer Operations.
 *                      Portions of the code have been adapted from the 3c505
 *                         driver for NCSA Telnet by Bruce Orchard and later
 *                         modified by Warren Van Houten and krus@diku.dk.
 *              3C505 technical information provided by
 *                      Terry Murphy, of 3Com Network Adapter Division
 *              Linux 1.3.0 changes by
 *                      Alan Cox <Alan.Cox@linux.org>
 *              More debugging, DMA support, currently maintained by
 *                      Philip Blundell <Philip.Blundell@pobox.com>
 *              Multicard/soft configurable dma channel/rev 2 hardware support
 *                      by Christopher Collins <ccollins@pcug.org.au>
 */

/* Theory of operation:
 *
 * The 3c505 is quite an intelligent board.  All communication with it is done
 * by means of Primary Command Blocks (PCBs); these are transferred using PIO
 * through the command register.  The card has 256k of on-board RAM, which is
 * used to buffer received packets.  It might seem at first that more buffers
 * are better, but in fact this isn't true.  From my tests, it seems that
 * more than about 10 buffers are unnecessary, and there is a noticeable
 * performance hit in having more active on the card.  So the majority of the
 * card's memory isn't, in fact, used.  Sadly, the card only has one transmit
 * buffer and, short of loading our own firmware into it (which is what some
 * drivers resort to) there's nothing we can do about this.
 *
 * We keep up to 4 "receive packet" commands active on the board at a time.
 * When a packet comes in, so long as there is a receive command active, the
 * board will send us a "packet received" PCB and then add the data for that
 * packet to the DMA queue.  If a DMA transfer is not already in progress, we
 * set one up to start uploading the data.  We have to maintain a list of
 * backlogged receive packets, because the card may decide to tell us about
 * a newly-arrived packet at any time, and we may not be able to start a DMA
 * transfer immediately (ie one may already be going on).  We can't NAK the
 * PCB, because then it would throw the packet away.
 *
 * Trying to send a PCB to the card at the wrong moment seems to have bad
 * effects.  If we send it a transmit PCB while a receive DMA is happening,
 * it will just NAK the PCB and so we will have wasted our time.  Worse, it
 * sometimes seems to interrupt the transfer.  The majority of the low-level
 * code is protected by one huge semaphore -- "busy" -- which is set whenever
 * it probably isn't safe to do anything to the card.  The receive routine
 * must gain a lock on "busy" before it can start a DMA transfer, and the
 * transmit routine must gain a lock before it sends the first PCB to the card.
 * The send_pcb() routine also has an internal semaphore to protect it against
 * being re-entered (which would be disastrous) -- this is needed because
 * several things can happen asynchronously (re-priming the receiver and
 * asking the card for statistics, for example).  send_pcb() will also refuse
 * to talk to the card at all if a DMA upload is happening.  The higher-level
 * networking code will reschedule a later retry if some part of the driver
 * is blocked.  In practice, this doesn't seem to happen very often.
 */

/* This driver may now work with revision 2.x hardware, since all the read
 * operations on the HCR have been removed (we now keep our own softcopy).
 * But I don't have an old card to test it on.
 *
 * This has had the bad effect that the autoprobe routine is now a bit
 * less friendly to other devices.  However, it was never very good.
 * before, so I doubt it will hurt anybody.
 */

/* The driver is a mess.  I took Craig's and Juha's code, and hacked it firstly
 * to make it more reliable, and secondly to add DMA mode.  Many things could
 * probably be done better; the concurrency protection is particularly awful.
 */

#include "pmdrvr.h"
#include "3c505.h"

static int start_receive (struct device *, pcb_struct *);

#define TIMEOUT_MSG(lineno)  printk ("** timeout at %s (line %d) ***\n", \
                                     __FILE__,lineno)

#define INVALID_PCB_MSG(len) printk ("** invalid pcb length %d at " \
                                     "%s (line %d) ***\n",          \
                                     len,__FILE__,__LINE__)

static const char *search_msg       = "%s: Looking for 3c505 adapter at address %x...";
static const char *stilllooking_msg = "still looking...";
static const char *found_msg        = "found.\n";
static const char *notfound_msg     = "not found (reason = %d)\n";
static const char *couldnot_msg     = "%s: 3c505 not found\n";
static const int  elp_debug         = 3;
/*
 *  0 = no messages (well, some)
 *  1 = messages when high level commands performed
 *  2 = messages when low level commands performed
 *  3 = messages when interrupts received
 */

/*
 * List of I/O-addresses we try to auto-sense
 * Last element MUST BE 0!
 */

static const int addr_list[] = { 0x300, 0x280, 0x310, 0 };

/* Dma Memory related stuff */

/*
 * Pure 2^n version of get_order
 */
static __inline int __get_order (DWORD size)
{
  int order = -1;

  size = (size-1) >> (PAGE_SHIFT-1);
  do
  {
    size >>= 1;
    order++;
  }
  while (size);
  return (order);
}

static void *dma_mem_alloc (int size, WORD *dma_sel)
{
  DWORD phys;
  int   sel;

  if (_dma_allocate(size,&sel,&phys) < 0)
     return (NULL);

  *dma_sel = (WORD)sel;
  return (void*)phys;
}

/*
 * Functions for I/O
 */
static __inline BYTE inb_status (DWORD base_addr)
{
  return inb (base_addr + PORT_STATUS);
}

static __inline int inb_command (DWORD base_addr)
{
  return inb (base_addr + PORT_COMMAND);
}

static __inline void outb_control (BYTE val, struct device *dev)
{
  outb (val, dev->base_addr + PORT_CONTROL);
  ((elp_device*)dev->priv)->hcr_val = val;
}

#define HCR_VAL(x) (((elp_device *)((x)->priv))->hcr_val)

static __inline void outb_command (BYTE val, DWORD base_addr)
{
  outb (val, base_addr + PORT_COMMAND);
}

static __inline DWORD inw_data (DWORD base_addr)
{
  return inw (base_addr + PORT_DATA);
}

static __inline void outw_data (DWORD val, DWORD base_addr)
{
  outw (val, base_addr + PORT_DATA);
}

static __inline DWORD backlog_next (DWORD n)
{
  return (n + 1) % BACKLOG_SIZE;
}

/*
 * use this routine when accessing the ASF bits as they are
 * changed asynchronously by the adapter
 *
 * get adapter PCB status
 */
#define GET_ASF(addr) (get_status(addr) & ASF_PCB_MASK)

static __inline int get_status (DWORD base_addr)
{
  int timeout = jiffies + 10;
  int stat1;
  do
  {
    stat1 = inb_status(base_addr);
  }
  while (stat1 != inb_status(base_addr) && jiffies < timeout);

  if (jiffies >= timeout)
     TIMEOUT_MSG (__LINE__);
  return (stat1);
}

static __inline void set_hsf (struct device *dev, int hsf)
{
  DISABLE();
  outb_control ((HCR_VAL(dev) & ~HSF_PCB_MASK) | hsf, dev);
  ENABLE();
}


static __inline void adapter_reset (struct device *dev)
{
  elp_device *adapter  = dev->priv;
  BYTE        orig_hcr = adapter->hcr_val;
  int         timeout;

  outb_control (0, dev);

  if (inb_status(dev->base_addr) & ACRF)
  {
    do
    {
      inb_command (dev->base_addr);
      timeout = jiffies + 2;
      while ((jiffies <= timeout) && !(inb_status(dev->base_addr) & ACRF));
    }
    while (inb_status(dev->base_addr) & ACRF);
    set_hsf (dev, HSF_PCB_NAK);
  }
  outb_control (adapter->hcr_val | ATTN | DIR, dev);
  timeout = jiffies + 1;
  while (jiffies <= timeout)
        ;
  outb_control (adapter->hcr_val & ~ATTN, dev);
  timeout = jiffies + 1;
  while (jiffies <= timeout)
        ;
  outb_control (adapter->hcr_val | FLSH, dev);
  timeout = jiffies + 1;
  while (jiffies <= timeout)
        ;
  outb_control (adapter->hcr_val & ~FLSH, dev);
  timeout = jiffies + 1;
  while (jiffies <= timeout)
        ;

  outb_control (orig_hcr, dev);
  if (!start_receive(dev, &adapter->tx_pcb))
     printk ("%s: start receive command failed \n", dev->name);
}

/*
 * Check to make sure that a DMA transfer hasn't timed out.  This should
 * never happen in theory, but seems to occur occasionally if the card gets
 * prodded at the wrong time.
 */
static __inline void check_dma (struct device *dev)
{
  elp_device *adapter = (elp_device *) &dev->priv;

  if (adapter->dmaing && (jiffies > (adapter->current_dma.start_time + 10)))
  {
    DISABLE();
    printk ("%s: DMA %s timed out, %d bytes left\n", dev->name,
            adapter->current_dma.direction ? "download" : "upload",
            get_dma_residue(dev->dma));

    adapter->dmaing = 0;
    adapter->busy   = 0;
    _dma_stop (dev->dma);
    if (adapter->rx_active)
        adapter->rx_active--;
    outb_control (adapter->hcr_val & ~(DMAE | TCEN | DIR), dev);
    ENABLE();
  }
}

/*
 * Primitive functions used by send_pcb()
 */
static __inline DWORD send_pcb_slow (DWORD base_addr, BYTE byte)
{
  DWORD timeout;

  outb_command (byte, base_addr);
  for (timeout = jiffies + 5; jiffies < timeout;)
  {
    if (inb_status(base_addr) & HCRE)
       return (FALSE);
  }
  printk ("3c505: send_pcb_slow timed out\n");
  return (TRUE);
}

static __inline DWORD send_pcb_fast (DWORD base_addr, BYTE byte)
{
  DWORD timeout;

  outb_command (byte, base_addr);
  for (timeout = 0; timeout < 40000; timeout++)
  {
    if (inb_status(base_addr) & HCRE)
       return (FALSE);
  }
  printk ("3c505: send_pcb_fast timed out\n");
  return (TRUE);
}

/*
 * Check to see if the receiver needs restarting, and kick it if so
 */
static __inline void prime_rx (struct device *dev)
{
  elp_device *adapter = (elp_device*) &dev->priv;

  while (adapter->rx_active < ELP_RX_PCBS && dev->start)
  {
    if (!start_receive(dev,&adapter->itx_pcb))
       break;
  }
}

/*
 * send_pcb
 *   Send a PCB to the adapter.
 *
 *  output byte to command reg  --<--+
 *  wait until HCRE is non zero      |
 *  loop until all bytes sent   -->--+
 *  set HSF1 and HSF2 to 1
 *  output pcb length
 *  wait until ASF give ACK or NAK
 *  set HSF1 and HSF2 to 0
 *
 */

/* This can be quite slow -- the adapter is allowed to take up to 40ms
 * to respond to the initial interrupt.
 *
 * We run initially with interrupts turned on, but with a semaphore set
 * so that nobody tries to re-enter this code.  Once the first byte has
 * gone through, we turn interrupts off and then send the others (the
 * timeout is reduced to 500us).
 */
static int send_pcb (struct device *dev, pcb_struct * pcb)
{
  int         i,timeout;
  elp_device *adapter = dev->priv;

  check_dma (dev);

  if (adapter->dmaing && adapter->current_dma.direction == 0)
     return (FALSE);

  /*
   * load each byte into the command register and
   * wait for the HCRE bit to indicate the adapter
   * had read the byte
   */
  set_hsf (dev, 0);

  if (send_pcb_slow(dev->base_addr, pcb->command))
     goto abort;

  DISABLE();

  if (send_pcb_fast(dev->base_addr, pcb->length))
     goto sti_abort;

  for (i = 0; i < pcb->length; i++)
  {
    if (send_pcb_fast(dev->base_addr, pcb->data.raw[i]))
       goto sti_abort;
  }

  outb_control (adapter->hcr_val | 3, dev);  /* signal end of PCB */
  outb_command (2 + pcb->length, dev->base_addr);

  /* now wait for the acknowledgement */
  ENABLE();

  for (timeout = jiffies + 5; jiffies < timeout; )
  {
    switch (GET_ASF(dev->base_addr))
    {
      case ASF_PCB_ACK:
           return (TRUE);
      case ASF_PCB_NAK:
           printk("%s: send_pcb got NAK\n", dev->name);
           goto abort;
    }
  }

  if (elp_debug >= 1)
     printk ("%s: timeout waiting for PCB acknowledge (status %02x)\n",
             dev->name, inb_status(dev->base_addr));

sti_abort:
  ENABLE();
abort:
  return (FALSE);
}


/*
 * receive_pcb
 *   Read a PCB from the adapter
 *
 *  wait for ACRF to be non-zero      ---<---+
 *  input a byte                             |
 *  if ASF1 and ASF2 were not both one       |
 *    before byte was read, loop      --->---+
 *  set HSF1 and HSF2 for ack
 *
 */
static int receive_pcb (struct device *dev, pcb_struct * pcb)
{
  int         i, j, total_length, stat, timeout;
  elp_device *adapter = dev->priv;

  set_hsf (dev,0);

  /* get the command code */
  timeout = jiffies + 2;
  while (((stat = get_status(dev->base_addr)) & ACRF) == 0 &&
         jiffies < timeout) ;

  if (jiffies >= timeout)
  {
    TIMEOUT_MSG(__LINE__);
    return (FALSE);
  }
  pcb->command = inb_command(dev->base_addr);

  /* read the data length */
  timeout = jiffies + 3;
  while (((stat = get_status(dev->base_addr)) & ACRF) == 0 &&
         jiffies < timeout) ;
  if (jiffies >= timeout)
  {
    TIMEOUT_MSG (__LINE__);
    printk ("%s: status %02x\n", dev->name, stat);
    return (FALSE);
  }
  pcb->length = inb_command(dev->base_addr);

  if (pcb->length > MAX_PCB_DATA)
  {
    INVALID_PCB_MSG (pcb->length);
    adapter_reset (dev);
    return (FALSE);
  }
  /* read the data */
  DISABLE();
  i = 0;
  do
  {
    j = 0;
    while (((stat = get_status(dev->base_addr)) & ACRF) == 0 && j++ < 20000)
          ;
    pcb->data.raw[i++] = inb_command(dev->base_addr);
    if (i > MAX_PCB_DATA)
       INVALID_PCB_MSG (i);
  }
  while ((stat & ASF_PCB_MASK) != ASF_PCB_END && j < 20000);
  ENABLE();
  if (j >= 20000)
  {
    TIMEOUT_MSG(__LINE__);
    return (FALSE);
  }

  /* woops, the last "data" byte was really the length!
   */
  total_length = pcb->data.raw[--i];

  /* safety check total length vs data length
   */
  if (total_length != (pcb->length + 2))
  {
    if (elp_debug >= 2)
       printk ("%s: mangled PCB received\n", dev->name);
    set_hsf(dev, HSF_PCB_NAK);
    return (FALSE);
  }

  if (pcb->command == CMD_RECEIVE_PACKET_COMPLETE)
     adapter->busy = 0;
  set_hsf (dev, HSF_PCB_ACK);
  return (TRUE);
}

/*
 *  queue a receive command on the adapter so we will get an
 *  interrupt when a packet is received.
 */
static int start_receive (struct device *dev, pcb_struct * tx_pcb)
{
  int         status;
  elp_device *adapter = dev->priv;

  if (elp_debug >= 3)
     printk ("%s: restarting receiver\n", dev->name);
  tx_pcb->command = CMD_RECEIVE_PACKET;
  tx_pcb->length  = sizeof(struct Rcv_pkt);
  tx_pcb->data.rcv_pkt.buf_seg = 0;
  tx_pcb->data.rcv_pkt.buf_ofs = 0;
  tx_pcb->data.rcv_pkt.buf_len = 1600;
  tx_pcb->data.rcv_pkt.timeout = 0;     /* set timeout to zero */
  status = send_pcb(dev, tx_pcb);
  if (status)
     adapter->rx_active++;
  return (status);
}

/*
 * extract a packet from the adapter
 * this routine is only called from within the interrupt
 * service routine, so no cli/sti calls are needed
 * note that the length is always assumed to be even
 */                 
static void receive_packet (struct device *dev, int len)
{
  elp_device *adapter = dev->priv;
  char       *target;
  static      char ebuf[1600];
  int         rlen = (len + 1) & ~1;

  target = ebuf;
  if (VIRT_TO_PHYS(target + rlen) >= MAX_DMA_ADDR)
  {
    adapter->current_dma.target = (void*)target;
    target = (char*)adapter->dma_buffer;
  }
  else
    adapter->current_dma.target = NULL;  

  /* if this happens, we die */
  if (adapter->dmaing)
     printk ("%s: rx blocked, DMA in progress, dir %ld\n",
             dev->name, adapter->current_dma.direction);

  adapter->dmaing                 = 1;
  adapter->current_dma.direction  = 0;
  adapter->current_dma.length     = rlen;
  adapter->current_dma.data       = ebuf;
  adapter->current_dma.data_len   = rlen;
  adapter->current_dma.start_time = jiffies;

  outb_control (adapter->hcr_val | DIR | TCEN | DMAE, dev);

  _dma_stop (dev->dma);
  clear_dma_ff (dev->dma);
  set_dma_mode (dev->dma, 0x04);  /* dma read */
  set_dma_addr (dev->dma, (DWORD)target);
  set_dma_count(dev->dma, rlen);
  enable_dma (dev->dma);

  if (elp_debug >= 3)
     printk ("%s: rx DMA transfer started\n", dev->name);

  if (adapter->rx_active)
    --adapter->rx_active;

  if (!adapter->busy)
     printk ("%s: receive_packet called, busy not set.\n", dev->name);
}

/*
 * interrupt handler
 */
static void elp_interrupt (int irq)
{
  struct device *dev     = irq2dev_map[irq];
  elp_device    *adapter = (elp_device *) dev->priv;
  int            len, dlen, icount = 0;
  int            timeout;

  if (!dev || dev->irq != irq)
  {
    printk ("elp_interrupt(): irq %d for unknown device.", irq);
    return;
  }

  if (dev->reentry)
  {
    printk ("%s: re-entering the interrupt handler!\n", dev->name);
    return;
  }
  dev->reentry = 1;

  do
  {
    /*
     * has a DMA transfer finished?
     */
    if (inb_status(dev->base_addr) & DONE)
    {
      if (!adapter->dmaing)
         printk ("%s: phantom DMA completed\n", dev->name);

      if (elp_debug >= 3)
         printk ("%s: %s DMA complete, status %02x\n", dev->name,
                 adapter->current_dma.direction ? "tx" : "rx",
                 inb_status(dev->base_addr));

      outb_control (adapter->hcr_val & ~(DMAE | TCEN | DIR), dev);
      if (adapter->current_dma.direction)
          adapter->current_dma.data_len = 0;

      else if (adapter->current_dma.data_len)
      {
        if (adapter->current_dma.target)
           memcpy (adapter->current_dma.target,
                   adapter->dma_buffer,
                   adapter->current_dma.length);

        if (dev->get_rx_buf)
        {
          int   len = adapter->current_dma.data_len;
          char *buf = (*dev->get_rx_buf) (len);

          if (buf)
              memcpy (buf, adapter->current_dma.data, len);
        }
        adapter->current_dma.data_len = 0;
      }

      adapter->dmaing = 0;
      if (adapter->rx_backlog.in != adapter->rx_backlog.out)
      {
        int t = adapter->rx_backlog.length [adapter->rx_backlog.out];
        adapter->rx_backlog.out = backlog_next(adapter->rx_backlog.out);
        if (elp_debug >= 2)
           printk ("%s: receiving backlogged packet (%d)\n", dev->name, t);
        receive_packet (dev, t);
      }
      else
        adapter->busy = 0;
    }
    else
    {
      /* has one timed out? */
      check_dma (dev);
    }
    STI();

    /*
     * receive a PCB from the adapter
     */
    timeout = jiffies + 3;
    while ((inb_status(dev->base_addr) & ACRF) != 0 && jiffies < timeout)
    {
      if (receive_pcb(dev, &adapter->irx_pcb))
      {
        switch (adapter->irx_pcb.command)
        {
          case 0:
               break;
              /*
               * received a packet - this must be handled fast
               */
          case 0xff:
          case CMD_RECEIVE_PACKET_COMPLETE:
              /* if the device isn't open, don't pass packets up the stack */
               if (dev->start == 0)
                  break;
               DISABLE();
               len  = adapter->irx_pcb.data.rcv_resp.pkt_len;
               dlen = adapter->irx_pcb.data.rcv_resp.buf_len;
               if (adapter->irx_pcb.data.rcv_resp.timeout != 0)
               {
                 printk ("%s: interrupt - packet not received correctly\n",
                         dev->name);
                 ENABLE();
               }
               else
               {
                 if (elp_debug >= 3)
                 {
                   ENABLE();
                   printk ("%s: interrupt - packet received of length %i (%i)\n",
                           dev->name, len, dlen);
                   DISABLE();
                 }
                 if (adapter->irx_pcb.command == 0xff)
                 {
                   if (elp_debug >= 2)
                      printk ("%s: adding packet to backlog (len = %d)\n",
                              dev->name, dlen);
                   adapter->rx_backlog.length [adapter->rx_backlog.in] = dlen;
                   adapter->rx_backlog.in = backlog_next (adapter->rx_backlog.in);
                 }
                 else
                   receive_packet (dev,dlen);
                 ENABLE();
                 if (elp_debug >= 3)
                    printk ("%s: packet received\n", dev->name);
               }
               break;

          /*
           * 82586 configured correctly
           */
          case CMD_CONFIGURE_82586_RESPONSE:
               adapter->got[CMD_CONFIGURE_82586] = 1;
               if (elp_debug >= 3)
                  printk ("%s: interrupt - configure response received\n",
                          dev->name);
               break;

            /*
             * Adapter memory configuration
             */
          case CMD_CONFIGURE_ADAPTER_RESPONSE:
               adapter->got[CMD_CONFIGURE_ADAPTER_MEMORY] = 1;
               if (elp_debug >= 3)
                  printk ("%s: Adapter memory configuration %s.\n", dev->name,
                          adapter->irx_pcb.data.failed ? "failed" : "succeeded");
               break;

            /*
             * Multicast list loading
             */
          case CMD_LOAD_MULTICAST_RESPONSE:
               adapter->got[CMD_LOAD_MULTICAST_LIST] = 1;
               if (elp_debug >= 3)
                  printk ("%s: Multicast address list loading %s.\n", dev->name,
                          adapter->irx_pcb.data.failed ? "failed" : "succeeded");
               break;

            /*
             * Station address setting
             */
          case CMD_SET_ADDRESS_RESPONSE:
               adapter->got[CMD_SET_STATION_ADDRESS] = 1;
               if (elp_debug >= 3)
                  printk ("%s: Ethernet address setting %s.\n", dev->name,
                          adapter->irx_pcb.data.failed ? "failed" : "succeeded");
               break;

            /*
             * received board statistics
             */
          case CMD_NETWORK_STATISTICS_RESPONSE:
               adapter->stats.rx_packets += adapter->irx_pcb.data.netstat.tot_recv;
               adapter->stats.tx_packets += adapter->irx_pcb.data.netstat.tot_xmit;
               adapter->stats.rx_crc_errors   += adapter->irx_pcb.data.netstat.err_CRC;
               adapter->stats.rx_frame_errors += adapter->irx_pcb.data.netstat.err_align;
               adapter->stats.rx_fifo_errors  += adapter->irx_pcb.data.netstat.err_ovrrun;
               adapter->stats.rx_over_errors  += adapter->irx_pcb.data.netstat.err_res;
               adapter->got[CMD_NETWORK_STATISTICS] = 1;
               if (elp_debug >= 3)
                  printk ("%s: interrupt - statistics response received\n",
                          dev->name);
               break;

            /*
             * sent a packet
             */
          case CMD_TRANSMIT_PACKET_COMPLETE:
               if (elp_debug >= 3)
                  printk ("%s: interrupt - packet sent\n", dev->name);
               if (dev->start == 0)
                  break;
               switch (adapter->irx_pcb.data.xmit_resp.c_stat)
               {
                 case 0xFFFF:
                      adapter->stats.tx_aborted_errors++;
                      printk ("%s: transmit timed out, network cable problem?\n",
                              dev->name);
                      break;
                 case 0xFFFE:
                      adapter->stats.tx_fifo_errors++;
                      printk ("%s: transmit timed out, FIFO underrun\n",
                              dev->name);
                      break;
               }
               dev->tx_busy = 0;
               break;

            /*
             * some unknown PCB
             */
          default:
               printk ("%s: unknown PCB received - %2.2x\n",
                       dev->name, adapter->irx_pcb.command);
               break;
        }
      }
      else
      {
        printk ("%s: failed to read PCB on interrupt\n", dev->name);
        adapter_reset (dev);
      }
    }
  }
  while (icount++ < 5 && (inb_status(dev->base_addr) & (ACRF | DONE)));

  prime_rx (dev);
  dev->reentry = 0;
}


/*
 * open the board
 */
static int elp_open (struct device *dev)
{
  elp_device *adapter = dev->priv;

  if (elp_debug >= 3)
     printk ("%s: request to open device\n", dev->name);

  /*
   * disable interrupts on the board
   */
  outb_control (0, dev);

  /*
   * clear any pending interrupts
   */
  inb_command (dev->base_addr);
  adapter_reset (dev);

  dev->reentry = 0;
  dev->tx_busy = 0;

  adapter->rx_active      = 0;
  adapter->busy           = 0;
  adapter->rx_backlog.in  = 0;
  adapter->rx_backlog.out = 0;

  /* make sure we can find the device header given the interrupt number
   */
  irq2dev_map[dev->irq] = dev;

  /* install our interrupt service routine
   */
  irq2dev_map[dev->irq] = dev;
  if (!request_irq(dev->irq,&elp_interrupt))
  {
    irq2dev_map[dev->irq] = NULL;
    return (0);
  }

  adapter->dma_buffer = dma_mem_alloc (DMA_BUFFER_SIZE, &adapter->dma_selector);
  if (!adapter->dma_buffer)
  {
    printk ("%s: Could not allocate DMA buffer\n", dev->name);
    return (0);
  }
  adapter->dmaing = 0;

  /* enable interrupts on the board
   */
  outb_control (CMDE, dev);

  /* configure adapter memory: we need 10 multicast addresses, default==0
   */
  if (elp_debug >= 3)
    printk ("%s: sending 3c505 memory configuration command\n", dev->name);
  adapter->tx_pcb.command = CMD_CONFIGURE_ADAPTER_MEMORY;
  adapter->tx_pcb.data.memconf.cmd_q = 10;
  adapter->tx_pcb.data.memconf.rcv_q = 20;
  adapter->tx_pcb.data.memconf.mcast = 10;
  adapter->tx_pcb.data.memconf.frame = 20;
  adapter->tx_pcb.data.memconf.rcv_b = 20;
  adapter->tx_pcb.data.memconf.progs = 0;
  adapter->tx_pcb.length = sizeof(struct Memconf);
  adapter->got[CMD_CONFIGURE_ADAPTER_MEMORY] = 0;
  if (!send_pcb(dev, &adapter->tx_pcb))
    printk ("%s: couldn't send memory configuration command\n", dev->name);
  else
  {
    int timeout = jiffies + TIMEOUT;
    while (adapter->got[CMD_CONFIGURE_ADAPTER_MEMORY] == 0 &&
           jiffies < timeout)
          ;
    if (jiffies >= timeout)
       TIMEOUT_MSG (__LINE__);
  }

  /* configure adapter to receive broadcast messages and wait for response
   */
  if (elp_debug >= 3)
     printk ("%s: sending 82586 configure command\n", dev->name);

  adapter->tx_pcb.command           = CMD_CONFIGURE_82586;
  adapter->tx_pcb.data.configure    = NO_LOOPBACK | RECV_BROAD;
  adapter->tx_pcb.length            = 2;
  adapter->got[CMD_CONFIGURE_82586] = 0;
  if (!send_pcb(dev, &adapter->tx_pcb))
     printk ("%s: couldn't send 82586 configure command\n", dev->name);
  else
  {
    int timeout = jiffies + TIMEOUT;
    while (adapter->got[CMD_CONFIGURE_82586] == 0 &&
           jiffies < timeout)
          ;
    if (jiffies >= timeout)
       TIMEOUT_MSG (__LINE__);
  }

  /* enable burst-mode DMA */
  /* outb (1, dev->base_addr + PORT_AUXDMA); */

  /* queue receive commands to provide buffering
   */
  prime_rx (dev);
  if (elp_debug >= 3)
     printk ("%s: %lu receive PCBs active\n", dev->name, adapter->rx_active);

  /* device is now officially open!
   */
  dev->start = 1;
  return (1);
}


/*
 * send a packet to the adapter
 */
static int send_packet (struct device *dev, const void *buf, int len)
{
  elp_device *adapter = dev->priv;
  char       *target;

  /* make sure the length is even and no shorter than 60 bytes
   */
  len = (((len < ETH_MIN) ? ETH_MIN : len) + 1) & ~1;

  if (adapter->busy)
  {
    if (elp_debug >= 2)
       printk ("%s: transmit blocked\n", dev->name);
    return (0);
  }
  adapter->busy = 1;
  adapter->stats.tx_bytes += len;
  
  /* send the adapter a transmit packet command. Ignore segment and offset
   * and make sure the length is even
   */
  adapter->tx_pcb.command = CMD_TRANSMIT_PACKET;
  adapter->tx_pcb.length  = sizeof(struct Xmit_pkt);
  adapter->tx_pcb.data.xmit_pkt.buf_ofs = 0;
  adapter->tx_pcb.data.xmit_pkt.buf_seg = 0;
  adapter->tx_pcb.data.xmit_pkt.pkt_len = len;

  if (!send_pcb(dev, &adapter->tx_pcb))
  {
    adapter->busy = 0;
    return (0);
  }

  /* if this happens, we die
   */
  if (adapter->dmaing)
     printk ("%s: tx: DMA %lu in progress\n",
             dev->name, adapter->current_dma.direction);

  adapter->dmaing                 = 1;
  adapter->current_dma.direction  = 1;
  adapter->current_dma.start_time = jiffies;

  target = (char*) VIRT_TO_PHYS ((char*)buf);
  if ((DWORD)target + len >= MAX_DMA_ADDR)
  {
    movedata ((unsigned)_my_ds(), (unsigned)buf,
              adapter->dma_selector, (unsigned)adapter->dma_buffer,
              len);
    target = (char*)adapter->dma_buffer;
  }
  adapter->current_dma.data     = (char*)buf;
  adapter->current_dma.data_len = len;

  DISABLE();
  disable_dma (dev->dma);
  clear_dma_ff (dev->dma);
  set_dma_mode (dev->dma, 0x48);  /* dma memory -> io */
  set_dma_addr (dev->dma, (DWORD)target);
  set_dma_count (dev->dma, len);
  outb_control (adapter->hcr_val | DMAE | TCEN, dev);
  enable_dma (dev->dma);
  if (elp_debug >= 3)
     printk ("%s: DMA transfer started\n", dev->name);

  return (1);
}


/*
 * start the transmitter
 *    return 0 if sent OK, else return 1
 */
static int elp_start_xmit (struct device *dev, const void *buf, int len)
{
  if (dev->reentry)
  {
    printk ("%s: start_xmit aborted (in irq)\n", dev->name);
    return (0);
  }
  check_dma (dev);

  /* if the transmitter is still busy, we have a transmit timeout...
   */
  if (dev->tx_busy)
  {
    elp_device *adapter    = dev->priv;
    int         tickssofar = jiffies - dev->tx_start;
    int         stat;

    if (tickssofar < 1000)
       return (0);

    stat = inb_status (dev->base_addr);
    printk ("%s: transmit timed out, lost %s?\n", dev->name,
            (stat & ACRF) ? "interrupt" : "command");
    if (elp_debug >= 1)
       printk ("%s: status %02x\n", dev->name, stat);
    dev->tx_start = jiffies;
    adapter->stats.tx_dropped++;
  }
  dev->tx_busy = 1;

  if (!send_packet(dev,buf,len))
  {
    if (elp_debug >= 2)
       printk ("%s: failed to transmit packet\n", dev->name);
    dev->tx_busy = 0;
    return (0);
  }
  if (elp_debug >= 3)
     printk ("%s: packet of length %d sent\n", dev->name,len);

  /* start the transmit timeout
   */
  dev->tx_start = jiffies;
  prime_rx (dev);
  return (1);
}

/*
 * return statistics from the board
 */
static void *elp_get_stats(struct device *dev)
{
  elp_device *adapter = (elp_device *) dev->priv;

  if (elp_debug >= 3)
     printk ("%s: request for stats\n", dev->name);

  /* If the device is closed, just return the latest stats we have,
   * - we cannot ask from the adapter without interrupts
   */
  if (!dev->start)
     return (void*)&adapter->stats;

  /* send a get statistics command to the board */
  adapter->tx_pcb.command = CMD_NETWORK_STATISTICS;
  adapter->tx_pcb.length  = 0;
  adapter->got[CMD_NETWORK_STATISTICS] = 0;
  if (!send_pcb(dev, &adapter->tx_pcb))
     printk ("%s: couldn't send get statistics command\n", dev->name);
  else
  {
    int timeout = jiffies + TIMEOUT;
    while (adapter->got[CMD_NETWORK_STATISTICS] == 0 && jiffies < timeout);
    if (jiffies >= timeout)
    {
      TIMEOUT_MSG(__LINE__);
      return (void*)&adapter->stats;
    }
  }

  /* statistics are now up to date
   */
  return (void*)&adapter->stats;
}

/*
 * close the board
 */
static void elp_close (struct device *dev)
{
  elp_device *adapter = dev->priv;

  if (elp_debug >= 3)
     printk ("%s: request to close device\n", dev->name);

  /* Someone may request the device statistic information even when
   * the interface is closed. The following will update the statistics
   * structure in the driver, so we'll be able to give current statistics.
   */
  elp_get_stats (dev);

  /* disable interrupts on the board
   */
  outb_control (0,dev);

  /* flag transmitter as busy (i.e. not available)
   */
  dev->tx_busy = 1;

  /* indicate device is closed
   */
  dev->start = 0;

  /* release the IRQ
   */
  free_irq (dev->irq);
  irq2dev_map[dev->irq] = NULL;

  _dma_stop (dev->dma);
  __dpmi_free_dos_memory (adapter->dma_selector);
}


/*
 * Set multicast list
 *   num_addrs ==0 : clear mc_list
 *   num_addrs ==-1: set promiscuous mode
 *   num_addrs > 0 : set mc_list
 */
static void elp_set_mc_list (struct device *dev)
{
  elp_device *adapter = (elp_device *) dev->priv;
  int i;

  if (elp_debug >= 3)
     printk ("%s: request to set multicast list\n", dev->name);

  if (!(dev->flags & (IFF_PROMISC | IFF_ALLMULTI)))
  {
    /* send a "load multicast list" command to the board, max 10 addrs/cmd
     * if num_addrs==0 the list will be cleared
     */
    adapter->tx_pcb.command = CMD_LOAD_MULTICAST_LIST;
    adapter->tx_pcb.length  = ETH_ALEN * dev->mc_count;
    for (i = 0; i < dev->mc_count; i++)
        memcpy (adapter->tx_pcb.data.multicast[i], dev->mc_list[i], ETH_ALEN);

    adapter->got[CMD_LOAD_MULTICAST_LIST] = 0;
    if (!send_pcb(dev, &adapter->tx_pcb))
       printk ("%s: couldn't send set_multicast command\n", dev->name);
    else
    {
      int timeout = jiffies + TIMEOUT;
      while (adapter->got[CMD_LOAD_MULTICAST_LIST] == 0 && jiffies < timeout)
            ;
      if (jiffies >= timeout)
         TIMEOUT_MSG (__LINE__);
    }
    if (dev->mc_count)
         adapter->tx_pcb.data.configure = NO_LOOPBACK | RECV_BROAD | RECV_MULTI;
    else adapter->tx_pcb.data.configure = NO_LOOPBACK | RECV_BROAD;
  }
  else
    adapter->tx_pcb.data.configure = NO_LOOPBACK | RECV_PROMISC;

  /* configure adapter to receive messages (as specified above)
   * and wait for response
   */
  if (elp_debug >= 3)
     printk ("%s: sending 82586 configure command\n", dev->name);
  adapter->tx_pcb.command = CMD_CONFIGURE_82586;
  adapter->tx_pcb.length  = 2;
  adapter->got[CMD_CONFIGURE_82586] = 0;
  if (!send_pcb(dev, &adapter->tx_pcb))
     printk ("%s: couldn't send 82586 configure command\n", dev->name);
  else
  {
    int timeout = jiffies + TIMEOUT;
    while (adapter->got[CMD_CONFIGURE_82586] == 0 && jiffies < timeout)
           ;
    if (jiffies >= timeout)
       TIMEOUT_MSG(__LINE__);
  }
}

/*
 * initialise Etherlink Plus board
 */
static void elp_init (struct device *dev)
{
  elp_device *adapter = dev->priv;

  /* set ptrs to various functions
   */
  dev->open      = elp_open;
  dev->close     = elp_close;
  dev->get_stats = elp_get_stats;
  dev->xmit      = elp_start_xmit;
  dev->set_multicast_list = elp_set_mc_list;

  /* setup ptr to adapter specific information
   */
  memset (&adapter->stats,0,sizeof(struct net_device_stats));
  dev->mem_start = dev->mem_end = dev->rmem_end = dev->rmem_start = 0;
}

/*
 * A couple of tests to see if there's 3C505 or not
 * Called only by elp_autodetect
 */
static int elp_sense (struct device *dev)
{
  int   timeout;
  int   addr = dev->base_addr;
  char *name = dev->name;
  BYTE  orig_HSR;

  orig_HSR = inb_status (addr);

  if (elp_debug > 0)
     printk (search_msg, name, addr);

  if (orig_HSR == 0xff)
  {
    if (elp_debug > 0)
       printk (notfound_msg, 1);
    return (-1);
  }

  /* Wait for a while; the adapter may still be booting up
   */
  if (elp_debug > 0)
     printk (stilllooking_msg);

  if (orig_HSR & DIR)
  {
    /* If HCR.DIR is up, we pull it down. HSR.DIR should follow.
     */
    outb (0, dev->base_addr + PORT_CONTROL);
    timeout = jiffies + 30;
    while (jiffies < timeout)
          ;
    if (inb_status(addr) & DIR)
    {
      if (elp_debug > 0)
         printk (notfound_msg, 2);
      return (-1);
    }
  }
  else
  {
    /* If HCR.DIR is down, we pull it up. HSR.DIR should follow.
     */
    outb (DIR, dev->base_addr + PORT_CONTROL);
    timeout = jiffies + 30;
    while (jiffies < timeout) ;
    if (!(inb_status(addr) & DIR))
    {
      if (elp_debug > 0)
         printk (notfound_msg,3);
      return (-1);
    }
  }
  /* It certainly looks like a 3c505.
   */
  if (elp_debug > 0)
     printk (found_msg);
  return (0);
}

/*
 * Search through addr_list[] and try to find a 3C505
 * Called only by eplus_probe
 */
static int elp_autodetect (struct device *dev)
{
  int idx = 0;

  /* if base address set, then only check that address
   * otherwise, run through the table
   */
  if (dev->base_addr)  /* dev->base_addr == 0 ==> plain autodetect */
  {
    if (elp_sense(dev) == 0)
       return (dev->base_addr);
  }
  else
    while ((dev->base_addr = addr_list[idx++]))
    {
      if (elp_sense(dev) == 0)
         return (dev->base_addr);
    }

  /* could not find an adapter
   */
  if (elp_debug > 0)
     printk (couldnot_msg, dev->name);

  return (0);
}


/*
 * Probe for an Etherlink Plus board at the specified address
 *
 * There are three situations we need to be able to detect here:
 *
 *  a) the card is idle
 *  b) the card is still booting up
 *  c) the card is stuck in a strange state (some DOS drivers do this)
 *
 * In case (a), all is well.  In case (b), we wait 10 seconds to see if the
 * card finishes booting, and carry on if so.  In case (c), we do a hard reset,
 * loop round, and hope for the best.
 *
 * This is all very unpleasant, but hopefully avoids the problems with the old
 * probe code (which had a 15-second delay if the card was idle, and didn't
 * work at all if it was in a weird state).
 */
int elplus_probe (struct device *dev)
{
  elp_device *adapter;
  int i, tries, tries1, timeout, okay;

  /* setup adapter structure
   */
  dev->base_addr = elp_autodetect (dev);
  if (!dev->base_addr)
     return (0);

  /* setup ptr to adapter specific information
   */
  adapter = (elp_device*) calloc (sizeof(elp_device),1);
  dev->priv = adapter;
  if (!adapter)
  {
    printk ("%s: out of memory\n", dev->name);
    return (0);
  }

  for (tries1 = 0; tries1 < 3; tries1++)
  {
    outb_control ((adapter->hcr_val | CMDE) & ~DIR, dev);

    /* First try to write just one byte, to see if the card is
     * responding at all normally.
     */
    timeout = jiffies + 5;
    okay = 0;
    while (jiffies < timeout && !(inb_status(dev->base_addr) & HCRE))
           ;
    if ((inb_status(dev->base_addr) & HCRE))
    {
      outb_command (0, dev->base_addr);  /* send a spurious byte */
      timeout = jiffies + 5;
      while (jiffies < timeout && !(inb_status(dev->base_addr) & HCRE))
             ;
      if (inb_status(dev->base_addr) & HCRE)
         okay = 1;
    }
    if (!okay)
    {
      /* Nope, it's ignoring the command register.  This means that
       * either it's still booting up, or it's died.
       */
      printk ("%s: command register wouldn't drain, ", dev->name);
      if ((inb_status(dev->base_addr) & 7) == 3)
      {
        /* If the adapter status is 3, it *could* still be booting.
         * Give it the benefit of the doubt for 10 seconds.
         */
        printk ("assuming 3c505 still starting\n");
        timeout = jiffies + 10 * HZ;
        while (jiffies < timeout && (inb_status(dev->base_addr) & 7))
               ;
        if (inb_status(dev->base_addr) & 7)
             printk("%s: 3c505 failed to start\n", dev->name);
        else okay = 1;  /* It started */
      }
      else
      {
        /* Otherwise, it must just be in a strange
         * state.  We probably need to kick it.
         */
        printk ("3c505 is sulking\n");
      }
    }

    for (tries = 0; tries < 5 && okay; tries++)
    {
      /* Try to set the Ethernet address, to make sure that the board
       * is working.
       */
      adapter->tx_pcb.command = CMD_STATION_ADDRESS;
      adapter->tx_pcb.length  = 0;
      autoirq_setup(0);
      if (!send_pcb(dev, &adapter->tx_pcb))
      {
        printk ("%s: could not send first PCB\n", dev->name);
        autoirq_report (0);
        continue;
      }
      if (!receive_pcb(dev, &adapter->rx_pcb))
      {
        printk ("%s: could not read first PCB\n", dev->name);
        autoirq_report (0);
        continue;
      }
      if ((adapter->rx_pcb.command != CMD_ADDRESS_RESPONSE) ||
          (adapter->rx_pcb.length != 6))
      {
        printk ("%s: first PCB wrong (%d, %d)\n", dev->name,
                adapter->rx_pcb.command, adapter->rx_pcb.length);
        autoirq_report (0);
        continue;
      }
      goto okay;
    }

    /* It's broken.  Do a hard reset to re-initialise the board,
     * and try again.
     */
    printk ("%s: resetting adapter\n", dev->name);
    outb_control (adapter->hcr_val | FLSH | ATTN, dev);
    outb_control (adapter->hcr_val & ~(FLSH | ATTN), dev);
  }
  printk ("%s: failed to initialise 3c505\n", dev->name);
  return (0);

okay:
  if (dev->irq)     /* Is there a preset IRQ? */
  {
    int rpt = autoirq_report (0);
    if (dev->irq != rpt)
       printk ("%s: warning, irq %d configured but %d detected\n",
               dev->name, dev->irq, rpt);

    /* if dev->irq == autoirq_report(0), all is well
     */
  }
  else           /* No preset IRQ; just use what we can detect */
    dev->irq = autoirq_report(0);

  switch (dev->irq)     /* Legal, sane? */
  {
    case 0:
         printk ("%s: IRQ probe failed: check 3c505 jumpers.\n", dev->name);
         return (0);
    case 1:
    case 6:
    case 8:
    case 13:
         printk ("%s: Impossible IRQ %d reported by autoirq_report().\n",
                 dev->name, dev->irq);
         return (0);
  }

  /* Now we have the IRQ number so we can disable the interrupts from
   * the board until the board is opened.
   */
  outb_control (adapter->hcr_val & ~CMDE, dev);

  /* copy ethernet address into structure
   */
  for (i = 0; i < ETH_ALEN; i++)
      dev->dev_addr[i] = adapter->rx_pcb.data.eaddr[i];

  /* find a DMA channel
   */
  if (!dev->dma)
  {
    if (dev->mem_start)
    {
      dev->dma = dev->mem_start & 7;
    }
    else
    {
      printk ("%s: warning, DMA channel not specified, using default\n",
              dev->name);
      dev->dma = ELP_DMA;
    }
  }

  /* print remainder of startup message
   */
  printk ("%s: 3c505 at %lx, irq %d, dma %d, ",
          dev->name, dev->base_addr, dev->irq, dev->dma);
  printk ("addr %02x:%02x:%02x:%02x:%02x:%02x, ",
          dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
          dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

  /* read more information from the adapter
   */
  adapter->tx_pcb.command = CMD_ADAPTER_INFO;
  adapter->tx_pcb.length  = 0;
  if (!send_pcb(dev, &adapter->tx_pcb)    ||
      !receive_pcb(dev, &adapter->rx_pcb) ||
      (adapter->rx_pcb.command != CMD_ADAPTER_INFO_RESPONSE) ||
      (adapter->rx_pcb.length != 10)) {
    printk ("not responding to second PCB\n");
  }
  printk ("rev %d.%d, %dk\n",
          adapter->rx_pcb.data.info.major_vers,
          adapter->rx_pcb.data.info.minor_vers,
          adapter->rx_pcb.data.info.RAM_sz);

  /* reconfigure the adapter memory to better suit our purposes
   */
  adapter->tx_pcb.command            = CMD_CONFIGURE_ADAPTER_MEMORY;
  adapter->tx_pcb.length             = 12;
  adapter->tx_pcb.data.memconf.cmd_q = 8;
  adapter->tx_pcb.data.memconf.rcv_q = 8;
  adapter->tx_pcb.data.memconf.mcast = 10;
  adapter->tx_pcb.data.memconf.frame = 10;
  adapter->tx_pcb.data.memconf.rcv_b = 10;
  adapter->tx_pcb.data.memconf.progs = 0;
  if (!send_pcb(dev, &adapter->tx_pcb)    ||
      !receive_pcb(dev, &adapter->rx_pcb) ||
      (adapter->rx_pcb.command != CMD_CONFIGURE_ADAPTER_RESPONSE) ||
      (adapter->rx_pcb.length != 2))
     printk ("%s: could not configure adapter memory\n", dev->name);

  if (adapter->rx_pcb.data.configure)
     printk ("%s: adapter configuration failed\n", dev->name);

  /* initialise the device
   */
  elp_init (dev);
  return (1);
} 


#ifdef __DLX__

#define SYSTEM_ID   ASCII ('_','W','A','T','T','3','2','_')
#define DRIVER_ID   ASCII ('3','C','5','0','5', 0,0,0)
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

