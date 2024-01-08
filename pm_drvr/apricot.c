/* apricot.c: An Apricot 82596 ethernet driver for linux.
 *
 * Apricot
 * Written 1994 by Mark Evans.
 * This driver is for the Apricot 82596 bus-master interface
 * 
 * Modularised 12/94 Mark Evans
 * 
 * Driver skeleton
 * Written 1993 by Donald Becker.
 * Copyright 1993 United States Government as represented by the Director,
 * National Security Agency.  This software may only be used and distributed
 * according to the terms of the GNU Public License as modified by SRC,
 * incorporated herein by reference.
 * 
 * The author may be reached as becker@super.org or
 * C/O Supercomputing Research Ctr., 17100 Science Dr., Bowie MD 20715
 */

static const char *version = "apricot.c:v0.2 05/12/94\n";

#include "pmdrvr.h"

#ifndef APRICOT_DEBUG
#define APRICOT_DEBUG 1
#endif
int i596_debug = APRICOT_DEBUG;

#define I596_NULL -1

#define CMD_EOL   0x8000        /* The last command of the list, stop. */
#define CMD_SUSP  0x4000	/* Suspend after doing cmd. */
#define CMD_INTR  0x2000	/* Interrupt after doing cmd. */
#define CMD_FLEX  0x0008        /* Enable flexible memory model */

enum commands {
     CmdNOp,
     CmdSASetup,
     CmdConfigure,
     CmdMulticastList,
     CmdTx,
     CmdTDR,
     CmdDump,
     CmdDiagnose
   };

#define STAT_C      0x8000      /* Set to 0 after execution */
#define STAT_B      0x4000      /* Command being executed */
#define STAT_OK     0x2000      /* Command executed ok */
#define STAT_A      0x1000      /* Command aborted */

#define CUC_START   0x0100
#define CUC_RESUME  0x0200
#define CUC_SUSPEND 0x0300
#define CUC_ABORT   0x0400
#define RX_START    0x0010
#define RX_RESUME   0x0020
#define RX_SUSPEND  0x0030
#define RX_ABORT    0x0040

struct i596_cmd {
       WORD   status;
       WORD   command;
       struct i596_cmd *next;
     };

#undef  EOF
#define EOF        0x8000
#define SIZE_MASK  0x3fff

struct i596_tbd {
       WORD   size;
       WORD   pad;
       struct i596_tbd *next;
       char  *data;
     };

struct tx_cmd {
       struct i596_cmd cmd;
       struct i596_tbd *tbd;
       WORD   size;
       WORD   pad;
     };

struct i596_rfd {
       WORD   stat;
       WORD   cmd;
       struct i596_rfd *next;
       long   rbd;
       WORD   count;
       WORD   size;
       char   data[1532];
     };

#define RX_RING_SIZE 8

struct i596_scb {
       WORD   status;
       WORD   command;
       struct i596_cmd *cmd;
       struct i596_rfd *rfd;
       DWORD  crc_err;
       DWORD  align_err;
       DWORD  resource_err;
       DWORD  over_err;
       DWORD  rcvdt_err;
       DWORD  short_err;
       WORD   t_on;
       WORD   t_off;
     };

struct i596_iscp {
       DWORD  stat;
       struct i596_scb *scb;
     };

struct i596_scp {
       DWORD  sysbus;
       DWORD  pad;
       struct i596_iscp *iscp;
     };

struct i596_private {
       struct i596_scp  scp;
       struct i596_iscp iscp;
       struct i596_scb  scb;
       struct i596_cmd  set_add;
       char   eth_addr[8];
       struct i596_cmd  set_conf;
       char   i596_config[16];
       struct i596_cmd  tdr;
       DWORD            stat;
       int              last_restart;
       struct i596_rfd *rx_tail;
       struct i596_cmd *cmd_tail;
       struct i596_cmd *cmd_head;
       int              cmd_backlog;
       DWORD            last_cmd;
       struct net_device_stats stats;
     };

