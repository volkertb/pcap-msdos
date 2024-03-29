/***    ltpc.c -- a driver for the LocalTalk PC card.
 *
 *      Copyright (c) 1995,1996 Bradford W. Johnson <johns393@maroon.tc.umn.edu>
 *
 *      This software may be used and distributed according to the terms
 *      of the GNU General Public License, incorporated herein by reference.
 *
 *      This is ALPHA code at best.  It may not work for you.  It may
 *      damage your equipment.  It may damage your relations with other
 *      users of your network.  Use it at your own risk!
 *
 *      Based in part on:
 *      skeleton.c      by Donald Becker
 *      dummy.c         by Nick Holloway and Alan Cox
 *      loopback.c      by Ross Biro, Fred van Kampen, Donald Becker
 *      the netatalk source code (UMICH)
 *      lots of work on the card...
 *
 *      I do not have access to the (proprietary) SDK that goes with the card.
 *      If you do, I don't want to know about it, and you can probably write
 *      a better driver yourself anyway.  This does mean that the pieces that
 *      talk to the card are guesswork on my part, so use at your own risk!
 *
 *      This is my first try at writing Linux networking code, and is also
 *      guesswork.  Again, use at your own risk!  (Although on this part, I'd
 *      welcome suggestions)
 *
 *      This is a loadable kernel module which seems to work at my site
 *      consisting of a 1.2.13 linux box running netatalk 1.3.3, and with
 *      the kernel support from 1.3.3b2 including patches routing.patch
 *      and ddp.disappears.from.chooser.  In order to run it, you will need
 *      to patch ddp.c and aarp.c in the kernel, but only a little...
 *
 *      I'm fairly confident that while this is arguably badly written, the
 *      problems that people experience will be "higher level", that is, with
 *      complications in the netatalk code.  The driver itself doesn't do
 *      anything terribly complicated -- it pretends to be an ether device
 *      as far as netatalk is concerned, strips the DDP data out of the ether
 *      frame and builds a LLAP packet to send out the card.  In the other
 *      direction, it receives LLAP frames from the card and builds a fake
 *      ether packet that it then tosses up to the networking code.  You can
 *      argue (correctly) that this is an ugly way to do things, but it
 *      requires a minimal amount of fooling with the code in ddp.c and aarp.c.
 *
 *      The card will do a lot more than is used here -- I *think* it has the
 *      layers up through ATP.  Even if you knew how that part works (which I
 *      don't) it would be a big job to carve up the kernel ddp code to insert
 *      things at a higher level, and probably a bad idea...
 *
 *      There are a number of other cards that do LocalTalk on the PC.  If
 *      nobody finds any insurmountable (at the netatalk level) problems
 *      here, this driver should encourage people to put some work into the
 *      other cards (some of which I gather are still commercially available)
 *      and also to put hooks for LocalTalk into the official ddp code.
 *
 *      I welcome comments and suggestions.  This is my first try at Linux
 *      networking stuff, and there are probably lots of things that I did
 *      suboptimally.  
 *
 ***/

