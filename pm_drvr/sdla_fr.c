/*****************************************************************************
* sdla_fr.c  WANPIPE(tm) Multiprotocol WAN Link Driver. Frame relay module.
*
* Author:  Gene Kozin  <genek@compuserve.com>
*
* Copyright:  (c) 1995-1997 Sangoma Technologies Inc.
*
*    This program is free software; you can redistribute it and/or
*    modify it under the terms of the GNU General Public License
*    as published by the Free Software Foundation; either version
*    2 of the License, or (at your option) any later version.
* ============================================================================
* Jan 02, 1997  Gene Kozin  Initial version.
*****************************************************************************/

#if  !defined(__KERNEL__) || !defined(MODULE)
#error  This code MUST be compiled as a kernel module!
#endif

#include <linux/kernel.h>  /* printk(), and other useful stuff */
#include <linux/stddef.h>  /* offsetof(), etc. */
#include <linux/errno.h>  /* return codes */
#include <linux/string.h>  /* inline memset(), etc. */
#include <linux/malloc.h>  /* kmalloc(), kfree() */
#include <linux/router.h>  /* WAN router definitions */
#include <linux/wanpipe.h>  /* WANPIPE common user API definitions */
#include <linux/if_arp.h>  /* ARPHRD_* defines */
#include <asm/byteorder.h>  /* htons(), etc. */
#include <asm/io.h>    /* for inb(), outb(), etc. */

#define  _GNUC_
#include <linux/sdla_fr.h>  /* frame relay firmware API definitions */

/****** Defines & Macros ****************************************************/

#define  CMD_OK    0    /* normal firmware return code */
#define  CMD_TIMEOUT  0xFF    /* firmware command timed out */
#define  MAX_CMD_RETRY  10    /* max number of firmware retries */

#define  FR_HEADER_LEN  8    /* max encapsulation header size */
#define  FR_CHANNEL_MTU  1500    /* unfragmented logical channel MTU */

/* Q.922 frame types */
#define  Q922_UI    0x03    /* Unnumbered Info frame */
#define  Q922_XID  0xAF    /* ??? */

/****** Data Structures *****************************************************/

/* This is an extention of the 'struct device' we create for each network
 * interface to keep the rest of channel-specific data.
 */
typedef struct fr_channel
{
  char name[WAN_IFNAME_SZ+1];  /* interface name, ASCIIZ */
  unsigned dlci;      /* logical channel number */
  unsigned cir;      /* committed information rate */
  char state;      /* channel state */
  DWORD state_tick;  /* time of the last state change */
  sdla_t* card;      /* -> owner */
  struct enet_statistics ifstats;  /* interface statistics */
} fr_channel_t;

typedef struct dlci_status
{
  WORD dlci  PACKED;
  BYTE state  PACKED;
} dlci_status_t;

/****** Function Prototypes *************************************************/

/* WAN link driver entry points. These are called by the WAN router module. */
static int update (wan_device_t* wandev);
static int new_if (wan_device_t* wandev, struct device* dev,
  wanif_conf_t* conf);
static int del_if (wan_device_t* wandev, struct device* dev);

/* Network device interface */
static int if_init   (struct device* dev);
static int if_open   (struct device* dev);
static int if_close  (struct device* dev);
static int if_header (struct sk_buff* skb, struct device* dev,
  WORD type, void* daddr, void* saddr, unsigned len);
static int if_rebuild_hdr (void* hdr, struct device* dev, DWORD raddr,
  struct sk_buff* skb);
static int if_send (struct sk_buff* skb, struct device* dev);
static struct enet_statistics* if_stats (struct device* dev);

/* Interrupt handlers */
static void fr502_isr (sdla_t* card);
static void fr508_isr (sdla_t* card);
static void fr502_rx_intr (sdla_t* card);
static void fr508_rx_intr (sdla_t* card);
static void tx_intr (sdla_t* card);
static void spur_intr (sdla_t* card);

/* Background polling routines */
static void wpf_poll (sdla_t* card);

/* Frame relay firmware interface functions */
static int fr_read_version (sdla_t* card, char* str);
static int fr_configure (sdla_t* card, fr_conf_t *conf);
static int fr_set_intr_mode (sdla_t* card, unsigned mode, unsigned mtu);
static int fr_comm_enable (sdla_t* card);
static int fr_comm_disable (sdla_t* card);
static int fr_add_dlci (sdla_t* card, int dlci, int num);
static int fr_activate_dlci (sdla_t* card, int dlci, int num);
static int fr_issue_isf (sdla_t* card, int isf);
static int fr502_send (sdla_t* card, int dlci, int attr, int len, void *buf);
static int fr508_send (sdla_t* card, int dlci, int attr, int len, void *buf);

/* Firmware asynchronous event handlers */
static int fr_event (sdla_t* card, int event, fr_mbox_t* mbox);
static int fr_modem_failure (sdla_t *card, fr_mbox_t* mbox);
static int fr_dlci_change (sdla_t *card, fr_mbox_t* mbox);

/* Miscellaneous functions */
static int update_chan_state (struct device* dev);
static void set_chan_state (struct device* dev, int state);
static struct device* find_channel (sdla_t* card, unsigned dlci);
static int is_tx_ready (sdla_t* card);
static DWORD dec_to_uint (BYTE* str, int len);
static DWORD hex_to_uint (BYTE* str, int len);

/****** Public Functions ****************************************************/