static char init_setup[] = {
            0x8E,     /* length, prefetch on */
            0xC8,     /* fifo to 8, monitor off */
            0x80,     /* don't save bad frames */
            0x2E,     /* No source address insertion, 8 byte preamble */
            0x00,     /* priority and backoff defaults */
            0x60,     /* interframe spacing */
            0x00,     /* slot time LSB */
            0xf2,     /* slot time and retries */
            0x00,     /* promiscuous mode */
            0x00,     /* collision detect */
            0x40,     /* minimum frame length */
            0xff,
            0x00,
            0x7f      /* multi IA */
          };

static int   i596_open (struct device *dev);
static int   i596_start_xmit (struct device *dev, const void *buf, int len);
static void  i596_interrupt (int irq);
static void  i596_close (struct device *dev);
static void *i596_get_stats (struct device *dev);
static void  i596_add_cmd (struct device *dev, struct i596_cmd *cmd);
static void  print_eth (char *);
static void  set_multicast_list (struct device *dev);

static __inline int init_rx_bufs (struct device *dev, int num)
{
  struct i596_private *lp = (struct i596_private*) dev->priv;
  struct i596_rfd     *rfd;
  int    i;

  lp->scb.rfd = (struct i596_rfd*) I596_NULL;

  if (i596_debug > 1)
     printk ("%s: init_rx_bufs %d.\n", dev->name, num);

  for (i = 0; i < num; i++)
  {
    rfd = k_calloc (sizeof(*rfd), 1);
    if (!rfd)
       break;

    rfd->stat  = 0x0000;
    rfd->rbd   = I596_NULL;
    rfd->count = 0;
    rfd->size  = 1532;
    if (i == 0)
    {
      rfd->cmd = CMD_EOL;
      lp->rx_tail = rfd;
    }
    else
      rfd->cmd = 0x0000;

    rfd->next = lp->scb.rfd;
    lp->scb.rfd = rfd;
  }

  if (i)
     lp->rx_tail->next = lp->scb.rfd;
  return (i);
}

static __inline void remove_rx_bufs (struct device *dev)
{
  struct i596_private *lp  = (struct i596_private*) dev->priv;
  struct i596_rfd     *rfd = lp->scb.rfd;

  lp->rx_tail->next = (struct i596_rfd*) I596_NULL;

  do
  {
    lp->scb.rfd = rfd->next;
    k_free (rfd);
    rfd = lp->scb.rfd;
  }
  while (rfd != lp->rx_tail);
}

static __inline void init_i596_mem (struct device *dev)
{
  struct i596_private *lp = (struct i596_private*) dev->priv;
  short  ioaddr   = dev->base_addr;
  int    boguscnt = 100;

  /* change the scp address
   */
  outw (0, ioaddr);
  outw (0, ioaddr);
  outb (4, ioaddr + 0xf);
  outw (((((int) &lp->scp) & 0xffff) | 2), ioaddr);
  outw ((((int) &lp->scp) >> 16) & 0xffff, ioaddr);

  lp->last_cmd    = jiffies;
  lp->scp.sysbus  = 0x00440000;
  lp->scp.iscp    = &lp->iscp;
  lp->iscp.scb    = &lp->scb;
  lp->iscp.stat   = 0x0001;
  lp->cmd_backlog = 0;
  lp->cmd_head    = lp->scb.cmd = (struct i596_cmd*) I596_NULL;

  if (i596_debug > 2)
     printk ("%s: starting i82596.\n", dev->name);

  inb (ioaddr + 0x10);
  outb (4, ioaddr + 0xf);
  outw (0, ioaddr + 4);

  while (lp->iscp.stat)
    if (--boguscnt == 0)
    {
      printk ("%s: i82596 initialization timed out with status %4x, cmd %4x.\n",
	      dev->name, lp->scb.status, lp->scb.command);
      break;
    }

  lp->scb.command = 0;

  memcpy (lp->i596_config, init_setup, 14);
  lp->set_conf.command = CmdConfigure;
  i596_add_cmd (dev, &lp->set_conf);

  memcpy (lp->eth_addr, dev->dev_addr, 6);
  lp->set_add.command = CmdSASetup;
  i596_add_cmd (dev, &lp->set_add);

  lp->tdr.command = CmdTDR;
  i596_add_cmd (dev, &lp->tdr);

  boguscnt = 200;
  while (lp->scb.status || lp->scb.command)
    if (--boguscnt == 0)
    {
      printk ("%s: receive unit start timed out with status %4x, cmd %4x.\n",
	      dev->name, lp->scb.status, lp->scb.command);
      break;
    }

  lp->scb.command = RX_START;
  outw (0, ioaddr + 4);

  boguscnt = 200;
  while (lp->scb.status || lp->scb.command)
    if (--boguscnt == 0)
    {
      printk ("i82596 init timed out with status %4x, cmd %4x.\n",
              lp->scb.status, lp->scb.command);
      break;
    }
}