/***
 *
 * $Log: ltpc.c,v $
 * Revision 1.8  1997/01/28 05:44:54  bradford
 * Clean up for non-module a little.
 * Hacked about a bit to clean things up - Alan Cox 
 * Probably broken it from the origina 1.8
 *
 * Revision 1.7  1996/12/12 03:42:33  bradford
 * DMA alloc cribbed from 3c505.c.
 *
 * Revision 1.6  1996/12/12 03:18:58  bradford
 * Added virt_to_bus; works in 2.1.13.
 *
 * Revision 1.5  1996/12/12 03:13:22  root
 * xmitQel initialization -- think through better though.
 *
 * Revision 1.4  1996/06/18 14:55:55  root
 * Change names to ltpc. Tabs. Took a shot at dma alloc,
 * although more needs to be done eventually.
 *
 * Revision 1.3  1996/05/22 14:59:39  root
 * Change dev->open, dev->close to track dummy.c in 1.99.(around 7)
 *
 * Revision 1.2  1996/05/22 14:58:24  root
 * Change tabs mostly.
 *
 * Revision 1.1  1996/04/23 04:45:09  root
 * Initial revision
 *
 * Revision 0.16  1996/03/05 15:59:56  root
 * Change ARPHRD_LOCALTLK definition to the "real" one.
 *
 * Revision 0.15  1996/03/05 06:28:30  root
 * Changes for kernel 1.3.70.  Still need a few patches to kernel, but
 * it's getting closer.
 *
 * Revision 0.14  1996/02/25 17:38:32  root
 * More cleanups.  Removed query to card on get_stats.
 *
 * Revision 0.13  1996/02/21  16:27:40  root
 * Refix debug_print_skb.  Fix mac.raw gotcha that appeared in 1.3.65.
 * Clean up receive code a little.
 *
 * Revision 0.12  1996/02/19  16:34:53  root
 * Fix debug_print_skb.  Kludge outgoing snet to 0 when using startup
 * range.  Change debug to mask: 1 for verbose, 2 for higher level stuff
 * including packet printing, 4 for lower level (card i/o) stuff.
 *
 * Revision 0.11  1996/02/12  15:53:38  root
 * Added router sends (requires new aarp.c patch)
 *
 * Revision 0.10  1996/02/11  00:19:35  root
 * Change source LTALK_LOGGING debug switch to insmod ... debug=2.
 *
 * Revision 0.9  1996/02/10  23:59:35  root
 * Fixed those fixes for 1.2 -- DANGER!  The at.h that comes with netatalk
 * has a *different* definition of struct sockaddr_at than the Linux kernel
 * does.  This is an "insidious and invidious" bug...
 * (Actually the preceding comment is false -- it's the atalk.h in the
 * ancient atalk-0.06 that's the problem)
 *
 * Revision 0.8  1996/02/10 19:09:00  root
 * Merge 1.3 changes.  Tested OK under 1.3.60.
 *
 * Revision 0.7  1996/02/10 17:56:56  root
 * Added debug=1 parameter on insmod for debugging prints.  Tried
 * to fix timer unload on rmmod, but I don't think that's the problem.
 *
 * Revision 0.6  1995/12/31  19:01:09  root
 * Clean up rmmod, irq comments per feedback from Corin Anderson (Thanks Corey!)
 * Clean up initial probing -- sometimes the card wakes up latched in reset.
 *
 * Revision 0.5  1995/12/22  06:03:44  root
 * Added comments in front and cleaned up a bit.
 * This version sent out to people.
 *
 * Revision 0.4  1995/12/18  03:46:44  root
 * Return shortDDP to longDDP fake to 0/0.  Added command structs.
 *
 ***/

/* ltpc jumpers are:
*
*  Interrupts -- set at most one.  If none are set, the driver uses
*  polled mode.  Because the card was developed in the XT era, the
*  original documentation refers to IRQ2.  Since you'll be running
*  this on an AT (or later) class machine, that really means IRQ9.
*
*  SW1  IRQ 4
*  SW2  IRQ 3
*  SW3  IRQ 9 (2 in original card documentation only applies to XT)
*
*
*  DMA -- choose DMA 1 or 3, and set both corresponding switches.
*
*  SW4  DMA 3
*  SW5  DMA 1
*  SW6  DMA 3
*  SW7  DMA 1
*
*
*  I/O address -- choose one.  
*
*  SW8  220 / 240
*/

/*  To have some stuff logged, do 
*  insmod ltpc.o debug=1
*
*  For a whole bunch of stuff, use higher numbers.
*
*  The default is 0, i.e. no messages except for the probe results.
*/

/* insmod-tweakable variables */
static int debug=0;
#define DEBUG_VERBOSE 1
#define DEBUG_UPPER 2
#define DEBUG_LOWER 4

#include <linux/config.h> /* for CONFIG_MAX_16M */

#ifdef MODULE
#include <linux/module.h>
#include <linux/version.h>
#endif

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/malloc.h>
#include <linux/string.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/dma.h>
#include <linux/errno.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <linux/if_arp.h>
#include <linux/if_ltalk.h>

#include <linux/delay.h>
#include <linux/timer.h>

#include <linux/atalk.h>

/* our stuff */
#include "ltpc.h"

/* function prototypes */
static int do_read(struct device *dev, void *cbuf, int cbuflen,
  void *dbuf, int dbuflen);
static int sendup_buffer (struct device *dev);

/* Dma Memory related stuff, cribbed directly from 3c505.c */

/* Pure 2^n version of get_order */
static inline int __get_order(DWORD size)
{
        int order;

        size = (size - 1) >> (PAGE_SHIFT - 1);
        order = -1;
        do {
                size >>= 1;
                order++;
        } while (size);
        return order;
}

static DWORD dma_mem_alloc(int size)
{
        int order = __get_order(size);

        return __get_dma_pages(GFP_KERNEL, order);
}

static BYTE *ltdmabuf;
static BYTE *ltdmacbuf;

struct xmitQel {
  struct xmitQel *next;
  BYTE *cbuf;
  short cbuflen;
  BYTE *dbuf;
  short dbuflen;
  BYTE QWrite;  /* read or write data */
  BYTE mailbox;
};

static struct xmitQel *xmQhd=NULL,*xmQtl=NULL;

static void enQ(struct xmitQel *qel)
{
  DWORD flags;
  qel->next = NULL;
  save_flags(flags);
  cli();
  if (xmQtl) {
    xmQtl->next = qel;
  } else {
    xmQhd = qel;
  }
  xmQtl = qel;
  restore_flags(flags);

  if (debug&DEBUG_LOWER)
    printk("enqueued a 0x%02x command\n",qel->cbuf[0]);
}