/*============================================================================
 * Frame relay protocol initialization routine.
 *
 * This routine is called by the main WANPIPE module during setup.  At this
 * point adapter is completely initialized and firmware is running.
 *  o read firmware version (to make sure it's alive)
 *  o configure adapter
 *  o initialize protocol-specific fields of the adapter data space.
 *
 * Return:  0  o.k.
 *    < 0  failure.
 */
int wpf_init (sdla_t* card, wandev_conf_t* conf)
{
  union
  {
    char str[80];
    fr_conf_t cfg;
  } u;

  /* Verify configuration ID */
  if (conf->config_id != WANCONFIG_FR)
  {
    printk(KERN_INFO "%s: invalid configuration ID %u!\n",
      card->devname, conf->config_id)
    ;
    return -EINVAL;
  }

  /* Initialize protocol-specific fields of adapter data space */
  switch (card->hw.fwid)
  {
  case SFID_FR502:
    card->mbox  = (void*)(card->hw.dpmbase + FR502_MBOX_OFFS);
    card->rxmb  = (void*)(card->hw.dpmbase + FR502_RXMB_OFFS);
    card->flags = (void*)(card->hw.dpmbase + FR502_FLAG_OFFS);
    card->isr = &fr502_isr;
    break;

  case SFID_FR508:
    card->mbox  = (void*)(card->hw.dpmbase + FR508_MBOX_OFFS);
    card->flags = (void*)(card->hw.dpmbase + FR508_FLAG_OFFS);
    card->isr = &fr508_isr;
    break;

  default:
    return -EINVAL;
  }

  /* Read firmware version.  Note that when adapter initializes, it
   * clears the mailbox, so it may appear that the first command was
   * executed successfully when in fact it was merely erased. To work
   * around this, we execute the first command twice.
   */
  if (fr_read_version(card, NULL) || fr_read_version(card, u.str))
    return -EIO
  ;
  printk(KERN_INFO "%s: running frame relay firmware v%s\n",
    card->devname, u.str)
  ;

  /* Adjust configuration */
  conf->mtu = max(min(conf->mtu, 4080), FR_CHANNEL_MTU + FR_HEADER_LEN);
  conf->bps = min(conf->bps, 2048000);

  /* Configure adapter firmware */
  memset(&u.cfg, 0, sizeof(u.cfg));
  u.cfg.mtu  = conf->mtu;
  u.cfg.t391  = 10;
  u.cfg.t392  = 15;
  u.cfg.n391  = 6;
  u.cfg.n392  = 3;
  u.cfg.n393  = 4;
  u.cfg.kbps  = conf->bps / 1000;
  u.cfg.cir_fwd  = max(min(u.cfg.kbps, 512), 1);
  u.cfg.cir_bwd = u.cfg.bc_fwd = u.cfg.bc_bwd = u.cfg.cir_fwd;
  u.cfg.options  = 0x0081;  /* direct Rx, no CIR check */
  switch (conf->u.fr.signalling)
  {
  case WANOPT_FR_Q933:  u.cfg.options |= 0x0200; break;
  case WANOPT_FR_LMI:  u.cfg.options |= 0x0400; break;
  }
  if (conf->station == WANOPT_CPE)
  {
    u.cfg.options |= 0x8000;  /* auto config DLCI */
  }
  else
  {
    u.cfg.station = 1;  /* switch emulation mode */
    card->u.f.node_dlci = conf->u.fr.dlci ? conf->u.fr.dlci : 16;
    card->u.f.dlci_num  = max(min(conf->u.fr.dlci_num, 1), 100);
  }
  if (conf->clocking == WANOPT_INTERNAL)
    u.cfg.port |= 0x0001
  ;
  if (conf->interface == WANOPT_RS232)
    u.cfg.port |= 0x0002
  ;
  if (conf->u.fr.t391)
    u.cfg.t391 = min(conf->u.fr.t391, 30)
  ;
  if (conf->u.fr.t392)
    u.cfg.t392 = min(conf->u.fr.t392, 30)
  ;
  if (conf->u.fr.n391)
    u.cfg.n391 = min(conf->u.fr.n391, 255)
  ;
  if (conf->u.fr.n392)
    u.cfg.n392 = min(conf->u.fr.n392, 10)
  ;
  if (conf->u.fr.n393)
    u.cfg.n393 = min(conf->u.fr.n393, 10)
  ;

  if (fr_configure(card, &u.cfg))
    return -EIO
  ;

  if (card->hw.fwid == SFID_FR508)
  {
    fr_buf_info_t* buf_info =
      (void*)(card->hw.dpmbase + FR508_RXBC_OFFS)
    ;

    card->rxmb =
      (void*)(buf_info->rse_next -
      FR_MB_VECTOR + card->hw.dpmbase)
    ;
    card->u.f.rxmb_base =
      (void*)(buf_info->rse_base -
      FR_MB_VECTOR + card->hw.dpmbase)
    ;
    card->u.f.rxmb_last =
      (void*)(buf_info->rse_base +
      (buf_info->rse_num - 1) * sizeof(fr_buf_ctl_t) -
      FR_MB_VECTOR + card->hw.dpmbase)
    ;
    card->u.f.rx_base = buf_info->buf_base;
    card->u.f.rx_top  = buf_info->buf_top;
  }
  card->wandev.mtu  = conf->mtu;
  card->wandev.bps  = conf->bps;
  card->wandev.interface  = conf->interface;
  card->wandev.clocking  = conf->clocking;
  card->wandev.station  = conf->station;
  card->poll    = &wpf_poll;
  card->wandev.update  = &update;
  card->wandev.new_if  = &new_if;
  card->wandev.del_if  = &del_if;
  card->wandev.state  = WAN_DISCONNECTED;
  return 0;
}

