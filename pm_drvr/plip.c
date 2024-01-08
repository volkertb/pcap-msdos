
/* PLIP: A parallel port "network" driver for Linux.
 * This driver is for parallel port with 5-bit cable (LapLink (R) cable).
 *
 * Authors:  Donald Becker,  <becker@super.org>
 *    Tommy Thorn, <thorn@daimi.aau.dk>
 *    Tanabe Hiroyasu, <hiro@sanpo.t.u-tokyo.ac.jp>
 *    Alan Cox, <gw4pts@gw4pts.ampr.org>
 *    Peter Bauer, <100136.3530@compuserve.com>
 *    Niibe Yutaka, <gniibe@mri.co.jp>
 *
 *    Modularization and ifreq/ifmap support by Alan Cox.
 *    Rewritten by Niibe Yutaka.
 *
 * Fixes:
 *    9-Sep-95 Philip Blundell <pjb27@cam.ac.uk>
 *      - only claim 3 bytes of I/O space for port at 0x3bc
 *      - treat NULL return from register_netdev() as success in
 *        init_module()
 *      - added message if driver loaded as a module but no
 *        interfaces present.
 *      - release claimed I/O ports if malloc() fails during init.
 *
 *    Niibe Yutaka
 *      - Module initialization.  You can specify I/O addr and IRQ:
 *      # insmod plip.o io=0x3bc irq=7
 *      - MTU fix.
 *      - Make sure other end is OK, before sending a packet.
 *      - Fix immediate timer problem.
 *
 *    This program is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    as published by the Free Software Foundation; either version
 *    2 of the License, or (at your option) any later version.
 */

/*
 * Original version and the name 'PLIP' from Donald Becker <becker@super.org>
 * inspired by Russ Nelson's parallel port packet driver.
 *
 * NOTE:
 *     Tanabe Hiroyasu had changed the protocol, and it was in Linux v1.0.
 *     Because of the necessity to communicate to DOS machines with the
 *     Crynwr packet driver, Peter Bauer changed the protocol again
 *     back to original protocol.
 *
 *     This version follows original PLIP protocol.
 *     So, this PLIP can't communicate the PLIP of Linux v1.0.
 */

/*
 *  To use with DOS box, please do (Turn on ARP switch):
 *  # ifconfig plip[0-2] arp
 */

/*
 * Sources:
 * Ideas and protocols came from Russ Nelson's <nelson@crynwr.com>
 * "parallel.asm" parallel port packet driver.
 *
 * The "Crynwr" parallel port standard specifies the following protocol:
 *   Trigger by sending '0x08' (this cause interrupt on other end)
 *   count-low octet
 *   count-high octet
 *   ... data octets
 *   checksum octet
 * Each octet is sent as <wait for rx. '0x1?'> <send 0x10+(octet&0x0F)>
 *     <wait for rx. '0x0?'> <send 0x00+((octet>>4)&0x0F)>
 *
 * The packet is encapsulated as if it were ethernet.
 *
 * The cable used is a de facto standard parallel null cable -- sold as
 * a "LapLink" cable by various places.  You'll need a 12-conductor cable to
 * make one yourself.  The wiring is:
 *   SLCTIN          17 - 17
 *   GROUND          25 - 25
 *   D0->ERROR        2 - 15    15 - 2
 *   D1->SLCT         3 - 13    13 - 3
 *   D2->PAPOUT       4 - 12    12 - 4
 *   D3->ACK          5 - 10    10 - 5
 *   D4->BUSY         6 - 11    11 - 6
 * Do not connect the other pins.  They are
 *   D5,D6,D7 are 7,8,9
 *   STROBE is 1, FEED is 14, INIT is 16
 *   extra grounds are 18,19,20,21,22,23,24
 */

#include "pmdrvr.h"
#include "iface.h"

int plip_debug = NET_DEBUG;

#define PLIP_DELAY_UNIT  1      /* in micro second */

/* Connection time out = PLIP_TRIGGER_WAIT * PLIP_DELAY_UNIT usec
 */
#define PLIP_TRIGGER_WAIT   500

/* Nibble time out = PLIP_NIBBLE_WAIT * PLIP_DELAY_UNIT usec
 */
#define PLIP_NIBBLE_WAIT        3000

#define PAR_INTR_ON       (LP_PINITP|LP_PSELECP|LP_PINTEN)
#define PAR_INTR_OFF      (LP_PINITP|LP_PSELECP)
#define PAR_DATA(dev)     ((dev)->base_addr+0)
#define PAR_STATUS(dev)   ((dev)->base_addr+1)
#define PAR_CONTROL(dev)  ((dev)->base_addr+2)