static struct xmitQel *deQ(void)
{
  DWORD flags;
  int i;
  struct xmitQel *qel=NULL;
  save_flags(flags);
  cli();
  if (xmQhd) {
    qel = xmQhd;
    xmQhd = qel->next;
    if(!xmQhd) xmQtl = NULL;
  }
  restore_flags(flags);

  if ((debug&DEBUG_LOWER) && qel) {
    int n;
    printk("ltpc: dequeued command ");
    n = qel->cbuflen;
    if (n>100) n=100;
    for(i=0;i<n;i++) printk("%02x ",qel->cbuf[i]);
    printk("\n");
  }

  return qel;
}

static struct xmitQel qels[16];

static BYTE mailbox[16];
static BYTE mboxinuse[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static int wait_timeout(struct device *dev, int c)
{
  /* returns true if it stayed c */
  /* this uses base+6, but it's ok */
  int i;
  int timeout;

  /* ten second or so total */

  for(i=0;i<10000;i++) {
    if ( c != inb_p(dev->base_addr+6) ) return 0;
    for(timeout=loops_per_sec/1000; timeout > 0; timeout--) ;
  }
  return 1; /* timed out */
}

static int getmbox(void)
{
  DWORD flags;
  int i;

  save_flags(flags);
  cli();
  for(i=1;i<16;i++) if(!mboxinuse[i]) {
    mboxinuse[i]=1;
    restore_flags(flags);
    return i;
  }
  restore_flags(flags);
  return 0;
}

static void handlefc(struct device *dev)
{
  /* called *only* from idle, non-reentrant */
  int dma = dev->dma;
  int base = dev->base_addr;

  disable_dma(dma);
  clear_dma_ff(dma);
  set_dma_mode(dma,DMA_MODE_READ);
  set_dma_addr(dma,virt_to_bus(ltdmacbuf));
  set_dma_count(dma,50);
  enable_dma(dma);

  inb_p(base+3);
  inb_p(base+2);

  if ( wait_timeout(dev,0xfc) ) printk("timed out in handlefc\n");
}

static void handlefd(struct device *dev)
{
  int dma = dev->dma;
  int base = dev->base_addr;

  disable_dma(dma);
  clear_dma_ff(dma);
  set_dma_mode(dma,DMA_MODE_READ);
  set_dma_addr(dma,virt_to_bus(ltdmabuf));
  set_dma_count(dma,800);
  enable_dma(dma);

  inb_p(base+3);
  inb_p(base+2);

  if ( wait_timeout(dev,0xfd) ) printk("timed out in handlefd\n");
  sendup_buffer(dev);
} 

static void handlewrite(struct device *dev)
{
  /* called *only* from idle, non-reentrant */
  /* on entry, 0xfb and ltdmabuf holds data */
  int dma = dev->dma;
  int base = dev->base_addr;


  disable_dma(dma);
  clear_dma_ff(dma);
  set_dma_mode(dma,DMA_MODE_WRITE);
  set_dma_addr(dma,virt_to_bus(ltdmabuf));
  set_dma_count(dma,800);
  enable_dma(dma);

  inb_p(base+3);
  inb_p(base+2);

  if ( wait_timeout(dev,0xfb) ) {
    printk("timed out in handlewrite, dma res %d\n",
      get_dma_residue(dev->dma) );
  }
}

static void handleread(struct device *dev)
{
  /* on entry, 0xfb */
  /* on exit, ltdmabuf holds data */
  int dma = dev->dma;
  int base = dev->base_addr;



  disable_dma(dma);
  clear_dma_ff(dma);
  set_dma_mode(dma,DMA_MODE_READ);
  set_dma_addr(dma,virt_to_bus(ltdmabuf));
  set_dma_count(dma,800);
  enable_dma(dma);

  inb_p(base+3);
  inb_p(base+2);
  if ( wait_timeout(dev,0xfb) ) printk("timed out in handleread\n");
}

static void handlecommand(struct device *dev)
{
  /* on entry, 0xfa and ltdmacbuf holds command */
  int dma = dev->dma;
  int base = dev->base_addr;

  disable_dma(dma);
  clear_dma_ff(dma);
  set_dma_mode(dma,DMA_MODE_WRITE);
  set_dma_addr(dma,virt_to_bus(ltdmacbuf));
  set_dma_count(dma,50);
  enable_dma(dma);

  inb_p(base+3);
  inb_p(base+2);
  if ( wait_timeout(dev,0xfa) ) printk("timed out in handlecommand\n");
} 

static BYTE rescbuf[2] = {0,0};
static BYTE resdbuf[2];

static int QInIdle=0;

/* idle expects to be called with the IRQ line high -- either because of
 * an interrupt, or because the line is tri-stated
 */

static void idle(struct device *dev)
{
  DWORD flags;
  int state;
  /* FIXME This is initialized to shut the warning up, but I need to
   * think this through again.
   */
  struct xmitQel *q=0;
  int oops;
  int i;
  int statusPort = dev->base_addr+6;

  save_flags(flags);
  cli();
  if(QInIdle) {
    restore_flags(flags);
    return;
  }
  QInIdle = 1;


  restore_flags(flags);


  (void) inb_p(statusPort); /* this tri-states the IRQ line */

  oops = 100;

loop:
  if (0>oops--) { 
    printk("idle: looped too many times\n");
    goto done;
  }

  state = inb_p(statusPort);
  if (state != inb_p(statusPort)) goto loop;

  switch(state) {
    case 0xfc: 
      if (debug&DEBUG_LOWER) printk("idle: fc\n");
      handlefc(dev); 
      break;
    case 0xfd: 
      if(debug&DEBUG_LOWER) printk("idle: fd\n");
      handlefd(dev); 
      break;
    case 0xf9: 
      if (debug&DEBUG_LOWER) printk("idle: f9\n");
      if(!mboxinuse[0]) {
        mboxinuse[0] = 1;
        qels[0].cbuf = rescbuf;
        qels[0].cbuflen = 2;
        qels[0].dbuf = resdbuf;
        qels[0].dbuflen = 2;
        qels[0].QWrite = 0;
        qels[0].mailbox = 0;
        enQ(&qels[0]);
      }
      inb_p(dev->base_addr+1);
      inb_p(dev->base_addr+0);
      if( wait_timeout(dev,0xf9) )
        printk("timed out idle f9\n");
      break;
    case 0xf8:
      if (xmQhd) {
        inb_p(dev->base_addr+1);
        inb_p(dev->base_addr+0);
        if(wait_timeout(dev,0xf8) )
          printk("timed out idle f8\n");
      } else {
        goto done;
      }
      break;
    case 0xfa: 
      if(debug&DEBUG_LOWER) printk("idle: fa\n");
      if (xmQhd) {
        q=deQ();
        memcpy(ltdmacbuf,q->cbuf,q->cbuflen);
        ltdmacbuf[1] = q->mailbox;
        if (debug>1) { 
          int n;
          printk("ltpc: sent command     ");
          n = q->cbuflen;
          if (n>100) n=100;
          for(i=0;i<n;i++)
            printk("%02x ",ltdmacbuf[i]);
          printk("\n");
        }
        handlecommand(dev);
          if(0xfa==inb_p(statusPort)) {
            /* we timed out, so return */
            goto done;
          } 
      } else {
        /* we don't seem to have a command */
        if (!mboxinuse[0]) {
          mboxinuse[0] = 1;
          qels[0].cbuf = rescbuf;
          qels[0].cbuflen = 2;
          qels[0].dbuf = resdbuf;
          qels[0].dbuflen = 2;
          qels[0].QWrite = 0;
          qels[0].mailbox = 0;
          enQ(&qels[0]);
        } else {
          printk("trouble: response command already queued\n");
          goto done;
        }
      } 
      break;
    case 0xfb:
      if(debug&DEBUG_LOWER) printk("idle: fb\n");
      if(q->QWrite) {
        memcpy(ltdmabuf,q->dbuf,q->dbuflen);
        handlewrite(dev);
      } else {
        handleread(dev);
        if(q->mailbox) {
          memcpy(q->dbuf,ltdmabuf,q->dbuflen);
        } else { 
          /* this was a result */
          mailbox[ 0x0f & ltdmabuf[0] ] = ltdmabuf[1];
          mboxinuse[0]=0;
        }
      }
      break;
  }
  goto loop;

done:
  QInIdle=0;

  /* now set the interrupts back as appropriate */
  /* the first 7 takes it out of tri-state (but still high) */
  /* the second resets it */
  /* note that after this point, any read of 6 will trigger an interrupt */

  if (dev->irq) {
    inb_p(dev->base_addr+7);
    inb_p(dev->base_addr+7);
  }
  return;
}


static int do_write(struct device *dev, void *cbuf, int cbuflen,
  void *dbuf, int dbuflen)
{

  int i = getmbox();
  int ret;

  if(i) {
    qels[i].cbuf = (BYTE *) cbuf;
    qels[i].cbuflen = cbuflen;
    qels[i].dbuf = (BYTE *) dbuf;
    qels[i].dbuflen = dbuflen;
    qels[i].QWrite = 1;
    qels[i].mailbox = i;  /* this should be initted rather */
    enQ(&qels[i]);
    idle(dev);
    ret = mailbox[i];
    mboxinuse[i]=0;
    return ret;
  }
  printk("ltpc: could not allocate mbox\n");
  return -1;
}

static int do_read(struct device *dev, void *cbuf, int cbuflen,
  void *dbuf, int dbuflen)
{

  int i = getmbox();
  int ret;

  if(i) {
    qels[i].cbuf = (BYTE *) cbuf;
    qels[i].cbuflen = cbuflen;
    qels[i].dbuf = (BYTE *) dbuf;
    qels[i].dbuflen = dbuflen;
    qels[i].QWrite = 0;
    qels[i].mailbox = i;  /* this should be initted rather */
    enQ(&qels[i]);
    idle(dev);
    ret = mailbox[i];
    mboxinuse[i]=0;
    return ret;
  }
  printk("ltpc: could not allocate mbox\n");
  return -1;
}

/* end of idle handlers -- what should be seen is do_read, do_write */

static struct timer_list ltpc_timer;

static int ltpc_xmit(struct sk_buff *skb, struct device *dev);
static struct enet_statistics *ltpc_get_stats(struct device *dev);

static int ltpc_open(struct device *dev)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
  return 0;
}