/******* WAN Device Driver Entry Points *************************************/

/*============================================================================
 * Update device status & statistics.
 */
static int update (wan_device_t* wandev)
{
/*
  sdla_t* card = wandev->private;
*/
  return 0;
}

/*============================================================================
 * Create new logical channel.
 * This routine is called by the router when ROUTER_IFNEW IOCTL is being
 * handled.
 * o parse media- and hardware-specific configuration
 * o make sure that a new channel can be created
 * o allocate resources, if necessary
 * o prepare network device structure for registaration.
 *
 * Return:  0  o.k.
 *    < 0  failure (channel will not be created)
 */
static int new_if (wan_device_t* wandev, struct device* dev, wanif_conf_t* conf)
{
  sdla_t* card = wandev->private;
  fr_channel_t* chan;
  int err = 0;

  if ((conf->name[0] == '\0') || (strlen(conf->name) > WAN_IFNAME_SZ))
  {
    printk(KERN_INFO "%s: invalid interface name!\n",
      card->devname)
    ;
    return -EINVAL;
  }

  /* allocate and initialize private data */
  chan = kmalloc(sizeof(fr_channel_t), GFP_KERNEL);
  if (chan == NULL)
    return -ENOMEM
  ;
  memset(chan, 0, sizeof(fr_channel_t));
  strcpy(chan->name, conf->name);
  chan->card = card;

  /* verify media address */
  if (is_digit(conf->addr[0]))
  {
    int dlci = dec_to_uint(conf->addr, 0);

    if (dlci && (dlci <= 4095))
    {
      chan->dlci = dlci;
    }
    else
    {
      printk(KERN_ERR
        "%s: invalid DLCI %u on interface %s!\n",
        wandev->name, dlci, chan->name)
      ;
      err = -EINVAL;
    }
  }
  else
  {
    printk(KERN_ERR
      "%s: invalid media address on interface %s!\n",
      wandev->name, chan->name)
    ;
    err = -EINVAL;
  }
  if (err)
  {
    kfree(chan);
    return err;
  }

  /* prepare network device data space for registration */
  dev->name = chan->name;
  dev->init = &if_init;
  dev->priv = chan;
  return 0;
}

/*============================================================================
 * Delete logical channel.
 */
static int del_if (wan_device_t* wandev, struct device* dev)
{
  if (dev->priv)
  {
    kfree(dev->priv);
    dev->priv = NULL;
  }
  return 0;
}

/****** Network Device Interface ********************************************/

/*============================================================================
 * Initialize Linux network interface.
 *
 * This routine is called only once for each interface, during Linux network
 * interface registration.  Returning anything but zero will fail interface
 * registration.
 */
static int if_init (struct device* dev)
{
  fr_channel_t* chan = dev->priv;
  sdla_t* card = chan->card;
  wan_device_t* wandev = &card->wandev;
  int i;

  /* Initialize device driver entry points */
  dev->open    = &if_open;
  dev->stop    = &if_close;
  dev->hard_header  = &if_header;
  dev->rebuild_header  = &if_rebuild_hdr;
  dev->hard_start_xmit  = &if_send;
  dev->get_stats    = &if_stats;

  /* Initialize media-specific parameters */
  dev->family    = AF_INET;  /* address family */
  dev->type    = ARPHRD_DLCI;  /* ARP h/w type */
  dev->mtu    = FR_CHANNEL_MTU;
  dev->hard_header_len  = FR_HEADER_LEN;/* media header length */
  dev->addr_len    = 2;    /* hardware address length */
  *(WORD*)dev->dev_addr = htons(chan->dlci);

  /* Initialize hardware parameters (just for reference) */
  dev->irq  = wandev->irq;
  dev->dma  = wandev->dma;
  dev->base_addr  = wandev->ioport;
  dev->mem_start  = wandev->maddr;
  dev->mem_end  = wandev->maddr + wandev->msize - 1;

  /* Initialize socket buffers */
  for (i = 0; i < DEV_NUMBUFFS; ++i)
    skb_queue_head_init(&dev->buffs[i])
  ;
  set_chan_state(dev, WAN_DISCONNECTED);
  return 0;
}

/*============================================================================
 * Open network interface.
 * o if this is the first open, then enable communications and interrupts.
 * o prevent module from unloading by incrementing use count
 *
 * Return 0 if O.k. or errno.
 */
static int if_open (struct device* dev)
{
  fr_channel_t* chan = dev->priv;
  sdla_t* card = chan->card;
  int err = 0;

  if (dev->start)
    return -EBUSY    /* only one open is allowed */
  ;
  if (set_bit(0, (void*)&card->wandev.critical))
    return -EAGAIN;
  ;
  if (!card->open_cnt)
  {
    if ((fr_comm_enable(card)) ||
        (fr_set_intr_mode(card, 0x03, card->wandev.mtu)))
    {
      err = -EIO;
      goto done;
    }
    if (card->wandev.station == WANOPT_CPE)
    {
      /* CPE: issue full status enquiry */
      fr_issue_isf(card, FR_ISF_FSE);
    }
    else  /* Switch: activate DLCI(s) */
    {
      fr_add_dlci(card,
        card->u.f.node_dlci, card->u.f.dlci_num)
      ;
      fr_activate_dlci(card,
        card->u.f.node_dlci, card->u.f.dlci_num)
      ;
    }
    wanpipe_set_state(card, WAN_CONNECTED);
  }
  dev->mtu = min(dev->mtu, card->wandev.mtu - FR_HEADER_LEN);
  dev->interrupt = 0;
  dev->tbusy = 0;
  dev->start = 1;
  wanpipe_open(card);
  update_chan_state(dev);

done:
  card->wandev.critical = 0;
  return err;
}

