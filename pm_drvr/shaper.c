/*
 *      Simple traffic shaper for Linux NET3.
 *
 *  (c) Copyright 1996 Alan Cox <alan@cymru.net>, All Rights Reserved.
 *        http://www.cymru.net
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *  
 *  Neither Alan Cox nor CymruNet Ltd. admit liability nor provide 
 *  warranty for any of this software. This material is provided 
 *  "AS-IS" and at no charge.  
 *
 *  
 *  Algorithm:
 *
 *  Queue Frame:
 *    Compute time length of frame at regulated speed
 *    Add frame to queue at appropriate point
 *    Adjust time length computation for followup frames
 *    Any frame that falls outside of its boundaries is freed
 *
 *  We work to the following constants
 *
 *    SHAPER_QLEN  Maximum queued frames
 *    SHAPER_LATENCY  Bounding latency on a frame. Leaving this latency
 *        window drops the frame. This stops us queueing 
 *        frames for a long time and confusing a remote
 *        host.
 *    SHAPER_MAXSLIP  Maximum time a priority frame may jump forward.
 *        That bounds the penalty we will inflict on low
 *        priority traffic.
 *    SHAPER_BURST  Time range we call "now" in order to reduce
 *        system load. The more we make this the burstier
 *        the behaviour, the better local performance you
 *        get through packet clustering on routers and the
 *        worse the remote end gets to judge rtts.
 *
 *  This is designed to handle lower speed links ( < 200K/second or so). We
 *  run off a 100-150Hz base clock typically. This gives us a resolution at
 *  200Kbit/second of about 2Kbit or 256 bytes. Above that our timer
 *  resolution may start to cause much more burstiness in the traffic. We
 *  could avoid a lot of that by calling kick_shaper() at the end of the 
 *  tied device transmissions. If you run above about 100K second you 
 *  may need to tune the supposed speed rate for the right values.
 *
 *  BUGS:
 *    Downing the interface under the shaper before the shaper
 *    will render your machine defunct. Don't for now shape over
 *    PPP or SLIP therefore!
 *    This will be fixed in BETA4
 */
 
 
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/fcntl.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <net/dst.h>
#include "shaper.h"

int sh_debug;    /* Debug flag */

#define SHAPER_BANNER  "CymruNet Traffic Shaper BETA 0.03 for Linux 2.1\n"

/*
 *  Locking
 */
 
static int shaper_lock(struct shaper *sh)
{
  DWORD flags;
  save_flags(flags);
  cli();
  /*
   *  Lock in an interrupt may fail
   */
  if(sh->locked && intr_count)
  {
    restore_flags(flags);
    return 0;
  }
  while(sh->locked)
    sleep_on(&sh->wait_queue);
  sh->locked=1;
  restore_flags(flags);
  return 1;
}

static void shaper_kick(struct shaper *sh);

static void shaper_unlock(struct shaper *sh)
{
  sh->locked=0;
  wake_up(&sh->wait_queue);
  shaper_kick(sh);
}

/*
 *  Compute clocks on a buffer
 */
  
static int shaper_clocks(struct shaper *shaper, struct sk_buff *skb)
{
   int t=skb->len/shaper->bytespertick;
   return t;
}

/*
 *  Set the speed of a shaper. We compute this in bytes per tick since
 *  thats how the machine wants to run. Quoted input is in bits per second
 *  as is traditional (note not BAUD). We assume 8 bit bytes. 
 */
  
static void shaper_setspeed(struct shaper *shaper, int bitspersec)
{
  shaper->bytespertick=(bitspersec/HZ)/8;
  if(!shaper->bytespertick)
    shaper->bytespertick++;
}

/*
 *  Throw a frame at a shaper.
 */
  