/* Bottom halfs */
static void plip_kick_bh (struct device *dev);
static void plip_bh (struct device *dev);

/* Interrupt handler */
static void plip_interrupt (int irq);

/* Functions for DEV methods */
static int   plip_rebuild_header (struct sk_buff *skb);
static int   plip_tx_packet (struct device *dev, BYTE * data, int len);
static int   plip_open (struct device *dev);
static int   plip_close (struct device *dev);
static void *plip_get_stats (struct device *dev);
static int   plip_config (struct device *dev, struct ifmap *map);
static int   plip_ioctl (struct device *dev, struct ifreq *ifr, int cmd);

static int plip_none (struct device *, struct net_local *, struct plip_local *, struct plip_local *);
static int plip_receive_packet (struct device *, struct net_local *, struct plip_local *, struct plip_local *);
static int plip_send_packet (struct device *, struct net_local *, struct plip_local *, struct plip_local *);
static int plip_connection_close (struct device *, struct net_local *, struct plip_local *, struct plip_local *);
static int plip_error (struct device *, struct net_local *, struct plip_local *, struct plip_local *);
static int plip_bh_timeout_error (struct device *dev, struct net_local *nl,
                                  struct plip_local *snd, struct plip_local *rcv, int error);


enum plip_connection_state {
     PLIP_CN_NONE = 0,
     PLIP_CN_RECEIVE,
     PLIP_CN_SEND,
     PLIP_CN_CLOSING,
     PLIP_CN_ERROR
   };

enum plip_packet_state {
     PLIP_PK_DONE = 0,
     PLIP_PK_TRIGGER,
     PLIP_PK_LENGTH_LSB,
     PLIP_PK_LENGTH_MSB,
     PLIP_PK_DATA,
     PLIP_PK_CHECKSUM
   };

enum plip_nibble_state {
     PLIP_NB_BEGIN,
     PLIP_NB_1,
     PLIP_NB_2,
   };

struct plip_local {
       enum plip_packet_state state;
       enum plip_nibble_state nibble;
       union {
         struct {
           BYTE lsb;
           BYTE msb;
         } b;
         WORD h;
       } length;
       WORD  byte;
       BYTE  checksum;
       BYTE  data;
       struct sk_buff *skb;
     };

struct net_local {
       struct net_device_stats enet_stats;
       struct tq_struct        immediate;
       struct tq_struct        deferred;
       struct plip_local       snd_data;
       struct plip_local       rcv_data;
       DWORD  trigger;
       DWORD  nibble;
       enum   plip_connection_state connection;
       WORD   timeout_count;
       char   is_deferred;
       int  (*orig_rebuild_header) (struct sk_buff * skb);
     };

/*
 * Entry point of PLIP driver.
 * Probe the hardware, and register/initialize the driver.
 */
int plip_init (struct device *dev)
{
  struct net_local *nl;
  int    iosize = (PAR_DATA (dev) == 0x3BC) ? 3 : 8;

  /* Check that there is something at base_addr.
   */
  outb (0, PAR_DATA (dev));
  udelay (1000);
  if (inb (PAR_DATA (dev)) != 0)
     return (0);

  printk (version);
  printk ("%s: Parallel port at %#3lx, ", dev->name, dev->base_addr);
  if (dev->irq)
     printk ("using assigned IRQ %d.\n", dev->irq);
  else
  {
    int   irq = 0;
    DWORD irqs = probe_irq_on();

    outb (0x00, PAR_CONTROL (dev));
    udelay (1000);
    outb (PAR_INTR_OFF, PAR_CONTROL (dev));
    outb (PAR_INTR_ON, PAR_CONTROL (dev));
    outb (PAR_INTR_OFF, PAR_CONTROL (dev));
    udelay (1000);
    irq = probe_irq_off (irqs);
    if (irq > 0)
    {
      dev->irq = irq;
      printk ("using probed IRQ %d.\n", dev->irq);
    }
    else
      printk ("failed to detect IRQ(%d) --Please set IRQ by ifconfig.\n", irq);
  }

  /* Fill in the generic fields of the device structure.
   */
  ether_setup (dev);

  /* Then, override parts of it
   */
  dev->xmit       = plip_tx_packet;
  dev->open       = plip_open;
  dev->close      = plip_close;
  dev->get_stats  = plip_get_stats;
  dev->set_config = plip_config;
  dev->do_ioctl   = plip_ioctl;
  dev->tx_queue_len = 10;
  dev->flags = IFF_POINTOPOINT | IFF_NOARP;

  /* Set the private structure */
  dev->priv = k_calloc (sizeof(struct net_local), 1);
  if (!dev->priv)
  {
    printk ("%s: out of memory\n", dev->name);
    return (0);
  }

  nl = (struct net_local *) dev->priv;
  nl->orig_rebuild_header = dev->rebuild_header;
  dev->rebuild_header = plip_rebuild_header;

  /* Initialize constants
   */
  nl->trigger = PLIP_TRIGGER_WAIT;
  nl->nibble  = PLIP_NIBBLE_WAIT;

  /* Initialize task queue structures
   */
  nl->immediate.next    = NULL;
  nl->immediate.sync    = 0;
  nl->immediate.routine = (void*) (void*) plip_bh;
  nl->immediate.data    = dev;
  nl->deferred.next     = NULL;
  nl->deferred.sync     = 0;
  nl->deferred.routine  = (void*) (void*) plip_kick_bh;
  nl->deferred.data     = dev;

  return (1);
}

