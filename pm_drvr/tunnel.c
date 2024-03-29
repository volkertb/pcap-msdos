/* tunnel.c: an IP tunnel driver

  The purpose of this driver is to provide an IP tunnel through
  which you can tunnel network traffic transparently across subnets.

  This was written by looking at Nick Holloway's dummy driver
  Thanks for the great code!

    -Sam Lantinga  (slouken@cs.ucdavis.edu)  02/01/95
    
  Minor tweaks:
    Cleaned up the code a little and added some pre-1.3.0 tweaks.
    dev->hard_header/hard_header_len changed to use no headers.
    Comments/bracketing tweaked.
    Made the tunnels use dev->name not tunnel: when error reporting.
    Added tx_dropped stat
    
    -Alan Cox  (Alan.Cox@linux.org) 21 March 95

  Reworked:
    Changed to tunnel to destination gateway in addition to the
      tunnel's pointopoint address
    Almost completely rewritten
    Note:  There is currently no firewall or ICMP handling done.

    -Sam Lantinga  (slouken@cs.ucdavis.edu) 02/13/96
    
*/

/* Things I wish I had known when writing the tunnel driver:

  When the tunnel_xmit() function is called, the skb contains the
  packet to be sent (plus a great deal of extra info), and dev
  contains the tunnel device that _we_ are.

  When we are passed a packet, we are expected to fill in the
  source address with our source IP address.

  What is the proper way to allocate, copy and free a buffer?
  After you allocate it, it is a "0 length" chunk of memory
  starting at zero.  If you want to add headers to the buffer
  later, you'll have to call "skb_reserve(skb, amount)" with
  the amount of memory you want reserved.  Then, you call
  "skb_put(skb, amount)" with the amount of space you want in
  the buffer.  skb_put() returns a pointer to the top (#0) of
  that buffer.  skb->len is set to the amount of space you have
  "allocated" with skb_put().  You can then write up to skb->len
  bytes to that buffer.  If you need more, you can call skb_put()
  again with the additional amount of space you need.  You can
  find out how much more space you can allocate by calling 
  "skb_tailroom(skb)".
  Now, to add header space, call "skb_push(skb, header_len)".
  This creates space at the beginning of the buffer and returns
  a pointer to this new space.  If later you need to strip a
  header from a buffer, call "skb_pull(skb, header_len)".
  skb_headroom() will return how much space is left at the top
  of the buffer (before the main data).  Remember, this headroom
  space must be reserved before the skb_put() function is called.
*/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <net/ip.h>
#include <linux/if_arp.h>

/*#define TUNNEL_DEBUG*/

/* 
 *  Our header is a simple IP packet with no options
 */
 
#define tunnel_hlen  sizeof(struct iphdr)

/*
 *  Okay, this needs to be high enough that we can fit a "standard"
 *  ethernet header and an IP tunnel header into the outgoing packet.
 *  [36 bytes]
 */
 
#define TUNL_HLEN  (((ETH_HLEN+15)&~15)+tunnel_hlen)


static int tunnel_open(struct device *dev)
{
  MOD_INC_USE_COUNT;
  return 0;
}

static int tunnel_close(struct device *dev)
{
  MOD_DEC_USE_COUNT;
  return 0;
}

#ifdef TUNNEL_DEBUG
void print_ip(struct iphdr *ip)
{
  BYTE *ipaddr;

  printk("IP packet:\n");
  printk("--- header len = %d\n", ip->ihl*4);
  printk("--- ip version: %d\n", ip->version);
  printk("--- ip protocol: %d\n", ip->protocol);
  ipaddr=(BYTE *)&ip->saddr;
  printk("--- source address: %u.%u.%u.%u\n", 
      *ipaddr, *(ipaddr+1), *(ipaddr+2), *(ipaddr+3));
  ipaddr=(BYTE *)&ip->daddr;
  printk("--- destination address: %u.%u.%u.%u\n", 
      *ipaddr, *(ipaddr+1), *(ipaddr+2), *(ipaddr+3));
  printk("--- total packet len: %d\n", ntohs(ip->tot_len));
}
#endif

/*
 *  This function assumes it is being called from dev_queue_xmit()
 *  and that skb is filled properly by that function.
 */