/*============================================================================
 * Close network interface.
 * o if this is the last open, then disable communications and interrupts.
 * o reset flags.
 */
static int if_close (struct device* dev)
{
  fr_channel_t* chan = dev->priv;
  sdla_t* card = chan->card;

  if (set_bit(0, (void*)&card->wandev.critical))
    return -EAGAIN;
  ;
  dev->start = 0;
  wanpipe_close(card);
  if (!card->open_cnt)
  {
    wanpipe_set_state(card, WAN_DISCONNECTED);
    fr_set_intr_mode(card, 0, 0);
    fr_comm_disable(card);
  }
  card->wandev.critical = 0;
  return 0;
}

/*============================================================================
 * Build media header.
 * o encapsulate packet according to encapsulation type.
 *
 * The trick here is to put packet type (Ethertype) into 'protocol' field of
 * the socket buffer, so that we don't forget it.  If encapsulation fails,
 * set skb->protocol to 0 and discard packet later.
 *
 * Return:  media header length.
 */
static int if_header (struct sk_buff* skb, struct device* dev,
  WORD type, void* daddr, void* saddr, unsigned len)
{
  int hdr_len = 0;

  skb->protocol = type;
  hdr_len = wan_encapsulate(skb, dev);
  if (hdr_len < 0)
  {
    hdr_len = 0;
    skb->protocol = 0;
  }
  skb_push(skb, 1);
  skb->data[0] = Q922_UI;
  ++hdr_len;
  return hdr_len;
}

/*============================================================================
 * Re-build media header.
 *
 * Return:  1  physical address resolved.
 *    0  physical address not resolved
 */
static int if_rebuild_hdr (void* hdr, struct device* dev, DWORD raddr,
         struct sk_buff* skb)
{
  fr_channel_t* chan = dev->priv;
  sdla_t* card = chan->card;

  printk(KERN_INFO "%s: rebuild_header() called for interface %s!\n",
    card->devname, dev->name)
  ;
  return 1;
}

/*============================================================================
 * Send a packet on a network interface.
 * o set tbusy flag (marks start of the transmission) to block a timer-based
 *   transmit from overlapping.
 * o check link state. If link is not up, then drop the packet.
 * o check channel status. If it's down then initiate a call.
 * o pass a packet to corresponding WAN device.
 * o free socket buffer
 *
 * Return:  0  complete (socket buffer must be freed)
 *    non-0  packet may be re-transmitted (tbusy must be set)
 *
 * Notes:
 * 1. This routine is called either by the protocol stack or by the "net
 *    bottom half" (with interrupts enabled).
 * 2. Setting tbusy flag will inhibit further transmit requests from the
 *    protocol stack and can be used for flow control with protocol layer.
 */
static int if_send (struct sk_buff* skb, struct device* dev)
{
  fr_channel_t* chan = dev->priv;
  sdla_t* card = chan->card;
  int retry = 0;

  if (skb == NULL)
  {
    /* If we get here, some higher layer thinks we've missed an
     * tx-done interrupt.
     */
#ifdef _DEBUG_
    printk(KERN_INFO "%s: interface %s got kicked!\n",
      card->devname, dev->name)
    ;
#endif
    dev_tint(dev);
    return 0;
  }

  if (set_bit(0, (void*)&card->wandev.critical))
  {
#ifdef _DEBUG_
    printk(KERN_INFO "%s: if_send() hit critical section!\n",
      card->devname)
    ;
#endif
    return 1;
  }

  if (set_bit(0, (void*)&dev->tbusy))
  {
#ifdef _DEBUG_
    printk(KERN_INFO "%s: Tx collision on interface %s!\n",
      card->devname, dev->name)
    ;
#endif
    chan->ifstats.tx_collisions++;
    retry = 1;
  }
  else if ((card->wandev.state != WAN_CONNECTED) ||
      (chan->state != WAN_CONNECTED))
    ++chan->ifstats.tx_dropped
  ;
  else if (!is_tx_ready(card))
    retry = 1
  ;
  else
  {
     int err = (card->hw.fwid == SFID_FR508) ?
      fr508_send(card, chan->dlci, 0, skb->len, skb->data) :
      fr502_send(card, chan->dlci, 0, skb->len, skb->data)
    ;
    if (err) ++chan->ifstats.tx_errors;
    else ++chan->ifstats.tx_packets;
  }
  if (!retry)
  {
    dev_kfree_skb(skb, FREE_WRITE);
    dev->tbusy = 0;
  }
  card->wandev.critical = 0;
  return retry;
}

/*============================================================================
 * Get ethernet-style interface statistics.
 * Return a pointer to struct enet_statistics.
 */
static struct enet_statistics* if_stats (struct device* dev)
{
  fr_channel_t* chan = dev->priv;

  return &chan->ifstats;
}

/****** Interrupt Handlers **************************************************/

/*============================================================================
 * S502 frame relay interrupt service routine.
 */
static void fr502_isr (sdla_t* card)
{
  fr502_flags_t* flags = card->flags;

  switch (flags->iflag)
  {
  case 0x01:  /* receive interrupt */
    fr502_rx_intr(card);
    break;

  case 0x02:  /* transmit interrupt */
    flags->imask &= ~0x02;
    tx_intr(card);
    break;

  default:
    spur_intr(card);
  }
  flags->iflag = 0;
}