/* Bottom half handler for the delayed request.
 * This routine is kicked by do_timer().
 * Request `plip_bh' to be invoked.
 */
static void plip_kick_bh (struct device *dev)
{
  struct net_local *nl = (struct net_local *) dev->priv;

  if (nl->is_deferred)
  {
    queue_task (&nl->immediate, &tq_immediate);
    mark_bh (IMMEDIATE_BH);
  }
}

#define OK        0
#define TIMEOUT   1
#define ERROR     2

typedef int (*plip_func) (struct device *dev, struct net_local *nl,
                          struct plip_local *snd, struct plip_local *rcv);

static plip_func connection_state_table[] = {
                 plip_none,
                 plip_receive_packet,
                 plip_send_packet,
                 plip_connection_close,
                 plip_error
               };

/*
 * Bottom half handler of PLIP.
 */
static void plip_bh (struct device *dev)
{
  struct net_local  *nl  = (struct net_local *) dev->priv;
  struct plip_local *snd = &nl->snd_data;
  struct plip_local *rcv = &nl->rcv_data;
  plip_func f;
  int       r;

  nl->is_deferred = 0;
  f = connection_state_table[nl->connection];
  if ((r = (*f) (dev, nl, snd, rcv)) != OK &&
      (r = plip_bh_timeout_error (dev, nl, snd, rcv, r)) != OK)
  {
    nl->is_deferred = 1;
    queue_task (&nl->deferred, &tq_timer);
  }
}

static int plip_bh_timeout_error (struct device *dev, struct net_local *nl,
                                  struct plip_local *snd, struct plip_local *rcv,
                                  int error)
{
  BYTE c0;

  DISABLE();
  if (nl->connection == PLIP_CN_SEND)
  {
    if (error != ERROR)         /* Timeout */
    {
      nl->timeout_count++;
      if ((snd->state == PLIP_PK_TRIGGER && nl->timeout_count <= 10) ||
          nl->timeout_count <= 3)
      {
        ENABLE();
        /* Try again later */
        return TIMEOUT;
      }
      c0 = inb (PAR_STATUS (dev));
      printk ("%s: transmit timeout(%d,%02x)\n", dev->name, snd->state, c0);
    }
    nl->enet_stats.tx_errors++;
    nl->enet_stats.tx_aborted_errors++;
  }
  else if (nl->connection == PLIP_CN_RECEIVE)
  {
    if (rcv->state == PLIP_PK_TRIGGER)
    {
      /* Transmission was interrupted. */
      ENABLE();
      return OK;
    }
    if (error != ERROR)         /* Timeout */
    {
      if (++nl->timeout_count <= 3)
      {
        ENABLE();
        /* Try again later */
        return TIMEOUT;
      }
      c0 = inb (PAR_STATUS (dev));
      printk ("%s: receive timeout(%d,%02x)\n", dev->name, rcv->state, c0);
    }
    nl->enet_stats.rx_dropped++;
  }
  rcv->state = PLIP_PK_DONE;
  if (rcv->skb)
  {
    kfree_skb (rcv->skb, FREE_READ);
    rcv->skb = NULL;
  }
  snd->state = PLIP_PK_DONE;
  if (snd->skb)
  {
    dev_kfree_skb (snd->skb, FREE_WRITE);
    snd->skb = NULL;
  }
  disable_irq (dev->irq);
  outb (PAR_INTR_OFF, PAR_CONTROL (dev));
  dev->tbusy = 1;
  nl->connection = PLIP_CN_ERROR;
  outb (0x00, PAR_DATA (dev));
  ENABLE();
  return (TIMEOUT);
}

static int plip_none (struct device *dev, struct net_local *nl,
                      struct plip_local *snd, struct plip_local *rcv)
{
  ARGSUSED (dev);
  ARGSUSED (nl);
  ARGSUSED (snd);
  ARGSUSED (rcv);
  return (OK);
}

