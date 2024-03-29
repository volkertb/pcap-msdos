/* ibmtr.c:  A shared-memory IBM Token Ring 16/4 driver for linux
 *
 *  Written 1993 by Mark Swanson and Peter De Schrijver.
 *  This software may be used and distributed according to the terms
 *  of the GNU Public License, incorporated herein by reference.
 *
 *  This device driver should work with Any IBM Token Ring Card that does
 *  not use DMA.
 *
 *  I used Donald Becker's (becker@cesdis.gsfc.nasa.gov) device driver work
 *  as a base for most of my initial work.
 *
 *  Changes by Peter De Schrijver (Peter.Deschrijver@linux.cc.kuleuven.ac.be) :
 *
 *  + changed name to ibmtr.c in anticipation of other tr boards.
 *  + changed reset code and adapter open code.
 *  + added SAP open code.
 *  + a first attempt to write interrupt, transmit and receive routines.
 *
 *  Changes by David W. Morris (dwm@shell.portal.com) :
 *  941003 dwm: - Restructure tok_probe for multiple adapters, devices.
 *  + Add comments, misc reorg for clarity.
 *  + Flatten interrupt handler levels.
 *
 *  Changes by Farzad Farid (farzy@zen.via.ecp.fr)
 *  and Pascal Andre (andre@chimay.via.ecp.fr) (March 9 1995) :
 *  + multi ring support clean up.
 *  + RFC1042 compliance enhanced.
 *
 *  Changes by Pascal Andre (andre@chimay.via.ecp.fr) (September 7 1995) :
 *  + bug correction in tr_tx
 *  + removed redundant information display
 *  + some code reworking
 *
 *  Changes by Michel Lespinasse (walken@via.ecp.fr),
 *  Yann Doussot (doussot@via.ecp.fr) and Pascal Andre (andre@via.ecp.fr)
 *  (February 18, 1996) :
 *  + modified shared memory and mmio access port the driver to
 *    alpha platform (structure access -> readb/writeb)
 *
 *  Changes by Steve Kipisz (bungy@ibm.net or kipisz@vnet.ibm.com)
 *  (January 18 1996):
 *  + swapped WWOR and WWCR in ibmtr.h
 *  + moved some init code from tok_probe into trdev_init.  The
 *    PCMCIA code can call trdev_init to complete initializing
 *    the driver.
 *  + added -DPCMCIA to support PCMCIA
 *  + detecting PCMCIA Card Removal in interrupt handler.  If
 *    ISRP is FF, then a PCMCIA card has been removed
 *
 *  Changes by Paul Norton (pnorton@cts.com) :
 *  + restructured the READ.LOG logic to prevent the transmit SRB
 *    from being rudely overwritten before the transmit cycle is
 *    complete. (August 15 1996)
 *  + completed multiple adapter support. (November 20 1996)
 */

#ifdef PCMCIA
#define MODULE
#endif

#include <linux/module.h>

#ifdef PCMCIA
#undef MODULE
#endif

#define NO_AUTODETECT 1
#undef NO_AUTODETECT
#undef ENABLE_PAGING

#define FALSE 0
#define TRUE (!FALSE)

/* changes the output format of driver initialisation */
#define TR_NEWFORMAT  1
#define TR_VERBOSE  0

/* some 95 OS send many non UI frame; this allow removing the warning */
#define TR_FILTERNONUI  1

/* use memcpy to read/write frame data instead of for-loop */
#define USE_MEMCPY 1

/* version and credits */
static char *version =
"ibmtr.c: v1.3.57  8/ 7/94 Peter De Schrijver and Mark Swanson\n"
"         v2.1.10 11/20/96 Paul Norton <pnorton@cts.com>\n";

static char pcchannelid[] = {
  0x05, 0x00, 0x04, 0x09,
  0x04, 0x03, 0x04, 0x0f,
  0x03, 0x06, 0x03, 0x01,
  0x03, 0x01, 0x03, 0x00,
  0x03, 0x09, 0x03, 0x09,
  0x03, 0x00, 0x02, 0x00
};

static char mcchannelid[] = {
  0x04, 0x0d, 0x04, 0x01,
  0x05, 0x02, 0x05, 0x03,
  0x03, 0x06, 0x03, 0x03,
  0x05, 0x08, 0x03, 0x04,
  0x03, 0x05, 0x03, 0x01,
  0x03, 0x08, 0x02, 0x00
};

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/in.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/trdevice.h>
#include <linux/stddef.h>

#include <asm/io.h>
#include <asm/system.h>
#include <asm/bitops.h>

#include "ibmtr.h"


#define DPRINTK(format, args...) printk("%s: " format, dev->name , ## args)
#define DPRINTD(format, args...) DummyCall("%s: " format, dev->name , ## args)

#if TR_NEWFORMAT
/* this allows displaying full adapter information */

const char *channel_def[] = { 
  "ISA", "MCA", "ISA P&P" 
};

char *adapter_def(char type)
{
  switch (type) 
  {
        case 0xF : return "PC Adapter | PC Adapter II | Adapter/A";
        case 0xE : return "16/4 Adapter | 16/4 Adapter/A (long)";
        case 0xD : return "16/4 Adapter/A (short) | 16/4 ISA-16 Adapter";
        case 0xC : return "Auto 16/4 Adapter";
        default  : return "adapter (unknown type)";
  };
};
#endif

#if !TR_NEWFORMAT
BYTE ibmtr_debug_trace=1;  /*  Patch or otherwise alter to
                                         control tokenring tracing.  */
#else
BYTE ibmtr_debug_trace=0;
#endif
#define TRC_INIT 0x01              /*  Trace initialization & PROBEs */
#define TRC_INITV 0x02             /*  verbose init trace points     */

int    ibmtr_probe(struct device *dev);
static int  ibmtr_probe1(struct device *dev, int ioaddr);
BYTE  get_sram_size(struct tok_info *adapt_info);
static int  tok_init_card(struct device *dev);
int    trdev_init(struct device *dev);
void    tok_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void   initial_tok_int(struct device *dev);
static void   open_sap(BYTE type,struct device *dev);
void    tok_open_adapter(DWORD dev_addr);
static    void tr_rx(struct device *dev);
static    void tr_tx(struct device *dev);
static int  tok_open(struct device *dev);
static int  tok_close(struct device *dev);
static int  tok_send_packet(struct sk_buff *skb, struct device *dev);
static struct net_device_stats * tok_get_stats(struct device *dev);
void    ibmtr_readlog(struct device *dev);
void    ibmtr_reset_timer(struct timer_list *tmr, struct device *dev);

static struct timer_list tr_timer;

static DWORD ibmtr_portlist[] = {
  0xa20, 0xa24, 0
};

static __u32 ibmtr_mem_base = 0xd0000;

static void PrtChanID(char *pcid, short stride) 
{
  short i, j;
  for (i=0, j=0; i<24; i++, j+=stride)
    printk("%1x", ((int) pcid[j]) & 0x0f);
  printk("\n");
}

static void HWPrtChanID (__u32 pcid, short stride)
{
  short i, j;
  for (i=0, j=0; i<24; i++, j+=stride)
    printk("%1x", ((int)readb(pcid + j)) & 0x0f);
  printk("\n");
}

/*
 *  ibmtr_probe():  Routine specified in the network device structure
 *  to probe for an IBM Token Ring Adapter.  Routine outline:
 *  I.    Interrogate hardware to determine if an adapter exists
 *        and what the speeds and feeds are
 *  II.   Setup data structures to control execution based upon
 *        adapter characteristics.
 *  III.  Initialize adapter operation
 *
 *  We expect ibmtr_probe to be called once for each device entry
 *  which references it.
 */
 