/*============================================================================
 * S508 frame relay interrupt service routine.
 */
static void fr508_isr (sdla_t* card)
{
  fr508_flags_t* flags = card->flags;
  fr_buf_ctl_t* bctl;

  switch (flags->iflag)
  {
  case 0x01:  /* receive interrupt */
    fr508_rx_intr(card);
    break;

  case 0x02:  /* transmit interrupt */
    bctl = (void*)(flags->tse_offs - FR_MB_VECTOR +
      card->hw.dpmbase)
    ;
    bctl->flag = 0x90;  /* disable further Tx interrupts */
    tx_intr(card);
    break;

  default:
    spur_intr(card);
  }
  flags->iflag = 0;
}

/*============================================================================
 * Receive interrupt handler.
 */
static void fr502_rx_intr (sdla_t* card)
{
  fr_mbox_t* mbox = card->rxmb;
  struct sk_buff* skb;
  struct device* dev;
  fr_channel_t* chan;
  unsigned dlci, len;
  void* buf;

  sdla_mapmem(&card->hw, FR502_RX_VECTOR);
  dlci = mbox->cmd.dlci;
  len  = mbox->cmd.length;

  /* Find network interface for this packet */
  dev = find_channel(card, dlci);
  if (dev == NULL)
  {
    /* Invalid channel, discard packet */
    printk(KERN_INFO "%s: receiving on orphaned DLCI %d!\n",
      card->devname, dlci)
    ;
    goto rx_done;
  }
  chan = dev->priv;
  if (!dev->start)
  {
    ++chan->ifstats.rx_dropped;
    goto rx_done;
  }

  /* Allocate socket buffer */
  skb = dev_alloc_skb(len);
  if (skb == NULL)
  {
    printk(KERN_INFO "%s: no socket buffers available!\n",
      card->devname)
    ;
    ++chan->ifstats.rx_dropped;
    goto rx_done;
  }

  /* Copy data to the socket buffer */
  buf = skb_put(skb, len);
  memcpy(buf, mbox->data, len);
  sdla_mapmem(&card->hw, FR_MB_VECTOR);

  /* Decapsulate packet and pass it up the protocol stack */
  skb->dev = dev;
  buf = skb_pull(skb, 1);  /* remove hardware header */
  if (!wan_type_trans(skb, dev))
  {
    /* can't decapsulate packet */
    dev_kfree_skb(skb, FREE_READ);
    ++chan->ifstats.rx_errors;
  }
  else
  {
    netif_rx(skb);
    ++chan->ifstats.rx_packets;
  }

rx_done:
  sdla_mapmem(&card->hw, FR_MB_VECTOR);
}

/*============================================================================
 * Receive interrupt handler.
 */
static void fr508_rx_intr (sdla_t* card)
{
  fr_buf_ctl_t* frbuf = card->rxmb;
  struct sk_buff* skb;
  struct device* dev;
  fr_channel_t* chan;
  unsigned dlci, len, offs;
  void* buf;

  if (frbuf->flag != 0x01)
  {
    printk(KERN_INFO "%s: corrupted Rx buffer @ 0x%X!\n",
      card->devname, (unsigned)frbuf)
    ;
    return;
  }
  len  = frbuf->length;
  dlci = frbuf->dlci;
  offs = frbuf->offset;

  /* Find network interface for this packet */
  dev = find_channel(card, dlci);
  if (dev == NULL)
  {
    /* Invalid channel, discard packet */
    printk(KERN_INFO "%s: receiving on orphaned DLCI %d!\n",
      card->devname, dlci)
    ;
    goto rx_done;
  }
  chan = dev->priv;
  if (!dev->start)
  {
    ++chan->ifstats.rx_dropped;
    goto rx_done;
  }

  /* Allocate socket buffer */
  skb = dev_alloc_skb(len);
  if (skb == NULL)
  {
    printk(KERN_INFO "%s: no socket buffers available!\n",
      card->devname)
    ;
    ++chan->ifstats.rx_dropped;
    goto rx_done;
  }

  /* Copy data to the socket buffer */
  if ((offs + len) > card->u.f.rx_top)
  {
    unsigned tmp = card->u.f.rx_top - offs;

    buf = skb_put(skb, tmp);
    sdla_peek(&card->hw, offs, buf, tmp);
    offs = card->u.f.rx_base;
    len -= tmp;
  }
  buf = skb_put(skb, len);
  sdla_peek(&card->hw, offs, buf, len);

  /* Decapsulate packet and pass it up the protocol stack */
  skb->dev = dev;
  buf = skb_pull(skb, 1);  /* remove hardware header */
  if (!wan_type_trans(skb, dev))
  {
    /* can't decapsulate packet */
    dev_kfree_skb(skb, FREE_READ);
    ++chan->ifstats.rx_errors;
  }
  else
  {
    netif_rx(skb);
    ++chan->ifstats.rx_packets;
  }

rx_done:
  /* Release buffer element and calculate a pointer to the next one */
  frbuf->flag = 0;
  card->rxmb = ++frbuf;
  if ((void*)frbuf > card->u.f.rxmb_last)
    card->rxmb = card->u.f.rxmb_base
  ;
}

/*============================================================================
 * Transmit interrupt handler.
 * o print a warning
 * o 
 * If number of spurious interrupts exceeded some limit, then ???
 */