/*
 * PLIP_RECEIVE --- receive a byte(two nibbles)
 * Returns OK on success, TIMEOUT on timeout
 */
static int plip_receive (WORD nibble_timeout, WORD status_addr,
                         enum plip_nibble_state *ns_p, BYTE * data_p)
{
  BYTE  c0, c1;
  DWORD cx;

  switch (*ns_p)
  {
    case PLIP_NB_BEGIN:
         cx = nibble_timeout;
         while (1)
         {
           c0 = inb (status_addr);
           udelay (PLIP_DELAY_UNIT);
           if ((c0 & 0x80) == 0)
           {
             c1 = inb (status_addr);
             if (c0 == c1)
               break;
           }
           if (--cx == 0)
             return TIMEOUT;
         }
         *data_p = (c0 >> 3) & 0x0f;
         outb (0x10, --status_addr); /* send ACK */
         status_addr++;
         *ns_p = PLIP_NB_1;

    case PLIP_NB_1:
         cx = nibble_timeout;
         while (1)
         {
           c0 = inb (status_addr);
           udelay (PLIP_DELAY_UNIT);
           if (c0 & 0x80)
           {
             c1 = inb (status_addr);
             if (c0 == c1)
               break;
           }
           if (--cx == 0)
             return TIMEOUT;
         }
         *data_p |= (c0 << 1) & 0xf0;
         outb (0x00, --status_addr); /* send ACK */
         status_addr++;
         *ns_p = PLIP_NB_BEGIN;

    case PLIP_NB_2:
         break;
  }
  return (OK);
}

/*
 * PLIP_RECEIVE_PACKET --- receive a packet
 */
static int plip_receive_packet (struct device *dev, struct net_local *nl,
                                struct plip_local *snd, struct plip_local *rcv)
{
  WORD  status_addr    = PAR_STATUS (dev);
  WORD  nibble_timeout = nl->nibble;
  BYTE *lbuf;

  switch (rcv->state)
  {
    case PLIP_PK_TRIGGER:
         disable_irq (dev->irq);
         outb (PAR_INTR_OFF, PAR_CONTROL (dev));
         dev->interrupt = 0;
         outb (0x01, PAR_DATA (dev)); /* send ACK */
         if (plip_debug > 2)
            printk ("%s: receive start\n", dev->name);
         rcv->state  = PLIP_PK_LENGTH_LSB;
         rcv->nibble = PLIP_NB_BEGIN;

    case PLIP_PK_LENGTH_LSB:
         if (snd->state != PLIP_PK_DONE)
         {
           if (plip_receive (nl->trigger, status_addr, &rcv->nibble, &rcv->length.b.lsb))
           {
             /* collision, here dev->tbusy == 1 */
             rcv->state = PLIP_PK_DONE;
             nl->is_deferred = 1;
             nl->connection = PLIP_CN_SEND;
             queue_task (&nl->deferred, &tq_timer);
             outb (PAR_INTR_ON, PAR_CONTROL (dev));
             enable_irq (dev->irq);
             return (OK);
           }
         }
         else
         {
           if (plip_receive (nibble_timeout, status_addr, &rcv->nibble, &rcv->length.b.lsb))
              return (TIMEOUT);
         }
         rcv->state = PLIP_PK_LENGTH_MSB;

    case PLIP_PK_LENGTH_MSB:
         if (plip_receive (nibble_timeout, status_addr, &rcv->nibble, &rcv->length.b.msb))
            return (TIMEOUT);
         if (rcv->length.h > dev->mtu + dev->hard_header_len || rcv->length.h < 8)
         {
           printk ("%s: bogus packet size %d.\n", dev->name, rcv->length.h);
           return (ERROR);
         }
         /* Malloc up new buffer. */
         rcv->skb = dev_alloc_skb (rcv->length.h);
         if (rcv->skb == NULL)
         {
           printk ("%s: Memory squeeze.\n", dev->name);
           return (ERROR);
         }
         skb_put (rcv->skb, rcv->length.h);
         rcv->skb->dev = dev;
         rcv->state    = PLIP_PK_DATA;
         rcv->byte     = 0;
         rcv->checksum = 0;

    case PLIP_PK_DATA:
         lbuf = rcv->skb->data;
         do
           if (plip_receive (nibble_timeout, status_addr, &rcv->nibble, &lbuf[rcv->byte]))
              return (TIMEOUT);
         while (++rcv->byte < rcv->length.h) ;
         do
           rcv->checksum += lbuf[--rcv->byte];
         while (rcv->byte);
         rcv->state = PLIP_PK_CHECKSUM;

    case PLIP_PK_CHECKSUM:
         if (plip_receive (nibble_timeout, status_addr, &rcv->nibble, &rcv->data))
            return (TIMEOUT);
         if (rcv->data != rcv->checksum)
         {
           nl->enet_stats.rx_crc_errors++;
           if (plip_debug)
              printk ("%s: checksum error\n", dev->name);
           return (ERROR);
         }
         rcv->state = PLIP_PK_DONE;

    case PLIP_PK_DONE:
         /* Inform the upper layer for the arrival of a packet. */
         rcv->skb->protocol = eth_type_trans (rcv->skb, dev);
         netif_rx (rcv->skb);
         nl->enet_stats.rx_packets++;
         rcv->skb = NULL;
         if (plip_debug > 2)
            printk ("%s: receive end\n", dev->name);

         /* Close the connection. */
         outb (0x00, PAR_DATA (dev));
         DISABLE();
         if (snd->state != PLIP_PK_DONE)
         {
           nl->connection = PLIP_CN_SEND;
           ENABLE();
           queue_task (&nl->immediate, &tq_immediate);
           mark_bh (IMMEDIATE_BH);
           outb (PAR_INTR_ON, PAR_CONTROL (dev));
           enable_irq (dev->irq);
           return (OK);
         }
         else
         {
           nl->connection = PLIP_CN_NONE;
           ENABLE();
           outb (PAR_INTR_ON, PAR_CONTROL (dev));
           enable_irq (dev->irq);
           return (OK);
         }
  }
  return (OK);
}