static int tunnel_xmit(struct sk_buff *skb, struct device *dev)
{
  struct net_device_stats *stats;    /* This device's statistics */
  struct rtable *rt;           /* Route to the other host */
  struct device *tdev;      /* Device to other host */
  struct iphdr  *iph;      /* Our new IP header */
  int    max_headroom;      /* The extra header space needed */

  stats = (struct net_device_stats *)dev->priv;
  
  /*
   *  First things first.  Look up the destination address in the 
   *  routing tables
   */
  iph = skb->nh.iph;

  if (ip_route_output(&rt, dev->pa_dstaddr, dev->pa_addr, RT_TOS(iph->tos), NULL)) {
    /* No route to host */
    printk ( KERN_INFO "%s: Can't reach target gateway!\n", dev->name);
    stats->tx_errors++;
    dev_kfree_skb(skb, FREE_WRITE);
    return 0;
  }
  tdev = rt->u.dst.dev;

  if (tdev->type == ARPHRD_TUNNEL) { 
    /* Tunnel to tunnel?  -- I don't think so. */
    printk ( KERN_INFO "%s: Packet targetted at myself!\n" , dev->name);
    ip_rt_put(rt);
    stats->tx_errors++;
    dev_kfree_skb(skb, FREE_WRITE);
    return 0;
  }

  skb->h.ipiph = skb->nh.iph;

  /*
   * Okay, now see if we can stuff it in the buffer as-is.
   */
  max_headroom = (((tdev->hard_header_len+15)&~15)+tunnel_hlen);

  if (skb_headroom(skb) < max_headroom || skb_shared(skb)) {
    struct sk_buff *new_skb = skb_realloc_headroom(skb, max_headroom);
    if (!new_skb) {
      ip_rt_put(rt);
        stats->tx_dropped++;
      dev_kfree_skb(skb, FREE_WRITE);
      return 0;
    }
    dev_kfree_skb(skb, FREE_WRITE);
    skb = new_skb;
  }

  skb->nh.iph = (struct iphdr *) skb_push(skb, tunnel_hlen);
  dst_release(skb->dst);
  memset(&(IPCB(skb)->opt), 0, sizeof(IPCB(skb)->opt));
  dst_release(skb->dst);
  skb->dst = &rt->u.dst;

  /*
   *  Push down and install the IPIP header.
   */
   
  iph       =  skb->nh.iph;
  iph->version    =   4;
  iph->tos    =  skb->h.ipiph->tos;
  iph->ttl    =  skb->h.ipiph->ttl;
  iph->frag_off    =  0;
  iph->daddr    =  dev->pa_dstaddr;
  iph->saddr    =  dev->pa_addr;
  iph->protocol    =  IPPROTO_IPIP;
  iph->ihl    =  5;
  iph->tot_len    =  htons(skb->len);
  iph->id      =  htons(ip_id_count++);  /* Race condition here? */
  ip_send_check(iph);

  stats->tx_bytes+=skb->len;
  
  ip_send(skb);

  /* Record statistics and return */
  stats->tx_packets++;
  return 0;
}

static struct net_device_stats *tunnel_get_stats(struct device *dev)
{
  return((struct net_device_stats*) dev->priv);
}

/*
 *  Called when a new tunnel device is initialized.
 *  The new tunnel device structure is passed to us.
 */
 
int tunnel_init(struct device *dev)
{
  /* Oh, just say we're here, in case anyone cares */
  static int tun_msg=0;
  if(!tun_msg)
  {
    printk ( KERN_INFO "tunnel: version v0.2b2\n" );
    tun_msg=1;
  }

  /* Add our tunnel functions to the device */
  dev->open    = tunnel_open;
  dev->stop    = tunnel_close;
  dev->hard_start_xmit  = tunnel_xmit;
  dev->get_stats    = tunnel_get_stats;
  dev->priv = kmalloc(sizeof(struct net_device_stats), GFP_KERNEL);
  if (dev->priv == NULL)
    return -ENOMEM;
  memset(dev->priv, 0, sizeof(struct net_device_stats));

  /* Initialize the tunnel device structure */
  
  dev_init_buffers(dev);

  dev->hard_header  = NULL;
  dev->rebuild_header   = NULL;
  dev->set_mac_address   = NULL;
  dev->hard_header_cache   = NULL;
  dev->header_cache_update= NULL;

  dev->type        = ARPHRD_TUNNEL;
  dev->hard_header_len   = TUNL_HLEN;
  dev->mtu    = 1500-tunnel_hlen;   /* eth_mtu */
  dev->addr_len    = 0;    /* Is this only for ARP? */
  dev->tx_queue_len  = 2;    /* Small queue */
  memset(dev->broadcast,0xFF, ETH_ALEN);

  /* New-style flags. */
  dev->flags    = IFF_NOARP;  /* Don't use ARP on this device */
            /* No broadcasting through a tunnel */
  dev->family    = AF_INET;
  dev->pa_addr    = 0;
  dev->pa_brdaddr   = 0;
  dev->pa_mask    = 0;
  dev->pa_alen    = 4;

  /* We're done.  Have I forgotten anything? */
  return 0;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*  Module specific interface                                      */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#ifdef MODULE

static int tunnel_probe(struct device *dev)
{
  tunnel_init(dev);
  return 0;
}

static struct device dev_tunnel = 
{
  "tunl0\0   ", 
  0, 0, 0, 0,
   0x0, 0,
   0, 0, 0, NULL, tunnel_probe 
 };

int init_module(void)
{
  /* Find a name for this unit */
  int ct= 1;
  
  while(dev_get(dev_tunnel.name)!=NULL && ct<100)
  {
    sprintf(dev_tunnel.name,"tunl%d",ct);
    ct++;
  }
  
#ifdef TUNNEL_DEBUG
  printk("tunnel: registering device %s\n", dev_tunnel.name);
#endif
  if (register_netdev(&dev_tunnel) != 0)
    return -EIO;
  return 0;
}

void cleanup_module(void)
{
  unregister_netdev(&dev_tunnel);
  kfree_s(dev_tunnel.priv,sizeof(struct net_device_stats));
  dev_tunnel.priv=NULL;
}
#endif /* MODULE */