static int ltpc_close(struct device *dev)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
  return 0;
}

static int read_30 ( struct device *dev)
{
  lt_command c;
  c.getflags.command = LT_GETFLAGS;
  return do_read(dev, &c, sizeof(c.getflags),&c,0);
}

static int set_30 (struct device *dev,int x)
{
  lt_command c;
  c.setflags.command = LT_SETFLAGS;
  c.setflags.flags = x;
  return do_write(dev, &c, sizeof(c.setflags),&c,0);
}

static int sendup_buffer (struct device *dev)
{
  /* on entry, command is in ltdmacbuf, data in ltdmabuf */
  /* called from idle, non-reentrant */

  int dnode, snode, llaptype, len; 
  int sklen;
  struct sk_buff *skb;
  struct enet_statistics *stats = (struct enet_statistics *)dev->priv;
  struct lt_rcvlap *ltc = (struct lt_rcvlap *) ltdmacbuf;

  if (ltc->command != LT_RCVLAP) {
    printk("unknown command 0x%02x from ltpc card\n",ltc->command);
    return(-1);
  }
  dnode = ltc->dnode;
  snode = ltc->snode;
  llaptype = ltc->laptype;
  len = ltc->length; 

  sklen = len;
  if (llaptype == 1) 
    sklen += 8;  /* correct for short ddp */
  if(sklen > 800) {
    printk(KERN_INFO "%s: nonsense length in ltpc command 0x14: 0x%08x\n",
      dev->name,sklen);
    return -1;
  }

  if ( (llaptype==0) || (llaptype>2) ) {
    printk(KERN_INFO "%s: unknown LLAP type: %d\n",dev->name,llaptype);
    return -1;
  }


  skb = dev_alloc_skb(3+sklen);
  if (skb == NULL) 
  {
    printk("%s: dropping packet due to memory squeeze.\n",
      dev->name);
    return -1;
  }
  skb->dev = dev;

  if (sklen > len)
    skb_reserve(skb,8);
  skb_put(skb,len+3);
  skb->protocol = htons(ETH_P_LOCALTALK);
  /* add LLAP header */
  skb->data[0] = dnode;
  skb->data[1] = snode;
  skb->data[2] = llaptype;
  skb->mac.raw = skb->data;  /* save pointer to llap header */
  skb_pull(skb,3);

  /* copy ddp(s,e)hdr + contents */
  memcpy(skb->data,(void*)ltdmabuf,len);

  skb->h.raw = skb->data;

  /* toss it onwards */
  netif_rx(skb);
  stats->rx_packets++;
  return 0;
}