static void tx_intr (sdla_t* card)
{
  printk(KERN_INFO "%s: transmit interrupt!\n", card->devname);
}

/*============================================================================
 * Spurious interrupt handler.
 * o print a warning
 * o 
 * If number of spurious interrupts exceeded some limit, then ???
 */
static void spur_intr (sdla_t* card)
{
  printk(KERN_INFO "%s: spurious interrupt!\n", card->devname);
}

/****** Background Polling Routines  ****************************************/

/*============================================================================
 * Main polling routine.
 * This routine is repeatedly called by the WANPIPE 'thead' to allow for
 * time-dependent housekeeping work.
 *
 * o fetch asynchronous network events.
 *
 * Notes:
 * 1. This routine may be called on interrupt context with all interrupts
 *    enabled. Beware!
 */
static void wpf_poll (sdla_t* card)
{
  fr502_flags_t* flags = card->flags;

  if (flags->event)
  {
    fr_mbox_t* mbox = card->mbox;
    int err;

    memset(&mbox->cmd, 0, sizeof(fr_cmd_t));
    mbox->cmd.command = FR_READ_STATUS;
    err = sdla_exec(mbox) ? mbox->cmd.result : CMD_TIMEOUT;
    if (err) fr_event(card, err, mbox);
  }
}

/****** Frame Relay Firmware-Specific Functions *****************************/

/*============================================================================
 * Read firmware code version.
 * o fill string str with firmware version info. 
 */
static int fr_read_version (sdla_t* card, char* str)
{
  fr_mbox_t* mbox = card->mbox;
  int retry = MAX_CMD_RETRY;
  int err;

  do
  {
    memset(&mbox->cmd, 0, sizeof(fr_cmd_t));
    mbox->cmd.command = FR_READ_CODE_VERSION;
    err = sdla_exec(mbox) ? mbox->cmd.result : CMD_TIMEOUT;
  }
  while (err && retry-- && fr_event(card, err, mbox));
  if (!err && str)
  {
    int len = mbox->cmd.length;

    memcpy(str, mbox->data, len);
          str[len] = '\0';
  }
  return err;
}

/*============================================================================
 * Set global configuration.
 */
static int fr_configure (sdla_t* card, fr_conf_t *conf)
{
  fr_mbox_t* mbox = card->mbox;
  int retry = MAX_CMD_RETRY;
  int dlci = card->u.f.node_dlci;
  int dlci_num = card->u.f.dlci_num;
  int err, i;

  do
  {
    memset(&mbox->cmd, 0, sizeof(fr_cmd_t));
    memcpy(mbox->data, conf, sizeof(fr_conf_t));
    if (dlci_num) for (i = 0; i < dlci_num; ++i)
      ((fr_conf_t*)mbox->data)->dlci[i] = dlci + i
    ;
    mbox->cmd.command = FR_SET_CONFIG;
    mbox->cmd.length =
      sizeof(fr_conf_t) + dlci_num * sizeof(short)
    ;
    err = sdla_exec(mbox) ? mbox->cmd.result : CMD_TIMEOUT;
  }
  while (err && retry-- && fr_event(card, err, mbox));
  return err;
}

/*============================================================================
 * Set interrupt mode.
 */
static int fr_set_intr_mode (sdla_t* card, unsigned mode, unsigned mtu)
{
  fr_mbox_t* mbox = card->mbox;
  int retry = MAX_CMD_RETRY;
  int err;

  do
  {
    memset(&mbox->cmd, 0, sizeof(fr_cmd_t));
    if (card->hw.fwid == SFID_FR502)
    {
      fr502_intr_ctl_t* ictl = (void*)mbox->data;

      memset(ictl, 0, sizeof(fr502_intr_ctl_t));
      ictl->mode   = mode;
      ictl->tx_len = mtu;
      mbox->cmd.length = sizeof(fr502_intr_ctl_t);
    }
    else
    {
      fr508_intr_ctl_t* ictl = (void*)mbox->data;

      memset(ictl, 0, sizeof(fr508_intr_ctl_t));
      ictl->mode   = mode;
      ictl->tx_len = mtu;
      ictl->irq    = card->hw.irq;
      mbox->cmd.length = sizeof(fr508_intr_ctl_t);
    }
    mbox->cmd.command = FR_SET_INTR_MODE;
    err = sdla_exec(mbox) ? mbox->cmd.result : CMD_TIMEOUT;
  }
  while (err && retry-- && fr_event(card, err, mbox));
  return err;
}

/*============================================================================
 * Enable communications.
 */
static int fr_comm_enable (sdla_t* card)
{
  fr_mbox_t* mbox = card->mbox;
  int retry = MAX_CMD_RETRY;
  int err;

  do
  {
    memset(&mbox->cmd, 0, sizeof(fr_cmd_t));
    mbox->cmd.command = FR_COMM_ENABLE;
    err = sdla_exec(mbox) ? mbox->cmd.result : CMD_TIMEOUT;
  }
  while (err && retry-- && fr_event(card, err, mbox));
  return err;
}

/*============================================================================
 * Disable communications. 
 */
static int fr_comm_disable (sdla_t* card)
{
  fr_mbox_t* mbox = card->mbox;
  int retry = MAX_CMD_RETRY;
  int err;

  do
  {
    memset(&mbox->cmd, 0, sizeof(fr_cmd_t));
    mbox->cmd.command = FR_COMM_DISABLE;
    err = sdla_exec(mbox) ? mbox->cmd.result : CMD_TIMEOUT;
  }
  while (err && retry-- && fr_event(card, err, mbox));
  return err;
}