/*
 * PLIP_SEND --- send a byte (two nibbles)
 * Returns OK on success, TIMEOUT when timeout
 */
 int plip_send (WORD nibble_timeout, WORD data_addr, enum plip_nibble_state *ns_p, BYTE data)
{
  BYTE c0;
  DWORD cx;

  switch (*ns_p)
  {
       case PLIP_NB_BEGIN:
         outb ((data & 0x0f), data_addr);
         *ns_p = PLIP_NB_1;

       case PLIP_NB_1:
         outb (0x10 | (data & 0x0f), data_addr);
         cx = nibble_timeout;
         data_addr++;
         while (1)
         {
           c0 = inb (data_addr);
           if ((c0 & 0x80) == 0)
             break;
           if (--cx == 0)
             return (TIMEOUT);
           udelay (PLIP_DELAY_UNIT);
         }
         outb (0x10 | (data >> 4), --data_addr);
         *ns_p = PLIP_NB_2;

       case PLIP_NB_2:
         outb ((data >> 4), data_addr);
         data_addr++;
         cx = nibble_timeout;
         while (1)
         {
           c0 = inb (data_addr);
           if (c0 & 0x80)
             break;
           if (--cx == 0)
             return (TIMEOUT);
           udelay (PLIP_DELAY_UNIT);
         }
         data_addr--;
         *ns_p = PLIP_NB_BEGIN;
         return (OK);
  }
  return (OK);
}