/* the handler for the board interrupt */
 
static void ltpc_interrupt(int irq, void *dev_id, struct pt_regs *reg_ptr)
{
  struct device *dev = (struct device *) irq2dev_map[irq];

  if (dev==NULL) {
    printk("ltpc_interrupt: unknown device.\n");
    return;
  }

  inb_p(dev->base_addr+6);  /* disable further interrupts from board */

  idle(dev); /* handle whatever is coming in */
 
  /* idle re-enables interrupts from board */ 

  return;
}

/***
 *
 *    The ioctls that the driver responds to are:
 *
 *    SIOCSIFADDR -- do probe using the passed node hint.
 *    SIOCGIFADDR -- return net, node.
 *
 *    some of this stuff should be done elsewhere.
 *
 ***/

static int ltpc_ioctl(struct device *dev, struct ifreq *ifr, int cmd)
{
  struct sockaddr_at *sa = (struct sockaddr_at *) &ifr->ifr_addr;
  /* we'll keep the localtalk node address in dev->pa_addr */
  struct at_addr *aa = (struct at_addr *) &dev->pa_addr;
  struct lt_init c;
  int ltflags;

  if(debug&DEBUG_VERBOSE) printk("ltpc_ioctl called\n");

  switch(cmd) {
    case SIOCSIFADDR:

      aa->s_net  = sa->sat_addr.s_net;
      
      /* this does the probe and returns the node addr */
      c.command = LT_INIT;
      c.hint = sa->sat_addr.s_node;

      aa->s_node = do_read(dev,&c,sizeof(c),&c,0);

      /* get all llap frames raw */
      ltflags = read_30(dev);
      ltflags |= LT_FLAG_ALLLAP;
      set_30 (dev,ltflags);  

      dev->broadcast[0] = 0xFF;
      dev->dev_addr[0] = aa->s_node;

      dev->addr_len=1;
   
      return 0;

    case SIOCGIFADDR:

      sa->sat_addr.s_net = aa->s_net;
      sa->sat_addr.s_node = aa->s_node;

      return 0;

    default: 
      return -EINVAL;
  }
}