/*============================================================================
 * Add DLCI(s) (Access Node only!). 
 */
static int fr_add_dlci (sdla_t* card, int dlci, int num)
{
  fr_mbox_t* mbox = card->mbox;
  int retry = MAX_CMD_RETRY;
  int err, i;

  do
  {
    WORD* dlci_list = (void*)mbox->data;

    memset(&mbox->cmd, 0, sizeof(fr_cmd_t));
    for (i = 0; i < num; ++i)
      dlci_list[i] = dlci + i
    ;
    mbox->cmd.length  = num * sizeof(short);
    mbox->cmd.command = FR_ADD_DLCI;
    err = sdla_exec(mbox) ? mbox->cmd.result : CMD_TIMEOUT;
  }
  while (err && retry-- && fr_event(card, err, mbox));
  return err;
}

/*============================================================================
 * Activate DLCI(s) (Access Node only!). 
 */
static int fr_activate_dlci (sdla_t* card, int dlci, int num)
{
  fr_mbox_t* mbox = card->mbox;
  int retry = MAX_CMD_RETRY;
  int err, i;

  do
  {
    WORD* dlci_list = (void*)mbox->data;

    memset(&mbox->cmd, 0, sizeof(fr_cmd_t));
    for (i = 0; i < num; ++i)
      dlci_list[i] = dlci + i
    ;
    mbox->cmd.length  = num * sizeof(short);
    mbox->cmd.command = FR_ACTIVATE_DLCI;
    err = sdla_exec(mbox) ? mbox->cmd.result : CMD_TIMEOUT;
  }
  while (err && retry-- && fr_event(card, err, mbox));
  return err;
}

/*============================================================================
 * Issue in-channel signalling frame. 
 */
static int fr_issue_isf (sdla_t* card, int isf)
{
  fr_mbox_t* mbox = card->mbox;
  int retry = MAX_CMD_RETRY;
  int err;

  do
  {
    memset(&mbox->cmd, 0, sizeof(fr_cmd_t));
    mbox->data[0] = isf;
    mbox->cmd.length  = 1;
    mbox->cmd.command = FR_ISSUE_IS_FRAME;
    err = sdla_exec(mbox) ? mbox->cmd.result : CMD_TIMEOUT;
  }
  while (err && retry-- && fr_event(card, err, mbox));
  return err;
}

/*============================================================================
 * Send a frame (S502 version). 
 */
static int fr502_send (sdla_t* card, int dlci, int attr, int len, void *buf)
{
  fr_mbox_t* mbox = card->mbox;
  int retry = MAX_CMD_RETRY;
  int err;

  do
  {
    memset(&mbox->cmd, 0, sizeof(fr_cmd_t));
    memcpy(mbox->data, buf, len);
    mbox->cmd.dlci    = dlci;
    mbox->cmd.attr    = attr;
    mbox->cmd.length  = len;
    mbox->cmd.command = FR_WRITE;
    err = sdla_exec(mbox) ? mbox->cmd.result : CMD_TIMEOUT;
  }
  while (err && retry-- && fr_event(card, err, mbox));
  return err;
}

/*============================================================================
 * Send a frame (S508 version). 
 */
static int fr508_send (sdla_t* card, int dlci, int attr, int len, void *buf)
{
  fr_mbox_t* mbox = card->mbox;
  int retry = MAX_CMD_RETRY;
  int err;

  do
  {
    memset(&mbox->cmd, 0, sizeof(fr_cmd_t));
    mbox->cmd.dlci    = dlci;
    mbox->cmd.attr    = attr;
    mbox->cmd.length  = len;
    mbox->cmd.command = FR_WRITE;
    err = sdla_exec(mbox) ? mbox->cmd.result : CMD_TIMEOUT;
  }
  while (err && retry-- && fr_event(card, err, mbox));

  if (!err)
  {
    fr_buf_ctl_t* frbuf = (void*)(*(DWORD*)mbox->data -
      FR_MB_VECTOR + card->hw.dpmbase)
    ;
    DWORD flags;

    save_flags(flags);
    cli();
    sdla_poke(&card->hw, frbuf->offset, buf, len);
    frbuf->flag = 0x01;
    restore_flags(flags);
  }
  return err;
}

/****** Firmware Asynchronous Event Handlers ********************************/

/*============================================================================
 * Main asyncronous event/error handler.
 *  This routine is called whenever firmware command returns non-zero
 *  return code.
 *
 * Return zero if previous command has to be cancelled.
 */
static int fr_event (sdla_t *card, int event, fr_mbox_t* mbox)
{
  switch (event)
  {
  case FRRES_MODEM_FAILURE:
    return fr_modem_failure(card, mbox);

  case FRRES_CHANNEL_DOWN:
    wanpipe_set_state(card, WAN_DISCONNECTED);
    return 1;

  case FRRES_CHANNEL_UP:
    wanpipe_set_state(card, WAN_CONNECTED);
    return 1;

  case FRRES_DLCI_CHANGE:
    return fr_dlci_change(card, mbox);

  case FRRES_DLCI_MISMATCH:
    printk(KERN_INFO "%s: DLCI list mismatch!\n", card->devname);
    return 1;

  case CMD_TIMEOUT:
    printk(KERN_ERR "%s: command 0x%02X timed out!\n",
      card->devname, mbox->cmd.command)
    ;
    break;

  default:
    printk(KERN_INFO "%s: command 0x%02X returned 0x%02X!\n",
      card->devname, mbox->cmd.command, event)
    ;
  }
  return 0;
}