static int shaper_qframe(struct shaper *shaper, struct sk_buff *skb)
{
   struct sk_buff *ptr;
   
   /*
    *  Get ready to work on this shaper. Lock may fail if its
    *  an interrupt and locked.
    */
    
   if(!shaper_lock(shaper))
     return -1;
   ptr=shaper->sendq.prev;
   
   /*
    *  Set up our packet details
    */
    
   skb->shapelatency=0;
   skb->shapeclock=shaper->recovery;
   if(skb->shapeclock<jiffies)
     skb->shapeclock=jiffies;
   skb->priority=0;  /* short term bug fix */
   skb->shapestamp=jiffies;
   
   /*
    *  Time slots for this packet.
    */
    
   skb->shapelen= shaper_clocks(shaper,skb);
   
#ifdef SHAPER_COMPLEX /* and broken.. */

   while(ptr && ptr!=(struct sk_buff *)&shaper->sendq)
   {
     if(ptr->pri<skb->pri 
       && jiffies - ptr->shapeclock < SHAPER_MAXSLIP)
     {
       struct sk_buff *tmp=ptr->prev;

       /*
        *  It goes before us therefore we slip the length
        *  of the new frame.
        */

       ptr->shapeclock+=skb->shapelen;
       ptr->shapelatency+=skb->shapelen;

       /*
        *  The packet may have slipped so far back it
        *  fell off.
        */
       if(ptr->shapelatency > SHAPER_LATENCY)
       {
         skb_unlink(ptr);
         dev_kfree_skb(ptr, FREE_WRITE);
       }
       ptr=tmp;
     }
     else
       break;
   }
   if(ptr==NULL || ptr==(struct sk_buff *)&shaper->sendq)
     skb_queue_head(&shaper->sendq,skb);
   else
   {
     struct sk_buff *tmp;
     /*
      *  Set the packet clock out time according to the
      *  frames ahead. Im sure a bit of thought could drop
      *  this loop.
      */
     for(tmp=skb_peek(&shaper->sendq); tmp!=NULL && tmp!=ptr; tmp=tmp->next)
       skb->shapeclock+=tmp->shapelen;
     skb_append(ptr,skb);
   }
#else
  {
    struct sk_buff *tmp;
    /*
     *  Up our shape clock by the time pending on the queue
     *  (Should keep this in the shaper as a variable..)
     */
    for(tmp=skb_peek(&shaper->sendq); tmp!=NULL && 
      tmp!=(struct sk_buff *)&shaper->sendq; tmp=tmp->next)
      skb->shapeclock+=tmp->shapelen;
    /*
     *  Queue over time. Spill packet.
     */
    if(skb->shapeclock-jiffies > SHAPER_LATENCY)
      dev_kfree_skb(skb, FREE_WRITE);
    else
      skb_queue_tail(&shaper->sendq, skb);
  }
#endif   
   if(sh_debug)
     printk("Frame queued.\n");
   if(skb_queue_len(&shaper->sendq)>SHAPER_QLEN)
   {
     ptr=skb_dequeue(&shaper->sendq);
     dev_kfree_skb(ptr, FREE_WRITE);
   }
   shaper_unlock(shaper);
   shaper_kick(shaper);
   return 0;
}

/*
 *  Transmit from a shaper
 */
 
static void shaper_queue_xmit(struct shaper *shaper, struct sk_buff *skb)
{
  struct sk_buff *newskb=skb_clone(skb, GFP_ATOMIC);
  if(sh_debug)
    printk("Kick frame on %p\n",newskb);
  if(newskb)
  {
    newskb->dev=shaper->dev;
    newskb->arp=1;
    newskb->priority=2;
    if(sh_debug)
      printk("Kick new frame to %s, %d\n",
        shaper->dev->name,newskb->priority);
    dev_queue_xmit(newskb);
    if(sh_debug)
      printk("Kicked new frame out.\n");
    dev_kfree_skb(skb, FREE_WRITE);
  }
}

/*
 *  Timer handler for shaping clock
 */
 
static void shaper_timer(DWORD data)
{
  struct shaper *sh=(struct shaper *)data;
  shaper_kick(sh);
}

/*
 *  Kick a shaper queue and try and do something sensible with the 
 *  queue. 
 */

static void shaper_kick(struct shaper *shaper)
{
  struct sk_buff *skb;
  DWORD flags;
  
  save_flags(flags);
  cli();

  del_timer(&shaper->timer);

  /*
   *  Shaper unlock will kick
   */
   
  if(shaper->locked)
  {  
    if(sh_debug)
      printk("Shaper locked.\n");
    shaper->timer.expires=jiffies+1;
    add_timer(&shaper->timer);
    restore_flags(flags);
    return;
  }

    
  /*
   *  Walk the list (may be empty)
   */
   
  while((skb=skb_peek(&shaper->sendq))!=NULL)
  {
    /*
     *  Each packet due to go out by now (within an error
     *  of SHAPER_BURST) gets kicked onto the link 
     */
     
    if(sh_debug)
      printk("Clock = %d, jiffies = %ld\n", skb->shapeclock, jiffies);
    if(skb->shapeclock <= jiffies + SHAPER_BURST)
    {
      /*
       *  Pull the frame and get interrupts back on.
       */
       
      skb_unlink(skb);
      shaper->recovery=jiffies+skb->shapelen;
      restore_flags(flags);

      /*
       *  Pass on to the physical target device via
       *  our low level packet thrower.
       */
      
      skb->shapepend=0;
      shaper_queue_xmit(shaper, skb);  /* Fire */
      cli();
    }
    else
      break;
  }

  /*
   *  Next kick.
   */
   
  if(skb!=NULL)
  {
    del_timer(&shaper->timer);
    shaper->timer.expires=skb->shapeclock;
    add_timer(&shaper->timer);
  }
    
  /*
   *  Interrupts on, mission complete
   */
    
  restore_flags(flags);
}