static void set_multicast_list(struct device *dev)
{
  /* This needs to be present to keep netatalk happy. */
  /* Actually netatalk needs fixing! */
}

static int ltpc_init(struct device *dev)
{
  /* Initialize the device structure. */
  
  /* Fill in the fields of the device structure with ethernet-generic values. */
  ltalk_setup(dev);
  dev->hard_start_xmit = ltpc_xmit;

  dev->priv = kmalloc(sizeof(struct net_device_stats), GFP_KERNEL);
  if(!dev->priv)
  {
    printk(KERN_INFO "%s: could not allocate statistics buffer\n", dev->name);
    return -ENOMEM;
  }

  memset(dev->priv, 0, sizeof(struct net_device_stats));
  dev->get_stats = ltpc_get_stats;

  dev->open = ltpc_open;
  dev->stop = ltpc_close;

  /* add the ltpc-specific things */
  dev->do_ioctl = &ltpc_ioctl;

  dev->set_multicast_list = &set_multicast_list;
  return 0;
}

static int ltpc_poll_counter = 0;

static void ltpc_poll(DWORD l)
{
  struct device *dev = (struct device *) l;

  del_timer(&ltpc_timer);

  if(debug&DEBUG_VERBOSE) {
    if (!ltpc_poll_counter) {
      ltpc_poll_counter = 50;
      printk("ltpc poll is alive\n");
    }
    ltpc_poll_counter--;
  }
  
  if (!dev) return;  /* we've been downed */

  if (dev->irq) {
    /* we're set up for interrupts */
    if (0xf8 != inb_p(dev->base_addr+7)) {
      /* trigger an interrupt */
      (void) inb_p(dev->base_addr+6);
    }
    ltpc_timer.expires = 100;
  } else {
    /* we're strictly polling mode */
    idle(dev);
    ltpc_timer.expires = 5;
  }

  ltpc_timer.expires += jiffies;  /* 1.2 to 1.3 change... */

  add_timer(&ltpc_timer);
}

static int ltpc_xmit(struct sk_buff *skb, struct device *dev)
{
  /* in kernel 1.3.xx, on entry skb->data points to ddp header,
   * and skb->len is the length of the ddp data + ddp header
   */

  struct net_device_stats *stats = (struct enet_statistics *)dev->priv;

  int i;
  struct lt_sendlap cbuf;

  cbuf.command = LT_SENDLAP;
  cbuf.dnode = skb->data[0];
  cbuf.laptype = skb->data[2];
  skb_pull(skb,3);  /* skip past LLAP header */
  cbuf.length = skb->len;  /* this is host order */
  skb->h.raw=skb->data;

  if(debug&DEBUG_UPPER) {
    printk("command ");
    for(i=0;i<6;i++)
      printk("%02x ",((BYTE *)&cbuf)[i]);
    printk("\n");
  }

  do_write(dev,&cbuf,sizeof(cbuf),skb->h.raw,skb->len);

  if(debug&DEBUG_UPPER) {
    printk("sent %d ddp bytes\n",skb->len);
    for(i=0;i<skb->len;i++) printk("%02x ",skb->h.raw[i]);
    printk("\n");
  }

  dev_kfree_skb(skb, FREE_WRITE);

  stats->tx_packets++;

  return 0;
}

static struct net_device_stats *ltpc_get_stats(struct device *dev)
{
  struct enet_statistics *stats = (struct enet_statistics*) dev->priv;
  return stats;
}

static WORD irqhitmask;

static void lt_probe_handler(int irq, void *dev_id, struct pt_regs *reg_ptr)
{
  irqhitmask |= 1<<irq;
}