/*============================================================================
 * Handle modem error.
 *
 * Return zero if previous command has to be cancelled.
 */
static int fr_modem_failure (sdla_t *card, fr_mbox_t* mbox)
{
  printk(KERN_INFO "%s: physical link down! (modem error 0x%02X)\n",
    card->devname, mbox->data[0])
  ;
  switch (mbox->cmd.command)
  {
  case FR_WRITE:
  case FR_READ:
    return 0;
  }
  return 1;
}

/*============================================================================
 * Handle DLCI status change.
 *
 * Return zero if previous command has to be cancelled.
 */
static int fr_dlci_change (sdla_t *card, fr_mbox_t* mbox)
{
  dlci_status_t* status = (void*)mbox->data;
  int cnt = mbox->cmd.length / sizeof(dlci_status_t);

  for (; cnt; --cnt, ++status)
  {
    WORD dlci = status->dlci;
    struct device* dev = find_channel(card, dlci);

    if (status->state & 0x01)
    {
      printk(KERN_INFO
        "%s: DLCI %u has been deleted!\n",
        card->devname, dlci)
      ;
      if (dev && dev->start)
        set_chan_state(dev, WAN_DISCONNECTED)
      ;
    }
    else if (status->state & 0x02)
    {
      printk(KERN_INFO
        "%s: DLCI %u becomes active!\n",
        card->devname, dlci)
      ;
      if (dev && dev->start)
        set_chan_state(dev, WAN_CONNECTED)
      ;
    }
  }
  return 1;
}

/******* Miscellaneous ******************************************************/

/*============================================================================
 * Update channel state. 
 */
static int update_chan_state (struct device* dev)
{
  fr_channel_t* chan = dev->priv;
  sdla_t* card = chan->card;
  fr_mbox_t* mbox = card->mbox;
  int retry = MAX_CMD_RETRY;
  int err;

  do
  {
    memset(&mbox->cmd, 0, sizeof(fr_cmd_t));
    mbox->cmd.command = FR_LIST_ACTIVE_DLCI;
    err = sdla_exec(mbox) ? mbox->cmd.result : CMD_TIMEOUT;
  }
  while (err && retry-- && fr_event(card, err, mbox));

  if (!err)
  {
    WORD* list = (void*)mbox->data;
    int cnt = mbox->cmd.length / sizeof(short);

    for (; cnt; --cnt, ++list)
    {
      if (*list == chan->dlci)
      {
         set_chan_state(dev, WAN_CONNECTED);
        break;
      }
    }
  }
  return err;
}

/*============================================================================
 * Set channel state.
 */
static void set_chan_state (struct device* dev, int state)
{
  fr_channel_t* chan = dev->priv;
  sdla_t* card = chan->card;
  DWORD flags;

  save_flags(flags);
  cli();
  if (chan->state != state)
  {
    switch (state)
    {
    case WAN_CONNECTED:
      printk (KERN_INFO "%s: interface %s connected!\n",
        card->devname, dev->name)
      ;
      break;

    case WAN_CONNECTING:
      printk (KERN_INFO "%s: interface %s connecting...\n",
        card->devname, dev->name)
      ;
      break;

    case WAN_DISCONNECTED:
      printk (KERN_INFO "%s: interface %s disconnected!\n",
        card->devname, dev->name)
      ;
      break;
    }
    chan->state = state;
  }
  chan->state_tick = jiffies;
  restore_flags(flags);
}

/*============================================================================
 * Find network device by its channel number.
 */
static struct device* find_channel (sdla_t* card, unsigned dlci)
{
  struct device* dev;

  for (dev = card->wandev.dev; dev; dev = dev->slave)
    if (((fr_channel_t*)dev->priv)->dlci == dlci) break
  ;
  return dev;
}

/*============================================================================
 * Check to see if a frame can be sent. If no transmit buffers available,
 * enable transmit interrupts.
 *
 * Return:  1 - Tx buffer(s) available
 *    0 - no buffers available
 */
static int is_tx_ready (sdla_t* card)
{
  if (card->hw.fwid == SFID_FR508)
  {
    fr508_flags_t* flags = card->flags;
    BYTE sb = inb(card->hw.port);

    if (sb & 0x02) return 1;
    flags->imask |= 0x02;
  }
  else
  {
    fr502_flags_t* flags = card->flags;

    if (flags->tx_ready) return 1;
    flags->imask |= 0x02;
  }
  return 0;
}

/*============================================================================
 * Convert decimal string to DWORDeger.
 * If len != 0 then only 'len' characters of the string are converted.
 */
static DWORD dec_to_uint (BYTE* str, int len)
{
  unsigned val;

  if (!len) len = strlen(str);
  for (val = 0; len && is_digit(*str); ++str, --len)
    val = (val * 10) + (*str - (unsigned)'0')
  ;
  return val;
}

/*============================================================================
 * Convert hex string to DWORDeger.
 * If len != 0 then only 'len' characters of the string are conferted.
 */
static DWORD hex_to_uint (BYTE* str, int len)
{
  unsigned val, ch;

  if (!len) len = strlen(str);
  for (val = 0; len; ++str, --len)
  {
    ch = *str;
    if (is_digit(ch))
      val = (val << 4) + (ch - (unsigned)'0')
    ;
    else if (is_hex_digit(ch))
      val = (val << 4) + ((ch & 0xDF) - (unsigned)'A' + 10)
    ;
    else break;
  }
  return val;
}

/****** End *****************************************************************/