/* PLIP_SEND_PACKET --- send a packet */
static int plip_send_packet (struct device *dev, struct net_local *nl, struct plip_local *snd, struct plip_local *rcv)
{
  WORD data_addr = PAR_DATA (dev);
  WORD nibble_timeout = nl->nibble;
  BYTE *lbuf;
  BYTE c0;
  DWORD cx;

  if (snd->skb == NULL || (lbuf = snd->skb->data) == NULL)
  {
    printk ("%s: send skb lost\n", dev->name);
    snd->state = PLIP_PK_DONE;
    snd->skb = NULL;
    return (ERROR);
  }

  switch (snd->state)
  {
       case PLIP_PK_TRIGGER:
         if ((inb (PAR_STATUS (dev)) & 0xf8) != 0x80)
           return (TIMEOUT);

         /* Trigger remote rx interrupt. */
         outb (0x08, data_addr);
         cx = nl->trigger;
         while (1)
         {
           udelay (PLIP_DELAY_UNIT);
           DISABLE();
           if (nl->connection == PLIP_CN_RECEIVE)
           {
             ENABLE();
             /* interrupted */
             nl->enet_stats.tx_collisions++;
             if (plip_debug > 1)
               printk ("%s: collision.\n", dev->name);
             return (OK);
           }
           c0 = inb (PAR_STATUS (dev));
           if (c0 & 0x08)
           {
             disable_irq (dev->irq);
             outb (PAR_INTR_OFF, PAR_CONTROL (dev));
             if (plip_debug > 2)
               printk ("%s: send start\n", dev->name);
             snd->state = PLIP_PK_LENGTH_LSB;
             snd->nibble = PLIP_NB_BEGIN;
             nl->timeout_count = 0;
             ENABLE();
             break;
           }
           ENABLE();
           if (--cx == 0)
           {
             outb (0x00, data_addr);
             return (TIMEOUT);
           }
         }

       case PLIP_PK_LENGTH_LSB:
         if (plip_send (nibble_timeout, data_addr, &snd->nibble, snd->length.b.lsb))
           return (TIMEOUT);
         snd->state = PLIP_PK_LENGTH_MSB;

       case PLIP_PK_LENGTH_MSB:
         if (plip_send (nibble_timeout, data_addr, &snd->nibble, snd->length.b.msb))
           return (TIMEOUT);
         snd->state = PLIP_PK_DATA;
         snd->byte = 0;
         snd->checksum = 0;

       case PLIP_PK_DATA:
         do
           if (plip_send (nibble_timeout, data_addr, &snd->nibble, lbuf[snd->byte]))
             return (TIMEOUT);
         while (++snd->byte < snd->length.h) ;
         do
           snd->checksum += lbuf[--snd->byte];
         while (snd->byte);
         snd->state = PLIP_PK_CHECKSUM;

       case PLIP_PK_CHECKSUM:
         if (plip_send (nibble_timeout, data_addr, &snd->nibble, snd->checksum))
           return (TIMEOUT);

         dev_kfree_skb (snd->skb, FREE_WRITE);
         nl->enet_stats.tx_packets++;
         snd->state = PLIP_PK_DONE;

       case PLIP_PK_DONE:
         /* Close the connection */
         outb (0x00, data_addr);
         snd->skb = NULL;
         if (plip_debug > 2)
           printk ("%s: send end\n", dev->name);
         nl->connection = PLIP_CN_CLOSING;
         nl->is_deferred = 1;
         queue_task (&nl->deferred, &tq_timer);
         outb (PAR_INTR_ON, PAR_CONTROL (dev));
         enable_irq (dev->irq);
         return (OK);
  }
  return (OK);
}

static int plip_connection_close (struct device *dev, struct net_local *nl,
                                  struct plip_local *snd, struct plip_local *rcv)
{
  DISABLE();
  if (nl->connection == PLIP_CN_CLOSING)
  {
    nl->connection = PLIP_CN_NONE;
    dev->tbusy = 0;
    mark_bh (NET_BH);
  }
  ENABLE();
  return (OK);
}

/* PLIP_ERROR --- wait till other end settled */
static int plip_error (struct device *dev, struct net_local *nl, struct plip_local *snd, struct plip_local *rcv)
{
  BYTE status;

  status = inb (PAR_STATUS (dev));
  if ((status & 0xf8) == 0x80)
  {
    if (plip_debug > 2)
      printk ("%s: reset interface.\n", dev->name);
    nl->connection = PLIP_CN_NONE;
    dev->tbusy = 0;
    dev->interrupt = 0;
    outb (PAR_INTR_ON, PAR_CONTROL (dev));
    enable_irq (dev->irq);
    mark_bh (NET_BH);
  }
  else
  {
    nl->is_deferred = 1;
    queue_task (&nl->deferred, &tq_timer);
  }

  return (OK);
}

/* Handle the parallel port interrupts. */
static void plip_interrupt (int irq)
{
  struct device *dev = (struct device *) irq2dev_map[irq];
  struct net_local *nl = (struct net_local *) dev->priv;
  struct plip_local *rcv = &nl->rcv_data;
  BYTE c0;

  if (dev == NULL)
  {
    printk ("plip_interrupt: irq %d for unknown device.\n", irq);
    return;
  }

  if (dev->interrupt)
    return;

  c0 = inb (PAR_STATUS (dev));
  if ((c0 & 0xf8) != 0xc0)
  {
    if (plip_debug > 1)
      printk ("%s: spurious interrupt\n", dev->name);
    return;
  }
  dev->interrupt = 1;
  if (plip_debug > 3)
    printk ("%s: interrupt.\n", dev->name);

  DISABLE();
  switch (nl->connection)
  {
       case PLIP_CN_CLOSING:
         dev->tbusy = 0;
       case PLIP_CN_NONE:
       case PLIP_CN_SEND:
         dev->last_rx = jiffies;
         rcv->state = PLIP_PK_TRIGGER;
         nl->connection = PLIP_CN_RECEIVE;
         nl->timeout_count = 0;
         queue_task (&nl->immediate, &tq_immediate);
         mark_bh (IMMEDIATE_BH);
         ENABLE();
         break;

       case PLIP_CN_RECEIVE:
         ENABLE();
         printk ("%s: receive interrupt when receiving packet\n", dev->name);
         break;

       case PLIP_CN_ERROR:
         ENABLE();
         printk ("%s: receive interrupt in error state\n", dev->name);
         break;
  }
}