static __inline void i596_rx (struct device *dev)
{
  struct i596_private *lp = (struct i596_private*) dev->priv;
  int    frames = 0;

  if (i596_debug > 3)
     printk ("i596_rx()\n");

  while ((lp->scb.rfd->stat) & STAT_C)
  {
    if (i596_debug > 2)
       print_eth (lp->scb.rfd->data);

    if ((lp->scb.rfd->stat) & STAT_OK)
    {
      /* a good frame */
      int pkt_len = lp->scb.rfd->count & 0x3fff;

      frames++;
      lp->stats.rx_packets++;

      if (dev->get_rx_buf)
      {
        char *buf = (*dev->get_rx_buf) (pkt_len);
        if (buf)
             memcpy (buf, lp->scb.rfd->data, pkt_len);
        else lp->stats.rx_dropped++;
      }

      if (i596_debug > 4)
         print_eth (lp->scb.rfd->data);
    }
    else
    {
      lp->stats.rx_errors++;
      if ((lp->scb.rfd->stat) & 0x0001)
         lp->stats.tx_collisions++;
      if ((lp->scb.rfd->stat) & 0x0080)
         lp->stats.rx_length_errors++;
      if ((lp->scb.rfd->stat) & 0x0100)
         lp->stats.rx_over_errors++;
      if ((lp->scb.rfd->stat) & 0x0200)
         lp->stats.rx_fifo_errors++;
      if ((lp->scb.rfd->stat) & 0x0400)
         lp->stats.rx_frame_errors++;
      if ((lp->scb.rfd->stat) & 0x0800)
         lp->stats.rx_crc_errors++;
      if ((lp->scb.rfd->stat) & 0x1000)
         lp->stats.rx_length_errors++;
    }

    lp->scb.rfd->stat  = 0;
    lp->rx_tail->cmd   = 0;
    lp->rx_tail        = lp->scb.rfd;
    lp->scb.rfd        = lp->scb.rfd->next;
    lp->rx_tail->count = 0;
    lp->rx_tail->cmd  = CMD_EOL;
  }

  if (i596_debug > 3)
     printk ("frames %d\n", frames);
}

static __inline void i596_cleanup_cmd (struct i596_private *lp)
{
  struct i596_cmd *ptr;
  int    boguscnt = 100;

  if (i596_debug > 4)
     printk ("i596_cleanup_cmd\n");

  while (lp->cmd_head != (struct i596_cmd*) I596_NULL)
  {
    ptr = lp->cmd_head;

    lp->cmd_head = lp->cmd_head->next;
    lp->cmd_backlog--;

    switch (ptr->command & 7)
    {
      case CmdTx:
	   {
             struct tx_cmd  *tx_cmd = (struct tx_cmd*) ptr;

	     lp->stats.tx_errors++;
	     lp->stats.tx_aborted_errors++;

             ptr->next = (struct i596_cmd*) I596_NULL;
             k_free ((BYTE*)tx_cmd);
	     break;
	   }
      case CmdMulticastList:
	   {
             ptr->next = (struct i596_cmd*) I596_NULL;
             k_free ((void*)ptr);
	     break;
	   }
      default:
           ptr->next = (struct i596_cmd*) I596_NULL;
    }
  }

  while (lp->scb.status || lp->scb.command)
    if (--boguscnt == 0)
    {
      printk ("i596_cleanup_cmd timed out with status %4x, cmd %4x.\n",
              lp->scb.status, lp->scb.command);
      break;
    }

  lp->scb.cmd = lp->cmd_head;
}