int ibmtr_probe(struct device *dev)
{
        int i;
        int base_addr = dev ? dev->base_addr : 0;

        if (base_addr > 0x1ff) 
        { 
          /*
           *  Check a single specified location. 
      */
      
          if (ibmtr_probe1(dev, base_addr)) 
          {
#ifndef MODULE
           tr_freedev(dev);
#endif
           return -ENODEV;
    } else
           return 0;
  }
        else if (base_addr != 0)   /* Don't probe at all. */
          return -ENXIO;

  for (i = 0; ibmtr_portlist[i]; i++) 
  {
          int ioaddr = ibmtr_portlist[i];
    if (check_region(ioaddr, IBMTR_IO_EXTENT))
            continue;
                if (ibmtr_probe1(dev, ioaddr)) {
#ifndef MODULE
            tr_freedev(dev);
#endif
    } else
      return 0;
        }

        return -ENODEV;
}

static int ibmtr_probe1(struct device *dev, int PIOaddr)
{
  BYTE segment=0, intr=0, irq=0, i=0, j=0, cardpresent=NOTOK,temp=0;
  __u32 t_mmio=0;
  struct tok_info *ti=0;
  __u32 cd_chanid;
  BYTE *tchanid, ctemp;

#ifndef MODULE
  dev = init_trdev(dev,0);
#endif

  /*  Query the adapter PIO base port which will return
   *  indication of where MMIO was placed. We also have a
   *  coded interrupt number.
   */

         segment = inb(PIOaddr);
  
  /*
   *  Out of range values so we'll assume non-existent IO device 
   */
   
  if (segment < 0x40 || segment > 0xe0)
                return -ENODEV;

  /*
   *  Compute the linear base address of the MMIO area
   *  as LINUX doesn't care about segments
   */

  t_mmio=(((__u32)(segment & 0xfc) << 11) + 0x80000);
  intr = segment & 0x03;   /* low bits is coded interrupt # */
  if (ibmtr_debug_trace & TRC_INIT)
    DPRINTK("PIOaddr: %4hx seg/intr: %2x mmio base: %08X intr: %d\n",
                         PIOaddr, (int)segment, t_mmio, (int)intr);

  /*
   *  Now we will compare expected 'channelid' strings with
   *  what we is there to learn of ISA/MCA or not TR card
   */
   
  cd_chanid = (CHANNEL_ID + t_mmio);  /* for efficiency */
  tchanid=pcchannelid;
  cardpresent=TR_ISA;  /* try ISA */

  /*
   *  Suboptimize knowing first byte different
   */

  ctemp = readb(cd_chanid) & 0x0f;
  if (ctemp != *tchanid) { /* NOT ISA card, try MCA */
    tchanid=mcchannelid;
    cardpresent=TR_MCA;
    if (ctemp != *tchanid)  /* Neither ISA nor MCA */
      cardpresent=NOTOK;
  }

  if (cardpresent != NOTOK) 
  {
    /* 
     *  Know presumed type, try rest of ID 
     */
    for (i=2,j=1; i<=46; i=i+2,j++) 
    {
      if ((readb(cd_chanid+i) & 0x0f) != tchanid[j]) {
        cardpresent=NOTOK;   /* match failed, not TR card */
        break;
      }
    }
  }

  /* 
   *  If we have an ISA board check for the ISA P&P version,
   *  as it has different IRQ settings 
   */

  if (cardpresent == TR_ISA && (readb(AIPFID + t_mmio)==0x0e))
          cardpresent=TR_ISAPNP;

  if (cardpresent == NOTOK) { /* "channel_id" did not match, report */
    if (ibmtr_debug_trace & TRC_INIT) {
      DPRINTK("Channel ID string not found for PIOaddr: %4hx\n", PIOaddr);
      DPRINTK("Expected for ISA: ");  PrtChanID(pcchannelid,1);
      DPRINTK("           found: ");  HWPrtChanID(cd_chanid,2);
      DPRINTK("Expected for MCA: ");  PrtChanID(mcchannelid,1);
    }
                return -ENODEV;
  }

  /* Now, allocate some of the pl0 buffers for this driver.. */
  ti = (struct tok_info *)kmalloc(sizeof(struct tok_info), GFP_KERNEL);
  if (ti == NULL) 
    return -ENOMEM;

  memset(ti, 0, sizeof(struct tok_info));

  ti->mmio= t_mmio;
  ti->readlog_pending = 0;

  dev->priv = ti;     /* this seems like the logical use of the
                         field ... let's try some empirical tests
                         using the token-info structure -- that
                         should fit with out future hope of multiple
                         adapter support as well /dwm   */

  switch (cardpresent) 
  {
    case TR_ISA:
      if (intr==0)
        irq=9; /* irq2 really is irq9 */
      if (intr==1)
        irq=3;
      if (intr==2)
        irq=6;
      if (intr==3)
        irq=7;
      ti->global_int_enable=GLOBAL_INT_ENABLE+((irq==9) ? 2 : irq);
      ti->adapter_int_enable=PIOaddr+ADAPTINTREL;
      ti->sram=0;
#if !TR_NEWFORMAT
      DPRINTK("ti->global_int_enable: %04X\n",ti->global_int_enable);
#endif
      break;
    case TR_MCA:
      if (intr==0)
        irq=9;
      if (intr==1)
        irq=3;
      if (intr==2)
        irq=10;
      if (intr==3)
        irq=11;
      ti->global_int_enable=0;
      ti->adapter_int_enable=0;
      ti->sram=((__u32)(inb(PIOaddr+ADAPTRESETREL) & 0xfe) << 12);
      break;
    case TR_ISAPNP:
      if (intr==0)
        irq=9;
      if (intr==1)
        irq=3;
      if (intr==2)
        irq=10;
      if (intr==3)
        irq=11;
      /*
       *  FIXME: this wait should have a timeout
       */
      while(!readb(ti->mmio + ACA_OFFSET + ACA_RW + RRR_EVEN));
      ti->sram=((__u32)readb(ti->mmio + ACA_OFFSET + ACA_RW + RRR_EVEN)<<12);
      ti->global_int_enable=PIOaddr+ADAPTINTREL;
      ti->adapter_int_enable=PIOaddr+ADAPTINTREL;
      break;
  }

  if (ibmtr_debug_trace & TRC_INIT) { /* just report int */
    DPRINTK("irq=%d",irq);
    if (ibmtr_debug_trace & TRC_INITV) { /* full chat in verbose only */
      DPRINTK(", ti->mmio=%08X",ti->mmio);
      printk(", segment=%02X",segment);
    }
    printk(".\n");
  }

  /* Get hw address of token ring card */
#if !TR_NEWFORMAT
  DPRINTK("hw address: ");
#endif
  j=0;
  for (i=0; i<0x18; i=i+2) 
  {
    /* technical reference states to do this */
    temp = readb(ti->mmio + AIP + i) & 0x0f;
#if !TR_NEWFORMAT
    printk("%1X",ti->hw_address[j]=temp);
#else
    ti->hw_address[j]=temp;
#endif
    if(j&1)
      dev->dev_addr[(j/2)]=ti->hw_address[j]+(ti->hw_address[j-1]<<4);
    ++j;
  }
#ifndef TR_NEWFORMAT
  printk("\n");
#endif

  /* get Adapter type:  'F' = Adapter/A, 'E' = 16/4 Adapter II,...*/
  ti->adapter_type = readb(ti->mmio + AIPADAPTYPE);

  /* get Data Rate:  F=4Mb, E=16Mb, D=4Mb & 16Mb ?? */
  ti->data_rate = readb(ti->mmio + AIPDATARATE);

  /* Get Early Token Release support?: F=no, E=4Mb, D=16Mb, C=4&16Mb */
  ti->token_release = readb(ti->mmio + AIPEARLYTOKEN);

  /* How much shared RAM is on adapter ? */
  ti->avail_shared_ram = get_sram_size(ti);

  /* We need to set or do a bunch of work here based on previous results.. */
  /* Support paging?  What sizes?:  F=no, E=16k, D=32k, C=16 & 32k */
  ti->shared_ram_paging = readb(ti->mmio + AIPSHRAMPAGE);

        /* Available DHB  4Mb size:   F=2048, E=4096, D=4464 */
  ti->dhb_size4mb = readb(ti->mmio + AIP4MBDHB);

  /* Available DHB 16Mb size:  F=2048, E=4096, D=8192, C=16384, B=17960 */
  ti->dhb_size16mb = readb(ti->mmio + AIP16MBDHB);

#if !TR_NEWFORMAT
  DPRINTK("atype=%x, drate=%x, trel=%x, asram=%dK, srp=%x, "
    "dhb(4mb=%x, 16mb=%x)\n",ti->adapter_type,
    ti->data_rate, ti->token_release, ti->avail_shared_ram/2,
    ti->shared_ram_paging, ti->dhb_size4mb, ti->dhb_size16mb);
#endif

  /*  We must figure out how much shared memory space this adapter
   *  will occupy so that if there are two adapters we can fit both
   *  in.  Given a choice, we will limit this adapter to 32K.  The
   *  maximum space will will use for two adapters is 64K so if the
   *  adapter we are working on demands 64K (it also doesn't support
   *  paging), then only one adapter can be supported.  
   */

  /*
   *  determine how much of total RAM is mapped into PC space 
   */
  ti->mapped_ram_size=1<<((((readb(ti->mmio+ ACA_OFFSET + ACA_RW + RRR_ODD)) >>2) & 0x03) + 4);
  ti->page_mask=0;
  if (ti->shared_ram_paging == 0xf) { /* No paging in adapter */
    ti->mapped_ram_size = ti->avail_shared_ram;
  } else {
#ifdef ENABLE_PAGING
    BYTE pg_size;
#endif

#if !TR_NEWFORMAT
    DPRINTK("shared ram page size: %dK\n",ti->mapped_ram_size/2);
#endif
#ifdef ENABLE_PAGING
  switch(ti->shared_ram_paging) 
  {
    case 0xf:
      break;
    case 0xe:
      ti->page_mask=(ti->mapped_ram_size==32) ? 0xc0 : 0;
      pg_size=32;   /* 16KB page size */
      break;
    case 0xd:
      ti->page_mask=(ti->mapped_ram_size==64) ? 0x80 : 0;
      pg_size=64;   /* 32KB page size */
      break;
    case 0xc:
      ti->page_mask=(ti->mapped_ram_size==32) ? 0xc0 : 0;
      ti->page_mask=(ti->mapped_ram_size==64) ? 0x80 : 0;
      DPRINTK("Dual size shared RAM page (code=0xC), don't support it!\n");
      /* nb/dwm: I did this because RRR (3,2) bits are documented as
         R/O and I can't find how to select which page size
         Also, the above conditional statement sequence is invalid
         as page_mask will always be set by the second stmt */
      kfree_s(ti, sizeof(struct tok_info));
      return -ENODEV;
      break;
    default:
      DPRINTK("Unknown shared ram paging info %01X\n",ti->shared_ram_paging);
      kfree_s(ti, sizeof(struct tok_info));
      return -ENODEV;
      break;
  }
  if (ti->page_mask) {
    if (pg_size > ti->mapped_ram_size) {
      DPRINTK("Page size (%d) > mapped ram window (%d), can't page.\n",
        pg_size, ti->mapped_ram_size);
        ti->page_mask = 0;    /* reset paging */
    } else {
      ti->mapped_ram_size=ti->avail_shared_ram;
      DPRINTK("Shared RAM paging enabled. Page size : %uK\n",
        ((ti->page_mask^ 0xff)+1)>>2);
    }
#endif
  }

  /* finish figuring the shared RAM address */
  if (cardpresent==TR_ISA) {
    static __u32 ram_bndry_mask[]={0xffffe000, 0xffffc000, 0xffff8000, 0xffff0000};
    __u32 new_base, rrr_32, chk_base, rbm;

    rrr_32 = ((readb(ti->mmio+ ACA_OFFSET + ACA_RW + RRR_ODD))>>2) & 0x00000003;
    rbm = ram_bndry_mask[rrr_32];
    new_base = (ibmtr_mem_base + (~rbm)) & rbm; /* up to boundary */
    chk_base = new_base + (ti->mapped_ram_size<<9);
    if (chk_base > (ibmtr_mem_base + IBMTR_SHARED_RAM_SIZE)) {
      DPRINTK("Shared RAM for this adapter (%05x) exceeds driver"
        " limit (%05x), adapter not started.\n",
        chk_base, ibmtr_mem_base + IBMTR_SHARED_RAM_SIZE);
      kfree_s(ti, sizeof(struct tok_info));
                        return -ENODEV;
    } else {  /* seems cool, record what we have figured out */
      ti->sram_base = new_base >> 12;
      ibmtr_mem_base = chk_base;
    }
  }

#if !TR_NEWFORMAT
  DPRINTK("Using %dK shared RAM\n",ti->mapped_ram_size/2);
#endif

  if (request_irq (dev->irq = irq, &tok_interrupt,0,"ibmtr", NULL) != 0) {
    DPRINTK("Could not grab irq %d.  Halting Token Ring driver.\n",irq);
    kfree_s(ti, sizeof(struct tok_info));
    return -ENODEV;
  }
  irq2dev_map[irq]=dev;

 /*?? Now, allocate some of the PIO PORTs for this driver.. */
  request_region(PIOaddr,IBMTR_IO_EXTENT,"ibmtr");  /* record PIOaddr range
                  as busy */
#if !TR_NEWFORMAT
  DPRINTK("%s",version); /* As we have passed card identification,
                                  let the world know we're here! */
#else

  if (version) {
          printk("%s",version);
                version = NULL;
        }
  DPRINTK("%s %s found\n",
    channel_def[cardpresent-1], adapter_def(ti->adapter_type));
  DPRINTK("using irq %d, PIOaddr %hx, %dK shared RAM.\n",
          irq, PIOaddr, ti->mapped_ram_size/2);
  DPRINTK("Hardware address : %02X:%02X:%02X:%02X:%02X:%02X\n",
    dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
    dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);
#endif

  dev->base_addr=PIOaddr; /* set the value for device */

  trdev_init(dev);
  tok_init_card(dev);

  return 0;  /* Return 0 to indicate we have found a Token Ring card. */
}

/* query the adapter for the size of shared RAM  */

BYTE get_sram_size(struct tok_info *adapt_info)
{

  BYTE avail_sram_code;
  static BYTE size_code[]={ 0,16,32,64,127,128 };

  /* Adapter gives
     'F' -- use RRR bits 3,2
     'E' -- 8kb   'D' -- 16kb
     'C' -- 32kb  'A' -- 64KB
     'B' - 64KB less 512 bytes at top
     (WARNING ... must zero top bytes in INIT */

  avail_sram_code=0xf-readb(adapt_info->mmio + AIPAVAILSHRAM);
  if (avail_sram_code)
    return size_code[avail_sram_code];
  else  /* for code 'F', must compute size from RRR(3,2) bits */
    return 1<<((readb(adapt_info->mmio+ ACA_OFFSET + ACA_RW + RRR_ODD)>>2)+4);
}

int trdev_init(struct device *dev)
{
  struct tok_info *ti=(struct tok_info *)dev->priv;

  ti->open_status    = CLOSED;

  dev->init    = tok_init_card;
  dev->open    = tok_open;
  dev->stop    = tok_close;
  dev->hard_start_xmit  = tok_send_packet;
  dev->get_stats     = NULL;
  dev->get_stats     = tok_get_stats;
  dev->set_multicast_list = NULL;

#ifndef MODULE
  tr_setup(dev);
#endif
  return 0;
}



static int tok_open(struct device *dev)
{
  struct tok_info *ti=(struct tok_info *)dev->priv;

  if (ti->open_status==CLOSED) tok_init_card(dev);

  if (ti->open_status==IN_PROGRESS) sleep_on(&ti->wait_for_reset);

  if (ti->open_status==SUCCESS) {
    dev->tbusy=0;
    dev->interrupt=0;
    dev->start=1;
    /* NEED to see smem size *AND* reset high 512 bytes if needed */

    MOD_INC_USE_COUNT;

    return 0;
  } else return -EAGAIN;

}

static int tok_close(struct device *dev)
{

  struct tok_info *ti=(struct tok_info *) dev->priv;

  writeb(DIR_CLOSE_ADAPTER,
         ti->srb + offsetof(struct srb_close_adapter, command));
  writeb(CMD_IN_SRB, ti->mmio + ACA_OFFSET + ACA_SET + ISRA_ODD);

  ti->open_status=CLOSED;

  sleep_on(&ti->wait_for_tok_int);

  if (readb(ti->srb + offsetof(struct srb_close_adapter, ret_code)))
    DPRINTK("close adapter failed: %02X\n",
      (int)readb(ti->srb + offsetof(struct srb_close_adapter, ret_code)));

        dev->start = 0;
  DPRINTK("Adapter closed.\n");
  MOD_DEC_USE_COUNT;

  return 0;
}

void tok_interrupt (int irq, void *dev_id, struct pt_regs *regs)
{
  BYTE status;
  struct tok_info *ti;
  struct device *dev;

#if TR_VERBOSE
  DPRINTK("Int from tok_driver, dev : %p\n",dev);
#endif
  dev = (struct device *)(irq2dev_map[irq]);
  ti  = (struct tok_info *) dev->priv;

        /* Disable interrupts till processing is finished */
  dev->interrupt=1;
  writeb((~INT_ENABLE), ti->mmio + ACA_OFFSET + ACA_RESET + ISRP_EVEN);

  /* Reset interrupt for ISA boards */
        if (ti->adapter_int_enable)
                outb(0,ti->adapter_int_enable);
  else
    outb(0,ti->global_int_enable);


  switch (ti->do_tok_int) {

        case NOT_FIRST:

    /*  Begin the regular interrupt handler HERE inline to avoid
        the extra levels of logic and call depth for the
        original solution.   */

    status=readb(ti->mmio + ACA_OFFSET + ACA_RW + ISRP_ODD);
#ifdef PCMCIA
          /* Check if the PCMCIA card was pulled. */
        if (status == 0xFF)
           {
              DPRINTK("PCMCIA card removed.\n");
              dev->interrupt = 0;
                return;
           }

              /* Check ISRP EVEN too. */
                if ( readb (ti->mmio + ACA_OFFSET + ACA_RW + ISRP_EVEN) == 0xFF)
              {
              DPRINTK("PCMCIA card removed.\n");
              dev->interrupt = 0;
              return;
           }
#endif


    if (status & ADAP_CHK_INT) {

      int i;
      __u32 check_reason;

      check_reason=ti->mmio + ntohs(readw(ti->sram + ACA_OFFSET + ACA_RW +WWCR_EVEN));

      DPRINTK("Adapter check interrupt\n");
      DPRINTK("8 reason bytes follow: ");
      for(i=0; i<8; i++, check_reason++)
        printk("%02X ", (int)readb(check_reason));
      printk("\n");

      writeb((~ADAP_CHK_INT), ti->mmio + ACA_OFFSET + ACA_RESET + ISRP_ODD);
      writeb(INT_ENABLE, ti->mmio + ACA_OFFSET + ACA_SET  + ISRP_EVEN);
      dev->interrupt=0;

    } else if (readb(ti->mmio + ACA_OFFSET + ACA_RW + ISRP_EVEN)
         & (TCR_INT | ERR_INT | ACCESS_INT)) {

      DPRINTK("adapter error: ISRP_EVEN : %02x\n",
        (int)readb(ti->mmio + ACA_OFFSET + ACA_RW + ISRP_EVEN));
      writeb(~(TCR_INT | ERR_INT | ACCESS_INT),
             ti->mmio + ACA_OFFSET + ACA_RESET + ISRP_EVEN);
      writeb(INT_ENABLE, ti->mmio + ACA_OFFSET + ACA_SET  + ISRP_EVEN);
      dev->interrupt=0;

    } else if (status
         & (SRB_RESP_INT | ASB_FREE_INT | ARB_CMD_INT | SSB_RESP_INT)) {
      /* SRB, ASB, ARB or SSB response */

      if (status & SRB_RESP_INT) { /* SRB response */

        switch(readb(ti->srb)) { /* SRB command check */

              case XMIT_DIR_FRAME: {
                BYTE xmit_ret_code;

                xmit_ret_code=readb(ti->srb + offsetof(struct srb_xmit, ret_code));
                if (xmit_ret_code != 0xff) {
                  DPRINTK("error on xmit_dir_frame request: %02X\n",
                    xmit_ret_code);
                  if (ti->current_skb) {
                    dev_kfree_skb(ti->current_skb, FREE_WRITE);
                    ti->current_skb=NULL;
                  }
                  dev->tbusy=0;
                  if (ti->readlog_pending) ibmtr_readlog(dev);
                }
              }
              break;

              case XMIT_UI_FRAME: {
                BYTE xmit_ret_code;

                xmit_ret_code=readb(ti->srb + offsetof(struct srb_xmit, ret_code));
                if (xmit_ret_code != 0xff) {
                  DPRINTK("error on xmit_ui_frame request: %02X\n",
                    xmit_ret_code);
                  if (ti->current_skb) {
                    dev_kfree_skb(ti->current_skb, FREE_WRITE);
                    ti->current_skb=NULL;
                  }
                  dev->tbusy=0;
                  if (ti->readlog_pending) ibmtr_readlog(dev);
                }
              }
              break;

              case DIR_OPEN_ADAPTER: {
                BYTE open_ret_code;
                __u16 open_error_code;

                ti->srb=ti->sram+ntohs(readw(ti->init_srb +offsetof(struct srb_open_response, srb_addr)));
                ti->ssb=ti->sram+ntohs(readw(ti->init_srb +offsetof(struct srb_open_response, ssb_addr)));
                ti->arb=ti->sram+ntohs(readw(ti->init_srb +offsetof(struct srb_open_response, arb_addr)));
                ti->asb=ti->sram+ntohs(readw(ti->init_srb +offsetof(struct srb_open_response, asb_addr)));
                ti->current_skb=NULL;

                open_ret_code = readb(ti->init_srb +offsetof(struct srb_open_response, ret_code));
                open_error_code = ntohs(readw(ti->init_srb +offsetof(struct srb_open_response, error_code)));

                if (open_ret_code==7) {

                  if (!ti->auto_ringspeedsave && (open_error_code==0x24)) {
                    DPRINTK("Open failed: Adapter speed must match ring "
                      "speed if Automatic Ring Speed Save is disabled.\n");
                    ti->open_status=FAILURE;
                    wake_up(&ti->wait_for_reset);
                  } else if (open_error_code==0x24)
                    DPRINTK("Retrying open to adjust to ring speed.\n");
                  else if ((open_error_code==0x2d) && ti->auto_ringspeedsave)
                    DPRINTK("No signal detected for Auto Speed Detection.\n");
                  else if (open_error_code==0x11)
                  {
                          ti->open_status=FAILURE;
                          DPRINTK("Ring broken/disconnected.\n");
                          wake_up(&ti->wait_for_reset);
                  }
                  else DPRINTK("Unrecoverable error: error code = %04x.\n",
                   open_error_code);

                } else if (!open_ret_code) {
#if !TR_NEWFORMAT
                  DPRINTK("board opened...\n");
#else
                  DPRINTK("Adapter initialized and opened.\n");
#endif
                  writeb(~(SRB_RESP_INT),
                   ti->mmio + ACA_OFFSET + ACA_RESET + ISRP_ODD);
                  writeb(~(CMD_IN_SRB),
                   ti->mmio + ACA_OFFSET + ACA_RESET + ISRA_ODD);
                  open_sap(EXTENDED_SAP,dev);

                  /* YdW probably hates me */
                  goto skip_reset;
                } else
                  DPRINTK("open failed: ret_code = %02X, retrying\n",
                    open_ret_code);

                if (ti->open_status != FAILURE) {
                  ibmtr_reset_timer(&tr_timer, dev);
                }

              }
              break;

              case DIR_CLOSE_ADAPTER:
          wake_up(&ti->wait_for_tok_int);
          break;

              case DLC_OPEN_SAP:
          if (readb(ti->srb+offsetof(struct dlc_open_sap, ret_code))) {
            DPRINTK("open_sap failed: ret_code = %02X,retrying\n",
              (int)readb(ti->srb+offsetof(struct dlc_open_sap, ret_code)));
            ibmtr_reset_timer(&tr_timer, dev);
          } else {
            ti->exsap_station_id=
              readw(ti->srb+offsetof(struct dlc_open_sap, station_id));
            ti->open_status=SUCCESS; /* TR adapter is now available */
            wake_up(&ti->wait_for_reset);
          }
          break;

              case DIR_INTERRUPT:
              case DIR_MOD_OPEN_PARAMS:
              case DIR_SET_GRP_ADDR:
              case DIR_SET_FUNC_ADDR:
              case DLC_CLOSE_SAP:
          if (readb(ti->srb+offsetof(struct srb_interrupt, ret_code)))
            DPRINTK("error on %02X: %02X\n",
              (int)readb(ti->srb+offsetof(struct srb_interrupt, command)),
              (int)readb(ti->srb+offsetof(struct srb_interrupt, ret_code)));
          break;

              case DIR_READ_LOG:
          if (readb(ti->srb+offsetof(struct srb_read_log, ret_code)))
            DPRINTK("error on dir_read_log: %02X\n",
              (int)readb(ti->srb+offsetof(struct srb_read_log, ret_code)));
          else
            DPRINTK(
              "Line errors %02X, Internal errors %02X, Burst errors %02X\n"
              "A/C errors %02X, Abort delimiters %02X, Lost frames %02X\n"
              "Receive congestion count %02X, Frame copied errors %02X\n"
              "Frequency errors %02X, Token errors %02X\n",
              (int)readb(ti->srb+offsetof(struct srb_read_log,
                        line_errors)),
              (int)readb(ti->srb+offsetof(struct srb_read_log,
                        internal_errors)),
              (int)readb(ti->srb+offsetof(struct srb_read_log,
                        burst_errors)),
              (int)readb(ti->srb+offsetof(struct srb_read_log, A_C_errors)),
              (int)readb(ti->srb+offsetof(struct srb_read_log,
                        abort_delimiters)),
              (int)readb(ti->srb+offsetof(struct srb_read_log,
                        lost_frames)),
              (int)readb(ti->srb+offsetof(struct srb_read_log,
                            recv_congest_count)),
              (int)readb(ti->srb+offsetof(struct srb_read_log,
                        frame_copied_errors)),
              (int)readb(ti->srb+offsetof(struct srb_read_log,
                        frequency_errors)),
              (int)readb(ti->srb+offsetof(struct srb_read_log,
                            token_errors)));
          dev->tbusy=0;
          break;

              default:
          DPRINTK("Unknown command %02X encountered\n",
            (int)readb(ti->srb));

        } /* SRB command check */

        writeb(~CMD_IN_SRB, ti->mmio + ACA_OFFSET + ACA_RESET + ISRA_ODD);
        writeb(~SRB_RESP_INT, ti->mmio + ACA_OFFSET + ACA_RESET + ISRP_ODD);

        skip_reset:
      } /* SRB response */

      if (status & ASB_FREE_INT) { /* ASB response */

        switch(readb(ti->asb)) { /* ASB command check */

              case REC_DATA:
              case XMIT_UI_FRAME:
              case XMIT_DIR_FRAME:
          break;

              default:
          DPRINTK("unknown command in asb %02X\n",
            (int)readb(ti->asb));

        } /* ASB command check */

        if (readb(ti->asb+2)!=0xff) /* checks ret_code */
            DPRINTK("ASB error %02X in cmd %02X\n",
              (int)readb(ti->asb+2),(int)readb(ti->asb));
        writeb(~ASB_FREE_INT, ti->mmio + ACA_OFFSET + ACA_RESET + ISRP_ODD);

      } /* ASB response */

      if (status & ARB_CMD_INT) { /* ARB response */

        switch (readb(ti->arb)) { /* ARB command check */

              case DLC_STATUS:
          DPRINTK("DLC_STATUS new status: %02X on station %02X\n",
            ntohs(readw(ti->arb + offsetof(struct arb_dlc_status, status))),
            ntohs(readw(ti->arb
                      +offsetof(struct arb_dlc_status, station_id))));
          break;

              case REC_DATA:
          tr_rx(dev);
          break;

              case RING_STAT_CHANGE: {
                WORD ring_status;

                ring_status=ntohs(readw(ti->arb
                      +offsetof(struct arb_ring_stat_change, ring_status)));

                if (ring_status & (SIGNAL_LOSS | LOBE_FAULT)) {

                  DPRINTK("Signal loss/Lobe fault\n");
                  DPRINTK("We try to reopen the adapter.\n");
                  ibmtr_reset_timer(&tr_timer, dev);
                } else if (ring_status & (HARD_ERROR | XMIT_BEACON
                      | AUTO_REMOVAL | REMOVE_RECV | RING_RECOVER))
                  DPRINTK("New ring status: %02X\n", ring_status);

                if (ring_status & LOG_OVERFLOW) {
                  if (dev->tbusy)
                                                              ti->readlog_pending = 1;
                  else
                                                              ibmtr_readlog(dev);
                }
              }
              break;

              case XMIT_DATA_REQ:
          tr_tx(dev);
          break;

              default:
          DPRINTK("Unknown command %02X in arb\n",
            (int)readb(ti->arb));
          break;

        } /* ARB command check */

        writeb(~ARB_CMD_INT, ti->mmio + ACA_OFFSET + ACA_RESET + ISRP_ODD);
        writeb(ARB_FREE, ti->mmio + ACA_OFFSET + ACA_SET + ISRA_ODD);

      } /* ARB response */

      if (status & SSB_RESP_INT) { /* SSB response */

        switch (readb(ti->ssb)) { /* SSB command check */

              case XMIT_DIR_FRAME:
              case XMIT_UI_FRAME:
          if (readb(ti->ssb+2)) /* checks ret_code */
            DPRINTK("xmit ret_code: %02X xmit error code: %02X\n",
              (int)readb(ti->ssb+2), (int)readb(ti->ssb+6));
          else ti->tr_stats.tx_packets++;
          break;

              case XMIT_XID_CMD:
          DPRINTK("xmit xid ret_code: %02X\n", (int)readb(ti->ssb+2));

              default:
          DPRINTK("Unknown command %02X in ssb\n", (int)readb(ti->ssb));

        } /* SSB command check */

        writeb(~SSB_RESP_INT, ti->mmio + ACA_OFFSET + ACA_RESET + ISRP_ODD);
        writeb(SSB_FREE, ti->mmio + ACA_OFFSET + ACA_SET + ISRA_ODD);

      } /* SSB response */

    }   /* SRB, ARB, ASB or SSB response */

    dev->interrupt=0;
    writeb(INT_ENABLE, ti->mmio + ACA_OFFSET + ACA_SET + ISRP_EVEN);
    break;

        case FIRST_INT:
    initial_tok_int(dev);
    break;

        default:
    DPRINTK("Unexpected interrupt from tr adapter\n");

  }
}

static void initial_tok_int(struct device *dev)
{

  __u32 encoded_addr;
  __u32 hw_encoded_addr;
  struct tok_info *ti;

  ti=(struct tok_info *) dev->priv;

  ti->do_tok_int=NOT_FIRST;

#ifndef TR_NEWFORMAT
  DPRINTK("Initial tok int received\n");
#endif

  /* we assign the shared-ram address for ISA devices */
  if(!ti->sram) {
    writeb(ti->sram_base, ti->mmio + ACA_OFFSET + ACA_RW + RRR_EVEN);
    ti->sram=((__u32)ti->sram_base << 12);
  }
  ti->init_srb=ti->sram
    +ntohs((WORD)readw(ti->mmio+ ACA_OFFSET + WRBR_EVEN));
  SET_PAGE(ntohs((WORD)readw(ti->mmio+ACA_OFFSET + WRBR_EVEN)));

  dev->mem_start = ti->sram;
  dev->mem_end = ti->sram + (ti->mapped_ram_size<<9) - 1;

#if TR_VERBOSE
  {
    int i;
    DPRINTK("init_srb(%p):", ti->init_srb);
    for (i=0;i<17;i++) printk("%02X ", (int)readb(ti->init_srb+i));
    printk("\n");
  }
#endif

  hw_encoded_addr = readw(ti->init_srb
        + offsetof(struct srb_init_response, encoded_address));

#if !TR_NEWFORMAT
  DPRINTK("srb_init_response->encoded_address: %04X\n", hw_encoded_addr);
  DPRINTK("ntohs(srb_init_response->encoded_address): %04X\n",
    ntohs(hw_encoded_addr));
#endif

  encoded_addr=(ti->sram + ntohs(hw_encoded_addr));

#if !TR_NEWFORMAT
  DPRINTK("encoded addr (%04X,%04X,%08X): ", hw_encoded_addr,
    ntohs(hw_encoded_addr), encoded_addr);
#else
  DPRINTK("Initial interrupt : %s Mbps, shared RAM base %08x.\n",
    (readb(ti->init_srb+offsetof(struct srb_init_response, init_status)) & 0x01) ? "16" : "4", ti->sram);
#endif

  ti->auto_ringspeedsave=readb(ti->init_srb
             +offsetof(struct srb_init_response, init_status_2)) & 0x4 ? TRUE : FALSE;

#if !TR_NEWFORMAT
  for(i=0;i<TR_ALEN;i++) {
    dev->dev_addr[i]=readb(encoded_addr + i);
    printk("%02X%s", dev->dev_addr[i], (i==TR_ALEN-1) ? "" : ":" );
  }
  printk("\n");
#endif

  tok_open_adapter((DWORD)dev);
}

static int tok_init_card(struct device *dev)
{
  struct tok_info *ti;
  short PIOaddr;
  int i;
  PIOaddr = dev->base_addr;
  ti=(struct tok_info *) dev->priv;

  /* Special processing for first interrupt after reset */
  ti->do_tok_int=FIRST_INT;

  /* Reset adapter */
  dev->tbusy=1; /* nothing can be done before reset and open completed */

#ifdef ENABLE_PAGING
  if(ti->page_mask)
    writeb(SRPR_ENABLE_PAGING, ti->mmio + ACA_OFFSET + ACA_RW + SRPR_EVEN);
#endif

  writeb(~INT_ENABLE, ti->mmio + ACA_OFFSET + ACA_RESET + ISRP_EVEN);

#if !TR_NEWFORMAT
  DPRINTK("resetting card\n");
#endif

  outb(0, PIOaddr+ADAPTRESET);
  for (i=jiffies+TR_RESET_INTERVAL; jiffies<=i;); /* wait 50ms */
  outb(0,PIOaddr+ADAPTRESETREL);

#if !TR_NEWFORMAT
  DPRINTK("card reset\n");
#endif

  ti->open_status=IN_PROGRESS;
  writeb(INT_ENABLE, ti->mmio + ACA_OFFSET + ACA_SET + ISRP_EVEN);
  return 0;
}

static void open_sap(BYTE type,struct device *dev)
{
  int i;
  struct tok_info *ti=(struct tok_info *) dev->priv;

  SET_PAGE(ti->srb);
  for (i=0; i<sizeof(struct dlc_open_sap); i++)
    writeb(0, ti->srb+i);

  writeb(DLC_OPEN_SAP, ti->srb + offsetof(struct dlc_open_sap, command));
  writew(htons(MAX_I_FIELD),
         ti->srb + offsetof(struct dlc_open_sap, max_i_field));
  writeb(SAP_OPEN_IND_SAP | SAP_OPEN_PRIORITY,
         ti->srb + offsetof(struct dlc_open_sap, sap_options));
  writeb(SAP_OPEN_STATION_CNT,
         ti->srb + offsetof(struct dlc_open_sap, station_count));
  writeb(type, ti->srb + offsetof(struct dlc_open_sap, sap_value));

  writeb(CMD_IN_SRB, ti->mmio + ACA_OFFSET + ACA_SET + ISRA_ODD);

}

void tok_open_adapter(DWORD dev_addr)
{

  struct device *dev=(struct device *)dev_addr;
  struct tok_info *ti;
  int i;

  ti=(struct tok_info *) dev->priv;

#if !TR_NEWFORMAT
  DPRINTK("now opening the board...\n");
#endif

  writeb(~SRB_RESP_INT, ti->mmio + ACA_OFFSET + ACA_RESET + ISRP_ODD);
  writeb(~CMD_IN_SRB, ti->mmio + ACA_OFFSET + ACA_RESET + ISRA_ODD);

  for (i=0; i<sizeof(struct dir_open_adapter); i++)
    writeb(0, ti->init_srb+i);

  writeb(DIR_OPEN_ADAPTER,
         ti->init_srb + offsetof(struct dir_open_adapter, command));
  writew(htons(OPEN_PASS_BCON_MAC),
         ti->init_srb + offsetof(struct dir_open_adapter, open_options));
  writew(htons(NUM_RCV_BUF),
         ti->init_srb + offsetof(struct dir_open_adapter, num_rcv_buf));
  writew(htons(RCV_BUF_LEN),
         ti->init_srb + offsetof(struct dir_open_adapter, rcv_buf_len));
  writew(htons(DHB_LENGTH),
         ti->init_srb + offsetof(struct dir_open_adapter, dhb_length));
  writeb(NUM_DHB,
         ti->init_srb + offsetof(struct dir_open_adapter, num_dhb));
  writeb(DLC_MAX_SAP,
         ti->init_srb + offsetof(struct dir_open_adapter, dlc_max_sap));
  writeb(DLC_MAX_STA,
         ti->init_srb + offsetof(struct dir_open_adapter, dlc_max_sta));

  ti->srb=ti->init_srb; /* We use this one in the interrupt handler */

  writeb(INT_ENABLE, ti->mmio + ACA_OFFSET + ACA_SET + ISRP_EVEN);
  writeb(CMD_IN_SRB, ti->mmio + ACA_OFFSET + ACA_SET + ISRA_ODD);

}

static void tr_tx(struct device *dev)
{
  struct tok_info *ti=(struct tok_info *) dev->priv;
  struct trh_hdr *trhdr=(struct trh_hdr *)ti->current_skb->data;
  DWORD hdr_len;
  __u32 dhb;
  BYTE xmit_command;
  int i;
  struct trllc  *llc;

  if (readb(ti->asb + offsetof(struct asb_xmit_resp, ret_code))!=0xFF)
    DPRINTK("ASB not free !!!\n");

  /* in providing the transmit interrupts,
     is telling us it is ready for data and
     providing a shared memory address for us
     to stuff with data.  Here we compute the
     effective address where we will place data.*/
  dhb=ti->sram
    +ntohs(readw(ti->arb + offsetof(struct arb_xmit_req, dhb_address)));
  llc = (struct trllc *) &(ti->current_skb->data[sizeof(struct trh_hdr)]);

  xmit_command = readb(ti->srb + offsetof(struct srb_xmit, command));

  writeb(xmit_command, ti->asb + offsetof(struct asb_xmit_resp, command));
  writew(readb(ti->srb + offsetof(struct srb_xmit, station_id)),
         ti->asb + offsetof(struct asb_xmit_resp, station_id));
  writeb(llc->ssap, ti->asb + offsetof(struct asb_xmit_resp, rsap_value));
  writeb(readb(ti->srb + offsetof(struct srb_xmit, cmd_corr)),
         ti->asb + offsetof(struct asb_xmit_resp, cmd_corr));
  writeb(0, ti->asb + offsetof(struct asb_xmit_resp, ret_code));

  if ((xmit_command==XMIT_XID_CMD) || (xmit_command==XMIT_TEST_CMD)) {

    writew(htons(0x11),
           ti->asb + offsetof(struct asb_xmit_resp, frame_length));
    writeb(0x0e, ti->asb + offsetof(struct asb_xmit_resp, hdr_length));
    writeb(AC, dhb);
    writeb(LLC_FRAME, dhb+1);

    for (i=0; i<TR_ALEN; i++) writeb((int)0x0FF, dhb+i+2);
    for (i=0; i<TR_ALEN; i++) writeb(0, dhb+i+TR_ALEN+2);

    writeb(RESP_IN_ASB, ti->mmio + ACA_OFFSET + ACA_SET + ISRA_ODD);
    return;

  }

  /* the token ring packet is copied from sk_buff to the adapter
     buffer identified in the command data received with the
     interrupt.  The sk_buff area was set up with a maximum
     sized route information field so here we must compress
     out the extra (all) rif fields.   */
  /* nb/dwm .... I re-arranged code here to avoid copy of extra
     bytes, ended up with fewer statements as well. */

  /* TR arch. identifies if RIF present by high bit of source
     address.  So here we check if RIF present */

  if (!(trhdr->saddr[0] & 0x80)) { /* RIF present : preserve it */
    hdr_len=sizeof(struct trh_hdr)-18;

#if TR_VERBOSE
    DPRINTK("hdr_length: %d, frame length: %ld\n", hdr_len,
      ti->current_skb->len-18);
#endif
  } else hdr_len=((ntohs(trhdr->rcf) & TR_RCF_LEN_MASK)>>8)
      +sizeof(struct trh_hdr)-18;

  /* header length including rif is computed above, now move the data
     and set fields appropriately. */
#if USE_MEMCPY
  memcpy_toio(dhb, ti->current_skb->data, hdr_len);
#else
  for (i=0; i<hdr_len; i++)
    writeb(*(BYTE *)(ti->current_skb->data +i), dhb++);
#endif

  writeb(hdr_len, ti->asb + offsetof(struct asb_xmit_resp, hdr_length));
  writew(htons(ti->current_skb->len-sizeof(struct trh_hdr)+hdr_len),
         ti->asb + offsetof(struct asb_xmit_resp, frame_length));

  /*  now copy the actual packet data next to hdr */
#if USE_MEMCPY
  memcpy_toio(dhb + hdr_len,
        ti->current_skb->data + sizeof(struct trh_hdr),
        ti->current_skb->len - sizeof(struct trh_hdr));
#else
  for (i=0; i<ti->current_skb->len-sizeof(struct trh_hdr); i++)
    writeb(*(BYTE *)(ti->current_skb->data +sizeof(struct trh_hdr)+i),
           dhb+i);
#endif

  writeb(RESP_IN_ASB, ti->mmio + ACA_OFFSET + ACA_SET + ISRA_ODD);
  dev->tbusy=0;
  dev_kfree_skb(ti->current_skb,FREE_WRITE);
  ti->current_skb=NULL;
  mark_bh(NET_BH);
  if (ti->readlog_pending) ibmtr_readlog(dev);
}

static void tr_rx(struct device *dev)
{
  int i;
  struct tok_info *ti=(struct tok_info *) dev->priv;
  __u32 rbuffer;
  __u32 llc;
  BYTE *data;
  DWORD rbuffer_len, lan_hdr_len;
  DWORD arb_frame_len;
  struct sk_buff *skb;
  DWORD skb_size = 0;
  int  is8022 = 0;

  rbuffer=(ti->sram
     +ntohs(readw(ti->arb + offsetof(struct arb_rec_req, rec_buf_addr))));

  if(readb(ti->asb + offsetof(struct asb_rec, ret_code))!=0xFF)
    DPRINTK("ASB not free !!!\n");

  writeb(REC_DATA,
         ti->asb + offsetof(struct asb_rec, command));
  writew(readw(ti->arb + offsetof(struct arb_rec_req, station_id)),
         ti->asb + offsetof(struct asb_rec, station_id));
  writew(readw(ti->arb + offsetof(struct arb_rec_req, rec_buf_addr)),
         ti->asb + offsetof(struct asb_rec, rec_buf_addr));

  lan_hdr_len=readb(ti->arb + offsetof(struct arb_rec_req, lan_hdr_len));

  llc=(rbuffer+offsetof(struct rec_buf, data) + lan_hdr_len);

#if TR_VERBOSE
  DPRINTK("offsetof data: %02X lan_hdr_len: %02X\n",
    (DWORD)offsetof(struct rec_buf,data), (DWORD)lan_hdr_len);
  DPRINTK("llc: %08X rec_buf_addr: %04X ti->sram: %p\n", llc,
    ntohs(readw(ti->arb + offsetof(struct arb_rec_req, rec_buf_addr))),
    ti->sram);
  DPRINTK("dsap: %02X, ssap: %02X, llc: %02X, protid: %02X%02X%02X, "
    "ethertype: %04X\n",
    (int)readb(llc + offsetof(struct trllc, dsap)),
    (int)readb(llc + offsetof(struct trllc, ssap)),
    (int)readb(llc + offsetof(struct trllc, protid)),
    (int)readb(llc + offsetof(struct trllc, protid)+1),
    (int)readb(llc + offsetof(struct trllc, protid)+2),
    (int)readw(llc + offsetof(struct trllc, ethertype)));
#endif

  if (readb(llc + offsetof(struct trllc, llc))!=UI_CMD) {
#if !TR_FILTERNONUI
    DPRINTK("non-UI frame arrived. dropped. llc= %02X\n",
      (int)readb(llc + offsetof(struct trllc, llc))
#endif
      writeb(DATA_LOST, ti->asb + offsetof(struct asb_rec, ret_code));
      ti->tr_stats.rx_dropped++;
      writeb(RESP_IN_ASB, ti->mmio + ACA_OFFSET + ACA_SET + ISRA_ODD);
      return;
      }

    if ((readb(llc + offsetof(struct trllc, dsap))!=0xAA) ||
        (readb(llc + offsetof(struct trllc, ssap))!=0xAA)) {
      is8022 = 1;
    }

#if TR_VERBOSE
    if ((readb(llc + offsetof(struct trllc, dsap))!=0xAA) ||
        (readb(llc + offsetof(struct trllc, ssap))!=0xAA)) {

      __u32 trhhdr;

      trhhdr=(rbuffer+offsetof(struct rec_buf,data));

      DPRINTK("Probably non-IP frame received.\n");
      DPRINTK("ssap: %02X dsap: %02X saddr: %02X:%02X:%02X:%02X:%02X:%02X "
        "daddr: %02X:%02X:%02X:%02X:%02X:%02X\n",
        (int)readb(llc + offsetof(struct trllc, ssap)),
        (int)readb(llc + offsetof(struct trllc, dsap)),
        (int)readb(trhhdr + offsetof(struct trh_hdr, saddr)),
        (int)readb(trhhdr + offsetof(struct trh_hdr, saddr)+1),
        (int)readb(trhhdr + offsetof(struct trh_hdr, saddr)+2),
        (int)readb(trhhdr + offsetof(struct trh_hdr, saddr)+3),
        (int)readb(trhhdr + offsetof(struct trh_hdr, saddr)+4),
        (int)readb(trhhdr + offsetof(struct trh_hdr, saddr)+5),
        (int)readb(trhhdr + offsetof(struct trh_hdr, daddr)),
        (int)readb(trhhdr + offsetof(struct trh_hdr, daddr)+1),
        (int)readb(trhhdr + offsetof(struct trh_hdr, daddr)+2),
        (int)readb(trhhdr + offsetof(struct trh_hdr, daddr)+3),
        (int)readb(trhhdr + offsetof(struct trh_hdr, daddr)+4),
        (int)readb(trhhdr + offsetof(struct trh_hdr, daddr)+5));
    }
#endif

    arb_frame_len=ntohs(readw(ti->arb+offsetof(struct arb_rec_req, frame_len)));
    skb_size = arb_frame_len-lan_hdr_len+sizeof(struct trh_hdr);
    if (is8022) {
      skb_size += sizeof(struct trllc);
    }

    if (!(skb=dev_alloc_skb(skb_size))) {
      DPRINTK("out of memory. frame dropped.\n");
      ti->tr_stats.rx_dropped++;
      writeb(DATA_LOST, ti->asb + offsetof(struct asb_rec, ret_code));
      writeb(RESP_IN_ASB, ti->mmio + ACA_OFFSET + ACA_SET + ISRA_ODD);
      return;
    }

    skb_put(skb, skb_size);
    skb->dev=dev;

    data=skb->data;
#if USE_MEMCPY
    memcpy_fromio(data, rbuffer + offsetof(struct rec_buf, data), lan_hdr_len);
#else
    for (i=0; i<lan_hdr_len; i++)
      data[i]=readb(rbuffer + offsetof(struct rec_buf, data)+i);
#endif
    if (lan_hdr_len<sizeof(struct trh_hdr))
      memset(data+lan_hdr_len, 0, sizeof(struct trh_hdr)-lan_hdr_len);

    data+=sizeof(struct trh_hdr);
    rbuffer_len=ntohs(readw(rbuffer + offsetof(struct rec_buf, buf_len)))
      -lan_hdr_len;
    if (is8022) {
      struct trllc  *local_llc = (struct trllc *)data;
      memset(local_llc, 0, sizeof(*local_llc));
      local_llc->ethertype = htons(ETH_P_TR_802_2);
      data += sizeof(struct trllc);
    }

#if TR_VERBOSE
    DPRINTK("rbuffer_len: %d, data: %p\n", rbuffer_len, data);
#endif
#if USE_MEMCPY
    memcpy_fromio(data, rbuffer + offsetof(struct rec_buf, data) + lan_hdr_len, rbuffer_len);
#else
    for (i=0; i<rbuffer_len; i++)
      data[i]=readb(rbuffer+ offsetof(struct rec_buf, data)+lan_hdr_len+i);
#endif
    data+=rbuffer_len;

    while (readw(rbuffer + offsetof(struct rec_buf, buf_ptr))) {
      rbuffer=(ti->sram
         +ntohs(readw(rbuffer + offsetof(struct rec_buf, buf_ptr)))-2);
      rbuffer_len=ntohs(readw(rbuffer + offsetof(struct rec_buf, buf_len)));
      for (i=0; i<rbuffer_len; i++)
        data[i]=readb(rbuffer + offsetof(struct rec_buf, data)+i);
      data+=rbuffer_len;

#if TR_VERBOSE
      DPRINTK("buf_ptr: %d, data =%p\n",
        ntohs((rbuffer + offsetof(struct rec_buf, buf_ptr))), data);
#endif
    }

    writeb(0, ti->asb + offsetof(struct asb_rec, ret_code));

    writeb(RESP_IN_ASB, ti->mmio + ACA_OFFSET + ACA_SET + ISRA_ODD);

    ti->tr_stats.rx_packets++;

    skb->protocol=tr_type_trans(skb,dev);
    netif_rx(skb);

  }

static int tok_send_packet(struct sk_buff *skb, struct device *dev)
{
  struct tok_info *ti;
  ti=(struct tok_info *) dev->priv;

  if (dev->tbusy) {
    int ticks_waited;

    ticks_waited=jiffies - dev->trans_start;
    if (ticks_waited<TR_BUSY_INTERVAL) return 1;

    DPRINTK("Arrg. Transmitter busy.\n");
    dev->trans_start+=5; /* we fake the transmission start time... */
    return 1;
  }

  /* Donald does this, so we do too. */
  if (skb==NULL) {
    dev_tint(dev);
    return 0;
  }

  if (set_bit(0,(void *)&dev->tbusy)!=0)
    DPRINTK("Transmitter access conflict\n");
  else {
    /* Save skb; we'll need it when the adapter asks for the data */
    ti->current_skb=skb;
    writeb(XMIT_UI_FRAME, ti->srb + offsetof(struct srb_xmit, command));
    writew(ti->exsap_station_id, ti->srb
           +offsetof(struct srb_xmit, station_id));
    writeb(CMD_IN_SRB, (ti->mmio + ACA_OFFSET + ACA_SET + ISRA_ODD));
    dev->trans_start=jiffies;
  }

  return 0;
}

void ibmtr_reset_timer(struct timer_list *tmr, struct device *dev) {
  tmr->expires  = jiffies + TR_RETRY_INTERVAL;
  tmr->data     = (DWORD) dev;
  tmr->function = tok_open_adapter;
  init_timer(tmr);
  add_timer(tmr);
}

void ibmtr_readlog(struct device *dev) {
   struct tok_info *ti;
   ti=(struct tok_info *) dev->priv;

   ti->readlog_pending = 0;
   writeb(DIR_READ_LOG, ti->srb);
   writeb(INT_ENABLE, ti->mmio + ACA_OFFSET + ACA_SET + ISRP_EVEN);
   writeb(CMD_IN_SRB, ti->mmio + ACA_OFFSET + ACA_SET + ISRA_ODD);
   dev->tbusy=1; /* really srb busy... */
}

/* tok_get_stats():  Basically a scaffold routine which will return
   the address of the tr_statistics structure associated with
   this device -- the tr.... structure is an ethnet look-alike
   so at least for this iteration may suffice.   */

static struct net_device_stats * tok_get_stats(struct device *dev) {

  struct tok_info *toki;
  toki=(struct tok_info *) dev->priv;
  return (struct net_device_stats *) &toki->tr_stats;
}

#ifdef MODULE

/* 3COM 3C619C supports 8 interrupts, 32 I/O ports */
static struct device* dev_ibmtr[IBMTR_MAX_ADAPTERS];
static int io[IBMTR_MAX_ADAPTERS] = {0xa20,0xa24};
static int irq[IBMTR_MAX_ADAPTERS] = {0,0};
static int mem[IBMTR_MAX_ADAPTERS] = {0,0};

MODULE_PARM(io, "1-" __MODULE_STRING(IBMTR_MAX_ADAPTERS) "i");
MODULE_PARM(irq, "1-" __MODULE_STRING(IBMTR_MAX_ADAPTERS) "i");
MODULE_PARM(mem, "1-" __MODULE_STRING(IBMTR_MAX_ADAPTERS) "i");

int init_module(void)
{
        int i;
        for (i = 0; io[i] && (i<IBMTR_MAX_ADAPTERS); i++) {
          irq[i] = 0;
    mem[i] = 0;
    dev_ibmtr[i] = NULL;
                dev_ibmtr[i] = init_trdev(dev_ibmtr[i], 0);
                if (dev_ibmtr[i] == NULL)
                        return -ENOMEM;

          dev_ibmtr[i]->base_addr = io[i];
          dev_ibmtr[i]->irq       = irq[i];
    dev_ibmtr[i]->mem_start = mem[i];
    dev_ibmtr[i]->init      = &ibmtr_probe;

          if (register_trdev(dev_ibmtr[i]) != 0) {
      kfree_s(dev_ibmtr[i], sizeof(struct dev));
      dev_ibmtr[i] = NULL;
            if (i == 0) {
              printk("ibmtr: register_trdev() returned non-zero.\n");
                    return -EIO;
      } else {
              return 0;
      }
          }
  }
  return 0;
}

void cleanup_module(void)
{
        int i;

        for (i = 0; i < IBMTR_MAX_ADAPTERS; i++)
          if (dev_ibmtr[i]) {
       unregister_trdev(dev_ibmtr[i]);
       free_irq(dev_ibmtr[i]->irq, NULL);
       irq2dev_map[dev_ibmtr[i]->irq] = NULL;
       release_region(dev_ibmtr[i]->base_addr, IBMTR_IO_EXTENT);
       kfree_s(dev_ibmtr[i]->priv, sizeof(struct tok_info));
       kfree_s(dev_ibmtr[i], sizeof(struct dev));
       dev_ibmtr[i] = NULL;
                }
}
#endif /* MODULE */