/*
 *  Flush the shaper queues on a closedown
 */
 
static void shaper_flush(struct shaper *shaper)
{
  struct sk_buff *skb;
  while((skb=skb_dequeue(&shaper->sendq))!=NULL)
    dev_kfree_skb(skb, FREE_WRITE);
}

/*
 *  Bring the interface up. We just disallow this until a 
 *  bind.
 */

static int shaper_open(struct device *dev)
{
  struct shaper *shaper=dev->priv;
  
  /*
   *  Can't open until attached.
   */
   
  if(shaper->dev==NULL)
    return -ENODEV;
  MOD_INC_USE_COUNT;
  return 0;
}

/*
 *  Closing a shaper flushes the queues.
 */
 
static int shaper_close(struct device *dev)
{
  struct shaper *shaper=dev->priv;
  shaper_flush(shaper);
  del_timer(&shaper->timer);
  MOD_DEC_USE_COUNT;
  return 0;
}

/*
 *  Revectored calls. We alter the parameters and call the functions
 *  for our attached device. This enables us to bandwidth allocate after
 *  ARP and other resolutions and not before.
 */


static int shaper_start_xmit(struct sk_buff *skb, struct device *dev)
{
  struct shaper *sh=dev->priv;
  return shaper_qframe(sh, skb);
}

static struct net_device_stats *shaper_get_stats(struct device *dev)
{
  return NULL;
}

static int shaper_header(struct sk_buff *skb, struct device *dev, 
  WORD type, void *daddr, void *saddr, unsigned len)
{
  struct shaper *sh=dev->priv;
  if(sh_debug)
    printk("Shaper header\n");
  return sh->hard_header(skb,sh->dev,type,daddr,saddr,len);
}

static int shaper_rebuild_header(struct sk_buff *skb)
{
  struct shaper *sh=skb->dev->priv;
  if(sh_debug)
    printk("Shaper rebuild header\n");
  return sh->rebuild_header(skb);
}

static int shaper_cache(struct dst_entry *dst, struct dst_entry *neigh, struct hh_cache *hh)
{
  struct shaper *sh=dst->dev->priv;
  struct device *tmp;
  int ret;
  if(sh_debug)
    printk("Shaper header cache bind\n");
  tmp=dst->dev;
  dst->dev=sh->dev;
  ret=sh->hard_header_cache(dst,neigh,hh);
  dst->dev=tmp;
  return ret;
}

static void shaper_cache_update(struct hh_cache *hh, struct device *dev,
  BYTE *haddr)
{
  struct shaper *sh=dev->priv;
  if(sh_debug)
    printk("Shaper cache update\n");
  sh->header_cache_update(hh, sh->dev, haddr);
}

static int shaper_attach(struct device *shdev, struct shaper *sh, struct device *dev)
{
  sh->dev = dev;
  sh->hard_start_xmit=dev->hard_start_xmit;
  sh->get_stats=dev->get_stats;
  if(dev->hard_header)
  {
    sh->hard_header=dev->hard_header;
    shdev->hard_header = shaper_header;
  }
  else
    shdev->hard_header = NULL;
    
  if(dev->rebuild_header)
  {
    sh->rebuild_header  = dev->rebuild_header;
    shdev->rebuild_header  = shaper_rebuild_header;
  }
  else
    shdev->rebuild_header  = NULL;
  
  if(dev->hard_header_cache)
  {
    sh->hard_header_cache  = dev->hard_header_cache;
    shdev->hard_header_cache= shaper_cache;
  }
  else
  {
    shdev->hard_header_cache= NULL;
  }
      
  if(dev->header_cache_update)
  {
    sh->header_cache_update  = dev->header_cache_update;
    shdev->header_cache_update = shaper_cache_update;
  }
  else
    shdev->header_cache_update= NULL;
  
  shdev->hard_header_len=dev->hard_header_len;
  shdev->type=dev->type;
  shdev->addr_len=dev->addr_len;
  shdev->mtu=dev->mtu;
  return 0;
}