static __inline void i596_reset (struct device *dev, struct i596_private *lp,
                                 int ioaddr)
{
  int boguscnt = 100;

  if (i596_debug > 4)
     printk ("i596_reset\n");

  while (lp->scb.status || lp->scb.command)
    if (--boguscnt == 0)
    {
      printk ("i596_reset timed out with status %4x, cmd %4x.\n",
              lp->scb.status, lp->scb.command);
      break;
    }

  dev->start   = 0;
  dev->tx_busy = 1;

  lp->scb.command = CUC_ABORT | RX_ABORT;
  outw (0, ioaddr + 4);

  /* wait for shutdown */
  boguscnt = 400;

  while (lp->scb.status || lp->scb.command)
    if (--boguscnt == 0)
    {
      printk ("i596_reset 2 timed out with status %4x, cmd %4x.\n",
              lp->scb.status, lp->scb.command);
      break;
    }

  i596_cleanup_cmd (lp);
  i596_rx (dev);

  dev->start   = 1;
  dev->tx_busy = 0;
  dev->reentry = 0;
  init_i596_mem (dev);
}

static void i596_add_cmd (struct device *dev, struct i596_cmd *cmd)
{
  struct i596_private *lp = (struct i596_private*) dev->priv;
  int    ioaddr = dev->base_addr;
  int    boguscnt = 100;

  if (i596_debug > 4)
     printk ("i596_add_cmd\n");

  cmd->status   = 0;
  cmd->command |= (CMD_EOL | CMD_INTR);
  cmd->next     = (struct i596_cmd*) I596_NULL;

  DISABLE();
  if (lp->cmd_head != (struct i596_cmd*) I596_NULL)
     lp->cmd_tail->next = cmd;
  else
  {
    lp->cmd_head = cmd;
    while (lp->scb.status || lp->scb.command)
      if (--boguscnt == 0)
      {
        printk ("i596_add_cmd timed out with status %4x, cmd %4x.\n",
                lp->scb.status, lp->scb.command);
	break;
      }

    lp->scb.cmd     = cmd;
    lp->scb.command = CUC_START;
    outw (0, ioaddr + 4);
  }
  lp->cmd_tail = cmd;
  lp->cmd_backlog++;

  lp->cmd_head = lp->scb.cmd;
  ENABLE();

  if (lp->cmd_backlog > 16)
  {
    int tickssofar = jiffies - lp->last_cmd;

    if (tickssofar < 25)
       return;

    printk ("%s: command unit timed out, status resetting.\n", dev->name);
    i596_reset (dev, lp, ioaddr);
  }
}

static int i596_open (struct device *dev)
{
  int i;

  if (i596_debug > 1)
     printk ("%s: i596_open() irq %d.\n", dev->name, dev->irq);

  irq2dev_map[dev->irq] = dev;
  if (!request_irq (dev->irq, i596_interrupt))
  {
    irq2dev_map[dev->irq] = NULL;
    return (0);
  }

  i = init_rx_bufs (dev, RX_RING_SIZE);
  i = init_rx_bufs (dev, RX_RING_SIZE);
  if (i < RX_RING_SIZE)
     printk ("%s: only able to allocate %d receive buffers\n", dev->name, i);

  if (i < 4)
  {
    free_irq (dev->irq);
    irq2dev_map[dev->irq] = NULL;
    return (0);
  }

  dev->tx_busy = 0;
  dev->reentry = 0;
  dev->start   = 1;

  /* Initialize the 82596 memory */
  init_i596_mem (dev);
  return (1);
}