int ltpc_probe(struct device *dev)
{
  int err;
  BYTE dma=0;
  short base=0;
  BYTE irq=0;
  int x=0,y=0;
  int timeout;
  int probe3, probe4, probe9;
  WORD straymask;
  DWORD flags;

  err = ltpc_init(dev);
  if (err) return err;

  /* occasionally the card comes up with reset latched, so we need
   * to "reset the reset" first of all -- check the irq first also
   */

  save_flags(flags);
  cli();

  probe3 = request_irq( 3, &lt_probe_handler, 0, "ltpc_probe",NULL);
  probe4 = request_irq( 4, &lt_probe_handler, 0, "ltpc_probe",NULL);
  probe9 = request_irq( 9, &lt_probe_handler, 0, "ltpc_probe",NULL);

  irqhitmask = 0;

  sti();

  timeout = jiffies+2;
  while(timeout>jiffies) ;  /* wait for strays */

  straymask = irqhitmask;  /* pick up any strays */
 
  /* if someone already owns this address, don't probe */ 
  if (!check_region(0x220,8)) {
    inb_p(0x227);
    inb_p(0x227);
    x=inb_p(0x226);
    timeout = jiffies+2;
    while(timeout>jiffies) ;
    if(straymask != irqhitmask) base = 0x220;
  }
  if (!check_region(0x240,8)) {
    inb_p(0x247);
    inb_p(0x247);
    y=inb_p(0x246);
    timeout = jiffies+2;
    while(timeout>jiffies) ;
    if(straymask != irqhitmask) base = 0x240;
  }

  /* at this point, either we have an irq and the base addr, or
   * there isn't any irq and we don't know the base address, but
   * in either event the card is no longer latched in reset and
   * the irq request line is tri-stated.
   */

  cli();
 
  if (!probe3) free_irq(3,NULL);
  if (!probe4) free_irq(4,NULL);
  if (!probe9) free_irq(9,NULL);

  sti();
 
  irqhitmask &= ~straymask;

  irq = ffz(~irqhitmask);
  if (irqhitmask != 1<<irq)
    printk("ltpc card raised more than one interrupt!\n");

  if (!base) {
    if (!check_region(0x220,8)) {
      x = inb_p(0x220+6);
      if ( (x!=0xff) && (x>=0xf0) ) base = 0x220;
    }
 
    if (!check_region(0x240,8)) {
      y = inb_p(0x240+6);
      if ( (y!=0xff) && (y>=0xf0) ) base = 0x240;
    } 
  }

  if(base) {
    request_region(base,8,"ltpc");
  } else {
    printk("LocalTalk card not found; 220 = %02x, 240 = %02x.\n",x,y);
    restore_flags(flags);
    return -1;
  }

  ltdmabuf = (BYTE *) dma_mem_alloc(1000);

  if (ltdmabuf) ltdmacbuf = &ltdmabuf[800];

  if (!ltdmabuf) {
    printk("ltpc: mem alloc failed\n");
    restore_flags(flags);
    return(-1);
  }

  if(debug&DEBUG_VERBOSE) {
    printk("ltdmabuf pointer %08lx\n",(DWORD) ltdmabuf);
  }

  /* reset the card */

  inb_p(base+1);
  inb_p(base+3);
  timeout = jiffies+2;
  while(timeout>jiffies) ; /* hold it in reset for a coupla jiffies */
  inb_p(base+0);
  inb_p(base+2);
  inb_p(base+7); /* clear reset */
  inb_p(base+4); 
  inb_p(base+5);
  inb_p(base+5); /* enable dma */
  inb_p(base+6); /* tri-state interrupt line */

  timeout = jiffies+100;
  while(timeout>jiffies) {
    /* wait for the card to complete initialization */
  }
 
  /* now, figure out which dma channel we're using */

  /* set up both dma 1 and 3 for read call */

  if (!request_dma(1,"ltpc")) {
    disable_dma(1);
    clear_dma_ff(1);
    set_dma_mode(1,DMA_MODE_WRITE);
    set_dma_addr(1,virt_to_bus(ltdmabuf));
    set_dma_count(1,sizeof(struct lt_mem));
    enable_dma(1);
    dma|=1;
  }
  if (!request_dma(3,"ltpc")) {
    disable_dma(3);
    clear_dma_ff(3);
    set_dma_mode(3,DMA_MODE_WRITE);
    set_dma_addr(3,virt_to_bus(ltdmabuf));
    set_dma_count(3,sizeof(struct lt_mem));
    enable_dma(3);
    dma|=2;
  }

  /* set up request */

  /* FIXME -- do timings better! */

  ltdmabuf[0] = 2; /* read request */
  ltdmabuf[1] = 1;  /* mailbox */
  ltdmabuf[2] = 0; ltdmabuf[3] = 0;  /* address */
  ltdmabuf[4] = 0; ltdmabuf[5] = 1;  /* read 0x0100 bytes */
  ltdmabuf[6] = 0; /* dunno if this is necessary */

  inb_p(base+1);
  inb_p(base+0);
  timeout = jiffies+100;
  while(timeout>jiffies) {
    if ( 0xfa == inb_p(base+6) ) break;
  }

  inb_p(base+3);
  inb_p(base+2);
  while(timeout>jiffies) {
    if ( 0xfb == inb_p(base+6) ) break;
  }

  /* release the other dma channel */

  if ( (dma&0x2) && (get_dma_residue(3)==sizeof(struct lt_mem)) ){
    dma&=1;
    free_dma(3);
  }
  
  if ( (dma&0x1) && (get_dma_residue(1)==sizeof(struct lt_mem)) ){
    dma&=0x2;
    free_dma(1);
  }
 
  if (!dma) {  /* no dma channel */
    printk("No DMA channel found on ltpc card.\n");
    restore_flags(flags);
    return -1;
  } 
  
  /* fix up dma number */
  dma|=1;

  /* set up read */

  if(irq)
    printk("LocalTalk card found at %03x, IR%d, DMA%d.\n",base,irq,dma);
  else
    printk("LocalTalk card found at %03x, DMA%d.  Using polled mode.\n",base,dma);
    
  dev->base_addr = base;
  dev->irq = irq;
  dev->dma = dma;

  if(debug&DEBUG_VERBOSE) {
    printk("finishing up transfer\n");
  }

  disable_dma(dma);
  clear_dma_ff(dma);
  set_dma_mode(dma,DMA_MODE_READ);
  set_dma_addr(dma,virt_to_bus(ltdmabuf));
  set_dma_count(dma,0x100);
  enable_dma(dma);

  (void) inb_p(base+3);
  (void) inb_p(base+2);
  timeout = jiffies+100;
  while(timeout>jiffies) {
    if( 0xf9 == inb_p(base+6)) break;
  }

  if(debug&DEBUG_VERBOSE) {
    printk("setting up timer and irq\n");
  }

  init_timer(&ltpc_timer);
  ltpc_timer.function=ltpc_poll;
  ltpc_timer.data = (DWORD) dev;

  if (irq) {
    irq2dev_map[irq] = dev;
    (void) request_irq( irq, &ltpc_interrupt, 0, "ltpc",NULL);
    (void) inb_p(base+7);  /* enable interrupts from board */
    (void) inb_p(base+7);  /* and reset irq line */
    ltpc_timer.expires = 100;
      /* poll it once per second just in case */
  } else {
    ltpc_timer.expires = 5;
      /* polled mode -- 20 times per second */
  }

  add_timer(&ltpc_timer);

  restore_flags(flags); 

  return 0;
}