/* We don't need to send arp, for plip is point-to-point. */
static int plip_rebuild_header (struct sk_buff *skb)
{
  struct device *dev = skb->dev;
  struct net_local *nl = (struct net_local *) dev->priv;
  struct ethhdr *eth = (struct ethhdr *) skb->data;
  int i;

  if ((dev->flags & IFF_NOARP) == 0)
    return nl->orig_rebuild_header (skb);

  if (eth->h_proto != __constant_htons (ETH_P_IP)
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
      && eth->h_proto != __constant_htons (ETH_P_IPV6)
#endif
    )
  {
    printk ("plip_rebuild_header: Don't know how to resolve type %d addresses?\n", (int) eth->h_proto);
    memcpy (eth->h_source, dev->dev_addr, dev->addr_len);
    return 0;
  }

  for (i = 0; i < ETH_ALEN - sizeof (u32); i++)
    eth->h_dest[i] = 0xfc;
#if 0
  *(u32 *) (eth->h_dest + i) = dst;
#else
  /* Do not want to include net/route.h here.
   * In any case, it is TOP of silliness to emulate
   * hardware addresses on PtP link. --ANK
   */
  *(u32 *) (eth->h_dest + i) = 0;
#endif
  return 0;
}

static int plip_tx_packet (struct device *dev, BYTE * data, int len)
{
  struct net_local *nl = (struct net_local *) dev->priv;
  struct plip_local *snd = &nl->snd_data;

  if (dev->tbusy)
    return 1;

  if (dev->tx_busy)
  {
    printk ("%s: Transmitter access conflict.\n", dev->name);
    return 1;
  }
  dev->tx_busy = 1;

  if (len > dev->mtu + dev->hard_header_len)
  {
    printk ("%s: packet too big, %d.\n", dev->name, len);
    dev->tx_busy = 0;
    return 0;
  }

  if (plip_debug > 2)
    printk ("%s: send request\n", dev->name);

  DISABLE();
  dev->tx_start = jiffies;
  snd->skb = data;
  snd->length.h = skb->len;
  snd->state = PLIP_PK_TRIGGER;
  if (nl->connection == PLIP_CN_NONE)
  {
    nl->connection = PLIP_CN_SEND;
    nl->timeout_count = 0;
  }
  queue_task (&nl->immediate, &tq_immediate);
  mark_bh (IMMEDIATE_BH);
  ENABLE();

  return 0;
}

/* Open/initialize the board.  This is called (in the current kernel)
 * sometime after booting when the 'ifconfig' program is run.
 * 
 * This routine gets exclusive access to the parallel port by allocating
 * its IRQ line.
 */
static int plip_open (struct device *dev)
{
  struct net_local *nl = (struct net_local *) dev->priv;
  int i;

  if (dev->irq == 0)
  {
    printk ("%s: IRQ is not set.  Please set it by ifconfig.\n", dev->name);
    return -EAGAIN;
  }
  DISABLE();
  if (request_irq (dev->irq, plip_interrupt, 0, dev->name, NULL) != 0)
  {
    ENABLE();
    printk ("%s: couldn't get IRQ %d.\n", dev->name, dev->irq);
    return -EAGAIN;
  }
  irq2dev_map[dev->irq] = dev;
  ENABLE();

  /* Clear the data port. */
  outb (0x00, PAR_DATA (dev));

  /* Enable rx interrupt. */
  outb (PAR_INTR_ON, PAR_CONTROL (dev));

  /* Initialize the state machine. */
  nl->rcv_data.state = nl->snd_data.state = PLIP_PK_DONE;
  nl->rcv_data.skb = nl->snd_data.skb = NULL;
  nl->connection = PLIP_CN_NONE;
  nl->is_deferred = 0;

  /* Fill in the MAC-level header. */
  for (i = 0; i < ETH_ALEN - sizeof (u32); i++)
    dev->dev_addr[i] = 0xfc;
  *(u32 *) (dev->dev_addr + i) = dev->pa_addr;

  dev->interrupt = 0;
  dev->start = 1;
  dev->tbusy = 0;
  MOD_INC_USE_COUNT;
  return 0;
}