static int i596_start_xmit (struct device *dev, const void *buf, int len)
{
  struct i596_private *lp = (struct i596_private*) dev->priv;
  struct tx_cmd       *tx_cmd;
  int    ioaddr = dev->base_addr;

  if (i596_debug > 2)
     printk ("%s: Apricot start xmit\n", dev->name);

  /* Transmitter timeout, serious problems.
   */
  if (dev->tx_busy)
  {
    int tickssofar = jiffies - dev->tx_start;

    if (tickssofar < 5)
       return (0);

    printk ("%s: transmit timed out, status resetting.\n", dev->name);
    lp->stats.tx_errors++;

    /* Try to restart the adapter
     */
    if (lp->last_restart == lp->stats.tx_packets)
    {
      if (i596_debug > 1)
         printk ("Resetting board.\n");

      /* Shutdown and restart */
      i596_reset (dev, lp, ioaddr);
    }
    else
    {
      /* Issue a channel attention signal
       */
      if (i596_debug > 1)
         printk ("Kicking board.\n");

      lp->scb.command = CUC_START | RX_START;
      outw (0, ioaddr + 4);
      lp->last_restart = lp->stats.tx_packets;
    }
    dev->tx_busy  = 0;
    dev->tx_start = jiffies;
  }

  /* shouldn't happen */
  if (len <= 0)
     return (0);

  if (i596_debug > 3)
     printk ("%s: i596_start_xmit() called\n", dev->name);

  if (dev->tx_busy)
     printk ("%s: Transmitter access conflict.\n", dev->name);
  else
  {
    short length = len < ETH_MIN ? ETH_MIN : len;

    dev->tx_start = jiffies;
    tx_cmd = k_calloc (sizeof(struct tx_cmd) + sizeof(struct i596_tbd), 1);

    if (!tx_cmd)
    {
      printk ("%s: i596_xmit Memory squeeze, dropping packet.\n", dev->name);
      lp->stats.tx_dropped++;
    }
    else
    {
      tx_cmd->tbd = (struct i596_tbd*) (tx_cmd + 1);
      tx_cmd->tbd->next   = (struct i596_tbd*) I596_NULL;
      tx_cmd->cmd.command = CMD_FLEX | CmdTx;

      tx_cmd->pad  = 0;
      tx_cmd->size = 0;
      tx_cmd->tbd->pad  = 0;
      tx_cmd->tbd->size = EOF | length;
      memcpy (tx_cmd->tbd->data, buf, length);

      if (i596_debug > 3)
         print_eth ((char*)buf);

      i596_add_cmd (dev, (struct i596_cmd*)tx_cmd);
      lp->stats.tx_packets++;
    }
  }
  dev->tx_busy = 0;
  return (1);
}


static void print_eth (char *add)
{
  int i;

  printk ("Dest  ");
  for (i = 0; i < 6; i++)
     printk (" %02X", add[i]);
  printk ("\n");

  printk ("Source");
  for (i = 0; i < 6; i++)
     printk (" %02X", add[i+6]);
  printk ("\n");
  printk ("type %02X%02X\n", add[12], add[13]);
}

int apricot_probe (struct device *dev)
{
  struct i596_private *lp;
  int    checksum = 0;
  int    ioaddr   = 0x300;
  int    i;
  char   eth_addr[6];

  /* this is easy the ethernet interface can only be at 0x300
   * first check nothing is already registered here
   */
  for (i = 0; i < 8; i++)
  {
    eth_addr[i] = inb (ioaddr + 8 + i);
    checksum += eth_addr[i];
  }

  /* checksum is a multiple of 0x100, got this wrong first time
   * some machines have 0x100, some 0x200. The DOS driver doesn't
   * even bother with the checksum
   */
  if (checksum % 0x100)
     return (0);

  /* Some other boards trip the checksum.. but then appear as ether
   * address 0. Trap these - AC
   */
  if (memcmp (eth_addr, "\x00\x00\x49", 3))
     return (1);

  dev->base_addr = ioaddr;
  ether_setup (dev);
  printk ("%s: Apricot 82596 at %3x,", dev->name, ioaddr);

  for (i = 0; i < 6; i++)
     printk (" %02X", dev->dev_addr[i] = eth_addr[i]);

  dev->base_addr = ioaddr;
  dev->irq       = 10;
  printk (" IRQ %d.\n", dev->irq);

  if (i596_debug > 0)
     printk (version);

  /* The APRICOT-specific entries in the device structure.
   */
  dev->open  = i596_open;
  dev->close = i596_close;
  dev->xmit  = i596_start_xmit;
  dev->get_stats          = i596_get_stats;
  dev->set_multicast_list = set_multicast_list;

  dev->mem_start = (DWORD) k_calloc (sizeof(struct i596_private) + 0x0F, 1);

  /* align for scp */
  dev->priv = (void*) ((dev->mem_start + 0xf) & 0xFFFFFFF0);

  lp = (struct i596_private*) dev->priv;
  lp->scb.command = 0;
  lp->scb.cmd = (struct i596_cmd*) I596_NULL;
  lp->scb.rfd = (struct i596_rfd*) I596_NULL;
  return (1);
}