static int shaper_ioctl(struct device *dev,  struct ifreq *ifr, int cmd)
{
  struct shaperconf *ss= (struct shaperconf *)&ifr->ifr_data;
  struct shaper *sh=dev->priv;
  struct device *them=dev_get(ss->ss_name);
  switch(ss->ss_cmd)
  {
    case SHAPER_SET_DEV:
      if(them==NULL)
        return -ENODEV;
      if(sh->dev)
        return -EBUSY;
      return shaper_attach(dev,dev->priv, them);
    case SHAPER_SET_SPEED:
      shaper_setspeed(sh,ss->ss_speed);
      return 0;
    default:
      return -EINVAL;
  }
}

static struct shaper *shaper_alloc(struct device *dev)
{
  struct shaper *sh=kmalloc(sizeof(struct shaper), GFP_KERNEL);
  if(sh==NULL)
    return NULL;
  memset(sh,0,sizeof(*sh));
  skb_queue_head_init(&sh->sendq);
  init_timer(&sh->timer);
  sh->timer.function=shaper_timer;
  sh->timer.data=(DWORD)sh;
  return sh;
}

/*
 *  Add a shaper device to the system
 */
 
int shaper_probe(struct device *dev)
{
  /*
   *  Set up the shaper.
   */
  
  dev->priv = shaper_alloc(dev);
  if(dev->priv==NULL)
    return -ENOMEM;
    
  dev->open    = shaper_open;
  dev->stop    = shaper_close;
  dev->hard_start_xmit   = shaper_start_xmit;
  dev->get_stats     = shaper_get_stats;
  dev->set_multicast_list = NULL;
  
  /*
   *  Intialise the packet queues
   */
   
  dev_init_buffers(dev);
  
  /*
   *  Handlers for when we attach to a device.
   */

  dev->hard_header   = shaper_header;
  dev->rebuild_header   = shaper_rebuild_header;
  dev->hard_header_cache  = shaper_cache;
  dev->header_cache_update= shaper_cache_update;
  dev->do_ioctl    = shaper_ioctl;
  dev->hard_header_len  = 0;
  dev->type    = ARPHRD_ETHER;  /* initially */
  dev->set_mac_address  = NULL;
  dev->mtu    = 1500;
  dev->addr_len    = 0;
  dev->tx_queue_len  = 10;
  dev->flags    = 0;
  dev->family    = AF_INET;
  dev->pa_addr    = 0;
  dev->pa_brdaddr    = 0;
  dev->pa_mask    = 0;
  dev->pa_alen    = 4;
    
  /*
   *  Shaper is ok
   */  
   
  return 0;
}
 
#ifdef MODULE

static char devicename[9];

static struct device dev_shape = 
{
  devicename,
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, NULL, shaper_probe 
};

int init_module(void)
{
  int i;
  for(i=0;i<99;i++)
  {
    sprintf(devicename,"shaper%d",i);
    if(dev_get(devicename)==NULL)
      break;
  }
  if(i==100)
    return -ENFILE;
  
  printk(SHAPER_BANNER);  
  if (register_netdev(&dev_shape) != 0)
    return -EIO;
  printk("Traffic shaper initialised.\n");
  return 0;
}

void cleanup_module(void)
{
  /*
   *  No need to check MOD_IN_USE, as sys_delete_module() checks.
   *  To be unloadable we must be closed and detached so we don't
   *  need to flush things.
   */
   
  unregister_netdev(&dev_shape);

  /*
   *  Free up the private structure, or leak memory :-) 
   */
   
  kfree(dev_shape.priv);
  dev_shape.priv = NULL;
}

#else

static struct device dev_sh0 = 
{
  "shaper0",
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, NULL, shaper_probe 
};


static struct device dev_sh1 = 
{
  "shaper1",
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, NULL, shaper_probe 
};


static struct device dev_sh2 = 
{
  "shaper2",
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, NULL, shaper_probe 
};

static struct device dev_sh3 = 
{
  "shaper3",
  0, 0, 0, 0,
  0, 0,
  0, 0, 0, NULL, shaper_probe 
};

void shaper_init(void)
{
  register_netdev(&dev_sh0);
  register_netdev(&dev_sh1);
  register_netdev(&dev_sh2);
  register_netdev(&dev_sh3);
}

#endif /* MODULE */