#ifdef MODULE
static struct device dev_ltpc = {
  "ltalk0\0   ", 
    0, 0, 0, 0,
     0x0, 0,
     0, 0, 0, NULL, ltpc_probe };

int init_module(void)
{
  /* Find a name for this unit */
  int ct= 1;
  
  while(dev_get(dev_ltpc.name)!=NULL && ct<100)
  {
    sprintf(dev_ltpc.name,"ltpc%d",ct);
    ct++;
  }
  if(ct==100)
    return -ENFILE;
  
  if (register_netdev(&dev_ltpc) != 0) {
    if(debug&DEBUG_VERBOSE) printk("EIO from register_netdev\n");
    return -EIO;
  } else {
    if(debug&DEBUG_VERBOSE) printk("0 from register_netdev\n");
    return 0;
  }
}

void cleanup_module(void)
{
  long timeout;

  ltpc_timer.data = 0;  /* signal the poll routine that we're done */

  if(debug&DEBUG_VERBOSE) printk("freeing irq\n");

  if(dev_ltpc.irq) {
    free_irq(dev_ltpc.irq,NULL);
    dev_ltpc.irq = 0;
  }

  if(del_timer(&ltpc_timer)) 
  {
    /* either the poll was never started, or a poll is in process */
    if(debug&DEBUG_VERBOSE) printk("waiting\n");
    /* if it's in process, wait a bit for it to finish */
    timeout = jiffies+HZ; 
    add_timer(&ltpc_timer)
    while(del_timer(&ltpc_timer) && (timeout > jiffies))
    {
      add_timer(&ltpc_timer);
      schedule();
    }
  }

  if(debug&DEBUG_VERBOSE) printk("freeing dma\n");

  if(dev_ltpc.dma) {
    free_dma(dev_ltpc.dma);
    dev_ltpc.dma = 0;
  }

  if(debug&DEBUG_VERBOSE) printk("freeing ioaddr\n");

  if(dev_ltpc.base_addr) {
    release_region(dev_ltpc.base_addr,8);
    dev_ltpc.base_addr = 0;
  }

  if(debug&DEBUG_VERBOSE) printk("free_pages\n");

  free_pages( (DWORD) ltdmabuf, __get_order(1000));
  ltdmabuf=NULL;
  ltdmacbuf=NULL;

  if(debug&DEBUG_VERBOSE) printk("unregister_netdev\n");

  unregister_netdev(&dev_ltpc);

  if(debug&DEBUG_VERBOSE) printk("returning from cleanup_module\n");
}
#endif /* MODULE */