static void i596_interrupt (int irq)
{
  struct device       *dev = irq2dev_map[irq];
  struct i596_private *lp;
  short  ioaddr;
  int    boguscnt = 200;
  WORD   status, ack_cmd = 0;

  if (!dev)
  {
    printk ("i596_interrupt(): irq %d for unknown device.\n", irq);
    return;
  }

  if (i596_debug > 3)
     printk ("%s: i596_interrupt(): irq %d\n", dev->name, irq);

  if (dev->reentry)
     printk ("%s: Re-entering the interrupt handler.\n", dev->name);

  dev->reentry = 1;
  ioaddr = dev->base_addr;

  lp = (struct i596_private*) dev->priv;

  while (lp->scb.status || lp->scb.command)
    if (--boguscnt == 0)
    {
      printk ("%s: i596 interrupt, timeout status %4x command %4x.\n",
              dev->name, lp->scb.status, lp->scb.command);
      break;
    }
  status = lp->scb.status;

  if (i596_debug > 4)
     printk ("%s: i596 interrupt, status %4x.\n", dev->name, status);

  ack_cmd = status & 0xf000;

  if ((status & 0x8000) || (status & 0x2000))
  {
    struct i596_cmd *ptr;

    if ((i596_debug > 4) && (status & 0x8000))
       printk ("%s: i596 interrupt completed command.\n", dev->name);
    if ((i596_debug > 4) && (status & 0x2000))
       printk ("%s: i596 interrupt command unit inactive %x.\n",
               dev->name, status & 0x0700);

    while ((lp->cmd_head != (struct i596_cmd*)I596_NULL) &&
           (lp->cmd_head->status & STAT_C))
    {
      ptr = lp->cmd_head;

      lp->cmd_head = lp->cmd_head->next;
      lp->cmd_backlog--;

      switch ((ptr->command) & 0x7)
      {
        case CmdTx:
	     {
               struct tx_cmd *tx_cmd = (struct tx_cmd*) ptr;

               if (!ptr->status & STAT_OK)
	       {
		 lp->stats.tx_errors++;
		 if ((ptr->status) & 0x0020)
                    lp->stats.tx_collisions++;
		 if (!((ptr->status) & 0x0040))
                    lp->stats.tx_heartbeat_errors++;
		 if ((ptr->status) & 0x0400)
                    lp->stats.tx_carrier_errors++;
		 if ((ptr->status) & 0x0800)
                    lp->stats.tx_collisions++;
		 if ((ptr->status) & 0x1000)
                    lp->stats.tx_aborted_errors++;
	       }

               ptr->next = (struct i596_cmd*) I596_NULL;
               k_free ((BYTE*)tx_cmd);
	       break;
	     }

        case CmdMulticastList:
	     {
               ptr->next = (struct i596_cmd*) I596_NULL;
               k_free ((BYTE*)ptr);
	       break;
	     }

        case CmdTDR:
	     {
               DWORD status = *((DWORD*)(ptr+1));

	       if (status & 0x8000)
	       {
		 if (i596_debug > 3)
                    printk ("%s: link ok.\n", dev->name);
	       }
	       else
	       {
		 if (status & 0x4000)
                    printk ("%s: Transceiver problem.\n", dev->name);
		 if (status & 0x2000)
                    printk ("%s: Termination problem.\n", dev->name);
		 if (status & 0x1000)
                    printk ("%s: Short circuit.\n", dev->name);
		 printk ("%s: Time %ld.\n", dev->name, status & 0x07ff);
	       }
	     }

        default:
             ptr->next = (struct i596_cmd*) I596_NULL;
	     lp->last_cmd = jiffies;
      }
    }

    ptr = lp->cmd_head;
    while ((ptr != (struct i596_cmd*)I596_NULL) && (ptr != lp->cmd_tail))
    {
      ptr->command &= 0x1fff;
      ptr = ptr->next;
    }

    if ((lp->cmd_head != (struct i596_cmd*)I596_NULL) && dev->start)
       ack_cmd |= CUC_START;
    lp->scb.cmd = lp->cmd_head;
  }

  if ((status & 0x1000) || (status & 0x4000))
  {
    if ((i596_debug > 4) && (status & 0x4000))
       printk ("%s: i596 interrupt received a frame.\n", dev->name);
    if ((i596_debug > 4) && (status & 0x1000))
       printk ("%s: i596 interrupt receive unit inactive %x.\n",
               dev->name, status & 0x0070);

    i596_rx (dev);

    if (dev->start)
       ack_cmd |= RX_START;
  }

#if 0
  /* acknowledge the interrupt
   */
   if ((lp->scb.cmd != (struct i596_cmd*)I596_NULL) && (dev->start))
       ack_cmd | = CUC_START;
#endif

  boguscnt = 100;
  while (lp->scb.status || lp->scb.command)
    if (--boguscnt == 0)
    {
      printk ("%s: i596 interrupt, timeout status %4x command %4x.\n",
              dev->name, lp->scb.status, lp->scb.command);
      break;
    }
  lp->scb.command = ack_cmd;

  inb (ioaddr + 0x10);
  outb (4, ioaddr + 0xf);
  outw (0, ioaddr + 4);

  if (i596_debug > 4)
     printk ("%s: exiting interrupt.\n", dev->name);

  dev->reentry = 0;
}