/* The inverse routine to plip_open(). */
static int plip_close (struct device *dev)
{
  struct net_local *nl = (struct net_local *) dev->priv;
  struct plip_local *snd = &nl->snd_data;
  struct plip_local *rcv = &nl->rcv_data;

  dev->tbusy = 1;
  dev->start = 0;
  DISABLE();
  free_irq (dev->irq, NULL);
  irq2dev_map[dev->irq] = NULL;
  nl->is_deferred = 0;
  nl->connection = PLIP_CN_NONE;
  ENABLE();
  outb (0x00, PAR_DATA (dev));

  snd->state = PLIP_PK_DONE;
  if (snd->skb)
  {
    dev_kfree_skb (snd->skb, FREE_WRITE);
    snd->skb = NULL;
  }
  rcv->state = PLIP_PK_DONE;
  if (rcv->skb)
  {
    kfree_skb (rcv->skb, FREE_READ);
    rcv->skb = NULL;
  }

  /* Reset. */
  outb (0x00, PAR_CONTROL (dev));
  MOD_DEC_USE_COUNT;
  return 0;
}

static struct net_device_stats *plip_get_stats (struct device *dev)
{
  struct net_local *nl = (struct net_local *) dev->priv;
  struct net_device_stats *r = &nl->enet_stats;

  return r;
}

static int plip_config (struct device *dev, struct ifmap *map)
{
  if (dev->flags & IFF_UP)
    return -EBUSY;

  if (map->base_addr != (DWORD) - 1 && map->base_addr != dev->base_addr)
    printk ("%s: You cannot change base_addr of this interface (ignored).\n", dev->name);

  if (map->irq != (BYTE) - 1)
    dev->irq = map->irq;
  return 0;
}

static int plip_ioctl (struct device *dev, struct ifreq *rq, int cmd)
{
  struct net_local *nl = (struct net_local *) dev->priv;
  struct plipconf *pc = (struct plipconf *) &rq->ifr_data;

  switch (pc->pcmd)
  {
       case PLIP_GET_TIMEOUT:
         pc->trigger = nl->trigger;
         pc->nibble = nl->nibble;
         break;
       case PLIP_SET_TIMEOUT:
         nl->trigger = pc->trigger;
         nl->nibble = pc->nibble;
         break;
       default:
         return -EOPNOTSUPP;
  }
  return 0;
}

#ifdef MODULE
static int io[] = { 0, 0, 0 };
static int irq[] = { 0, 0, 0 };

MODULE_PARM (io, "1-3i");
MODULE_PARM (irq, "1-3i");

static struct device dev_plip[] = {
  {
   "plip0",
   0, 0, 0, 0,                  /* memory */
   0x3BC, 5,                    /* base, irq */
   0, 0, 0, NULL, plip_init},
  {
   "plip1",
   0, 0, 0, 0,                  /* memory */
   0x378, 7,                    /* base, irq */
   0, 0, 0, NULL, plip_init},
  {
   "plip2",
   0, 0, 0, 0,                  /* memory */
   0x278, 2,                    /* base, irq */
   0, 0, 0, NULL, plip_init}
};

int init_module (void)
{
  int no_parameters = 1;
  int devices = 0;
  int i;

  /* When user feeds parameters, use them */
  for (i = 0; i < 3; i++)
  {
    int specified = 0;

    if (io[i] != 0)
    {
      dev_plip[i].base_addr = io[i];
      specified++;
    }
    if (irq[i] != 0)
    {
      dev_plip[i].irq = irq[i];
      specified++;
    }
    if (specified)
    {
      if (register_netdev (&dev_plip[i]) != 0)
      {
        printk (KERN_INFO "plip%d: Not found\n", i);
        return -EIO;
      }
      no_parameters = 0;
    }
  }
  if (!no_parameters)
    return 0;

  /* No parameters.  Default action is probing all interfaces. */
  for (i = 0; i < 3; i++)
  {
    if (register_netdev (&dev_plip[i]) == 0)
      devices++;
  }
  if (devices == 0)
  {
    printk (KERN_INFO "plip: no interfaces found\n");
    return -EIO;
  }
  return 0;
}

void cleanup_module (void)
{
  int i;

  for (i = 0; i < 3; i++)
  {
    if (dev_plip[i].priv)
    {
      unregister_netdev (&dev_plip[i]);
      release_region (PAR_DATA (&dev_plip[i]), (PAR_DATA (&dev_plip[i]) == 0x3bc) ? 3 : 8);
      kfree_s (dev_plip[i].priv, sizeof (struct net_local));

      dev_plip[i].priv = NULL;
    }
  }
}
#endif     /* MODULE */

/*
 * Local variables:
 * compile-command: "gcc -DMODULE -DMODVERSIONS -D__KERNEL__ -Wall -Wstrict-prototypes -O2 -g -fomit-frame-pointer -pipe -m486 -c plip.c"
 * End:
 */