static void i596_close (struct device *dev)
{
  struct i596_private *lp = (struct i596_private*) dev->priv;
  int    ioaddr   = dev->base_addr;
  int    boguscnt = 200;

  dev->start   = 0;
  dev->tx_busy = 1;

  if (i596_debug > 1)
     printk ("%s: Shutting down ethercard, status was %4x.\n",
             dev->name, lp->scb.status);

  lp->scb.command = CUC_ABORT | RX_ABORT;
  outw (0, ioaddr + 4);

  i596_cleanup_cmd (lp);

  while (lp->scb.status || lp->scb.command)
    if (--boguscnt == 0)
    {
      printk ("%s: close timed timed out with status %4x, cmd %4x.\n",
              dev->name, lp->scb.status, lp->scb.command);
      break;
    }
  free_irq (dev->irq);
  irq2dev_map[dev->irq] = 0;
  remove_rx_bufs (dev);
}

static void*i596_get_stats (struct device *dev)
{
  struct i596_private *lp = (struct i596_private*) dev->priv;
  return (void*)&lp->stats;
}

/*
 *  Set or clear the multicast filter for this adaptor.
 */
static void set_multicast_list (struct device *dev)
{
  struct i596_private *lp = (struct i596_private*) dev->priv;
  struct i596_cmd     *cmd;

  if (i596_debug > 1)
     printk ("%s: set multicast list %d\n", dev->name, dev->mc_count);

  if (dev->mc_count > 0)
  {
    ETHER *eth;
    char  *cp;
    int    i;

    cmd = k_calloc (sizeof(struct i596_cmd) + 2 + dev->mc_count * 6, 1);
    if (!cmd)
    {
      printk ("%s: set_multicast Memory squeeze.\n", dev->name);
      return;
    }
    cmd->command = CmdMulticastList;
    *(WORD*) (cmd + 1) = dev->mc_count * 6;
    cp = ((char*) (cmd + 1)) + 2;
    for (i = 0, eth = &dev->mc_list[0]; i < DIM(dev->mc_list); i++)
    {
      memcpy (cp, eth, sizeof(*eth));
      cp += sizeof(*eth);
    }
    print_eth ((char*)(cmd+1) + 2);
    i596_add_cmd (dev, cmd);
  }
  else
  {
    if (lp->set_conf.next != (struct i596_cmd*) I596_NULL)
       return;

    if (dev->mc_count == 0 && !(dev->flags & (IFF_PROMISC | IFF_ALLMULTI)))
    {
      if (dev->flags & IFF_ALLMULTI)
          dev->flags |= IFF_PROMISC;
      lp->i596_config[8] &= ~0x01;
    }
    else
      lp->i596_config[8] |= 1;

    i596_add_cmd (dev, &lp->set_conf);
  }
}

