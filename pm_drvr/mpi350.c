/* 

    Copyright (C) 2002, 2003, 2004 Cisco Systems, Inc. All Rights Reserved.

    Dec 2003 - Masukawa - fixed compatibility with newer firmware
                          added driver version request
                          added wireless-tools interface support
			  added RF monitor support
*/
#undef BAD_CARD
int bad_card = 0;
/*
 * MIC Compatable MPI350 mini pci card driver 
 */
#ifndef __KERNEL__
#define __KERNEL__
#endif

#ifndef MODULE
#define MODULE
#endif

#include <linux/config.h>
#include <linux/version.h>
#include <linux/init.h>
#include <asm/segment.h>
#ifdef CONFIG_MODVERSIONS
#define MODVERSIONS
#include <linux/modversions.h>
#endif                 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,6)
#include <linux/slab.h>
#else
#include <linux/malloc.h>
#endif

#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/tqueue.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/bitops.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/ioport.h>
#include <linux/config.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

#ifdef CONFIG_PCI
#include <linux/pci.h>
#endif

/*
#include <linux/wireless.h>
*/

#include "aes.h"
#include "mpi350.h"

#ifndef MIN
#define MIN(x,y) ((x<y)?x:y)
#endif

#ifndef RUN_AT
#define RUN_AT(x) (jiffies+(x))
#endif

/*
 INCLUDE_RFMONITOR - "#define" this will include RF monitor mode code for use
                     with sniffers
                     must have WIRELESS_EXT  (include linux/wireless.h)
#define INCLUDE_RFMONITOR
 */

static struct net_device *venus_device;

#if (LINUX_VERSION_CODE >= 0x20420)
MODULE_LICENSE("MPL");
#endif
MODULE_AUTHOR("Roland Wilcher");
MODULE_DESCRIPTION("Support for Cisco/Aironet 802.11 wireless ethernet \
                   cards.  Direct support for MPI350 PCI cards with MIC support");

MODULE_SUPPORTED_DEVICE("Aironet Cisco MPI350 PCI card.");

unsigned char iobuf[2048];
static char flashbuffer[FLASHSIZE];

typedef struct aironet_ioctl {
  unsigned short command;	// What to do
  unsigned short length;	// Len of data
  unsigned short ridnum;	// rid number
  unsigned char *data;		// d-data
} aironet_ioctl;


#if (LINUX_VERSION_CODE < 0x20313)

static struct proc_dir_entry venus_entry = {
  0,                              // low ino  
  6,                              // namelen
  PRODNAME,                       // name
  S_IFDIR | S_IRUGO | S_IXUGO,    // mode
  2,                              // nlink
  0, 0,                           // uid gid
  0,                              // size
  &proc_dir_inode_operations,     // ops
  0,                              // get_info
  0,                              // fill inode 
  0,&proc_root,0                  // next parent subdir
};

static void *pci_alloc_consistent(struct pci_dev *,size_t,dma_addr_t*);
static void pci_free_consistent(struct pci_dev *,size_t,void *,dma_addr_t);

#if LINUX_VERSION_CODE < VERSION_CODE(2,3,55)
static int  pci_enable_device(struct pci_dev *);
#endif
static unsigned long pci_resource_start(struct pci_dev *,int);

#if LINUX_VERSION_CODE < VERSION_CODE(2,2,19)
static unsigned long pci_resource_len(struct pci_dev *,int);
#endif
#endif


static char *version = "mpi350.c 2.1 2002/05/08 (R. Wilcher )"; 
static char *swversion = "2.1";
static int venus_clear(struct venus_info *);
#if (LINUX_VERSION_CODE < 0x20355 )
static void venus_timeout(u_long data);
#endif
static int start_venus(struct net_device *);
static int venus_init_descriptors(struct net_device *);
static int flashcard(struct net_device *, aironet_ioctl *);
static void venustxtmo(struct net_device *);
static int txreclaim(struct net_device *);
static int send_packet(struct net_device *);
static void venus_linkstat(struct net_device *,int );
static struct net_device_stats *venus_get_stats(struct net_device *);
#ifdef WIRELESS_EXT
static struct iw_statistics *airo_get_wireless_stats(struct net_device *dev);
#endif
static int venus_change_mtu(struct net_device *,int);
static int venus_close(struct net_device *);
static int venus_transmit(struct sk_buff *, struct net_device *);
static void venus_kick(struct venus_info *);
static unsigned short venusin(struct venus_info *,u16 register );
static void venusout(struct venus_info *,u16 register,u16 value );
static int stop_venus_card( struct net_device *vdev);
static u16 venuscommand(struct venus_info*, Cmd *pCmd, Resp *pRsp);
struct net_device *init_venus_card(struct pci_dev *);
void   venus_remove(void);
static int  venus_probe(struct pci_dev *);
static int vreadrid(struct venus_info *,unsigned short,unsigned char *, int);
static int vwriterid(struct venus_info *,unsigned short,unsigned char *);
static void venus_interrupt( int irq, void* dev_id, struct pt_regs    *regs);
static int venus_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static void venus_set_multicast_list(struct net_device *dev);
#ifdef INCLUDE_RFMONITOR
static void venus_set_rfmonitor(struct venus_info *v_info);
#endif
static int mac_enable(struct venus_info *, Resp * );
static void mac_disable( struct venus_info * );
static void disable_interrupts(struct venus_info *);
static void enable_interrupts(struct  venus_info *);
static int takedown_proc_entry( struct net_device *,struct venus_info  *);
/* Mic functions*/
static void micinit(struct venus_info *, STMIC *);
static void micsetup(struct venus_info *);
static int RxSeqValid (struct venus_info *,MICCNTX *context, u32 micSeq);
static void MoveWindow(MICCNTX *, u32 );
static int Encapsulate(struct venus_info *,ETH_HEADER_STRUC *, u32 *,int );
static int Decapsulate(struct venus_info *,ETH_HEADER_STRUC *, u32 *);
static void UshortByteSwap(u16 *);
static void UlongByteSwap(u32 *);

static ssize_t proc_read( struct file *,char *,size_t ,loff_t * );
static ssize_t proc_write( struct file *,const char *,size_t ,loff_t * );
static int proc_close( struct inode *, struct file * ); 
static int proc_ssid_open( struct inode *, struct file * );
static int proc_status_open( struct inode *, struct file * );

static int venus_reinit(struct net_device *vdev);
static int venus_suspend (struct pci_dev *pdev, u32 state);
static int venus_resume(struct pci_dev *pdev);
static int venus_init_one (struct pci_dev *pdev, const struct pci_device_id *ent);
static int venus_save_state (struct pci_dev *pdev, u32 state);
static int venus_enable_wake (struct pci_dev *pdev, u32 state, int enable);
static void venus_remove_one (struct pci_dev *pdev);

#ifdef WIRELESS_EXT
static int getQuality(STSTATUS *statusRid, STCAPS *capsRid);
#endif

static struct net_device *save_net_dev;

#define MODNAME "mpi350"
/* 
 * Proc filesystem definitions
 */

static struct file_operations proc_ssid_ops = {
  read:          proc_read,
  write:         proc_write,
  open:          proc_ssid_open,
  release:       proc_close
};

static struct file_operations proc_status_ops = {
	read:            proc_read,
	open:            proc_status_open,
	release:         proc_close
};

/*
 * Handle 2.2 /proc
 */

#if (LINUX_VERSION_CODE < 0x20311)
static struct inode_operations proc_inode_ssid_ops = {
  &proc_ssid_ops};

static struct inode_operations proc_inode_status_ops = {
  &proc_status_ops};

static struct proc_dir_entry ssid_entry = {
  0, 4, "SSID",
  S_IFREG | S_IRUGO | S_IWUSR, 2, 0, 0,
  13,
  &proc_inode_ssid_ops, NULL
};

static struct proc_dir_entry status_entry = {
  0,6,"Status",
  S_IFREG | S_IRUGO | S_IWUSR, 2, 0, 0,
  13,
  &proc_inode_status_ops,NULL
};
#endif

static struct pci_device_id netdrv_pci_tbl[] = {
  {0x14b9, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}
};

static struct pci_driver venus_pci_driver = {
        name:           MODNAME,
#if 0
        id_table:       netdrv_pci_tbl,
        probe:          netdrv_init_one,
        remove:         __devexit_p(netdrv_remove_one),
#endif
        id_table:       netdrv_pci_tbl,
        save_state:     venus_save_state,
        enable_wake:    venus_enable_wake,
        remove:         venus_remove_one,
        probe:          venus_init_one,
        suspend:        venus_suspend,
        resume:         venus_resume,
};


#if (LINUX_VERSION_CODE < 0x20355 )
static void venus_timeout(u_long data){
  struct net_device *dev;
  struct venus_info *v_info ;

  dev = (struct net_device *)data;
  v_info = (struct venus_info *)dev->priv;

  venustxtmo(dev);

  if(!timer_pending(&v_info->timer)){
    v_info->timer.expires = RUN_AT(HZ * TXTIME);
    add_timer(&v_info->timer);
  }
}
#endif



/*
 * Tickle TX ring if needed  for unwedging tx
 */
static void venustxtmo(struct net_device *dev){
  struct venus_info *v_info ;
  int    lasttx;

  lasttx = jiffies - dev->trans_start;
  v_info = (struct venus_info *)dev->priv;
  /* 
   * 200 ticks between xmit packets is a stall
   */
  if(lasttx >= 200  && (skb_queue_len(&v_info->txq) !=0)  && v_info->flags & ASSOC){
    /*
     * If presently locked, try again next pass 
     */
   if(!spin_trylock(&v_info->txd_lock)) {
      v_info->flags |= TXBUSY;
	
      if(txreclaim(dev)){
	send_packet(dev);
	netif_wake_queue(dev);
      }
#ifdef DEBUG_TXD
      else
	printk(KERN_INFO "No TXD's txt =%x!!\n",lasttx);      
#endif
      spin_unlock(&v_info->txd_lock);
   }
   else
     return;
  }
}


/*
 * Reclaim any outstanding TX descriptor(s). Return number 
 * reclaimed. 
 */

static int txreclaim(struct net_device *dev){
  struct       venus_info *v_info  = (struct venus_info*)dev->priv;
  CARD_TX_DESC txd;

  memset((char *)&txd,0,sizeof(txd));
  memcpy((char *)&txd,(char *)v_info->txfids[0].CardRamOff,sizeof(txd));

  if(!txd.valid ){
    txd.valid = 1;
    txd.eoc = 1;
    memcpy((char *)v_info->txfids[0].CardRamOff,(char *)&txd,sizeof(txd));
    v_info->txdfc = 1;

    return 1;
  }
  else
    return 0;
}

/*
 * @send_packet
 *
 * Attempt to transmit a packet. Can be called from interrupt 
 * or transmit . return number of packets we tried to send 
 */
static int send_packet(struct net_device *dev){
  struct   sk_buff *skb;
  int      npacks,adhoc;
  unsigned char *buffer;
  s16      len,*payloadlen;
  struct   venus_info *v_info  = (struct venus_info*)dev->priv;
  u32      miclen;
  u8       *sendbuf;

  adhoc = 0;
  npacks = skb_queue_len(&v_info->txq);

  if(!npacks){
    v_info->flags &= ~(TXBUSY);
//    printk(KERN_ERR "MPI350 : send_packet : transmit busy!\n");
    return 0;
  }

  /* check for txd available. Its assumed
   * that reclaim has been called. and an
   * outgoing packet has been queue'd
   */
  if(v_info->txdfc <= 0){
//    printk(KERN_ERR "MPI350 : send_packet : txd busy!\n");
    return 0;
  }

  /* get packet to send */

  if((skb = skb_dequeue(&v_info->txq))==0){
//    printk(KERN_ERR "Dequeue'd zero in send_packet !!!!\n");
    return 0;
  }

  len = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;  
  buffer = skb->data;

  v_info->txfids[0].TxDesc.Offset=  0; 
  v_info->txfids[0].TxDesc.valid =  1;
  v_info->txfids[0].TxDesc.eoc   =  1;
  v_info->txfids[0].TxDesc.length= len + sizeof(HDR_802_11);

  /*
   * Magic, the cards firmware needs a length count (2 bytes) in the host buffer 
   * right after  TXFID_HDR. The TXFID_HDR contains the status short so payloadlen 
   * is immediatly after it. ------------------------------------------------
   *                         |TXFIDHDR+STATUS|PAYLOADLEN|802.3HDR|PACKETDATA|
   *                         ------------------------------------------------
   */


  memcpy((char *)v_info->txfids[0].VirtualHostAddress,
	 (char *)&v_info->txFidHdr,sizeof(TXFID_HDR));

  payloadlen = (s16 *)(v_info->txfids[0].VirtualHostAddress + sizeof(TXFID_HDR));
  sendbuf = v_info->txfids[0].VirtualHostAddress + sizeof(TXFID_HDR) + 2;
  /*
   * Firmware automaticly puts 802 header on so
   * we don't need to account for it in the length
   */
  miclen = len;
  *payloadlen = len - sizeof(HDR_802_3);
  dev->trans_start = jiffies;

  /* copy data into venus dma buffer */
  memcpy(sendbuf ,buffer,len  );  

  /* Do the right thing (TM) for mic */

  if(v_info->flags & MIC_CAPABLE){
    adhoc = (v_info->flags & ADHOC) ? 1:0;
    if(Encapsulate(v_info,(ETH_HEADER_STRUC *)sendbuf,&miclen , adhoc))  {
      *payloadlen = (s16 ) (miclen - sizeof(HDR_802_3));
      v_info->txfids[0].TxDesc.length= miclen + sizeof(HDR_802_11);
    }
    else
      {
#ifdef DEBUG_MIC
	printk(KERN_INFO "send_packet : Dumped tx packet\n");
#endif
	goto dumptx;
      }      
  }
  memcpy((char *)v_info->txfids[0].CardRamOff,
	 (char *)&v_info->txfids[0].TxDesc, sizeof(CARD_TX_DESC));
  --v_info->txdfc;
  venusout(v_info,V_EVACK,8);  /* Send packet */
 dumptx:

  if(in_irq())
    dev_kfree_skb_irq(skb);
  else
    dev_kfree_skb(skb);
  return 1;
}


static void venus_linkstat(struct net_device *dev,int linkstatus){
  struct   venus_info *v_info  = (struct venus_info*)dev->priv;

#if ASSOCDEBUG
  printk(KERN_INFO "Link stat int ls=%x\n",linkstatus);
#endif

  if(linkstatus & 0x8000 ){
#ifdef ASSOCDEBUG
    printk(KERN_INFO "No carrier\n");
#endif
    v_info->flags &= ~(ASSOC);
    netif_carrier_off(  dev);
    netif_stop_queue( dev );
  }
  else 
    if(linkstatus & 0x400){
      v_info->flags |= ASSOC;
#ifdef ASSOCDEBUG
      printk(KERN_INFO "Carrier On\n");
#endif
      /* Update sequence numbers 
       * upon assoc 
       */
      v_info->updateMultiSeq = TRUE;     
      v_info->updateUniSeq = TRUE;

      netif_carrier_on( dev);
      netif_wake_queue(dev);
    }
}


/*
 * @interrupt
 */
static void venus_interrupt ( int irq, void *dev_id, struct pt_regs *regs) {
  struct net_device *dev = (struct net_device *)dev_id;
  u16 stat1,stat=0,len,i;
  struct venus_info *v_info = (struct venus_info *)dev->priv;
  CARD_RX_DESC rxd;
  u32    len32;
  struct sk_buff *skb;
  STMIC  mic;
#ifdef INCLUDE_RFMONITOR
  char *buffer;
#endif

  if (!netif_device_present(dev))
    return;

  spin_lock(&v_info->mpi_lock ); /* MPI350 Globl lock */
  memset((char *)&rxd,0,sizeof(rxd));

  stat1  = venusin(v_info,V_EVSTAT);

  if(stat1 == 0 ){
    spin_unlock(&v_info->mpi_lock ); /* Bogus interrupt */
    return;
  }
  if(stat1 & EV_MIC){
#ifdef DEBUG_MIC
    printk(KERN_INFO "MIC interrupt\n");
#endif
    venusout(v_info,V_EVACK,EV_MIC);
    vreadrid((struct venus_info *)dev->priv,RID_MIC,(char *)&mic, sizeof(STMIC));
    micinit(v_info,&mic);
//    enable_interrupts(v_info);
    spin_unlock(&v_info->mpi_lock);
    return;
  }
  if(stat1 & EV_LINK){
    stat = venusin(v_info,V_LINKSTAT);
    venus_linkstat(dev,stat);
    venusout(v_info,V_EVACK,EV_LINK);
    stat1 &= EV_LINK;
  }

  if(stat1 & EV_AWAKE )
    stat &= ~(EV_AWAKE);

  if(stat1 & EV_CMD ){
    venusout(v_info, V_EVACK, EV_CMD);  
#ifdef DEBUG_CMD
    printk(KERN_ERR "Command int!\n");
#endif
    stat1 &= EV_CMD;
  }

  if(stat1 & EV_RX){
    memcpy((char *)&rxd, v_info->rxfids[0].CardRamOff,sizeof(rxd));
    /* Make sure we got something */
    if(rxd.RxDone && rxd.valid ==0 ){
#ifdef INCLUDE_RFMONITOR
      if (v_info->rfMonitor) {
        if (v_info->rxfids[0].VirtualHostAddress[4] & 0x2) { /* CRC error */
          len = 0;
        } else {
          len = *(u16*)((u16*)(v_info->rxfids[0].VirtualHostAddress)+3)+30;
        }
        if (len && len < 2312) {
          v_info->stats.rx_packets++;
          v_info->stats.rx_bytes += len;
          skb = dev_alloc_skb( len );	
          buffer = skb_put(skb, len);
          memcpy(buffer, v_info->rxfids[0].VirtualHostAddress+20, 30);
          /* gap length */
          i = *(u16*)((u16*)(v_info->rxfids[0].VirtualHostAddress)+25);
          if (i+len+52 > rxd.length) {
            i = 0;
          }
          /* skip the gap */
          memcpy(buffer+30, v_info->rxfids[0].VirtualHostAddress+52+i,len-30);
          skb->mac.raw = skb->data;
          skb->pkt_type = PACKET_OTHERHOST;
          skb->protocol = htons(ETH_P_802_2);
          skb->dev = dev;
          skb->ip_summed = CHECKSUM_NONE;
          dev->last_rx = jiffies;
          netif_rx( skb );
        }
      } else
#endif
      {
          len = rxd.length + 12;
          if( len > 12 && len < 2048 ){
          skb = dev_alloc_skb( len  );	
          if(v_info->flags &  MIC_CAPABLE && !v_info->rfMonitor){ 	/* Check for MIC */
	    len32 = (u32)len;

	    if(Decapsulate(v_info ,
			 (ETH_HEADER_STRUC *)v_info->rxfids[0].VirtualHostAddress, 
			 &len32 ))
	    {
#ifdef DEBUG_MIC
	        printk(KERN_ERR "venus_interrupt : Dumping packet from decap\n");
#endif
	        dev_kfree_skb_irq(skb);
	        goto dumpit;
	      }
	    len = len32 ;
          }

          v_info->stats.rx_packets++;
	  memcpy(skb_put(skb,len),v_info->rxfids[0].VirtualHostAddress,len);
	  skb->protocol = eth_type_trans( skb, dev );
	  v_info->stats.rx_bytes += len;
	  skb->dev = dev;
	  skb->ip_summed = CHECKSUM_NONE;
	  dev->last_rx = jiffies;
	  netif_rx( skb );
        }
      }
    }
  dumpit:
    if(rxd.valid == 0){
      rxd.valid = 1;
      rxd.RxDone= 0;
      rxd.length= HOSTBUFSIZ;  
      memcpy(v_info->rxfids[0].CardRamOff,(char *)&rxd,sizeof(rxd));
    }
  }

  if((stat1 & EV_TX) || (stat1 & EV_TXCPY) ){
//    printk(KERN_INFO "MPI350 : transmit interrupt\n");
    ++v_info->stats.tx_packets;
    v_info->stats.tx_bytes += v_info->txfids[0].TxDesc.length;

    if(skb_queue_len(&v_info->txq) !=0){
      v_info->flags |= TXBUSY;
      i = txreclaim(dev);
      send_packet(dev);
    }
    else
      {
	v_info->flags &= ~(TXBUSY);
	netif_wake_queue(dev);  
      }
  }
  venusout(v_info,V_EVACK,stat1);
  spin_unlock(&v_info->mpi_lock ); /* MPI350 Globl lock */
  return;
}


/*
 * @open
 */
static int venus_open(struct net_device *vdev){  
  struct venus_info *v_info;

//  printk( KERN_ERR "MPI350 : venus_open\n" );
  MOD_INC_USE_COUNT;
  v_info = (struct venus_info *)vdev->priv;
  v_info->open++;
  enable_interrupts(v_info);
  netif_start_queue(vdev);
  return 0;

}

static void venus_remove_one (struct pci_dev *pdev)
{
//  printk( KERN_ERR "MPI350 : venus_remove\n");
}

static int venus_enable_wake (struct pci_dev *pdev, u32 state, int enable)
{
//  printk( KERN_ERR "MPI350 : venus_enable_wake state %d enable %d\n", state, enable);
  return 0;
}

static int venus_save_state (struct pci_dev *pdev, u32 state)
{
//  printk( KERN_ERR "MPI350 : venus_save_state state %d\n", state);
  return 0;
}

static int venus_init_one (struct pci_dev *pdev, const struct pci_device_id *ent)

{
//  struct net_device *dev;
//  void *ioaddr = NULL;
  int status = 0;

//  printk( KERN_ERR "MPI350 : venus_init_one\n" );
  pci_set_drvdata(pdev, (void*)ent->driver_data);
  return status;
}
/*
 * suspend
 */
static int venus_suspend (struct pci_dev *pdev, u32 state)
{
  struct net_device *dev;
//  struct netdrv_private *tp = dev->priv;
//  void *ioaddr = tp->mmio_addr;
//  unsigned long flags;
  struct venus_info *v_info;

//  printk( KERN_ERR "MPI350 : venus_suspend venus_device %p\n", venus_device);
//  pci_unregister_driver(pdev->driver);
  if (!pdev) {
      return 0;
  }
  dev = pci_get_drvdata (pdev);
  if (!dev) {
      return 0;
  }
  if (!venus_device) {
      return 0;
  }
  v_info = (struct venus_info *)dev->priv;
//  printk( KERN_ERR "MPI350 : venus_suspend awaken %d\n", v_info->awaken );
  v_info->awaken = 0;
#if 1
  v_info->saveMask = venusin(v_info,V_EVINTEN);
  vreadrid(v_info,RID_CONFIG,(char *)v_info->saveConfig, 2048);
  *(unsigned short *) (v_info->saveConfig) = 0x9c;
  vreadrid(v_info,RID_SSID,(char *)v_info->saveSSID, 2048);
  *(unsigned short *) (v_info->saveSSID) = 0x68;
  vreadrid(v_info,0xff12,(char *)v_info->saveAPList, 2048);
  *(unsigned short *) (v_info->saveAPList) = 0x1a;
#endif
  return 0;
  if (!netif_running(dev))
    return -1;
  netif_device_detach (dev);

//  spin_lock_irqsave(&v_info->mpi_lock,flags);  

  /* Disable interrupts, stop Tx and Rx. */
//  NETDRV_W16 (IntrMask, 0x0000);
//  NETDRV_W8 (ChipCmd, (NETDRV_R8 (ChipCmd) & ChipCmdClear));

  /* Update the error counts. */
//  tp->stats.rx_missed_errors += NETDRV_R32 (RxMissed);
//  NETDRV_W32 (RxMissed, 0);

//  spin_unlock_irqrestore (&v_info->mpi_lock, flags);

//  pci_power_off (pdev, -1);

  return 0;
}

/*
 * resume
 */
static int venus_resume(struct pci_dev *pdev)
{
  struct net_device *dev;
  struct venus_info *v_info;

//  printk( KERN_ERR "MPI350 : venus_resume\n");
  if (!pdev) {
      return 0;
  }
  dev = pci_get_drvdata (pdev);
  if (!dev) {
      return 0;
  }
  if (!venus_device) {
      return 0;
  }
  v_info = (struct venus_info *)dev->priv;
//  printk( KERN_ERR "MPI350 : venus_resume awaken %d\n", v_info->awaken );
  v_info->awaken = 1;

//  pci_set_power_state(pdev, 0);
  venus_reinit(dev);
  v_info->awaken = 0;
#if 1
  vwriterid((struct venus_info *)dev->priv,RID_SSID,(char *)v_info->saveSSID);
  vwriterid((struct venus_info *)dev->priv,0xff12,(char *)v_info->saveAPList);
  vwriterid((struct venus_info *)dev->priv,RID_CONFIG,(char *)v_info->saveConfig);
#endif
//  netif_start_queue(pdev );
//  printk( KERN_ERR "MPI350 : venus_resume EVINTEN %02x\n", v_info->saveMask );
  venusout(v_info,V_EVINTEN,STATUS_INTS);
//  v_info->saveMask = venusin(v_info,V_EVINTEN);
//  printk( KERN_ERR "MPI350 : venus_resume EVINTEN %02x\n", v_info->saveMask );
  return 0;
  if (!netif_running(dev))
    return -1;
//  pci_power_on (pdev);
  netif_device_attach (dev);
//  netdrv_hw_start (dev);

  return 0;
}

/*
 * reinit
 */
static int venus_reinit(struct net_device *vdev){  
  struct venus_info *v_info;
  Cmd  cmd;
  Resp rsp;
  int status;

//  printk( KERN_ERR "MPI350 : venus_reinit\n" );
  v_info = (struct venus_info *)vdev->priv;
#if 0
  if (pci_enable_device(v_info->pcip)){
    printk(KERN_INFO "MPI350 venus_reinit:Can't init card\n");
    return 0;
  }
  mdelay(4096);
#endif
#if 1
  venusout(v_info,V_EVINTEN,0x0);
#if 0
  memset(&rsp,0,sizeof(rsp));
  memset(&cmd,0,sizeof(cmd));  

  cmd.cmd = CMD_X500_ResetCard;
  cmd.parm0 = cmd.parm1 = cmd.parm2 = 0;
  venuscommand(v_info,&cmd,&rsp);

  mdelay(2048);
#endif
  venus_init_descriptors(vdev);

  memset(&rsp,0,sizeof(rsp));
  memset(&cmd,0,sizeof(cmd));
  cmd.cmd = CMD_X500_NOP10;	

  if((status = venuscommand(v_info,&cmd,&rsp))!=0) {
    printk(KERN_INFO "MPI350 venus_reinit:Can't NOP card\n");
    return status;
  }
//  mdelay(4096);

//  vreadrid(v_info,RID_CAPABILITIES,iobuf);
//  vreadrid(v_info,RID_CONFIG,iobuf);
//  pci_set_master (v_info->pcip); /* Gets called twice no harm */

  if((status = mac_enable(v_info,&rsp))!=0) {
    printk(KERN_INFO "MPI350 venus_reinit:Can't enable card\n");
    return status;
  }
#endif
  return 0;
}

static struct net_device_stats *venus_get_stats(struct net_device *dev){
  return &(((struct venus_info *)dev->priv)->stats);
}

static int  venus_change_mtu(struct net_device *dev, int new_mtu){

  if ((new_mtu < 68) || (new_mtu > 2400))
    return -EINVAL;
  dev->mtu = new_mtu;
  return 0;
}

/*
 * @close
 */
static int venus_close(struct net_device *dev) { 
  struct venus_info *vi = (struct venus_info*)dev->priv;

//  printk( KERN_ERR "MPI350 : venus_close\n" );
  vi->open--;
  netif_stop_queue(dev);

  if ( !vi->open ) {
    disable_interrupts( vi );
    vi->open = 0;
  }
  MOD_DEC_USE_COUNT;
  return 0;
}


/*
 * @venus_transmit
 */

static int venus_transmit(struct sk_buff *skb, struct net_device *dev) {
  int    flags,npacks;
  struct venus_info *v_info  = (struct venus_info*)dev->priv;
  
#ifdef BAD_CARD
  if (bad_card) {
    return 0;
  }
#endif
  if ( skb == NULL ) {
    printk( KERN_ERR "MPI350 :  skb == NULL!!!\n" );
    return 0;
  }
  if ( v_info->rfMonitor ) {
    printk( KERN_ERR "MPI350 : no transmit - rfmonitor mode!!!\n" );
    return 0;
  }
  disable_interrupts(v_info);

  spin_lock_irqsave(&v_info->mpi_lock,flags);  

  npacks = skb_queue_len(&v_info->txq);

  if(npacks >= MAXTXQ ){
    netif_stop_queue(dev);
    skb_queue_tail(&v_info->txq,skb);
    dev_kfree_skb(skb); /* Throw on floor */
    v_info->stats.tx_dropped++;
    enable_interrupts(v_info);
    spin_unlock_irqrestore(&v_info->mpi_lock,flags);
    return 0;
  }
  if(npacks >= MAXTXQ - 10 ){
    netif_stop_queue(dev);
    skb_queue_tail(&v_info->txq,skb);
    enable_interrupts(v_info);
    spin_unlock_irqrestore(&v_info->mpi_lock,flags);
    return 0;
  }
  
  skb_queue_tail(&v_info->txq, skb);   

  netif_wake_queue(dev);

  if((v_info->flags & TXBUSY) == 0){
    txreclaim(dev);
    send_packet(dev);  
  }
  enable_interrupts(v_info);
  spin_unlock_irqrestore(&v_info->mpi_lock,flags);
  return 0;
}





/* @RAW
 * @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
 * @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
 * Setup mpi350 card.
 */

/*
 * @init_mod
 */
int init_module( void )
{
  struct   pci_dev *pcidev = 0;	
  struct   venus_info *v_info ;
#if LINUX_VERSION_CODE > VERSION_CODE(2,3,55)
  struct   proc_dir_entry *entry;
#endif
  unsigned cardid;
  int      i;

  printk(KERN_INFO "Probing for MPI350 card\n");

  /* Scan through all known Id's 
   * for this device
   */
  for(i=0;(cardid = venus_ids[i])!=0;i++){
    if((pcidev = pci_find_device(AIRONET,cardid,pcidev))!=0){
      printk(KERN_INFO "MPI350 found\n");
      /* 
       * Check for braindead card
       */
      if(venus_probe(pcidev)!=0)
	return -ENODEV;
      v_info = (struct venus_info *)venus_device->priv;

#if (LINUX_VERSION_CODE > 0x20313)
      v_info->proc_entry=create_proc_entry(PRODNAME,S_IFDIR | S_IRUGO|S_IXUGO,proc_root_driver);
      
      entry = create_proc_entry("SSID",S_IFREG|S_IRUGO,v_info->proc_entry);
      entry->data = venus_device;
      entry->proc_fops = &proc_ssid_ops;
      
      entry = create_proc_entry("Status",S_IFREG|S_IRUGO,v_info->proc_entry);
      entry->data = venus_device;	
      entry->proc_fops = &proc_status_ops;
#else  
      ssid_entry.data = venus_device;
      status_entry.data = venus_device;
      proc_register( &proc_root,&venus_entry);
      proc_register(&venus_entry,&ssid_entry);
      proc_register(&venus_entry,&status_entry);
#endif
      return 0;
    }
  }
  printk(KERN_ERR "MPI350 card not found\n");
  return -ENODEV;    
}




/*
 * @cleanup_mod
 */
void cleanup_module( void ){
  
//  printk(KERN_INFO "MPI350 Cleanup mod \n");

  
  if(venus_device){
    takedown_proc_entry(venus_device,venus_device->priv);
    stop_venus_card(venus_device);
  }
}



/* 
 * Proc filesystem processing 
 */

static int takedown_proc_entry( struct net_device *dev,struct venus_info *apriv ) {
  struct venus_info *v_info ;

  v_info = (struct venus_info *)venus_device->priv;

#if (LINUX_VERSION_CODE > 0x20355)
  remove_proc_entry("SSID",v_info->proc_entry);
  remove_proc_entry("Status",v_info->proc_entry);
  remove_proc_entry(PRODNAME, proc_root_driver);
#else
  proc_unregister(&venus_entry,ssid_entry.low_ino);
  proc_unregister(&venus_entry,status_entry.low_ino);
  proc_unregister(&proc_root,venus_entry.low_ino);
#endif
  return 0;
}


static ssize_t proc_read( struct file *file,char *buffer,size_t len,loff_t *offset )
{
	int i;
	int pos;
	struct proc_data *priv = (struct proc_data*)file->private_data;
	

	if( !priv->rbuffer ) return -EINVAL;
	
	pos = *offset;

	for( i = 0; i+pos < priv->readlen && i < len; i++ ) {
		put_user( priv->rbuffer[i+pos], buffer+i );
	}
	*offset += i;
	return i;
}

/*
 *  The write routine is generic, it fills in a preallocated rbuffer
 *  to supply the data.
 */

static ssize_t proc_write( struct file *file,const char *buffer,size_t len,loff_t *offset ) 
{
	int i;
	int pos;
	struct proc_data *priv = (struct proc_data*)file->private_data;
	

	if ( !priv->wbuffer ) {
		return -EINVAL;
	}	
	pos = *offset;
	
	for( i = 0; i + pos <  priv->maxwritelen &&
		     i < len; i++ ) {
		get_user( priv->wbuffer[i+pos], buffer + i );
	}
	if ( i+pos > priv->writelen ) priv->writelen = i+file->f_pos;

	*offset += i;
	return i;
}

static int proc_close( struct inode *inode, struct file *file ) 
{
	struct proc_data *data = (struct proc_data *)file->private_data;


	if ( data->on_close != NULL ) data->on_close( inode, file );
	MOD_DEC_USE_COUNT;
	if ( data->rbuffer ) kfree( data->rbuffer );
	if ( data->wbuffer ) kfree( data->wbuffer );
	kfree( data );
	return 0;
}

static void proc_ssid_on_close( struct inode *inode, struct file *file ) {
	struct proc_data *data = (struct proc_data *)file->private_data;
	struct proc_dir_entry *dp = inode->u.generic_ip;
	struct net_device *dev = dp->data;
	struct venus_info *vi = (struct venus_info*)dev->priv;
	PC3500_SID_LIST *SSID_rid;
	char ssidp[200];	
	int i;
	int offset = 0;
	
	SSID_rid = (PC3500_SID_LIST *)ssidp;

	if ( !data->writelen ) return;
	
	memset( SSID_rid, 0, sizeof( SSID_rid ) );
	
	for( i = 0; i < 3; i++ ) {
		int j;
		for( j = 0; j+offset < data->writelen && j < 32 &&
			     data->wbuffer[offset+j] != '\n'; j++ ) {
			SSID_rid->aSsid[i].Ssid[j] = data->wbuffer[offset+j];
		}
		if ( j == 0 ) break;
		SSID_rid->aSsid[i].SsidLen = cpu_to_le16(j);
		offset += j;
		while( data->wbuffer[offset] != '\n' && 
		       offset < data->writelen ) offset++;
		offset++;
	}
	SSID_rid->u16RidLen = sizeof(PC3500_SID_LIST);
	vwriterid(vi, RID_SSID, (unsigned char *)SSID_rid);
}




static int proc_ssid_open( struct inode *inode, struct file *file ) {
	struct proc_data *data;
	struct proc_dir_entry *dp = inode->u.generic_ip;
	struct net_device *dev = dp->data;
	struct venus_info *vi = (struct venus_info*)dev->priv;
	int i;
	char *ptr,blargbuf[1023];

	PC3500_SID_LIST *SSID_rid;	
	SSID_rid = (PC3500_SID_LIST *)blargbuf;


	MOD_INC_USE_COUNT;
	
	dp = (struct proc_dir_entry *) inode->u.generic_ip;
	
	file->private_data = kmalloc(sizeof(struct proc_data ), GFP_KERNEL);
	memset(file->private_data, 0, sizeof(struct proc_data));
	data = (struct proc_data *)file->private_data;
	data->rbuffer = kmalloc( 104, GFP_KERNEL );
	data->writelen = 0;
	data->maxwritelen = 33*3;
	data->wbuffer = kmalloc( 33*3, GFP_KERNEL );
	memset( data->wbuffer, 0, 33*3 );
	data->on_close = proc_ssid_on_close;
	
	vreadrid(vi,RID_SSLIST,(unsigned char *)SSID_rid, 1023);

	ptr = data->rbuffer;

	for( i = 0; i < 3; i++ ) {
		int j;
		if ( !SSID_rid->aSsid[i].SsidLen ) break;
		for( j = 0; j < 32 && j < le16_to_cpu(SSID_rid->aSsid[i].SsidLen) 
		       && SSID_rid->aSsid[i].Ssid[j]; j++ ) {
			*ptr++ = SSID_rid->aSsid[i].Ssid[j]; 
		}
		*ptr++ = '\n';
	}
	*ptr = '\0';

	data->readlen = strlen( data->rbuffer );
	return 0;
}

static int proc_status_open( struct inode *inode, struct file *file ) {
	struct  proc_data *data;
	struct  proc_dir_entry *dp = inode->u.generic_ip;
	struct  net_device *dev = dp->data;
	struct  venus_info *vi = (struct venus_info *)dev->priv;
	STCAPS  cap_rid;
	StatRid status_rid;
	
	MOD_INC_USE_COUNT;
	
	dp = (struct proc_dir_entry *) inode->u.generic_ip;
	
	file->private_data = kmalloc(sizeof(struct proc_data ), GFP_KERNEL);
	memset(file->private_data, 0, sizeof(struct proc_data));
	data = (struct proc_data *)file->private_data;
	data->rbuffer = kmalloc( 2048, GFP_KERNEL );
	
	vreadrid(vi,RID_STATUS,(unsigned char *)&status_rid, sizeof(StatRid));
	vreadrid(vi,RID_CAPABILITIES,(unsigned char *)&cap_rid, sizeof(STCAPS));

	sprintf( data->rbuffer, "Mode: %x\n"
		 "Signal Strength: %d\n"
		 "Signal Quality: %d\n"
		 "SSID: %-.*s\n"
		 "AP: %-.16s\n"
		 "Freq: %d\n"
		 "BitRate: %dmbs\n"
		 "Driver Version: %s\n"
		 "Device: %s\nManufacturer: %s\nFirmware Version: %s\n"
		 "Radio type: %x\nCountry: %x\nHardware Version: %x\n"
		 "Software Version: %x\nSoftware Subversion: %x\n"
		 "Boot block version: %x\n",
		 (int)le16_to_cpu(status_rid.mode),
		 (int)le16_to_cpu(status_rid.normalizedSignalStrength),
		 (int)le16_to_cpu(status_rid.signalQuality),
		 (int)status_rid.SSIDlen,
		 status_rid.SSID,
		 status_rid.apName,
		 (int)le16_to_cpu(status_rid.channel),
		 (int)le16_to_cpu(status_rid.currentXmitRate)/2,
		 version,
		 cap_rid.au8ProductName,
		 cap_rid.au8ManufacturerName,
		 cap_rid.au8ProductVersion,
		 le16_to_cpu(cap_rid.u16RadioType),
		 le16_to_cpu(cap_rid.u16RegDomain),
		 le16_to_cpu(cap_rid.u16HardwareVersion),
		 (int)le16_to_cpu(cap_rid.u16SoftwareVersion),
		 (int)le16_to_cpu(cap_rid.u16SoftwareSubVersion),
		 (int)le16_to_cpu(cap_rid.u16BootBlockVersion) );
	data->readlen = strlen( data->rbuffer );
	return 0;
}


/*
 *@remove
 */
void venus_remove(void){
//  printk(KERN_INFO "remove mod called\n");
  if(venus_device)
    stop_venus_card(venus_device);
}



/*
 * @vreadrid
 * Return 0 on sucess or other on failure . Uses the len value of all rids 
 * in the first 2 bytes .
 */
static int vreadrid(struct venus_info *v_info,unsigned short rid,unsigned char *ridp, int length){
  Cmd            cmd;
  Resp           rsp;
  int            status;
  unsigned short *ridlen;

  memset(&rsp,0,sizeof(rsp));
  memset(&cmd,0,sizeof(cmd));
  v_info->ConfigDesc.RIDDesc.valid   = 1;
  v_info->ConfigDesc.RIDDesc.length  = 1840; 
  v_info->ConfigDesc.RIDDesc.PhyHostAddress = v_info->ridbus;

  cmd.cmd   = CMD_X500_AccessRIDRead;
  cmd.parm0 = rid;

  memcpy((char *)v_info->ConfigDesc.CardRamOff,
	 (char *)&v_info->ConfigDesc.RIDDesc,sizeof(CARD_RID_DESC));

  status = venuscommand(v_info,&cmd,&rsp);

  /* If status error return qualifier
   */
  if((rsp.status & 0x7f00) !=0 )
    return rsp.rsp0;  /* return error qualifier */

  if(!status){
    ridlen = VADDR(unsigned short,v_info->ConfigDesc);
    memcpy((char *)ridp,(char *)v_info->ConfigDesc.VirtualHostAddress,MIN(*ridlen, length));
  }
  return status;
}

/*
 * @writerid
 *
 */
static int vwriterid(struct venus_info *v_info,unsigned short rid,unsigned char *ridp){
  Cmd            cmd;
  Resp           rsp;
  ConfigRid      *cfg;
  int            status;
  int            irqstate;
  u16 *ridlen;

  memset(&rsp,0,sizeof(rsp));
  memset(&cmd,0,sizeof(cmd));

  irqstate = venusin(v_info,V_EVINTEN);
  venusout(v_info,V_EVINTEN,0);

  mac_disable(v_info );

  /* 
   * Check for addhoc change in cfg. MIC needs 
   * to know about it.
   */  

  if(rid == RID_CONFIG){
    cfg = (ConfigRid *)ridp;
    if((cfg->opmode & 1)==0)
      v_info->flags |= ADHOC;
    else
      v_info->flags &= ~(ADHOC);
  }

  v_info->ConfigDesc.RIDDesc.valid   = 1;
  v_info->ConfigDesc.RIDDesc.length  = 2048; 
  
  cmd.cmd   = CMD_X500_AccessRIDWrite;
  cmd.parm0 = rid;

  ridlen = (u16 *)ridp;
  v_info->ConfigDesc.RIDDesc.length  = (*ridlen); 

  memcpy((char *)v_info->ConfigDesc.CardRamOff,
	 (char *)&v_info->ConfigDesc.RIDDesc,sizeof(CARD_RID_DESC));

//  printk(KERN_ERR "vwrite rid %x len %x\n",rid, *ridlen);
  if(*ridlen < 4 || *ridlen > 2047 ){
    printk(KERN_ERR "RIDLEN in vwrite = %x rid %04x\n",*ridlen, rid);
    return -1;
  }
  memcpy((char *)v_info->ConfigDesc.VirtualHostAddress,(char *)ridp,*ridlen);

  if(((status = venuscommand(v_info,&cmd,&rsp)) & 0xff00) !=0){
    printk(KERN_ERR "Write rid Error %d\n",status);
    printk(KERN_ERR "Cmd = %x\n",cmd.cmd);
  }

  mac_enable(v_info,&rsp);
  venusout(v_info,V_EVINTEN,irqstate);

  /* If status error return qualifier
   */
  if((rsp.status & 0x7f00)!=0)
    return rsp.rsp0;  

  return status;
}



/*
 * @stop
 * Stop MPI350 
 */
int stop_venus_card( struct net_device *vdev){
	struct venus_info *v_info = (struct venus_info*)vdev->priv;
	struct sk_buff    *skb=0;

	/* device has been freed
	 * already bail
	 */
//  printk(KERN_ERR "MPI350 stop_venus venus_device %p! \n", venus_device);
        pci_unregister_driver(&venus_pci_driver);
	venus_device = 0;
	if(vdev == 0)
	  return -1;

	/* Turn off mac (disable card ) before
	 * freeing stuff or (boom)
	 */
	
	mac_disable(v_info);
	venusout(v_info,V_EVINTEN,0 );
#if (LINUX_VERSION_CODE < 0x20355 )
	del_timer(&v_info->timer);
#endif
	/* Clean out tx queue 
	 */

	if(skb_queue_len(&v_info->txq)> 0 )
	  for(;(skb = skb_dequeue(&v_info->txq));)
	    dev_kfree_skb(skb);

	free_irq( vdev->irq, vdev );
	release_mem_region(v_info->pci_auxbase,v_info->auxmembasesize);
       	release_mem_region(v_info->pci_controlbase,v_info->controlmembasesize);

	pci_free_consistent(v_info->pcip, SHAREDMEMSIZE,
			    v_info->SharedRegion,v_info->SharedBusaddr);
       	release_region(vdev->base_addr,0x100);

	iounmap(v_info->auxregmembase);
	iounmap(v_info->controlregmembase);

       	unregister_netdev(vdev);

	kfree(vdev);
	vdev = 0;
	return 0;
}


/* In/out  word 
 * @venusout
 */
static unsigned short venusin(struct venus_info *vp,u16 reg){
  unsigned short ret;
  ret = inw(vp->iobase+reg);
  return ret;
}

static void venusout(struct venus_info *vp,u16 reg,u16 val){
  outw(val,vp->iobase+reg);
}

/* @command 
 */

static u16 venuscommand(struct venus_info *ai, Cmd *pCmd, Resp *pRsp) {
  int flags;
  int max_tries = 600000;  
  int rc = SUCCESS;
  int i,ik=0;

  spin_lock_irqsave(&ai->cmd_lock, flags);

  // wait for no busy 

  for(i=0;i!=600000;i++){
    if (venusin(ai, V_COMMAND) & COMMAND_BUSY) {
      mdelay(10);
    }
    else
      break;
  }

  if(i==600000){
#ifdef DEBUG_CMD
    printk(KERN_ERR "Was busy too long \n");
#endif
    rc = ERROR;
    goto trynclear;
  }

  venusout(ai, V_PARAM0, pCmd->parm0);
  venusout(ai, V_PARAM1, pCmd->parm1);
  venusout(ai, V_PARAM2, pCmd->parm2);
  venusout(ai, V_COMMAND, pCmd->cmd);

  /* 
   * Check for reset command . Takes a while and does 
   * not return EV_CMD status. Caller must delay ~1s .
   * after calling.
   */
  if(pCmd->cmd == CMD_X500_ResetCard )
    goto done;
  
  while ( max_tries-- && (venusin(ai, V_EVSTAT) & EV_CMD)== 0) {
    if ( (ik = venusin(ai, V_COMMAND)) == pCmd->cmd) { 
      // didn't notice command, try again
      venusout(ai, V_COMMAND, pCmd->cmd);
    }
  }
  
 trynclear:
  if ( max_tries == -1 ) {
    printk( KERN_ERR "MPI350: Max tries exceeded when issueing command\n" );
    rc = ERROR;
#ifdef DEBUG_CMD
    printk(KERN_ERR "Hung command reg = %x\n",venusin(ai,V_COMMAND));
#endif
    venus_kick(ai);
    if ( !venus_clear(ai)){
#ifdef DEBUG_CMD
      printk(KERN_ERR "Command register cleared!\n");
#endif
      goto done;
    }
    else{
      printk(KERN_ERR "Could not clear command register\n");
      goto done;
    }
  }
  // command completed
  pRsp->status = venusin(ai, V_STATUS);
  pRsp->rsp0 = venusin(ai, V_RESP0);
  pRsp->rsp1 = venusin(ai, V_RESP1);
  pRsp->rsp2 = venusin(ai, V_RESP2);
  
  // clear stuck command busy if necessary
  if (venusin(ai, V_COMMAND) & COMMAND_BUSY) {
    venusout(ai, V_EVACK, EV_CLEARCOMMANDBUSY);
  }
  // acknowledge processing the status/response
  venusout(ai, V_EVACK, EV_CMD);

 done:
  if((pRsp->status & 0xff00)!=0 && pCmd->cmd != CMD_X500_ResetCard ){
    printk(KERN_ERR "venuscommand cmd = %x\n",pCmd->cmd);
    printk(KERN_ERR "venuscommand status = %x\n",pRsp->status);
    printk(KERN_ERR "venuscommand Rsp0 = %x\n",pRsp->rsp0);
    printk(KERN_ERR "venuscommand Rsp1 = %x\n",pRsp->rsp1);
    printk(KERN_ERR "venuscommand Rsp2 = %x\n",pRsp->rsp2);
  }
  spin_unlock_irqrestore(&ai->cmd_lock, flags);
  return rc;
}

static void  venus_kick(struct venus_info *v_info){
#ifdef DEBUG_CMD
  printk(KERN_ERR "Venus kick!!\n");
#endif
  venusout(v_info,V_COMMAND,0x10);
  mdelay(20);
}

/*
 * Try to clear stuck command busy bit return 
 * 0 if cleared NZ if not
 */

static int venus_clear(struct venus_info *v_info){
  u32 ccmd,maxtries = 10000;

  if (venusin(v_info, V_COMMAND) & COMMAND_BUSY) {
    venusout(v_info, V_EVACK, EV_CMD);
  }
  else
    {
#ifdef DEBUG_CMD
      printk(KERN_ERR "Busy not set in venus clear !!\n");
#endif
      return 0; /* Busybit not set */
    }
  while ( maxtries-- &&	((venusin(v_info, V_EVSTAT)) & EV_CMD)== 0) {
    mdelay(10);
    if ( (ccmd = venusin(v_info, V_COMMAND)) & COMMAND_BUSY ) { 
      venusout(v_info, V_EVACK, EV_CMD);
    }
  }
  if(maxtries > 0 ){
#if DEBUG_CMD
    printk(KERN_ERR "Acknowledge command in clear!!\n");
#endif
    venusout(v_info, V_EVACK, EV_CMD);
    return 0;
  }
  else
    printk(KERN_ERR "Could not clear command busy !\n");

  return 1;
}



/*******************************************************************
 * @init_venus_card the descriptors etc start ball rolling.        *
 *******************************************************************
 */


struct net_device *init_venus_card(struct pci_dev *vencard)
{
  TXFID_HDR txFidHdr = {{0, 0, 0, 0, 0x20, 0, 0, 0}};
  struct net_device *vdev;
  int      i,status;
  struct   venus_info *v_info;
  unsigned char *busaddroff,*vpackoff;
  unsigned char *pciaddroff;
  Cmd  cmd;
  Resp rsp;

//  printk( KERN_INFO "MPI350 : init_venus_card\n");
  vdev = 0;
  vdev = init_etherdev(0,sizeof(struct venus_info));

  if(!vdev){
	printk( KERN_INFO "MPI350_DEV: etherdev alloc FAILED\n");
	return 0;
  }

#if (LINUX_VERSION_CODE > 0x20355)
  SET_MODULE_OWNER(vdev);
#endif

  vdev->do_ioctl = &venus_ioctl;
  vdev->open =     &venus_open;
  vdev->stop =     &venus_close;
  vdev->hard_start_xmit = &venus_transmit;
  vdev->get_stats       = &venus_get_stats;
  vdev->change_mtu = &venus_change_mtu;
  vdev->set_multicast_list = &venus_set_multicast_list;
#if (LINUX_VERSION_CODE > 0x20355 )
  vdev->watchdog_timeo = 200;
  vdev->tx_timeout = venustxtmo;
#endif
#ifdef WIRELESS_EXT
  vdev->get_wireless_stats = airo_get_wireless_stats;
#endif 
  netif_stop_queue(vdev);

  v_info = (struct venus_info *)vdev->priv;  
  memset((char *)v_info,0,sizeof(v_info));
  memcpy((char *)&v_info->txFidHdr,(char *)&txFidHdr,sizeof(TXFID_HDR));

  skb_queue_head_init(&v_info->txq);

  if (pci_enable_device(vencard)){
    printk(KERN_INFO "MPI350 Can't init card\n");
    return 0;
  }

  vdev->irq       = vencard->irq;
  vdev->base_addr = pci_resource_start(vencard,0);
  v_info->iosize  = pci_resource_len(vencard, 0);
  v_info->iobase  = vdev->base_addr;
  v_info->pcip    = vencard;
  v_info->dev     = vdev;

  /* 
   * Set locks 
   */
  v_info->mpi_lock = SPIN_LOCK_UNLOCKED;
  v_info->txlist_lock = SPIN_LOCK_UNLOCKED;
  v_info->txd_lock    = SPIN_LOCK_UNLOCKED;
  v_info->aux_lock = SPIN_LOCK_UNLOCKED;
  v_info->cmd_lock = SPIN_LOCK_UNLOCKED;

  request_region(vdev->base_addr,v_info->iosize,"MPI350-CARD");

  /* @reset
   * Turn off interrupts and reset Card
   */
  venusout(v_info,V_EVINTEN,0x0);
  memset(&rsp,0,sizeof(rsp));
  memset(&cmd,0,sizeof(cmd));  

  cmd.cmd = CMD_X500_ResetCard;
  cmd.parm0 = cmd.parm1 = cmd.parm2 = 0;
  venuscommand(v_info,&cmd,&rsp);

  mdelay(2048);

  if((status = request_irq(vdev->irq, venus_interrupt,SA_SHIRQ|SA_INTERRUPT,
			   vdev->name,vdev)))
    {
      printk(KERN_ERR "MPI350 ---- request irq %d failed status %d!!!!\n",
	     vdev->irq,status);
 
    }
  
  v_info->pci_controlbase     = pci_resource_start(vencard,1);
  v_info->controlmembasesize  = pci_resource_len(vencard,1);
  v_info->pci_auxbase    = pci_resource_start(vencard,2);

  /* Aux mem must be 256 * 1024 
   * for flash to work
   */
  v_info->auxmembasesize = AUXMEMSIZE;

  /* Shared mem */
  if((v_info->SharedRegion = pci_alloc_consistent(vencard, SHAREDMEMSIZE, 
						  &v_info->SharedBusaddr))==0){
      printk(KERN_INFO "MPI350 Consistant alloc failed !\n");
      kfree(vdev);
      return 0;
  }
  memset((void *)v_info->SharedRegion,0,SHAREDMEMSIZE); 

  if (!request_mem_region(v_info->pci_auxbase,
			  v_info->auxmembasesize, "MPI350DEV")){
    printk(KERN_INFO "MPI350 Failed to get AUX region %lx\n",v_info->pci_auxbase);
    kfree(vdev);
    vdev = 0;
    return 0;
  }

  if (!request_mem_region(v_info->pci_controlbase,
			  v_info->controlmembasesize, "MPI350DEV")){
    printk(KERN_INFO "MPI350 Failed to get CONTROL region %lx\n",v_info->pci_controlbase);
    pci_free_consistent(vencard, SHAREDMEMSIZE,v_info->SharedRegion,v_info->SharedBusaddr);
    kfree(vdev);
    vdev = 0;
    return 0;
  }

  if((v_info->auxregmembase = ioremap (v_info->pci_auxbase, 
				       v_info->auxmembasesize))==0){
    printk(KERN_INFO "MPI350_DEV ioremap AUX: FAILED ! %lx = %p\n",
	   v_info->pci_auxbase,
	   v_info->auxregmembase);
    pci_free_consistent(vencard,SHAREDMEMSIZE ,
			v_info->SharedRegion,v_info->SharedBusaddr);
    kfree(vdev);
    vdev=0;
    return 0;
  }

  /* 
   * Memory mapped control registers.
   */
  if((v_info->controlregmembase = ioremap (v_info->pci_controlbase, 
					   v_info->controlmembasesize))==0){

      printk(KERN_INFO "MPI350_DEV ioremap CONTROL: FAILED ! %lx = %x\n",
			 v_info->pci_controlbase,
			 v_info->controlmembasesize);
	  pci_free_consistent(vencard, SHAREDMEMSIZE,
			      v_info->SharedRegion,v_info->SharedBusaddr);
	  kfree(vdev);
	  vdev = 0;
	  iounmap(v_info->auxregmembase);
      return 0;
  }

  /*
   * Setup descriptors RX,TX,CONFIG
   */
  busaddroff  = (unsigned char *)v_info->SharedBusaddr;
  pciaddroff   = v_info->auxregmembase + 0x800;
  vpackoff     = v_info->SharedRegion;
  
  /* RX descriptor setup */

  for(i=0;i!=MAX_DESC;i++){
    v_info->rxfids[i].pending = FALSE;
    v_info->rxfids[i].CardRamOff = pciaddroff; 
    v_info->rxfids[i].RxDesc.PhyHostAddress =(unsigned long) busaddroff;
    v_info->rxfids[i].RxDesc.valid  = 1;
    v_info->rxfids[i].RxDesc.length = HOSTBUFSIZ;
    v_info->rxfids[i].RxDesc.RxDone = 0;
    v_info->rxfids[i].VirtualHostAddress    = vpackoff;

    pciaddroff  += sizeof(CARD_RX_DESC);
    busaddroff  += HOSTBUFSIZ;
    vpackoff    += HOSTBUFSIZ;
  }
  /* TX descriptor setup */

  for(i=0;i!=MAX_DESC;i++){
    v_info->txfids[i].CardRamOff = pciaddroff;
    v_info->txfids[i].TxDesc.PhyHostAddress = (unsigned long)busaddroff;
    v_info->txfids[i].TxDesc.valid = 1;
    v_info->txfids[i].VirtualHostAddress     = vpackoff;

    memcpy((char *)v_info->txfids[i].VirtualHostAddress,
	   (char *)&v_info->txFidHdr,sizeof(txFidHdr));

    pciaddroff  += sizeof(CARD_TX_DESC);
    busaddroff  +=HOSTBUFSIZ;
    vpackoff    +=HOSTBUFSIZ;
  }
  v_info->txfids[i-1].TxDesc.eoc   = 1; /* Last descriptor has EOC set */

  /* Rid descriptor setup */

  v_info->ConfigDesc.CardRamOff  =  pciaddroff;
  v_info->ConfigDesc.VirtualHostAddress = vpackoff;
  v_info->ConfigDesc.RIDDesc.PhyHostAddress = (unsigned long)busaddroff;
  v_info->ridbus = (unsigned long)busaddroff;
  v_info->ConfigDesc.RIDDesc.RID     = 0;
  v_info->ConfigDesc.RIDDesc.length  = 2048;
  v_info->ConfigDesc.RIDDesc.valid   = 1;

  /* Tell card about descriptors */

  venus_init_descriptors(vdev);
  pci_set_master (vencard);

  save_net_dev = vdev;
  netdrv_pci_tbl[0].driver_data = (unsigned long) vdev;
  pci_module_init (&venus_pci_driver);
  return vdev;
}

/* @probe 
 */
static int  venus_probe(struct pci_dev *pci_p){
  struct venus_info *v_info;
  char *nowhine = version;
  int  status=0;

  nowhine = 0;

  if((venus_device=init_venus_card(pci_p))==0){
	printk(KERN_INFO "MPI350 init_venus_card FAILED\n");
	return ERROR;
  }

  /*
   * Print out.
   */
  if(venus_device){
	v_info = (struct venus_info *)venus_device->priv;

	if((status = start_venus(venus_device))!=0){
	  printk(KERN_ERR "Cannot init venus card! \n" );
	  return -1;
	}
	return 0;
  }
  return ERROR;
}


/*************************************************************
 * Start card after flash or power cycle.
 * @start_venus
 */
static int start_venus(struct net_device *dev){
  struct venus_info *v_info = (struct venus_info*)dev->priv;
  Cmd  cmd;
  Resp rsp;
#if (LINUX_VERSION_CODE < 0x20355 )
  struct timer_list *timer;
#endif
  int  i,status;
  ConfigRid *cfg;
  EXSTCAPS  xcaps;

  /* @restart
   */
//  printk(KERN_ERR "MPI350 start_venus! \n" );

#ifdef BAD_CARD
  if (!bad_card) {
#endif
  venusout(v_info,V_EVINTEN,0x0);
  memset(&rsp,0,sizeof(rsp));
  memset(&cmd,0,sizeof(cmd));
  cmd.cmd = CMD_X500_NOP10;	

  if((status = venuscommand(v_info,&cmd,&rsp))!=0)
    return status;

  pci_set_master (v_info->pcip); /* Gets called twice no harm */

  cfg = (ConfigRid *)iobuf;
  vreadrid((struct venus_info *)dev->priv,RID_CAPABILITIES,(char *)&xcaps, sizeof(EXSTCAPS));
  vreadrid((struct venus_info *)dev->priv,RID_CONFIG,iobuf, sizeof(ConfigRid));

  /*
   * Find out if an extended capability rid was returned
   */
  if(xcaps.u16RidLen >= sizeof(xcaps)){
#ifdef DEBUG_MIC
    printk(KERN_INFO "Extended cap's\n");
#endif
    /* Check for MIC capability */
    if(xcaps.u16ExtSoftwareCapabilities & EXT_SOFT_CAPS_MIC ){
#ifdef DEBUG_MIC
      printk(KERN_INFO "Mic capable\n");
#endif
      cfg->opmode   |= CFG_OPMODE_MIC;
      v_info->flags |= MIC_CAPABLE;
      micsetup(v_info);
    }
  }
#ifdef BAD_CARD
  }
#endif

  /* 
   * Set dev macaddr 
   */
  for(i=0;i!=6;i++){
    dev->dev_addr[i] = cfg->macAddr[i];
  }
  printk( KERN_INFO "MPI350 start: MAC  %x:%x:%x:%x:%x:%x\n",
	  dev->dev_addr[0],dev->dev_addr[1],dev->dev_addr[2],
	  dev->dev_addr[3],dev->dev_addr[4],dev->dev_addr[5]); 

#ifdef BAD_CARD
  if (!bad_card) {
#endif
  if((status = mac_enable(v_info,&rsp))!=0)
    return status;
#ifdef BAD_CARD
  }
#endif

#if (LINUX_VERSION_CODE < 0x20355 )
  timer = &v_info->timer;
  init_timer(timer);
  timer->function = venus_timeout;
  timer->data     = (u_long)dev;
  timer->expires  = RUN_AT(HZ * TXTIME);
  add_timer(timer);
#endif

#ifdef BAD_CARD
  if (!bad_card) {
#endif
  /* Write config if MIC needs to be on */
  if(v_info->flags & MIC_CAPABLE)
    vwriterid(v_info, RID_CONFIG, (unsigned char *)cfg);  
#ifdef BAD_CARD
  }
#endif

  /*  venusout(v_info,V_EVINTEN,STATUS_INTS ); @RAW */
  netif_start_queue(dev);
  return status;
}



/*************************************************************
 *  This routine assumes that descriptors have been setup .
 *  Run at insmod time or after reset  when the decriptors 
 *  have been initialized . Returns 0 if all is well nz 
 *  otherwise . Does not allocate memory but sets up card
 *  using previously allocated descriptors.
 */

static int venus_init_descriptors(struct net_device *dev){
  struct venus_info *v_info = (struct venus_info *)dev->priv;
  Cmd  cmd;
  Resp rsp;
  int  i;

  /* Alloc  card RX descriptors */

  netif_stop_queue(dev);
#ifdef BAD_CARD
  if (bad_card) return(0);
#endif

  v_info->rxdfc = MAX_DESC;
  v_info->txdfc = MAX_DESC;

  memset(&rsp,0,sizeof(rsp));
  memset(&cmd,0,sizeof(cmd));
  
  cmd.cmd   = CMD_X500_AllocDescriptor;
  cmd.parm0 = DESCRIPTOR_RX;
  cmd.parm1 = (v_info->rxfids[0].CardRamOff  -  v_info->auxregmembase);
  cmd.parm2 = MAX_DESC;

  if( (i = venuscommand(v_info,&cmd,&rsp))!=0){
    printk(KERN_INFO "init_descriptors returns %d DESCRIPTOR_RX on init \n",i);
    return -1;
  }

  for(i=0;i!=MAX_DESC;i++){
//    v_info->rxfids[i].RxDesc.valid  = 1;
//    v_info->rxfids[i].RxDesc.length = HOSTBUFSIZ;
//    v_info->rxfids[i].RxDesc.RxDone = 0;
    memcpy((char *)v_info->rxfids[i].CardRamOff ,
	   (char *)&v_info->rxfids[i].RxDesc,sizeof(CARD_RX_DESC));
  }

  /* Alloc  card TX descriptors */

  memset(&rsp,0,sizeof(rsp));
  memset(&cmd,0,sizeof(cmd));
  
  cmd.cmd   = CMD_X500_AllocDescriptor;
  cmd.parm0 = DESCRIPTOR_TX;
  cmd.parm1 = (v_info->txfids[0].CardRamOff  -  v_info->auxregmembase);
  cmd.parm2 = MAX_DESC;

  for(i=0;i!=MAX_DESC;i++){
    v_info->txfids[i].TxDesc.valid  = 1;
    memcpy((char *)v_info->txfids[i].CardRamOff ,
	   (char *)&v_info->txfids[i].TxDesc,sizeof(CARD_TX_DESC));
  }
  v_info->txfids[i-1].TxDesc.eoc   = 1; /* Last descriptor has EOC set */

  if( (i = venuscommand(v_info,&cmd,&rsp))!=0){
	printk(KERN_INFO "init_descriptors returns %d DESCRIPTOR_TX on init \n",i);
	return -1;
  }

  /* Rid descriptor setup */

  memcpy((char *)v_info->ConfigDesc.CardRamOff,(char *)&v_info->ConfigDesc.RIDDesc,
	 sizeof(CARD_RID_DESC));
  memset(&rsp,0,sizeof(rsp));
  memset(&cmd,0,sizeof(cmd));
  
  cmd.cmd   = CMD_X500_AllocDescriptor;
  cmd.parm0 = DESCRIPTOR_HOSTRW;
  cmd.parm1 = (v_info->ConfigDesc.CardRamOff -  v_info->auxregmembase);
  cmd.parm2 = 1;

  if( (i = venuscommand(v_info,&cmd,&rsp))!=0){
    printk(KERN_INFO "init_descriptors returns %d on init HOSTRW\n",i);
    return -1;
  }
  return 0;
}


/*
 * *********************************IOCTL ACU API *****************************************
 */

/* @mac_ */
static int mac_enable(struct venus_info *vi, Resp *rsp ){
  Cmd cmd;
  int i;
  memset(&cmd,0,sizeof(cmd));

  cmd.cmd =  CMD_X500_EnableMAC; 

  if(vi->macstatus == 0 ){
    if((i= venuscommand(vi,&cmd,rsp)))
      printk(KERN_ERR "Cannot enable mac err %d\n",i);
    vi->macstatus = 1;
  }
  else i = 0;

  return( i); 
}


static void mac_disable( struct venus_info *vi ) {
  Cmd cmd;
  Resp rsp;
  
  memset(&cmd, 0, sizeof(cmd));
  memset(&rsp, 0, sizeof(rsp));

  if(vi->macstatus == 1 ){
    cmd.cmd = CMD_X500_DisableMAC ; 
    venuscommand(vi, &cmd, &rsp);
    vi->macstatus = 0;
  }	
}


static void enable_interrupts(struct venus_info *v_info){
//  printk(KERN_ERR "MPI350 : enable_interrupts\n");
  venusout(v_info,V_EVINTEN,STATUS_INTS);
}


static void disable_interrupts(struct venus_info *v_info){
//  printk(KERN_ERR "MPI350 : disable_interrupts\n");
  venusout(v_info,V_EVINTEN,0);
}


/*
 * This just translates from driver IOCTL codes to the command codes to 
 * feed to the radio's host interface. Things can be added/deleted 
 * as needed.  This represents the READ side of control I/O to 
 * the card
 */
static int readrids(struct net_device *dev, aironet_ioctl *comp) {
  unsigned short ridcode;
  struct venus_info *v_info ;
  DRVRTYPE dt;
  
  v_info = (struct venus_info *)dev->priv;
  
  if(v_info->flags & FLASHING ) /* Is busy */
    return -EIO;
  
  switch(comp->command)
    {
    case AIROGCAP:      ridcode = RID_CAPABILITIES; break;
    case AIROGCFG:      ridcode = RID_CONFIG;       break;
    case AIROGSLIST:    ridcode = RID_SSID;         break;
    case AIROGVLIST:    ridcode = RID_APLIST;       break;
    case AIROGDRVNAM:   ridcode = RID_DRVNAME;      break;
    case AIROGEHTENC:   ridcode = RID_ETHERENCAP;   break;
    case AIROGWEPKTMP:  ridcode = RID_WEP_TEMP;
		break;

    case AIROGWEPKNV:   ridcode = RID_WEP_PERM;	break;
    case AIROGSTAT:     ridcode = RID_STATUS;       break;
    case AIROGSTATSD32: ridcode = RID_STATSDELTA;   break;
    case AIROGSTATSC32: ridcode = RID_STATS;        break;
    case AIROGMICRID:   ridcode = RID_MIC;          break;
    case AIROGMICSTATS:
      if(copy_to_user(comp->data,(char *)&v_info->micstats,MIN(comp->length,sizeof(STMICSTATISTICS32)))){
	return -EFAULT; 
      }
      else
	return 0;
      break;
    case AIROGID:
      /* DRIVER VERSION */

      memset((char *)&dt,0,sizeof(dt));

      /* @Driver version */
      dt.version[0]=1;            /* Major Minor version */
      dt.version[1]=0;            /* Minor */
      dt.version[2]=15;            /* vers  */ 
      dt.flashcode = 0xd0;        /* venus image firmware type */
      dt.devtype = VENUS_TYPE;    /* What this driver talks to */

      if(copy_to_user(comp->data,(char *)&dt,MIN(comp->length,sizeof(DRVRTYPE)))){
	return -EFAULT; 
      }
      else
	return 0;
      break;
    case AIRORRID:
      ridcode = comp->ridnum;
      break;
    default:
      return -EINVAL;  
      break;
    }

  vreadrid((struct venus_info *)dev->priv,ridcode,iobuf, comp->length);
  
  if (copy_to_user(comp->data, iobuf, MIN (comp->length, sizeof(iobuf))))
    return -EFAULT;
  return 0;
}

/*
 * Danger Vorlon death command here.
 */

static int writerids(struct net_device *dev, aironet_ioctl *comp) {
  int       ridcode,stat;
  Resp      rsp;
  unsigned char blargo[2048];
  ConfigRid *cfg;
  struct venus_info *v_info ;


  v_info = (struct venus_info *)dev->priv;
  
  if(v_info->flags & FLASHING )
    return -EIO;	/* Cant touch this. */
  
  memset(blargo,0,sizeof(blargo));
  ridcode = 0;
	
  switch(comp->command)
    {
    case AIROPSIDS:     ridcode = RID_SSID;         break;
    case AIROPCAP:      ridcode = RID_CAPABILITIES; break;
    case AIROPAPLIST:   ridcode = RID_APLIST;       break;
    case AIROPCFG:      ridcode = RID_CONFIG;       break;
    case AIROPWEPKEYNV: ridcode = RID_WEP_PERM;     break;
    case AIROPLEAPUSR:  ridcode = RID_LEAPUSERNAME; break;
    case AIROPLEAPPWD:  ridcode = RID_LEAPPASSWORD; break;
    case AIROPWEPKEY:   ridcode = RID_WEP_TEMP; 	break;
    case AIROPLEAPUSR+1: ridcode = 0xff2a; 	break;
    case AIROPLEAPUSR+2: ridcode = 0xff2b; 	break;

      /* this is not really a rid but a command given to the card 
       * same with MAC off
       */
    case AIROPMACON:
      
      if (mac_enable((struct venus_info *)dev->priv, &rsp) != 0)
	return -EIO;
      return 0;
      break;
      /* 
       * Evidently this code in the airo driver does not get a symbol
       * as disable_MAC. it's probably so short the compiler does not gen one.
       */
    case AIROPMACOFF:
      
      mac_disable((struct venus_info *)dev->priv);

      return 0;
      break;
      /* This command merely clears the counts does not actually store any data
       * only reads rid. But as it changes the cards state, I put it in the
       * writerid routines. Added code to clear the MIC status.
       */
    case AIROPSTCLR:
      ridcode = RID_STATSDELTACLEAR;
      memset((char *)&v_info->micstats,0,sizeof(v_info->micstats)); /*Clear micstats */
      vreadrid((struct venus_info *)dev->priv,ridcode,iobuf, 0);
      
      if (copy_to_user(comp->data,blargo,MIN(comp->length,sizeof(blargo))))
	return -EFAULT;
      return 0;
      break;
      
    default:
      return -EOPNOTSUPP;	/* Blarg! */
    }
  
  
  if(comp->length > sizeof(blargo))
    return -EINVAL;
  
  copy_from_user(blargo,comp->data,comp->length);
  
  *(unsigned short *)blargo = comp->length; 
  
  /* 
   * Whenever a cfg write is done
   * if MIC capable turn it on
   */
  if(comp->command ==  AIROPCFG){
    cfg = (ConfigRid *)blargo;
    if(v_info->flags & MIC_CAPABLE)
      cfg->opmode |= CFG_OPMODE_MIC;
  }

  if((stat = vwriterid((struct venus_info *)dev->priv, ridcode, blargo))){
    return -EIO;
  }
  return 0;
}

#ifdef WIRELESS_EXT
int getQuality(STSTATUS *statusRid, STCAPS *capsRid)
{
  int quality = 0;

 if ((statusRid->u16OperationalMode&0x3f) == 0x3f) {
   if (capsRid->u16HardwareCapabilities & 0x0008) {
     if (!memcmp(capsRid->au8ProductName, "350", 3)) {
        if (statusRid->u16SignalQuality > 0xb0) {
          quality = 0;
        } else if (statusRid->u16SignalQuality < 0x10) {
          quality = 100;
        } else {
          quality = ((0xb0-statusRid->u16SignalQuality)*100)/0xa0;
        }
      } else {
        if (statusRid->u16SignalQuality > 0x20) {
          quality = 0;
        } else {
          quality = ((0x20-statusRid->u16SignalQuality)*100)/0x20;
        }
      }
    } else {
      quality = 0;
    }
  }
  return(quality);
}
#endif

#ifdef WIRELESS_EXT
/*
 * Get the Wireless stats out of the driver
 * Note : irq and spinlock protection will occur in the subroutines
 *
 * TODO :
 *	o Check if work in Ad-Hoc mode (otherwise, use SPY, as in wvlan_cs)
 *	o Find the noise level
 *	o Convert values to dBm
 *	o Fill out discard.misc with something interesting
 *
 * Jean
 */
struct iw_statistics *airo_get_wireless_stats(struct net_device *dev)
{
	STSTATUS *statusRid;
	STCAPS *capsRid;
	int *vals;
        struct venus_info *v_info = (struct venus_info *)dev->priv;
        int value;
        int flags;

#ifdef BAD_CARD
        if (bad_card) {
          memset(&v_info->wstats, 0, sizeof(struct iw_statistics));
          return (&v_info->wstats);
        }
#endif
        if (v_info->flags & FLASHING) {
          memset(&v_info->wstats, 0, sizeof(struct iw_statistics));
          return (&v_info->wstats);
        }
        spin_lock_irqsave(&v_info->mpi_lock,flags);
	/* Get stats out of the card */
        vreadrid(v_info,0xff00,(char *)(v_info->saveConfig), 2048);
        capsRid = (STCAPS *)(v_info->saveConfig);
        vreadrid(v_info,0xff50,(char *)(v_info->saveSSID), 2048);
        statusRid = (STSTATUS *)(v_info->saveSSID);
        vreadrid(v_info,0xff68,(char *)(v_info->saveAPList), 2048);
	vals = (int*)&(v_info->saveAPList[4]);

	/* The status */
	v_info->wstats.status = le16_to_cpu(statusRid->u16OperationalMode);

	/* Signal quality and co. But where is the noise level ??? */
        value = getQuality(statusRid, capsRid);
	v_info->wstats.qual.qual = le16_to_cpu(value);
        if ((statusRid->u16OperationalMode&0x3f) == 0x3f) {
  	  v_info->wstats.qual.level = le16_to_cpu(statusRid->u16NormalizedSignalStrength);
        } else {
  	  v_info->wstats.qual.level = 0;
        }
	v_info->wstats.qual.noise = le16_to_cpu(statusRid->u16MaxNoiseLevelLastSecond);
	v_info->wstats.qual.updated = 3;

	/* Packets discarded in the wireless adapter due to wireless
	 * specific problems */
	v_info->wstats.discard.nwid = le32_to_cpu(vals[56]) + le32_to_cpu(vals[57]) + le32_to_cpu(vals[58]);/* SSID Mismatch */
	v_info->wstats.discard.code = le32_to_cpu(vals[6]);/* RxWepErr */
	v_info->wstats.discard.misc = le32_to_cpu(vals[1]) + le32_to_cpu(vals[2])
		+ le32_to_cpu(vals[3]) + le32_to_cpu(vals[4])
		+ le32_to_cpu(vals[30]) + le32_to_cpu(vals[32]);
        spin_unlock_irqrestore(&v_info->mpi_lock,flags);
	return (&v_info->wstats);
}
#endif 

#ifdef INCLUDE_RFMONITOR
static void venus_set_rfmonitor(struct venus_info *v_info)
{
  ConfigRid *configRid;
  PC3500_SID_LIST *ssidRid;

  if (v_info->rfMonitor) {
printk(KERN_INFO "venus_set_rfmonitor : rfmonitor\n");
    vreadrid(v_info,0xff11,(char *)(v_info->saveSSID), 2048);
    memcpy(v_info->saveAPList, v_info->saveSSID, *(unsigned short *)(v_info->saveSSID));
    ssidRid = (PC3500_SID_LIST *)(v_info->saveAPList);
    ssidRid->aSsid[0].SsidLen = 3;
    memcpy(ssidRid->aSsid[0].Ssid, "ANY", 3);
    memset(ssidRid->aSsid[0].Ssid+3, 0, 29);
    vwriterid(v_info,0xff11,(char *)(v_info->saveAPList));
    vreadrid(v_info,0xff21,(char *)(v_info->saveConfig), 2048);
    memcpy(v_info->saveAPList, v_info->saveConfig, *(unsigned short *)(v_info->saveConfig));
    configRid = (ConfigRid *)(v_info->saveAPList);
    configRid->rmode = 0x300 | RXMODE_RFMON_ANYBSS;
    configRid->opmode = 0x102;
    configRid->atimDuration = 5;
    configRid->refreshInterval = 0xffff;
    configRid->authType = AUTH_OPEN;
    v_info->rfMonitor = TRUE;
    vwriterid(v_info,0xff10,(char *)(v_info->saveAPList));
  } else {
    vreadrid(v_info,0xff21,(char *)(v_info->saveAPList), 2048);
    vwriterid(v_info,0xff10,(char *)(v_info->saveAPList));
    v_info->rfMonitor = FALSE;
printk(KERN_INFO "venus_set_rfmonitor : not-rfmonitor\n");
  }
}
#endif

static void venus_set_multicast_list(struct net_device *dev)
{
  struct venus_info *v_info ;

  v_info = (struct venus_info *)dev->priv;
  if ((dev->flags) & IFF_PROMISC) {
  } else {
  }
}
/*
 * Here determination is made weather a read or a write 
 * to the card must be done. Handle userspace locking 
 * here.
 */

static int venus_ioctl(struct net_device *dev, struct ifreq *rq,int cmd) {
  int           status;
  aironet_ioctl com;
  struct venus_info *v_info ;
  int rc = -EINVAL;
#ifdef WIRELESS_EXT
  struct iw_range range;
  int flags;
  struct iwreq *iwreq = (struct iwreq *) rq;
  ConfigRid *configRid;
  STSTATUS *statusRid;
  STCAPS *capsRid;
  PC3500_SID_LIST *ssidRid;
  int i;
  int index;
#endif
  int irqflags;


  v_info = (struct venus_info *)dev->priv;
  spin_lock_irqsave(&v_info->mpi_lock,irqflags);

  /* 
   * return identify magic number for this interface.
   * upper level should have given a pointer to an int
   * to store the stuff.
   */   

  if(cmd == AIROIDIFC ){
    copy_from_user(&com,rq->ifr_data,sizeof(com));
    status      = AIROMAGIC;
    com.command = 0;
    if(copy_to_user(com.data,(char *)&status,sizeof(status))){
      rc =  -EFAULT;
    }
    else
      {
	rc = 0;
      }
  } else if(cmd == AIROIOCTL ){
    /* Get the command struct and hand it off for evaluation by 
     * the proper subfunction
     */
    copy_from_user(&com,rq->ifr_data,sizeof(com));
    status = com.command;          
#ifdef BAD_CARD
    if (bad_card) {
      if ( status >= AIROFLSHRST && status <= AIRORESTART ){
        rc = flashcard(dev,&com);
        spin_unlock_irqrestore(&v_info->mpi_lock,flags);
        return rc;
      } else {
        return -EFAULT;
      }
    }

#endif

    /* Seperate R/W functions bracket legality here
     */
    if( status == AIRORSWVERSION) {
      if (copy_to_user(com.data, swversion, MIN (com.length, strlen(swversion)+1))) {
        rc = -EFAULT;
      } else {
        rc = 0;
      }
    } else if( status == AIRORRID || status == AIROGID) {
      rc = readrids(dev,&com);
    } else if( status >= AIROGCAP && status <= AIROGMICSTATS  ){
      rc = readrids(dev,&com);
    } else if( status >=AIROPCAP && status <= (AIROPLEAPUSR+2) ){
      rc = writerids(dev,&com);
    } else if ( status >= AIROFLSHRST && status <= AIRORESTART ){
      rc = flashcard(dev,&com);
    }
  } else {
#ifdef BAD_CARD
    if (bad_card) return(-EBUSY);
#endif
#ifdef WIRELESS_EXT
    if (v_info->flags & FLASHING) {
      rc = -EBUSY;
    } else {
    switch (cmd) {
    case SIOCGIWNAME:
       strcpy(iwreq->u.name, "IEEE 802.11-DS");
       rc = 0;
       break;
    case SIOCSIWESSID:
       if (!iwreq->u.data.flags) {
         printk("SIOCSIWESSID flags %x\n", iwreq->u.data.flags);
       } else if (iwreq->u.data.pointer) {
         if (--iwreq->u.data.length > 32) {
           rc = -E2BIG;
         } else {
           ssidRid = (PC3500_SID_LIST *)(v_info->saveSSID);
           memset(ssidRid, 0, 0x68);
           ssidRid->aSsid[0].SsidLen = iwreq->u.data.length;
           strcpy(ssidRid->aSsid[0].Ssid, iwreq->u.data.pointer);
           ssidRid->u16RidLen = 0x68;
           vwriterid(v_info,RID_SSID,(char *)ssidRid);
           rc = 0;
         }
       } else {
         rc = -EINVAL;
       }
       break;
    case SIOCGIWESSID:
       iwreq->u.data.flags = 1;
       vreadrid(v_info,RID_SSID,(char *)(v_info->saveSSID), 2048);
       ssidRid = (PC3500_SID_LIST *)(v_info->saveSSID);
       ssidRid->aSsid[1].SsidLen = 0;
       strncpy(iwreq->u.data.pointer, ssidRid->aSsid[0].Ssid, 32);
       iwreq->u.data.length = ssidRid->aSsid[0].SsidLen;
       rc = 0;
       break;
    case SIOCSIWNICKN:
       if (iwreq->u.data.pointer) {
         if (iwreq->u.data.length > 32) {
           rc = -E2BIG;
         } else {
           vreadrid(v_info,RID_CONFIG,(char *)(v_info->saveConfig), 2048);
           configRid = (ConfigRid *)(v_info->saveConfig);
           strncpy(configRid->nodeName, iwreq->u.data.pointer, 16);
           vwriterid(v_info,RID_CONFIG,(char *)configRid);
           rc = 0;
         }
       }
       break;
    case SIOCGIWNICKN:
       iwreq->u.data.flags = 1;
       vreadrid(v_info,RID_CONFIG,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
       configRid->arlThreshold = 0;
       strcpy(iwreq->u.data.pointer, configRid->nodeName);
       iwreq->u.data.length = strlen(configRid->nodeName);
       rc = 0;
       break;
    case SIOCSIWMODE:
       vreadrid(v_info,RID_CONFIG,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
       if (iwreq->u.mode == IW_MODE_INFRA || iwreq->u.mode == IW_MODE_AUTO) {
         configRid->opmode |= MODE_STA_ESS;
         vwriterid(v_info,RID_CONFIG,(char *)configRid);
         rc = 0;
       } else if (iwreq->u.mode == IW_MODE_ADHOC) {
         configRid->opmode &= ~MODE_STA_ESS;
         vwriterid(v_info,RID_CONFIG,(char *)configRid);
         rc = 0;
       } else if (iwreq->u.mode == IW_MODE_MONITOR) {
printk(KERN_INFO "mpi350 monitor mode\n");
       } else {
         rc = -EINVAL;
       }
       break;
    case SIOCGIWMODE:
       vreadrid(v_info,RID_CONFIG,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
       if (configRid->opmode & MODE_STA_ESS) {
         iwreq->u.mode = IW_MODE_INFRA;
       } else {
         iwreq->u.mode = IW_MODE_ADHOC;
       }
       rc = 0;
       break;
    case SIOCSIWTXPOW:
       if (iwreq->u.txpower.flags & IW_TXPOW_MWATT) {
         vreadrid(v_info,RID_CONFIG,(char *)(v_info->saveConfig), 2048);
         configRid = (ConfigRid *)(v_info->saveConfig);
         configRid->txPower = iwreq->u.txpower.value;
         rc = 0;
       } else if (!(iwreq->u.txpower.flags & IW_TXPOW_MWATT)) {
         vreadrid(v_info,RID_CONFIG,(char *)(v_info->saveConfig), 2048);
         configRid = (ConfigRid *)(v_info->saveConfig);
         rc = 0;
         switch (iwreq->u.txpower.value) {
         case 0: configRid->txPower = 1; break;
         case 7: configRid->txPower = 5; break;
         case 10: configRid->txPower = 10; break;
         case 12: configRid->txPower = 15; break;
         case 14: configRid->txPower = 20; break;
         case 15: configRid->txPower = 30; break;
         case 17: configRid->txPower = 50; break;
         case 20: configRid->txPower = 100; break;
         default: rc = -EOPNOTSUPP; break;
         }
       } else {
         rc = -EOPNOTSUPP;
       }
       if (rc == 0) {
         rc = -EOPNOTSUPP;
         vreadrid(v_info,0xff00,(char *)(v_info->saveSSID), 2048);
         capsRid = (STCAPS *)(v_info->saveSSID);
         for (i = 0 ; capsRid->au16TxPowerLevels[i] ; i++) {
           if (capsRid->au16TxPowerLevels[i] == configRid->txPower) {
             vwriterid(v_info,RID_CONFIG,(char *)configRid);
             rc = 0;
             break;
           }
         }
       }
       break;
    case SIOCGIWTXPOW:
       vreadrid(v_info,0xff20,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
       iwreq->u.txpower.flags = IW_TXPOW_MWATT;
       iwreq->u.txpower.value = configRid->txPower;
       iwreq->u.txpower.disabled = !(v_info->macstatus);
       rc = 0;
       break;
    case SIOCSIWFREQ:
       vreadrid(v_info,0xff10,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
       index = -1;
       rc = 0;
       if (iwreq->u.freq.m < 1000) {
         index = iwreq->u.freq.m;
       } else {
         for (i = 1 ; iwreq->u.freq.e > 1 ; --iwreq->u.freq.e) {
           i *= 10;
         } 
         iwreq->u.freq.m *= i;
         if (iwreq->u.freq.m % 100000) {
           rc = -EINVAL;
         }
         iwreq->u.freq.m /= 100000;
         index = (iwreq->u.freq.m - 2412)/5 + 1;
       }
       if (rc == 0) {
         if ((index >= 1) && (index <= 14)) {
           configRid->channelSet = index;
           vwriterid(v_info,RID_CONFIG,(char *)configRid);
           rc = 0;
         } else {
           rc = -EINVAL;
         }
       }
       break;
    case SIOCGIWFREQ:
       vreadrid(v_info,0xff50,(char *)(v_info->saveConfig), 2048);
       statusRid = (STSTATUS *)(v_info->saveConfig);
       iwreq->u.freq.m = statusRid->channel.u16DsChannel;
       iwreq->u.freq.e = 0;
       iwreq->u.freq.i = 0;
       rc = 0;
       break;
    case SIOCGIWRTS:
       vreadrid(v_info,0xff10,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
       iwreq->u.rts.value = configRid->rtsThres;
       rc = 0;
       break;
    case SIOCSIWRTS:
       vreadrid(v_info,0xff10,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
       configRid->rtsThres = iwreq->u.rts.value;
       vwriterid(v_info,RID_CONFIG,(char *)configRid);
       rc = 0;
       break;
    case SIOCGIWFRAG:
       vreadrid(v_info,0xff10,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
       iwreq->u.rts.value = configRid->fragThresh;
       rc = 0;
       break;
    case SIOCSIWFRAG:
       vreadrid(v_info,0xff10,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
       configRid->fragThresh = iwreq->u.rts.value/500000;
       vwriterid(v_info,RID_CONFIG,(char *)configRid);
       rc = 0;
       break;
    case SIOCGIWRATE:
       vreadrid(v_info,0xff50,(char *)(v_info->saveConfig), 2048);
       statusRid = (STSTATUS *)(v_info->saveConfig);
       iwreq->u.bitrate.value = statusRid->u16CurrentTxRate * 500000;
       rc = 0;
       break;
    case SIOCSIWRATE:
       vreadrid(v_info,0xff10,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
       if (iwreq->u.bitrate.fixed == 0) { /* auto */
         index = 1;
       } else {
         index = 0;
       }
       flags = iwreq->u.bitrate.value/500000;
       for (i = 0 ; i < 8 ; i++) {
         if (((configRid->rates[i] & 0x7f) == flags) || index) {
           configRid->rates[i] |= 0x80;
           if ((configRid->rates[i] & 0x7f) == flags) {
             break;
           }
         } else {
           configRid->rates[i] &= 0x7f;
         }
       }
       vwriterid(v_info,RID_CONFIG,(char *)configRid);
       rc = 0;
       break;
    case SIOCSIWAP:
       vreadrid(v_info,0xff12,(char *)(v_info->saveConfig), 2048);
       memset((v_info->saveConfig)+2, 0, 24);
       if (iwreq->u.ap_addr.sa_family != ARPHRD_ETHER) {
         rc = -EOPNOTSUPP;
       } else {
         memcpy((v_info->saveConfig)+2, iwreq->u.ap_addr.sa_data, 6);
         vwriterid(v_info,0xff12,(char *)(v_info->saveConfig));
         rc = 0;
       }
       break;
    case SIOCGIWAP:
       vreadrid(v_info,0xff50,(char *)(v_info->saveConfig), 2048);
       statusRid = (STSTATUS *)(v_info->saveConfig);
       memcpy(iwreq->u.ap_addr.sa_data, statusRid->au8CurrentBssid, 6);
       iwreq->u.ap_addr.sa_family = ARPHRD_ETHER;
       rc = 0;
       break;
    case SIOCSIWENCODE:
       index = iwreq->u.encoding.flags & IW_ENCODE_INDEX;
       vreadrid(v_info,0xff10,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
       /*
        * kludge - network config uses 0
        * iwconfig only accepts 1 and above
        */
        if (index >= 1) {
          index--;
        }
//printk("SIOCSIWENCODE index %d length %d flags %04x\n", index, iwreq->u.encoding.length, iwreq->u.encoding.flags);
       rc = 0;
       if (iwreq->u.encoding.length != 0) {
         if ((iwreq->u.encoding.length != 5) && (iwreq->u.encoding.length != 13)) {
           rc = -EINVAL;
         }
         if ((index < 0) || (index > 3)) {
           rc = -EINVAL;
         }
         if (!iwreq->u.encoding.pointer) {
           rc = -EINVAL;
         }
         if (rc == 0) {
           memset((v_info->saveSSID), 0, 25);
           *(unsigned short *) (v_info->saveSSID) = 25;
           *(unsigned short *) ((v_info->saveSSID)+2) = index;
           (v_info->saveSSID)[4] = 1;
           *(unsigned short *) ((v_info->saveSSID)+10) = iwreq->u.encoding.length;
           copy_from_user((v_info->saveSSID)+12, iwreq->u.encoding.pointer, iwreq->u.encoding.length);
//for (i = 0 ; i < iwreq->u.encoding.length ; i++) {
//printk("key[%d] %02x\n", i, (v_info->saveSSID)[12+i]);
//}
           vwriterid(v_info,0xff15,(char *)(v_info->saveSSID));
         }
       }
       if ((index >= 0) && (index <= 3) && !(iwreq->u.encoding.flags & IW_ENCODE_NOKEY)) {
         memset((v_info->saveSSID), 0, 25);
         *(unsigned short *) (v_info->saveSSID) = 25;
         *(unsigned short *) ((v_info->saveSSID)+2) = 0xffff;
         *(unsigned short *) ((v_info->saveSSID)+4) = index;
         vwriterid(v_info,0xff15,(char *)(v_info->saveSSID));
       }
       configRid->authType &= ~0x1000;
       if (iwreq->u.encoding.flags & IW_ENCODE_DISABLED) {
         configRid->authType &= ~0x100;
       } else {
         configRid->authType |= 0x100;
       }
       if (iwreq->u.encoding.flags & IW_ENCODE_RESTRICTED) {
         configRid->authType &= ~0x200;
       }
       if (iwreq->u.encoding.flags & IW_ENCODE_OPEN) {
         configRid->authType |= 0x200;
       }
       vwriterid(v_info,RID_CONFIG,(char *)configRid);
       break;
    case SIOCGIWENCODE:
       vreadrid(v_info,0xff15,(char *)(v_info->saveSSID), 2048);
       iwreq->u.encoding.length = 0;
       if (*(unsigned short *) ((v_info->saveSSID)+2) != 0xffff) {
         iwreq->u.encoding.length = *(unsigned short *)((v_info->saveSSID)+10);
       }
       iwreq->u.encoding.flags = IW_ENCODE_NOKEY;
       rc = 0;
       break;
    case SIOCGIWSENS:
       vreadrid(v_info,0xff10,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
       iwreq->u.sens.value = cpu_to_le16(configRid->rssiThreshold);
       iwreq->u.sens.disabled = (iwreq->u.sens.value == 0);
       iwreq->u.sens.fixed = 1;
       rc = 0;
       break;

    case SIOCSIWSENS:
      vreadrid(v_info,0xff10,(char *)(v_info->saveConfig), 2048);
      configRid = (ConfigRid *)(v_info->saveConfig);
      configRid->rssiThreshold = cpu_to_le16(iwreq->u.sens.disabled ? RSSI_DEFAULT : iwreq->u.sens.value);
      vwriterid(v_info,RID_CONFIG,(char *)configRid);
      rc = 0;
      break;
#if 0
   case SIOCSIWSPY:
      rc = 0;
      break;
   case SIOCGIWSPY:
      vreadrid(v_info,0xff50,(char *)temp);
      statusRid = (STSTATUS *)temp;
      iwreq->u.data.length = 0;
      spy_number = 1;
      if (iwreq->u.data.pointer && ((statusRid->u16OperationalMode & 0x3f) == 0x3f )) {
        iwreq->u.data.length = spy_number;
        memcpy(address[i].sa_data, statusRid->au8CurrentBssid, 6);
        address[i].sa_family = AF_UNIX;
        copy_to_user(iwreq->u.data.pointer, address, sizeof(struct sockaddr)*spy_number);
        quality[0].qual = getQuality(statusRid, capsRid);
        quality[0].level = statusRid->u16NormalizedSignalStrength;
        quality[0].noise =  statusRid->u16MaxNoiseLevelLastSecond;
        quality[0].updated = 1;
        copy_to_user(iwreq->u.data.pointer + (sizeof(struct sockaddr)*spy_number), quality, sizeof(struct iw_quality) * spy_number);
      }
      rc = 0;
      break;
#endif
/*
 *
 * private ioctls:
 *   sets:
 *     enableLeap [on | off]
 *     powerSaving [cam | psp | max]
 *   gets:
 *     getPowerSaving results: [cam | psp | max]
 *     getAssocStatus results: [associated | enabled | disabled]
 *     getSigStrength results: 0 - 100
 *     getSigQuality results: 0 - 100
 *     getFWVersion results: x.xx.xx
 *     getDriverVersion results: x.x
 *     getProductName
 *
 */
    case SIOCGIWPRIV:
       if (iwreq->u.data.pointer) {
         struct iw_priv_args priv[] = {
        /* { cmd, set_args, get_args, name } */
          { SIOCIWFIRSTPRIV,
            IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 3,
            0,
            "enableLeap" },
          { SIOCIWFIRSTPRIV+2,
            IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 3,
            0,
            "powerSaving" },
          { SIOCIWFIRSTPRIV+3,
            0,
            IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 3,
            "getPowerSaving" },
          { SIOCIWFIRSTPRIV+5,
            0,
            IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 10,
            "getAssocStatus" },
          { SIOCIWFIRSTPRIV+7,
            0,
            IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 10,
            "getFWVersion" },
          { SIOCIWFIRSTPRIV+9,
            0,
            IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 10,
            "getDrvrVersion" },
          { SIOCIWFIRSTPRIV+11,
            0,
            IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 16,
            "getProductName" },
#ifdef INCLUDE_RFMONITOR
          { SIOCIWFIRSTPRIV+12,
            IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 3,
            0,
            "setRFMonitor" },
          { SIOCIWFIRSTPRIV+13,
            0,
            IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 3,
            "getRFMonitor" },
#endif
          };
#ifdef INCLUDE_RFMONITOR
          iwreq->u.data.length = 9;
#else
          iwreq->u.data.length = 7;
#endif
          copy_to_user(iwreq->u.data.pointer, (u8 *) priv, sizeof(priv));
          rc = 0;
       } else {
         rc = -EFAULT;
       }
       break;
    case SIOCIWFIRSTPRIV:	/* enable leap */
       vreadrid(v_info,0xff10,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
//       printk("SIOCSIWFIRSTPRIV enable LEAP %s!\n", iwreq->u.name);
       if (!strcmp(iwreq->u.name, "yes")) {
         configRid->authType = 0x1101;
       } else if (!strcmp(iwreq->u.name, "no")) {
         configRid->authType = 0x0001;
       }
       vwriterid(v_info,RID_CONFIG,(char *)configRid);
       rc = 0;
       break;
    case SIOCIWFIRSTPRIV+2:	/* set power saving mode */
       vreadrid(v_info,0xff10,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
//       printk("SIOCSIWFIRSTPRIV power saving mode %s!\n", iwreq->u.name);
       rc = 0;
       if (!strcmp(iwreq->u.name, "cam")) {
         configRid->powerSaveMode = POWERSAVE_CAM;
       } else if (!strcmp(iwreq->u.name, "psp")) {
         configRid->powerSaveMode = POWERSAVE_PSP;
       } else if (!strcmp(iwreq->u.name, "max")) {
         configRid->powerSaveMode = POWERSAVE_PSPCAM;
       } else {
         rc = -EINVAL;
       }
       if (rc == 0) {
         vwriterid(v_info,RID_CONFIG,(char *)configRid);
       }
       break;
    case SIOCIWFIRSTPRIV+3:	/* get power saving mode */
       vreadrid(v_info,0xff20,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
       if (configRid->powerSaveMode == POWERSAVE_CAM) {
         strcpy(iwreq->u.name, "cam");
       } else if (configRid->powerSaveMode == POWERSAVE_PSP) {
         strcpy(iwreq->u.name, "psp");
       } else if (configRid->powerSaveMode == POWERSAVE_PSPCAM) {
         strcpy(iwreq->u.name, "max");
       }
       rc = 0;
       break;
    case SIOCIWFIRSTPRIV+5:	/* association status */
       vreadrid(v_info,0xff50,(char *)(v_info->saveConfig), 2048);
       statusRid = (STSTATUS *)(v_info->saveConfig);
       if (!statusRid->u16OperationalMode) {
         strcpy(iwreq->u.name, "disabled");
       } else if ((statusRid->u16OperationalMode & 0x3f) != 0x3f) {
         strcpy(iwreq->u.name, "enabled");
       } else {
         strcpy(iwreq->u.name, "associated");
       }
       rc = 0;
       break;
    case SIOCIWFIRSTPRIV+7:	/* FW version */
       vreadrid(v_info,0xff00,(char *)(v_info->saveConfig), 2048);
       capsRid = (STCAPS *)(v_info->saveConfig);
       strcpy(iwreq->u.name, capsRid->au8ProductVersion);
       rc = 0;
       break;
    case SIOCIWFIRSTPRIV+9:	/* driver version */
       strcpy(iwreq->u.name, swversion);
       rc = 0;
       break;
    case SIOCIWFIRSTPRIV+11:	/* product name */
       vreadrid(v_info,0xff00,(char *)(v_info->saveConfig), 2048);
       capsRid = (STCAPS *)(v_info->saveConfig);
       strcpy(iwreq->u.name, capsRid->au8ProductName);
       rc = 0;
       break;
#ifdef INCLUDE_RFMONITOR
    case SIOCIWFIRSTPRIV+12:	/* rf monitor */
       rc = 0;
       if (!strcmp(iwreq->u.name, "on")) {
         v_info->rfMonitor = TRUE;
       } else if (!strcmp(iwreq->u.name, "off")) {
         v_info->rfMonitor = FALSE;
       } else {
         rc = -EINVAL;
       }
       if (rc == 0) {
         venus_set_rfmonitor(v_info);
       }
       break;
    case SIOCIWFIRSTPRIV+13:	/* rf monitor */
       vreadrid(v_info,0xff20,(char *)(v_info->saveConfig), 2048);
       configRid = (ConfigRid *)(v_info->saveConfig);
       if (configRid->rmode == (0x300 | RXMODE_RFMON_ANYBSS)) {
         strcpy(iwreq->u.name, "on");
       } else {
         strcpy(iwreq->u.name, "off");
       }
       rc = 0;
       break;
#endif
    case SIOCGIWRANGE: 
       vreadrid(v_info,0xff00,(char *)(v_info->saveConfig), 2048);
       capsRid = (STCAPS *)(v_info->saveConfig);
       memset(&range, 0, sizeof(struct iw_range));
       range.num_channels = 14;
       range.num_frequency = 14;
       for (i = 0 ; i < 14 ; i++) {
         range.freq[i].m = 2412 + i*5;
         range.freq[i].e = 6;
         range.freq[i].i = i+1;
       }
       for (i = 0 ; capsRid->au8SupportedRates[i] ; i++) {
         range.bitrate[i] = (capsRid->au8SupportedRates[i] & 0x7f) * 500000;
       }
       range.num_bitrates = i;
       for (i = 0 ; capsRid->au16TxPowerLevels[i] ; i++) {
         range.txpower[i] = capsRid->au16TxPowerLevels[i];
       }
       range.num_txpower = i;
       range.txpower_capa = IW_TXPOW_MWATT;

       range.we_version_compiled = WIRELESS_EXT;
       range.min_rts = 16;
       range.max_rts = 2312;
       range.min_frag = 256;
       range.max_frag = 2312;

       range.max_qual.qual = 100;
       range.max_qual.level = 100;
       range.max_qual.noise = 0;

       if(capsRid->u16SoftwareCapabilities & 2) {
         range.encoding_size[0] = 5;
         if (capsRid->u16SoftwareCapabilities & 0x100) {
           range.encoding_size[1] = 13;
           range.num_encoding_sizes = 2;
         } else
           range.num_encoding_sizes = 1;
           range.max_encoding_tokens = 4;  // 4 keys
         } else {
           range.num_encoding_sizes = 0;
           range.max_encoding_tokens = 0;
       }

       copy_to_user(iwreq->u.data.pointer, (u8 *) &range, sizeof(struct iw_range));
       rc = 0;
       break;
    default:
//       printk(KERN_INFO "mpi350 unknown command %04x\n", cmd);
       rc = -EINVAL;
       break;
    }
    }
#endif
  }
  spin_unlock_irqrestore(&v_info->mpi_lock,irqflags);
  return(rc);
}




/*****************************************************************************
 * Ancillary flash / mod functions much black magic lurkes here              *
 *****************************************************************************
 */

/* 
 * Flash command switch table
 */
int flashcard(struct net_device *dev, aironet_ioctl *comp) {
  int z;
  int cmdreset(struct venus_info *);
  int setflashmode(struct venus_info *);
  int flashgchar(struct venus_info *,int,int);
  int flashpchar(struct venus_info *,int,int);
  int flashputbuf(struct venus_info *);
  int flashrestart(struct venus_info *,struct net_device *);
  
  switch(comp->command)
    {
    case AIROFLSHRST:
#ifdef BAD_CARD
      if (bad_card) return(0);
#endif
      return cmdreset((struct venus_info *)dev->priv);
      
    case AIROFLSHSTFL:
      return setflashmode((struct venus_info *)dev->priv);
      
    case AIROFLSHGCHR: /* Get char from aux */
      if(comp->length != sizeof(int))
	return -EINVAL;
      copy_from_user(&z,comp->data,comp->length);
      return flashgchar((struct venus_info *)dev->priv,z,8000);
      
    case AIROFLSHPCHR: /* Send char to card. */
      if(comp->length != sizeof(int))
	return -EINVAL;
      copy_from_user(&z,comp->data,comp->length);
      return flashpchar((struct venus_info *)dev->priv,z,8000);
      
    case AIROFLPUTBUF: /* Send 32k to card */
      if(comp->length > FLASHSIZE)
	return -EINVAL;
      if(copy_from_user(flashbuffer,comp->data,comp->length)) {
	return -EINVAL;
      }
      
      flashputbuf((struct venus_info *)dev->priv);
      return 0;
      
    case AIRORESTART:
#ifdef BAD_CARD
      bad_card = 0;
#endif
      if(flashrestart((struct venus_info *)dev->priv,dev))
	return -EIO;
      return 0;
    }

  return -EINVAL;
}

#define FLASH_COMMAND  0x7e7e

int unstickbusy(struct venus_info *ai) {
  unsigned short uword;

  /* clear stuck command busy if necessary */
  if ((uword=venusin(ai, V_COMMAND)) & COMMAND_BUSY) {
    venusout(ai, V_EVACK, EV_CLEARCOMMANDBUSY);
    return 1;
  } 
  return 0;
}

/* Wait for busy completion from card
 * wait for delay uSec's Return true 
 * for success meaning command reg is 
 * clear
 */
int WaitBusy(struct venus_info *ai,int uSec){
  int statword =0xffff;
  int delay =0; 
  
  while((statword & COMMAND_BUSY) && delay <= (1000 * 100) ){
    udelay(10);
    delay += 10;
    statword = venusin(ai,V_COMMAND);  
    
    if((COMMAND_BUSY & statword) && (delay%200)){
      unstickbusy(ai);
    }
  }  
  return 0 == (COMMAND_BUSY & statword);
}
/* 
 * STEP 1)
 * Disable MAC and do soft reset on 
 * card. 
 */

int cmdreset(struct venus_info *ai) {
  int status;
  Cmd cmd;
  Resp rsp;

  mac_disable(ai);
  
  if(!(status = WaitBusy(ai,600))){
    printk(KERN_INFO "Waitbusy hang b4 RESET =%d\n",status);
    return -EBUSY;
  }
  ai->flags |= FLASHING;
  venusout(ai,V_EVINTEN,0x0);
  cmd.cmd = CMD_X500_ResetCard;
  cmd.parm0 = cmd.parm1 = cmd.parm2 = 0;
  venuscommand(ai,&cmd,&rsp);
  
  mdelay(1024);          /* WAS 600 12/7/00 */
  
  if(!(status=WaitBusy(ai,100))){
    printk(KERN_INFO "Waitbusy hang AFTER RESET =%d\n",status);
    ai->flags &=~(FLASHING);
    return -EBUSY;
  }
  return 0;
}

/* STEP 2)
 * Put the card in legendary flash 
 * mode
 */

int setflashmode (struct venus_info *ai) {
  int   status;
  
  venusout(ai, V_SWS0, FLASH_COMMAND);
  venusout(ai, V_SWS1, FLASH_COMMAND);
  venusout(ai, V_SWS0, FLASH_COMMAND);
  venusout(ai, V_COMMAND,0x10);
  mdelay(500); /* 500ms delay */

  if(!(status=WaitBusy(ai,600))) {
    printk(KERN_INFO "Waitbusy hang after setflash mode\n");
    ai->flags &=~(FLASHING);
    return -EIO;
  }
  return 0;
}

/* Put character to V_SWS0 wait for dwelltime 
 * x 50us for  echo . 
 */

int flashpchar(struct venus_info *ai,int byte,int dwelltime) {
	int echo;
	int pollbusy,waittime;

	byte |= 0x8000;

	if(dwelltime == 0 )
		dwelltime = 200;
  
	waittime=dwelltime;


	/* Wait for busy bit d15 to 
	 * go false indicating buffer
	 * empty
	 */
	do {
	  pollbusy = venusin(ai,V_SWS0);
	  
	  if(pollbusy & 0x8000){
	    udelay(50);
	    waittime -= 50;
	  } else 
	    break;
	} while(waittime >=0);

	/* timeout for busy clear wait */

	if(waittime <= 0 ){
	  printk(KERN_INFO "flash putchar busywait timeout! \n");
	  return -EBUSY;
	}

	/* Port is clear now write byte and 
	 *  wait for it to echo back
	 */
	do{
	  venusout(ai,V_SWS0,byte);
	  udelay(50);
	  dwelltime -= 50;
	  echo = venusin(ai,V_SWS1);
	}while (dwelltime >= 0 && echo != byte);

	venusout(ai,V_SWS1,0);

	return (echo == byte) ? 0 : -EIO;
}

/*
 * Get a character from the card matching matchbyte
 * Step 3)
 */
int flashgchar(struct venus_info *ai,int matchbyte,int dwelltime){
  int           rchar;
  unsigned char rbyte=0;
  
  do {
    rchar = venusin(ai,V_SWS1);
    
    if(dwelltime && !(0x8000 & rchar)){
      dwelltime -= 10;
      mdelay(10);
      continue;
    }
    rbyte = 0xff & rchar;
    
    if( (rbyte == matchbyte) && (0x8000 & rchar) ){
      venusout(ai,V_SWS1,0);
      return 0;
    }
    if( rbyte == 0x81 || rbyte == 0x82 || rbyte == 0x83 || rbyte == 0x1a || 0xffff == rchar)
      break;
    venusout(ai,V_SWS1,0);
    
  }while(dwelltime > 0);
  return -EIO;
}

/* 
 * Transfer 32k of firmware data from user buffer to our buffer and 
 * send to the card .
 */
int flashputbuf(struct venus_info *ai){

  /* Write stuff */

  memcpy(ai->auxregmembase + 0x8000 ,flashbuffer, 1024 * 32);
  venusout(ai,V_SWS0,0x8000);

  return 0;
}



/*
 *
 */
int flashrestart(struct venus_info *ai,struct net_device *dev){
  int    status=-1;

  mdelay(1024); 
  ai->flags &= ~(FLASHING);
  if((status = venus_init_descriptors(dev))==0){
    status = start_venus(dev);
    mdelay(1024);          
  }
  return status;
}

/* 
 * Emulate 2.4 PCI and mem calls
 */
#if LINUX_VERSION_CODE < VERSION_CODE(2,2,19)
static unsigned rsizes[6] = {
  256,
  16384,
  4194304
};
#endif

#if LINUX_VERSION_CODE < VERSION_CODE(2,3,55)
static void *pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
			   dma_addr_t *dma_handle)
{
	void *ret;
	int gfp = GFP_ATOMIC;

	ret = (void *)__get_free_pages(gfp, get_order(size));

	if (ret != NULL) {
	  memset(ret, 0, size);		
	  *dma_handle = virt_to_bus(ret);
	}
	return ret;
}

static void pci_free_consistent(struct pci_dev *hwdev, size_t size,
			 void *vaddr, dma_addr_t dma_handle)
{
	free_pages((unsigned long)vaddr, get_order(size));
}
#endif

#if LINUX_VERSION_CODE < VERSION_CODE(2,3,55)

static unsigned long pci_resource_start(struct pci_dev *pcidev,int regno){
  unsigned long regptr;
  unsigned evmsk = 0xfffe;

  if((regptr = pcidev->base_address[regno])){
    if(regno == 0)
      regptr &= evmsk; /* Only even adresses here */
    return regptr;
  }
  else
    return 0;
}
#endif

#if LINUX_VERSION_CODE < VERSION_CODE(2,2,18)
static unsigned long pci_resource_len(struct pci_dev *pcdev,int regno){
  if(regno < 3 )
    return rsizes[regno];
  else
    return 0;
}
#endif

#if LINUX_VERSION_CODE < VERSION_CODE(2,3,55)
static int pci_enable_device(struct pci_dev *dev){
  //  pci_set_master(dev);
  return 0;
}
#endif

/***********************************************************************
 *                              MIC ROUTINES                           *
 ***********************************************************************
 */

/*
 * @micinit
 * Initialize mic seed 
 */

static void micinit(struct venus_info *vinfo, STMIC *key)
{
  int micEnabled=0;

  if (NULL == key) {
    return;
  }
#ifdef DEBUG_MIC
  printk(KERN_INFO "MiC stat=%04x mcastvalid=%d unicastvalid=%d\n",
	 key->micState,
	 key->micMulticastValid,
	 key-> micUnicastValid);
#endif
  micEnabled = key->micState & 0x00FF;

  if(micEnabled){
    vinfo->flags |= MIC_ENABLED;
    vinfo->micstats.Enabled = 1;
  }
  else
    {
      vinfo->flags &= ~(MIC_ENABLED);
      vinfo->micstats.Enabled = 0;
    }

  if (micEnabled) {
    //Key must be valid
    if (key->micMulticastValid) {
      //Key must be different
      if (!vinfo->mod[0].mCtx.valid || (memcmp(vinfo->mod[0].mCtx.key,
		  key->micMulticast,sizeof(vinfo->mod[0].mCtx.key))!=0)) {
#ifdef DEBUG_MIC     
	printk(KERN_INFO "Updating multicast Key\n");
#endif
	
	//Age current mic Context
	memcpy(&vinfo->mod[1].mCtx,&vinfo->mod[0].mCtx,sizeof(MICCNTX));
	  
	//Initialize new context
	  
	memcpy(&vinfo->mod[0].mCtx.key,key->micMulticast,sizeof(key->micMulticast));
	vinfo->mod[0].mCtx.window  = 33;       //Window always points to the middle
	vinfo->mod[0].mCtx.rx      = 0;        //Rx Sequence numbers
	vinfo->mod[0].mCtx.tx      = 0;        //Tx sequence numbers
	vinfo->mod[0].mCtx.valid   = 1;
	  
	//Give key to mic seed
	emmh32_setseed(&vinfo->mod[0].mCtx.seed,key->micMulticast,sizeof(key->micMulticast));
      }
    }

    //Key must be valid 
    if (key->micUnicastValid) {
      //Key must be different
      if (!vinfo->mod[0].uCtx.valid || 
	  (memcmp(vinfo->mod[0].uCtx.key,
		  key->micUnicast,
		  sizeof(vinfo->mod[0].uCtx.key)) != 0)) {
#ifdef DEBUG_MIC     
	printk(KERN_INFO "Updating unicast Key\n");
#endif
	
	//Age current mic Context
	memcpy(&vinfo->mod[1].uCtx, &vinfo->mod[0].uCtx,sizeof(MICCNTX));
	
	//Initialize new context
	memcpy(&vinfo->mod[0].uCtx.key,key->micUnicast,sizeof(key->micUnicast));
	
	vinfo->mod[0].uCtx.window  = 33;       //Window always points to the middle
	vinfo->mod[0].uCtx.rx      = 0;        //Rx Sequence numbers
	vinfo->mod[0].uCtx.tx      = 0;        //Tx sequence numbers
	vinfo->mod[0].uCtx.valid   = 1;     //Key is now valid
	
	//Give key to mic seed
	emmh32_setseed(&vinfo->mod[0].uCtx.seed, key->micUnicast, sizeof(key->micUnicast));
      }
    }
  }
  else 
    {
      //So next time we have a valid key and mic is enabled, we will update
      //the sequence number if the key is the same as before.
      vinfo->mod[0].uCtx.valid = 0;
      vinfo->mod[0].mCtx.valid = 0;
    }
#ifdef DEBUG_MIC     
	printk(KERN_INFO "Device flags=%x\n",vinfo->flags);
#endif
}

/*
 * @micsetup 
 * Get ready for business
 */

static void micsetup(struct venus_info *v_info){
  int x;
  u8 tmp[]= {0xAA,0xAA,0x03,0x00,0x40,0x96,0x00,0x02};
#ifdef DEBUG_MIC
    printk(KERN_INFO "micsetup \n");
#endif

  memcpy(&v_info->snap,tmp,sizeof(v_info->snap));

  for (x=0; x < NUM_MODULES; x++) {
    memset(&v_info->mod[x].mCtx,0,sizeof(MICCNTX));
    memset(&v_info->mod[x].uCtx,0,sizeof(MICCNTX));
    //Mark multicast context as multicast.
    v_info->mod[x].mCtx.multicast=TRUE;
  }
}


 

/*===========================================================================
 * Description: Mic a packet
 *    
 *      Inputs: ETH_HEADER_STRUC * pointer to an 802.3 frame
 *    
 *     Returns: BOOLEAN if successful, otherwise false.
 *             PacketTxLen will be updated with the mic'd packets size.
 *
 *    Caveats: It is assumed that the frame buffer will already
 *             be big enough to hold the largets mic message possible.
 *            (No memory allocationis done here).
 *  
 *    Author: sbraneky (10/15/01)
 *    Merciless hacks by rwilcher (1/14/02)
 */

static int Encapsulate(struct venus_info *v_info
		       ,ETH_HEADER_STRUC *frame, u32 *PacketTxLen,int adhoc)
{
  MICCNTX   *context;
  int       micEnabled;
  u32       tmp;
  u16       len;
  u16       payLen;
  MIC_BUFFER_STRUCT *mic;

  micEnabled = (v_info->flags & MIC_ENABLED) ? 1:0;

  if (micEnabled && (frame->TypeLength != 0x8E88)) {
    mic = (MIC_BUFFER_STRUCT*)frame;
    len = (u16 )*PacketTxLen;             //Original Packet len
    payLen = (u16)((*PacketTxLen) - 12);  //skip DA, SA

    //Determine correct context

    if (adhoc) {
      if (mic->DA[0] & 0x1) {
	//Broadcast message
	context =  &v_info->mod[0].mCtx;
      }
      else {
	context =  &v_info->mod[0].uCtx;
      }
    }
    else {
      //If not adhoc, always use unicast key
      context =  &v_info->mod[0].uCtx;
    }
    

    if (!context->valid) {
#ifdef DEBUG_MIC
      printk( KERN_INFO "[ENCAP] Context is not valid, not encapsulating\n");
#endif
      return FALSE;
    }

    *PacketTxLen = len +18;                             //Add mic bytes to packet length
    
    //Move payload into new position
    memmove(&mic->payload,&frame->TypeLength,payLen);
    
    //Add Snap
    memcpy(&mic->u.snap,v_info->snap,sizeof(v_info->snap));
    //Add Tx sequence
    tmp = context->tx ;
    UlongByteSwap(&tmp);                    //Convert big/little endian
    memcpy(&mic->SEQ,&tmp,4);
    context->tx += 2;

    //Mic the packet
    emmh32_init(&context->seed);
    
    mic->TypeLength = payLen+16;            //Length of Mic'd packet
    UshortByteSwap(&mic->TypeLength);      //Put in network byte order
    
    emmh32_update(&context->seed, mic->DA,22);  //DA,SA Type/Length, and Snap
    emmh32_update(&context->seed, (u8 *)&mic->SEQ, sizeof(mic->SEQ)+ payLen); //SEQ + payload
    emmh32_final(&context->seed,  (u8 *)&mic->MIC);

    /*    New Type/length ?????????? */
     mic->TypeLength = 0x0000;           //Let NIC know it could be an oversized packet
  }
  return TRUE;
}

/*===========================================================================
 *  Description: Decapsulates a MIC'd packet and returns the 802.3 packet
 *               (removes the MIC stuff) if packet is a valid packet.
 *      
 *       Inputs: ETH_HEADER_STRUC  pointer to the 802.3 packet             
 *     
 *      Returns: BOOLEAN - TRUE if packet should be dropped otherwise FALSE
 *     
 *      Author: sbraneky (10/15/01)
 *    Merciless hacks by rwilcher (1/14/02)
 *---------------------------------------------------------------------------
 */

static int Decapsulate(struct venus_info *v_info,ETH_HEADER_STRUC *ptr, u32 *PacketRxLen)
{
  int      micEnabled,micPacket,x;
  int      valid = TRUE;
  u32      micSEQ,seq;
  u16      payLen;
  MICCNTX  *context;
  MIC_BUFFER_STRUCT *mic ;
  u8       digest[4],*miccp;
  MIC_ERROR micError = NONE;
  struct   net_device *devp;

  mic = (MIC_BUFFER_STRUCT*)ptr ;
  micEnabled = (v_info->flags & MIC_ENABLED) ? 1:0;

  devp = v_info->dev;

  if (mic->TypeLength <= 0x5DC || mic->TypeLength != 0x8E88) {
      valid = FALSE;                             //Assume failure
      //Check if the packet is a Mic'd packet

      micPacket = (0 == memcmp(mic->u.snap,v_info->snap,sizeof(v_info->snap)));

      if (!micPacket && !micEnabled ) {
	return FALSE; /* MIC is off here. */
      }
      
      if (micPacket &&  !micEnabled) {
	//No Mic set or Mic OFF but we received a MIC'd packet.
#ifdef DEBUG_MIC
	printk(KERN_INFO "[DEMIC] Mic'd packet received but mic not enabled\n");
#endif
	v_info->micstats.RxMICPlummed++;
	return TRUE;
      }
      
      if (!micPacket && micEnabled) {
	//Mic enabled but packet isn't Mic'd
#ifdef DEBUG_MIC
	printk(KERN_INFO "[DEMIC] Non mic'd packet received when mic enabled\n");
#endif
	v_info->micstats.RxMICPlummed++;
    	return TRUE;
      }

      micSEQ = mic->SEQ;            //store SEQ as little endian
      UlongByteSwap(&micSEQ);       //Convert to little/big endian

      //At this point we a have a mic'd packet and mic is enabled
      //Now do the mic error checking.

      if ( 0 == ( 1 & micSEQ)) {
	//Receive seq must be odd
#ifdef DEBUG_MIC
	printk(KERN_INFO "[DEMIC] Even seqence number received %x\n",micSEQ);
#endif
	v_info->micstats.RxWrongSequence++;
//	return 1;
      }

      payLen    = (u16) ((*PacketRxLen) - 30);   //Payload length
      seq       = 0;                               //Sequence # relative to window

      
      for (x=0; !valid && x < NUM_MODULES; x++) {
	//Determine proper context 
	context = (mic->DA[0] & 0x1) ? &v_info->mod[x].mCtx : &v_info->mod[x].uCtx;

       	//Make sure context is valid
	if (!context->valid) {
#ifdef DEBUG_MIC
	  printk(KERN_INFO "[DEMIC] Context is not valid, not decapsulating\n");
#endif
	  micError = NOMICPLUMMED;
	  continue;                
	}
       	//DeMic it 
	if (0 == mic->TypeLength) {
	  mic->TypeLength = (*PacketRxLen - sizeof(ETH_HEADER_STRUC));
	  UshortByteSwap(&mic->TypeLength);    //Convert to Network byte order
	}
	emmh32_init(&context->seed);
	emmh32_update(&context->seed, mic->DA,
		      sizeof(mic->DA)+sizeof(mic->SA)+sizeof(mic->TypeLength)+sizeof(mic->u.snap)); 
	emmh32_update(&context->seed, (u8 *) &mic->SEQ,sizeof(mic->SEQ)+payLen);	
	emmh32_final(&context->seed, digest); 	//Calculate MIC

	if (memcmp(digest,&mic->MIC,4 )) { 	//Make sure the mics match
	  //Invalid Mic
#ifdef DEBUG_MIC
	  printk(KERN_INFO "[DEMIC] Invalid Mic'd message received %x%x%x%x - %08x\n",
		 digest[0],digest[1],digest[2],digest[3], mic->MIC);
#endif	  
	  if (0 == x) {
	    micError = INCORRECTMIC;
	    v_info->micstats.RxIncorrectMIC++;
	  }
	  continue;
	}
	else
	  {
	    miccp = (u8 *)&mic->MIC;
	  }
	//Check Sequence number if mics pass
	if (FALSE == RxSeqValid(v_info,context,micSEQ)) {
#ifdef DEBUG_MIC
	  printk(KERN_INFO "[DEMIC] Invalid sequence number\n");
#endif 	  
	  if (0 == x) {
	    micError = SEQUENCE;
	  }
	  continue;
	}

	//Mic and SEQ match
	valid = TRUE;
	
	//Remove  0 eType, snap, MIC, and SEQ
	memmove(&mic->TypeLength,&mic->payload,payLen);

	//Update Packet length
	*PacketRxLen  -= 18;
	
	if( *PacketRxLen < 60 ) {
	  //Promote to minimum length
	  *PacketRxLen = 60;
	}

	//Update statistics
        switch (micError) 
	  {
            case NONE:
                v_info->micstats.RxSuccess++;
            break;
            case NOMIC:
                v_info->micstats.RxNotMICed++;
            break;
            case NOMICPLUMMED:
                v_info->micstats.RxMICPlummed++;
            break;
            case SEQUENCE:
                v_info->micstats.RxWrongSequence++;
            break;
            case INCORRECTMIC:
                v_info->micstats.RxIncorrectMIC++;
            break;
	  }
      }      
  }
  return (valid==1 ? 0 : 1);
}


/*===========================================================================
 * Description:  Checks the Rx Seq number to make sure it is valid
 *               and hasn't already been received
 *   
 *     Inputs: MICCNTX - mic context to check seq against
 *             micSeq  - the Mic seq number
 *   
 *    Returns: TRUE if valid otherwise FALSE. 
 *
 *    Author: sbraneky (10/15/01)
 *    Merciless hacks by rwilcher (1/14/02)
 *---------------------------------------------------------------------------
 */

static int RxSeqValid (struct venus_info *v_info,MICCNTX *context, u32 micSeq)
{
    u32 seq,index ;

    #if DEBUG_SEQ
        printk(KERN_INFO "Mic Seq %d,window %d, rx=%x\n",micSeq,context->window,context->rx);
    #endif

    //Allow for the ap being rebooted - if it is then use the next 
    //sequence number of the current sequence number - might go backwards

    if (v_info->updateMultiSeq && context->multicast) {
        v_info->updateMultiSeq  = FALSE;
        //Move window
        context->window = ((micSeq > 33) ? micSeq : 33);
        context->rx     = 0;            //Reset rx
        #if DEBUG_SEQ
            printk( KERN_INFO "Updating mulicast context window=%x\n",context->window);
        #endif

    }
    else {
        if (v_info->updateUniSeq && !context->multicast) {
            v_info->updateUniSeq    = FALSE;
            context->window = ((micSeq > 33) ? micSeq : 33);
            context->rx     = 0;        //Reset rx
            #if DEBUG_SEQ
                printk( KERN_INFO "Updating unicast context window=%x\n",context->window);
            #endif
        }
    }

    //Make sequence number relative to START of window
    seq = micSeq - (context->window-33);

    if ((u32)seq < 0) {
        //Too old of a SEQ number to check.
        #if DEBUG_SEQ
            printk( KERN_INFO "Seq number is too old to check %x.\n",seq);
        #endif
        return FALSE;
    }
    
    if ( seq > 64 )  {
        #if DEBUG_SEQ
            printk( KERN_INFO "Seq number is greater than window %x\n",seq);
        #endif
        //Window is infinite forward
        MoveWindow(context,micSeq);
        return TRUE;
    }

    //We are in the window. Now check the context rx bit to see if it was already sent
    seq >>= 1;                      //divide by 2 because we only have odd numbers
    index = (0x01 << seq);    //Get an index number

    #if DEBUG_SEQ
    printk( KERN_INFO "Relative seq = %x, index = %x rx =%x\n",seq,index,context->rx);
    #endif

    if (0 == (context->rx & index)) {
        //micSEQ falls inside the window.
        //Add seqence number to the list of received numbers.
        context->rx |= index;

        MoveWindow(context,micSeq);

        return TRUE;
    }

    #if DEBUG_SEQ
        printk( KERN_INFO "Mic Sequnce number already received window=%x rx=%x seq= %x index=%x \n",context->window,context->rx,seq,index);
    #endif

    return FALSE;
}

static void MoveWindow(MICCNTX *context, u32 micSeq)
{
  u32 shift;

  //Move window if seq greater than the middle of the window
  if (micSeq > context->window) {
    shift = micSeq - context->window;    //Move relative to middle of window
    shift >>= 1;                         //divide by 2
    
    //Shift out old
    if (shift < 32) {
      context->rx >>= shift;
    }
    else {
      //Are we going to shift everything out
      context->rx = 0;
    }

    context->window = micSeq;      //Move window
#if DEBUG_SEQ
    printk( KERN_INFO "Moved window window=%x, rx=%x\n",context->window,context->rx);
#endif
  }
}

static void UshortByteSwap(u16 *source)
{
    u16 tmp = *source;    
    u8 *ptr = (u8*)source;
    
    ptr[0] = (u8)(tmp >> 8) & 0x00FF;
    ptr[1] = (u8)(tmp  & 0x00FF);
}

static void UlongByteSwap(u32 *source)
{
    u32 tmp = *source;    
    u8 *ptr = (u8*)source;
    
    ptr[0] = (u8)(tmp >> 24) & 0x00FF;
    ptr[1] = (u8)(tmp >> 16) & 0x00FF;
    ptr[2] = (u8)(tmp >>  8) & 0x00FF;
    ptr[3] = (u8) tmp        & 0x00FF;
}

#define GENBC
#undef  GENBC

/*==========================================================================================*/
/*========== ENDIANESS =====================================================================*/
/*==========================================================================================*/
/* macros for dealing with endianess */
#define SWAPU32(d)				( ((d)<<24) | ( ((d)&0xFF00)<<8) | \
						  (((d)>>8)&0xFF00) | ((d)>>24) )
//#if 0

#if defined(BIG_ENDIAN) && defined(LITTLE_ENDIAN)
#error ENDIAN -- both BIG_ENDIAN and LITTLE_ENDIAN are defined
#elif defined(BIG_ENDIAN)
#define BIGEND32(d)				(d)
#elif defined(LITTLE_ENDIAN)
#define BIGEND32(d)				SWAPU32(d)
#else
/* fine, make a runtime decision, not as efficient.... */
static unsigned short endian_ref = { 0x1234 };
#define ISBIGENDIAN					( (*(u8*)(&endian_ref)) == 0x12 )
#define BIGEND32(d)				( ISBIGENDIAN ? (d) : SWAPU32(d))
#endif

//#endif

#if 0
static unsigned short endian_ref = { 0x1234 }; /* @RAW */
#endif

#ifndef UNALIGN32
#error UNALIGN32 must be defined.
#elif UNALIGN32
/* unaligned accesses are allowed -- fetch u32 and swap endian */
#define	GETBIG32(p)				BIGEND32(*(u32*)(p))
#else
/* unaligned accesses are disallowed ... slow GET32() */
#define GB(p,i,s)				( ((u32) *((u8*)(p)+i) ) << (s) )
#define GETBIG32(p)				GB(p,0,24)|GB(p,1,16)|GB(p,2,8)|GB(p,3,0)
#endif


/*==========================================================================================*/
/*========== EMMH ROUTINES  ================================================================*/
/*==========================================================================================*/

/* mic accumulate */
#define MIC_ACCUM(v)	\
	context->accum += (u64)val * context->coeff[coeff_position++];

#ifdef GENBC
/* use unix BC for verification since BC supports arbitrary length integers */
FILE *fpbc = stdout;
#undef  MIC_ACCUM
#define MIC_ACCUM(v)	\
	fprintf(fpbc, "bc:(.+%8lX*%8lX)%%10000000000000000\n", v, context->coeff[coeff_position]); \
	context->accum += (u64)val * context->coeff[coeff_position++]; \
	printf("%08lX:%08lX\n", (u32)(context->accum >> 32), (u32)(context->accum));
#endif

static aes cx;
static unsigned char aes_counter[16];
static unsigned char aes_cipher[16];

/* expand the key to fill the MMH coefficient array */
void emmh32_setseed(emmh32_context *context, u8 *pkey, int keylen)
{
  /* take the keying material, expand if necessary, truncate at 16-bytes */
  /* run through AES counter mode to generate context->coeff[] */
  
  int i,j;
  u32 val;
  u32 counter;
#if 0
  if (keylen != 16) { /* @RAW Cant touch this */
    fprintf(stderr, "ERROR: key length not 16\n");
    exit(1);
  }
#endif 

#if 0
dumpbytes("micseed = ", pkey, 16);
#endif

 set_key(pkey, 16, enc, &cx);
 counter = 0;
 for (i=0; i< (sizeof(context->coeff)/sizeof(context->coeff[0])); ) {
   aes_counter[15] = (u8)(counter >> 0);
   aes_counter[14] = (u8)(counter >> 8);
   aes_counter[13] = (u8)(counter >> 16);
   aes_counter[12] = (u8)(counter >> 24);
   counter++;
   encrypt(&aes_counter[0], &aes_cipher[0], &cx);
   for (j=0; (j<sizeof(aes_cipher)) && (i< (sizeof(context->coeff)/sizeof(context->coeff[0]))); ) {
     val = GETBIG32(&aes_cipher[j]);
     context->coeff[i++] = val;
     j += 4;
   }
 }
#if 0
 dumplong("coeff = ", &context->coeff[0], 8);
#endif
}

/* prepare for calculation of a new mic */
void emmh32_init(emmh32_context *context)
{
	/* prepare for new mic calculation */
  context->accum = 0;
  context->position = 0;
#ifdef GENBC
  fprintf(fpbc, "bc:ibase=10\n");
  fprintf(fpbc, "bc:ibase=A\n");
  fprintf(fpbc, "bc:obase=16\n");
  fprintf(fpbc, "bc:ibase=16\n");
  fprintf(fpbc, "bc:0\n");
#endif
}

/* add some bytes to the mic calculation */
void emmh32_update(emmh32_context *context, u8 *pOctets, int len)
{
  int	coeff_position, byte_position;
  u32	val;
  
  if (len == 0) return;
  
  coeff_position = context->position >> 2;
  
  /* deal with partial 32-bit word left over from last update */
  if ( (byte_position = (context->position & 3)) != 0) {
    /* have a partial word in part to deal with */
    do {
      if (len == 0) return;
      context->part.d8[byte_position++] = *pOctets++;
      context->position++;
      len--;
    } while (byte_position < 4);
    val = context->part.d32;
    val = BIGEND32(val);
    MIC_ACCUM(val);
  }

  /* deal with full 32-bit words */
  while (len >= 4) {
    val = GETBIG32(pOctets);
    MIC_ACCUM(val);
    context->position += 4;
    pOctets += 4;
    len -= 4;
  }

  /* deal with partial 32-bit word that will be left over from this update */
  byte_position = 0;
  while (len > 0) {
    if (len == 0) return;
    context->part.d8[byte_position++] = *pOctets++;
    context->position++;
    len--;
  }
}

/* mask used to zero empty bytes for final parial word */
static u32 mask32[4] = { 0x00000000L, 0xFF000000L, 0xFFFF0000L, 0xFFFFFF00L };

/* calculate the mic */
void emmh32_final(emmh32_context *context, u8 digest[4])
{
  int	coeff_position, byte_position;
  u32	val;
  
  u64 sum, utmp;
  s64 stmp;

  coeff_position = context->position >> 2;
  
  /* deal with partial 32-bit word left over from last update */
  if ( (byte_position = (context->position & 3)) != 0) {
    /* have a partial word in part to deal with */
    val = context->part.d32;
    val = BIGEND32(val);		/* convert to big endian if required */
    val &= mask32[byte_position];	/* zero empty bytes */
    MIC_ACCUM(val);
  }

  /* reduce the accumulated u64 to a 32-bit MIC */
  sum = context->accum;
  stmp = (sum  & 0xffffffffLL) - ((sum >> 32)  * 15);
  utmp = (stmp & 0xffffffffLL) - ((stmp >> 32) * 15);
  sum = utmp & 0xffffffffLL;
  if (utmp > 0x10000000fLL)
    sum -= 15;
  
  val = (u32)sum;
  digest[0] = (val>>24) & 0xFF;
  digest[1] = (val>>16) & 0xFF;
  digest[2] = (val>>8) & 0xFF;
  digest[3] = (val>>0) & 0xFF;
#ifdef GENBC
  fprintf(fpbc, "bc:.%%10000000000000000\n");
  fprintf(fpbc, "bc:.%%10000000F\n");
  fprintf(fpbc, "bc:\"result = \"\n");
  fprintf(fpbc, "bc:.\n");
#endif
}


 /*
   -----------------------------------------------------------------------
   Copyright (c) 2001 Dr Brian Gladman <brg@gladman.uk.net>, Worcester, UK
   
   TERMS

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   This software is provided 'as is' with no guarantees of correctness or
   fitness for purpose.
   -----------------------------------------------------------------------

   1. FUNCTION
 
   The AES algorithm Rijndael implemented for block and key sizes of 128,
   192 and 256 bits (16, 24 and 32 bytes) by Brian Gladman.

   This is an implementation of the AES encryption algorithm (Rijndael)
   designed by Joan Daemen and Vincent Rijmen. This version is designed
   to provide both fixed and dynamic block and key lengths and can also 
   run with either big or little endian internal byte order (see aes.h). 
   It inputs block and key lengths in bytes with the legal values being 
   16, 24 and 32.
 
   2. CONFIGURATION OPTIONS (see also aes.h)
 
   a.  Define UNROLL for full loop unrolling in encryption and decryption.
   b.  Define PARTIAL_UNROLL to unroll two loops in encryption and decryption.
   c.  Define FIXED_TABLES for compiled rather than dynamic tables.
   d.  Define FF_TABLES to use tables for field multiplies and inverses.
   e.  Define ARRAYS to use arrays to hold the local state block. If this
       is not defined, individually declared 32-bit words are used.
   f.  Define FAST_VARIABLE if a high speed variable block implementation
       is needed (essentially three separate fixed block size code sequences)
   g.  Define either ONE_TABLE or FOUR_TABLES for a fast table driven 
       version using 1 table (2 kbytes of table space) or 4 tables (8
       kbytes of table space) for higher speed.
   h.  Define either ONE_LR_TABLE or FOUR_LR_TABLES for a further speed 
       increase by using tables for the last rounds but with more table
       space (2 or 8 kbytes extra).
   i.  If neither ONE_TABLE nor FOUR_TABLES is defined, a compact but 
       slower version is provided.
   j.  If fast decryption key scheduling is needed define ONE_IM_TABLE
       or FOUR_IM_TABLES for higher speed (2 or 8 kbytes extra).

   3. USE OF DEFINES
  
   NOTE: some combinations of the following defines are disabled below.

   UNROLL or PARTIAL_UNROLL control the extent to which loops are unrolled
   in the main encryption and decryption routines. UNROLL does a complete
   unroll while PARTIAL_UNROLL uses a loop with two rounds in it.
 
#define UNROLL
#define PARTIAL_UNROLL
 
   If FIXED_TABLES is defined, the tables are comipled statically into the 
   code, otherwise they are computed once when the code is first used.
 
#define FIXED_TABLES
 
   If FF_TABLES is defined faster finite field arithmetic is performed by 
   using tables.
 
#define FF_TABLES

   If ARRAYS is defined the state variables for encryption are defined as
   arrays, otherwise they are defined as individual variables. The latter
   is useful on machines where these variables can be mapped to registers. 
 
#define ARRAYS

   If FAST_VARIABLE is defined with variable block length, faster but larger
   code is used for encryption and decryption.

#define FAST_VARIABLE
 */

#define UNROLL
#define FIXED_TABLES
#define FF_TABLES
#undef ARRAYS
#define FAST_VARIABLE

 /*
   This code uses three sets of tables, each of which can be a single table
   or four sub-tables to gain a further speed advantage.

   The defines ONE_TABLE and FOUR_TABLES control the use of tables in the 
   main encryption rounds and have the greatest impact on speed.  If neither
   is defined, tables are not used and the resulting code is then very slow.
   Defining ONE_TABLE gives a substantial speed increase using 2 kbytes of 
   table space; FOUR_TABLES gives a further speed increase but uses 8 kbytes
   of table space.
   
#define ONE_TABLE
#define FOUR_TABLES

   The defines ONE_LR_TABLE and FOUR_LR_TABLES apply to the last round only
   and their impact on speed is hence less. It is unlikely to be sensible to
   apply these options unless the correspnding option above is also used.    

#define ONE_LR_TABLE
#define FOUR_LR_TABLES

   The ONE_IM_TABLE and FOUR_IM_TABLES options use tables to speed up the 
   generation of the decryption key schedule. This will only be useful in
   limited situations where decryption speed with frequent re-keying is
   needed.

#define ONE_IM_TABLE
#define FOUR_IM_TABLES
 */

#define FOUR_TABLES
#define FOUR_LR_TABLES
#define FOUR_IM_TABLES

 /*
   In this implementation the columns of the state array are each held in
   32-bit words. The state array can be held in various ways: in an array
   of words, in a number of individual word variables or in a number of 
   processor registers. The following define maps a variable name x and
   a column number c to the way the state array variable is to be held.
 */

#if defined(ARRAYS)
#define s(x,c) x[c]
#else
#define s(x,c) x##c
#endif

#if defined(MMHBLOCK_SIZE) && (MMHBLOCK_SIZE == 20 || MMHBLOCK_SIZE == 28)
#error an illegal block size has been specified
#endif  

#if defined(UNROLL) && defined (PARTIAL_UNROLL)
#error both UNROLL and PARTIAL_UNROLL are defined
#endif

#if defined(ONE_TABLE) && defined (FOUR_TABLES)
#error both ONE_TABLE and FOUR_TABLES are defined
#endif

#if defined(ONE_LR_TABLE) && defined (FOUR_LR_TABLES)
#error both ONE_LR_TABLE and FOUR_LR_TABLES are defined
#endif

#if defined(ONE_IM_TABLE) && defined (FOUR_IM_TABLES)
#error both ONE_IM_TABLE and FOUR_IM_TABLES are defined
#endif

/* End of configuration options */

#include "aes.h"

/* Disable at least some poor combinations of options */

#if !defined(ONE_TABLE) && !defined(FOUR_TABLES)
#define FIXED_TABLES
#undef  UNROLL
#undef  ONE_LR_TABLE
#undef  FOUR_LR_TABLES
#undef  ONE_IM_TABLE
#undef  FOUR_IM_TABLES
#elif !defined(FOUR_TABLES)
#ifdef  FOUR_LR_TABLES
#undef  FOUR_LR_TABLES
#define ONE_LR_TABLE
#endif
#ifdef  FOUR_IM_TABLES
#undef  FOUR_IM_TABLES
#define ONE_IM_TABLE
#endif
#elif !defined(MMHBLOCK_SIZE)
#if defined(UNROLL)
#define PARTIAL_UNROLL
#undef UNROLL
#endif
#endif

/* the finite field modular polynomial and elements */

#define ff_poly 0x011b
#define ff_hi   0x80

/* multiply four bytes in GF(2^8) by 'x' {02} in parallel */

#define m1  0x80808080
#define m2  0x7f7f7f7f
#define m3  0x0000001b
#define FFmulX(x)  ((((x) & m2) << 1) ^ ((((x) & m1) >> 7) * m3))

 /* 
   The following defines provide alternative definitions of FFmulX that might
   give improved performance if a fast 32-bit multiply is not available. Note
   that a temporary variable u needs to be defined where FFmulX is used.

#define FFmulX(x) (u = (x) & m1, u |= (u >> 1), ((x) & m2) << 1) ^ ((u >> 3) | (u >> 6)) 
#define m4  0x1b1b1b1b
#define FFmulX(x) (u = (x) & m1, ((x) & m2) << 1) ^ ((u - (u >> 7)) & m4) 

 */

/* perform column mix operation on four bytes in parallel */

#define fwd_mcol(x) (f2 = FFmulX(x), f2 ^ upr(x ^ f2,3) ^ upr(x,2) ^ upr(x,1))

#if defined(FIXED_TABLES)

#include "aestab.h"

#else

static byte  s_box[256];
static byte  inv_s_box[256];
static word  rcon_tab[RC_LENGTH];

#if defined(ONE_TABLE)
static word  ft_tab[256];
static word  it_tab[256];
#elif defined(FOUR_TABLES)
static word  ft_tab[4][256];
static word  it_tab[4][256];
#endif

#if defined(ONE_LR_TABLE)
static word  fl_tab[256];
static word  il_tab[256];
#elif defined(FOUR_LR_TABLES)
static word  fl_tab[4][256];
static word  il_tab[4][256];
#endif

#if defined(ONE_IM_TABLE)
static word  im_tab[256];
#elif defined(FOUR_IM_TABLES)
static word  im_tab[4][256];
#endif

#if !defined(FF_TABLES)

/*
   Generate the tables for the dynamic table option

   It will generally be sensible to use tables to compute finite 
   field multiplies and inverses but where memory is scarse this 
   code might sometimes be better.

   return 2 ^ (n - 1) where n is the bit number of the highest bit
   set in x with x in the range 1 < x < 0x00000200.   This form is
   used so that locals within FFinv can be bytes rather than words
*/

static byte hibit(const word x)
{   byte r = (byte)((x >> 1) | (x >> 2));
    
    r |= (r >> 2);
    r |= (r >> 4);
    return (r + 1) >> 1;
}

/* return the inverse of the finite field element x */

static byte FFinv(const byte x)
{   byte    p1 = x, p2 = 0x1b, n1 = hibit(x), n2 = 0x80, v1 = 1, v2 = 0;

    if(x < 2) return x;

    for(;;)
    {
        if(!n1) return v1;

        while(n2 >= n1)
        {   
            n2 /= n1; p2 ^= p1 * n2; v2 ^= v1 * n2; n2 = hibit(p2);
        }
        
        if(!n2) return v2;

        while(n1 >= n2)
        {   
            n1 /= n2; p1 ^= p2 * n1; v1 ^= v2 * n1; n1 = hibit(p1);
        }
    }
}

/* define the finite field multiplies required for Rijndael */

#define FFmul02(x)  ((((x) & 0x7f) << 1) ^ ((x) & 0x80 ? 0x1b : 0))
#define FFmul03(x)  ((x) ^ FFmul02(x))
#define FFmul09(x)  ((x) ^ FFmul02(FFmul02(FFmul02(x))))
#define FFmul0b(x)  ((x) ^ FFmul02((x) ^ FFmul02(FFmul02(x))))
#define FFmul0d(x)  ((x) ^ FFmul02(FFmul02((x) ^ FFmul02(x))))
#define FFmul0e(x)  FFmul02((x) ^ FFmul02((x) ^ FFmul02(x)))

#else

#define FFinv(x)    ((x) ? pow[255 - log[x]]: 0)

#define FFmul02(x) (x ? pow[log[x] + 0x19] : 0)
#define FFmul03(x) (x ? pow[log[x] + 0x01] : 0)
#define FFmul09(x) (x ? pow[log[x] + 0xc7] : 0)
#define FFmul0b(x) (x ? pow[log[x] + 0x68] : 0)
#define FFmul0d(x) (x ? pow[log[x] + 0xee] : 0)
#define FFmul0e(x) (x ? pow[log[x] + 0xdf] : 0)

#endif

/* The forward and inverse affine transformations used in the S-box */

#define fwd_affine(x) \
    (w = (word)x, w ^= (w<<1)^(w<<2)^(w<<3)^(w<<4), 0x63^(byte)(w^(w>>8)))

#define inv_affine(x) \
    (w = (word)x, w = (w<<1)^(w<<3)^(w<<6), 0x05^(byte)(w^(w>>8)))

static void gen_tabs(void)
{   word  i, w;

#if defined(FF_TABLES)

    byte  pow[512], log[256];

    /*
       log and power tables for GF(2^8) finite field with
       0x011b as modular polynomial - the simplest primitive
       root is 0x03, used here to generate the tables
    */

    i = 0; w = 1; 
    do
    {   
        pow[i] = (byte)w;
        pow[i + 255] = (byte)w;
        log[w] = (byte)i++;
        w ^=  (w << 1) ^ (w & ff_hi ? ff_poly : 0);
    }
    while (w != 1);

#endif

    for(i = 0, w = 1; i < RC_LENGTH; ++i)
    {
        rcon_tab[i] = bytes2word(w, 0, 0, 0);
        w = (w << 1) ^ (w & ff_hi ? ff_poly : 0);
    }

    for(i = 0; i < 256; ++i)
    {   byte    b;

        s_box[i] = b = fwd_affine(FFinv((byte)i));

        w = bytes2word(b, 0, 0, 0);
#if defined(ONE_LR_TABLE)
        fl_tab[i] = w;
#elif defined(FOUR_LR_TABLES)
        fl_tab[0][i] = w;
        fl_tab[1][i] = upr(w,1);
        fl_tab[2][i] = upr(w,2);
        fl_tab[3][i] = upr(w,3);
#endif
        w = bytes2word(FFmul02(b), b, b, FFmul03(b));
#if defined(ONE_TABLE)
        ft_tab[i] = w;
#elif defined(FOUR_TABLES)
        ft_tab[0][i] = w;
        ft_tab[1][i] = upr(w,1);
        ft_tab[2][i] = upr(w,2);
        ft_tab[3][i] = upr(w,3);
#endif
        inv_s_box[i] = b = FFinv(inv_affine((byte)i));

        w = bytes2word(b, 0, 0, 0);
#if defined(ONE_LR_TABLE)
        il_tab[i] = w;
#elif defined(FOUR_LR_TABLES)
        il_tab[0][i] = w;
        il_tab[1][i] = upr(w,1);
        il_tab[2][i] = upr(w,2);
        il_tab[3][i] = upr(w,3);
#endif
        w = bytes2word(FFmul0e(b), FFmul09(b), FFmul0d(b), FFmul0b(b));
#if defined(ONE_TABLE)
        it_tab[i] = w;
#elif defined(FOUR_TABLES)
        it_tab[0][i] = w;
        it_tab[1][i] = upr(w,1);
        it_tab[2][i] = upr(w,2);
        it_tab[3][i] = upr(w,3);
#endif
#if defined(ONE_IM_TABLE)
        im_tab[b] = w;
#elif defined(FOUR_IM_TABLES)
        im_tab[0][b] = w;
        im_tab[1][b] = upr(w,1);
        im_tab[2][b] = upr(w,2);
        im_tab[3][b] = upr(w,3);
#endif

    }
}

#endif

#define no_table(x,box,vf,rf,c) bytes2word( \
    box[bval(vf(x,0,c),rf(0,c))], \
    box[bval(vf(x,1,c),rf(1,c))], \
    box[bval(vf(x,2,c),rf(2,c))], \
    box[bval(vf(x,3,c),rf(3,c))])

#define one_table(x,op,tab,vf,rf,c) \
 (     tab[bval(vf(x,0,c),rf(0,c))] \
  ^ op(tab[bval(vf(x,1,c),rf(1,c))],1) \
  ^ op(tab[bval(vf(x,2,c),rf(2,c))],2) \
  ^ op(tab[bval(vf(x,3,c),rf(3,c))],3))

#define four_tables(x,tab,vf,rf,c) \
 (  tab[0][bval(vf(x,0,c),rf(0,c))] \
  ^ tab[1][bval(vf(x,1,c),rf(1,c))] \
  ^ tab[2][bval(vf(x,2,c),rf(2,c))] \
  ^ tab[3][bval(vf(x,3,c),rf(3,c))])

#define vf1(x,r,c)  (x)
#define rf1(r,c)    (r)
#define rf2(r,c)    ((r-c)&3)

#if defined(FOUR_LR_TABLES)
#define ls_box(x,c)     four_tables(x,fl_tab,vf1,rf2,c)
#elif defined(ONE_LR_TABLE)
#define ls_box(x,c)     one_table(x,upr,fl_tab,vf1,rf2,c)
#else
#define ls_box(x,c)     no_table(x,s_box,vf1,rf2,c)
#endif

#if defined(FOUR_IM_TABLES)
#define inv_mcol(x)     four_tables(x,im_tab,vf1,rf1,0)
#elif defined(ONE_IM_TABLE)
#define inv_mcol(x)     one_table(x,upr,im_tab,vf1,rf1,0)
#else
#define inv_mcol(x) \
    (f9 = (x),f2 = FFmulX(f9), f4 = FFmulX(f2), f8 = FFmulX(f4), f9 ^= f8, \
    f2 ^= f4 ^ f8 ^ upr(f2 ^ f9,3) ^ upr(f4 ^ f9,2) ^ upr(f9,1))
#endif

 /* 
   Subroutine to set the block size (if variable) in bytes, legal
   values being 16, 24 and 32.
 */

#if defined(MMHBLOCK_SIZE)
#define nc   (Ncol)
#else
#define nc   (cx->Ncol)

cf_dec c_name(set_blk)(const word n_bytes, c_name(aes) *cx)
{
#if !defined(FIXED_TABLES)
    if(!(cx->mode & 0x08)) { gen_tabs(); cx->mode = 0x08; }
#endif

    if((n_bytes & 7) || n_bytes < 16 || n_bytes > 32) 
    {     
        return (n_bytes ? cx->mode &= ~0x07, aes_bad : (aes_ret)(nc << 2));
    }

    cx->mode = cx->mode & ~0x07 | 0x04;
    nc = n_bytes >> 2;
    return aes_good;
}

#endif

 /*
   Initialise the key schedule from the user supplied key. The key
   length is now specified in bytes - 16, 24 or 32 as appropriate.
   This corresponds to bit lengths of 128, 192 and 256 bits, and
   to Nk values of 4, 6 and 8 respectively.
 */

#define mx(t,f) (*t++ = inv_mcol(*f),f++)
#define cp(t,f) *t++ = *f++

#if   MMHBLOCK_SIZE == 16
#define cpy(d,s)    cp(d,s); cp(d,s); cp(d,s); cp(d,s)
#define mix(d,s)    mx(d,s); mx(d,s); mx(d,s); mx(d,s)
#elif MMHBLOCK_SIZE == 24
#define cpy(d,s)    cp(d,s); cp(d,s); cp(d,s); cp(d,s); \
                    cp(d,s); cp(d,s)
#define mix(d,s)    mx(d,s); mx(d,s); mx(d,s); mx(d,s); \
                    mx(d,s); mx(d,s)
#elif MMHBLOCK_SIZE == 32
#define cpy(d,s)    cp(d,s); cp(d,s); cp(d,s); cp(d,s); \
                    cp(d,s); cp(d,s); cp(d,s); cp(d,s)
#define mix(d,s)    mx(d,s); mx(d,s); mx(d,s); mx(d,s); \
                    mx(d,s); mx(d,s); mx(d,s); mx(d,s)
#else

#define cpy(d,s) \
switch(nc) \
{   case 8: cp(d,s); cp(d,s); \
    case 6: cp(d,s); cp(d,s); \
    case 4: cp(d,s); cp(d,s); \
            cp(d,s); cp(d,s); \
}

#define mix(d,s) \
switch(nc) \
{   case 8: mx(d,s); mx(d,s); \
    case 6: mx(d,s); mx(d,s); \
    case 4: mx(d,s); mx(d,s); \
            mx(d,s); mx(d,s); \
}

#endif

cf_dec c_name(set_key)(const byte in_key[], const word n_bytes, const enum aes_key f, c_name(aes) *cx)
{   word    *kf, *kt, rci;

#if !defined(FIXED_TABLES)
    if(!(cx->mode & 0x08)) { gen_tabs(); cx->mode = 0x08; }
#endif

#if !defined(MMHBLOCK_SIZE)
    if(!(cx->mode & 0x04)) c_name(set_blk)(16, cx);
#endif
    if((n_bytes & 7) || n_bytes < 16 || n_bytes > 32 || (!(f & 1) && !(f & 2)) )
    {     
        return (n_bytes ? cx->mode &= ~0x03, aes_bad : (aes_ret)(cx->Nkey << 2));
    }
    cx->mode = (cx->mode & ~0x03) | ((byte)f & 0x03);
    cx->Nkey = n_bytes >> 2;
    cx->Nrnd = Nr(cx->Nkey, nc);

    cx->e_key[0] = word_in(in_key     );
    cx->e_key[1] = word_in(in_key +  4);
    cx->e_key[2] = word_in(in_key +  8);
    cx->e_key[3] = word_in(in_key + 12);

    kf = cx->e_key; 
    kt = kf + nc * (cx->Nrnd + 1) - cx->Nkey; 
    rci = 0;

    switch(cx->Nkey)
    {
    case 4: do
            {   kf[4] = kf[0] ^ ls_box(kf[3],3) ^ rcon_tab[rci++];
                kf[5] = kf[1] ^ kf[4];
                kf[6] = kf[2] ^ kf[5];
                kf[7] = kf[3] ^ kf[6];
                kf += 4;
            }
            while(kf < kt);
            break;

    case 6: cx->e_key[4] = word_in(in_key + 16);
            cx->e_key[5] = word_in(in_key + 20);
            do
            {   kf[ 6] = kf[0] ^ ls_box(kf[5],3) ^ rcon_tab[rci++];
                kf[ 7] = kf[1] ^ kf[ 6];
                kf[ 8] = kf[2] ^ kf[ 7];
                kf[ 9] = kf[3] ^ kf[ 8];
                kf[10] = kf[4] ^ kf[ 9];
                kf[11] = kf[5] ^ kf[10];
                kf += 6;
            }
            while(kf < kt);
            break;

    case 8: cx->e_key[4] = word_in(in_key + 16);
            cx->e_key[5] = word_in(in_key + 20);
            cx->e_key[6] = word_in(in_key + 24);
            cx->e_key[7] = word_in(in_key + 28);
            do
            {   kf[ 8] = kf[0] ^ ls_box(kf[7],3) ^ rcon_tab[rci++];
                kf[ 9] = kf[1] ^ kf[ 8];
                kf[10] = kf[2] ^ kf[ 9];
                kf[11] = kf[3] ^ kf[10];
                kf[12] = kf[4] ^ ls_box(kf[11],0);
                kf[13] = kf[5] ^ kf[12];
                kf[14] = kf[6] ^ kf[13];
                kf[15] = kf[7] ^ kf[14];
                kf += 8;
            }
            while (kf < kt);
            break;
    }

    if((cx->mode & 3) != enc)
    {   word    i;
        
        kt = cx->d_key + nc * cx->Nrnd;
        kf = cx->e_key;
        
        cpy(kt, kf); kt -= 2 * nc;

        for(i = 1; i < cx->Nrnd; ++i)
        { 
#if defined(ONE_TABLE) || defined(FOUR_TABLES)
#if !defined(ONE_IM_TABLE) && !defined(FOUR_IM_TABLES)
            word    f2, f4, f8, f9;
#endif
            mix(kt, kf);
#else
            cpy(kt, kf);
#endif
            kt -= 2 * nc;
        }
        
        cpy(kt, kf);
    }

    return aes_good;
}

 /*
   I am grateful to Frank Yellin for the following constructions
   which, given the column (c) of the output state variable, give
   the input state variables which are needed for each row (r) of 
   the state.

   For the fixed block size options, compilers should reduce these 
   two expressions to fixed variable references. But for variable 
   block size code conditional clauses will sometimes be returned.

   y = output word, x = input word, r = row, c = column for r = 0, 
   1, 2 and 3 = column accessed for row r.
 */

#define unused  77  /* Sunset Strip */

#define fwd_var(x,r,c) \
 ( r==0 ?           \
    ( c==0 ? s(x,0) \
    : c==1 ? s(x,1) \
    : c==2 ? s(x,2) \
    : c==3 ? s(x,3) \
    : c==4 ? s(x,4) \
    : c==5 ? s(x,5) \
    : c==6 ? s(x,6) \
    : s(x,7))       \
 : r==1 ?           \
    ( c==0 ? s(x,1) \
    : c==1 ? s(x,2) \
    : c==2 ? s(x,3) \
    : c==3 ? nc==4 ? s(x,0) : s(x,4) \
    : c==4 ? s(x,5) \
    : c==5 ? nc==8 ? s(x,6) : s(x,0) \
    : c==6 ? s(x,7) \
    : s(x,0))       \
 : r==2 ?           \
    ( c==0 ? nc==8 ? s(x,3) : s(x,2) \
    : c==1 ? nc==8 ? s(x,4) : s(x,3) \
    : c==2 ? nc==4 ? s(x,0) : nc==8 ? s(x,5) : s(x,4) \
    : c==3 ? nc==4 ? s(x,1) : nc==8 ? s(x,6) : s(x,5) \
    : c==4 ? nc==8 ? s(x,7) : s(x,0) \
    : c==5 ? nc==8 ? s(x,0) : s(x,1) \
    : c==6 ? s(x,1) \
    : s(x,2))       \
 :                  \
    ( c==0 ? nc==8 ? s(x,4) : s(x,3) \
    : c==1 ? nc==4 ? s(x,0) : nc==8 ? s(x,5) : s(x,4) \
    : c==2 ? nc==4 ? s(x,1) : nc==8 ? s(x,6) : s(x,5) \
    : c==3 ? nc==4 ? s(x,2) : nc==8 ? s(x,7) : s(x,0) \
    : c==4 ? nc==8 ? s(x,0) : s(x,1) \
    : c==5 ? nc==8 ? s(x,1) : s(x,2) \
    : c==6 ? s(x,2) \
    : s(x,3)))

#define inv_var(x,r,c) \
 ( r==0 ?           \
    ( c==0 ? s(x,0) \
    : c==1 ? s(x,1) \
    : c==2 ? s(x,2) \
    : c==3 ? s(x,3) \
    : c==4 ? s(x,4) \
    : c==5 ? s(x,5) \
    : c==6 ? s(x,6) \
    : s(x,7))       \
 : r==1 ?           \
    ( c==0 ? nc==4 ? s(x,3) : nc==8 ? s(x,7) : s(x,5) \
    : c==1 ? s(x,0) \
    : c==2 ? s(x,1) \
    : c==3 ? s(x,2) \
    : c==4 ? s(x,3) \
    : c==5 ? s(x,4) \
    : c==6 ? s(x,5) \
    : s(x,6))       \
 : r==2 ?           \
    ( c==0 ? nc==4 ? s(x,2) : nc==8 ? s(x,5) : s(x,4) \
    : c==1 ? nc==4 ? s(x,3) : nc==8 ? s(x,6) : s(x,5) \
    : c==2 ? nc==8 ? s(x,7) : s(x,0) \
    : c==3 ? nc==8 ? s(x,0) : s(x,1) \
    : c==4 ? nc==8 ? s(x,1) : s(x,2) \
    : c==5 ? nc==8 ? s(x,2) : s(x,3) \
    : c==6 ? s(x,3) \
    : s(x,4))       \
 :                  \
    ( c==0 ? nc==4 ? s(x,1) : nc==8 ? s(x,4) : s(x,3) \
    : c==1 ? nc==4 ? s(x,2) : nc==8 ? s(x,5) : s(x,4) \
    : c==2 ? nc==4 ? s(x,3) : nc==8 ? s(x,6) : s(x,5) \
    : c==3 ? nc==8 ? s(x,7) : s(x,0) \
    : c==4 ? nc==8 ? s(x,0) : s(x,1) \
    : c==5 ? nc==8 ? s(x,1) : s(x,2) \
    : c==6 ? s(x,2) \
    : s(x,3)))

#define si(y,x,k,c) s(y,c) = word_in(x + 4 * c) ^ k[c]
#define so(y,x,c)   word_out(y + 4 * c, s(x,c))

#if defined(FOUR_TABLES)
#define fwd_rnd(y,x,k,c)    s(y,c)= (k)[c] ^ four_tables(x,ft_tab,fwd_var,rf1,c)
#define inv_rnd(y,x,k,c)    s(y,c)= (k)[c] ^ four_tables(x,it_tab,inv_var,rf1,c)
#elif defined(ONE_TABLE)
#define fwd_rnd(y,x,k,c)    s(y,c)= (k)[c] ^ one_table(x,upr,ft_tab,fwd_var,rf1,c)
#define inv_rnd(y,x,k,c)    s(y,c)= (k)[c] ^ one_table(x,upr,it_tab,inv_var,rf1,c)
#else
#define fwd_rnd(y,x,k,c)    s(y,c) = fwd_mcol(no_table(x,s_box,fwd_var,rf1,c)) ^ (k)[c]
#define inv_rnd(y,x,k,c)    s(y,c) = inv_mcol(no_table(x,inv_s_box,inv_var,rf1,c) ^ (k)[c])
#endif

#if defined(FOUR_LR_TABLES)
#define fwd_lrnd(y,x,k,c)   s(y,c)= (k)[c] ^ four_tables(x,fl_tab,fwd_var,rf1,c)
#define inv_lrnd(y,x,k,c)   s(y,c)= (k)[c] ^ four_tables(x,il_tab,inv_var,rf1,c)
#elif defined(ONE_LR_TABLE)
#define fwd_lrnd(y,x,k,c)   s(y,c)= (k)[c] ^ one_table(x,ups,fl_tab,fwd_var,rf1,c)
#define inv_lrnd(y,x,k,c)   s(y,c)= (k)[c] ^ one_table(x,ups,il_tab,inv_var,rf1,c)
#else
#define fwd_lrnd(y,x,k,c)   s(y,c) = no_table(x,s_box,fwd_var,rf1,c) ^ (k)[c]
#define inv_lrnd(y,x,k,c)   s(y,c) = no_table(x,inv_s_box,inv_var,rf1,c) ^ (k)[c]
#endif

#if MMHBLOCK_SIZE == 16

#if defined(ARRAYS)
#define locals(y,x)     x[4],y[4]
#else
#define locals(y,x)     x##0,x##1,x##2,x##3,y##0,y##1,y##2,y##3
 /* 
   the following defines prevent the compiler requiring the declaration
   of generated but unused variables in the fwd_var and inv_var macros
 */
#define b04 unused
#define b05 unused
#define b06 unused
#define b07 unused
#define b14 unused
#define b15 unused
#define b16 unused
#define b17 unused
#endif
#define l_copy(y, x)    s(y,0) = s(x,0); s(y,1) = s(x,1); \
                        s(y,2) = s(x,2); s(y,3) = s(x,3);
#define state_in(y,x,k) si(y,x,k,0); si(y,x,k,1); si(y,x,k,2); si(y,x,k,3)
#define state_out(y,x)  so(y,x,0); so(y,x,1); so(y,x,2); so(y,x,3)
#define round(rm,y,x,k) rm(y,x,k,0); rm(y,x,k,1); rm(y,x,k,2); rm(y,x,k,3)

#elif MMHBLOCK_SIZE == 24

#if defined(ARRAYS)
#define locals(y,x)     x[6],y[6]
#else
#define locals(y,x)     x##0,x##1,x##2,x##3,x##4,x##5, \
                        y##0,y##1,y##2,y##3,y##4,y##5
#define b06 unused
#define b07 unused
#define b16 unused
#define b17 unused
#endif
#define l_copy(y, x)    s(y,0) = s(x,0); s(y,1) = s(x,1); \
                        s(y,2) = s(x,2); s(y,3) = s(x,3); \
                        s(y,4) = s(x,4); s(y,5) = s(x,5);
#define state_in(y,x,k) si(y,x,k,0); si(y,x,k,1); si(y,x,k,2); \
                        si(y,x,k,3); si(y,x,k,4); si(y,x,k,5)
#define state_out(y,x)  so(y,x,0); so(y,x,1); so(y,x,2); \
                        so(y,x,3); so(y,x,4); so(y,x,5)
#define round(rm,y,x,k) rm(y,x,k,0); rm(y,x,k,1); rm(y,x,k,2); \
                        rm(y,x,k,3); rm(y,x,k,4); rm(y,x,k,5)
#else

#if defined(ARRAYS)
#define locals(y,x)     x[8],y[8]
#else
#define locals(y,x)     x##0,x##1,x##2,x##3,x##4,x##5,x##6,x##7, \
                        y##0,y##1,y##2,y##3,y##4,y##5,y##6,y##7
#endif
#define l_copy(y, x)    s(y,0) = s(x,0); s(y,1) = s(x,1); \
                        s(y,2) = s(x,2); s(y,3) = s(x,3); \
                        s(y,4) = s(x,4); s(y,5) = s(x,5); \
                        s(y,6) = s(x,6); s(y,7) = s(x,7);

#if MMHBLOCK_SIZE == 32

#define state_in(y,x,k) si(y,x,k,0); si(y,x,k,1); si(y,x,k,2); si(y,x,k,3); \
                        si(y,x,k,4); si(y,x,k,5); si(y,x,k,6); si(y,x,k,7)
#define state_out(y,x)  so(y,x,0); so(y,x,1); so(y,x,2); so(y,x,3); \
                        so(y,x,4); so(y,x,5); so(y,x,6); so(y,x,7)
#define round(rm,y,x,k) rm(y,x,k,0); rm(y,x,k,1); rm(y,x,k,2); rm(y,x,k,3); \
                        rm(y,x,k,4); rm(y,x,k,5); rm(y,x,k,6); rm(y,x,k,7)
#else

#define state_in(y,x,k) \
switch(nc) \
{   case 8: si(y,x,k,7); si(y,x,k,6); \
    case 6: si(y,x,k,5); si(y,x,k,4); \
    case 4: si(y,x,k,3); si(y,x,k,2); \
            si(y,x,k,1); si(y,x,k,0); \
}

#define state_out(y,x) \
switch(nc) \
{   case 8: so(y,x,7); so(y,x,6); \
    case 6: so(y,x,5); so(y,x,4); \
    case 4: so(y,x,3); so(y,x,2); \
            so(y,x,1); so(y,x,0); \
}

#if defined(FAST_VARIABLE)

#define round(rm,y,x,k) \
switch(nc) \
{   case 8: rm(y,x,k,7); rm(y,x,k,6); \
            rm(y,x,k,5); rm(y,x,k,4); \
            rm(y,x,k,3); rm(y,x,k,2); \
            rm(y,x,k,1); rm(y,x,k,0); \
            break; \
    case 6: rm(y,x,k,5); rm(y,x,k,4); \
            rm(y,x,k,3); rm(y,x,k,2); \
            rm(y,x,k,1); rm(y,x,k,0); \
            break; \
    case 4: rm(y,x,k,3); rm(y,x,k,2); \
            rm(y,x,k,1); rm(y,x,k,0); \
            break; \
}
#else

#define round(rm,y,x,k) \
switch(nc) \
{   case 8: rm(y,x,k,7); rm(y,x,k,6); \
    case 6: rm(y,x,k,5); rm(y,x,k,4); \
    case 4: rm(y,x,k,3); rm(y,x,k,2); \
            rm(y,x,k,1); rm(y,x,k,0); \
}

#endif

#endif
#endif

cf_dec c_name(encrypt)(const byte in_blk[], byte out_blk[], const c_name(aes) *cx)
{   word        locals(b0, b1);
    const word  *kp = cx->e_key;

#if !defined(ONE_TABLE) && !defined(FOUR_TABLES)
    word        f2;
#endif

    if(!(cx->mode & 0x01)) return aes_bad;

    state_in(b0, in_blk, kp); kp += nc;

#if defined(UNROLL)

    switch(cx->Nrnd)
    {
    case 14:    round(fwd_rnd,  b1, b0, kp         ); 
                round(fwd_rnd,  b0, b1, kp + nc    ); kp += 2 * nc;
    case 12:    round(fwd_rnd,  b1, b0, kp         ); 
                round(fwd_rnd,  b0, b1, kp + nc    ); kp += 2 * nc;
    case 10:    round(fwd_rnd,  b1, b0, kp         );             
                round(fwd_rnd,  b0, b1, kp +     nc);
                round(fwd_rnd,  b1, b0, kp + 2 * nc); 
                round(fwd_rnd,  b0, b1, kp + 3 * nc);
                round(fwd_rnd,  b1, b0, kp + 4 * nc); 
                round(fwd_rnd,  b0, b1, kp + 5 * nc);
                round(fwd_rnd,  b1, b0, kp + 6 * nc); 
                round(fwd_rnd,  b0, b1, kp + 7 * nc);
                round(fwd_rnd,  b1, b0, kp + 8 * nc);
                round(fwd_lrnd, b0, b1, kp + 9 * nc);
    }
#elif defined(PARTIAL_UNROLL)
    {   word    rnd;

        for(rnd = 0; rnd < (cx->Nrnd >> 1) - 1; ++rnd)
        {
            round(fwd_rnd, b1, b0, kp); 
            round(fwd_rnd, b0, b1, kp + nc); kp += 2 * nc;
        }

        round(fwd_rnd,  b1, b0, kp);
        round(fwd_lrnd, b0, b1, kp + nc);
    }
#else
    {   word    rnd;

        for(rnd = 0; rnd < cx->Nrnd - 1; ++rnd)
        {
            round(fwd_rnd, b1, b0, kp); 
            l_copy(b0, b1); kp += nc;
        }

        round(fwd_lrnd, b0, b1, kp);
    }
#endif

    state_out(out_blk, b0);
    return aes_good;
}

cf_dec c_name(decrypt)(const byte in_blk[], byte out_blk[], const c_name(aes) *cx)
{   word        locals(b0, b1);
    const word  *kp = cx->d_key;

#if !defined(ONE_TABLE) && !defined(FOUR_TABLES)
    word        f2, f4, f8, f9; 
#endif

    if(!(cx->mode & 0x02)) return aes_bad;

    state_in(b0, in_blk, kp); kp += nc;

#if defined(UNROLL)

    switch(cx->Nrnd)
    {
    case 14:    round(inv_rnd,  b1, b0, kp         );
                round(inv_rnd,  b0, b1, kp + nc    ); kp += 2 * nc;
    case 12:    round(inv_rnd,  b1, b0, kp         );
                round(inv_rnd,  b0, b1, kp + nc    ); kp += 2 * nc;
    case 10:    round(inv_rnd,  b1, b0, kp         );             
                round(inv_rnd,  b0, b1, kp +     nc);
                round(inv_rnd,  b1, b0, kp + 2 * nc); 
                round(inv_rnd,  b0, b1, kp + 3 * nc);
                round(inv_rnd,  b1, b0, kp + 4 * nc); 
                round(inv_rnd,  b0, b1, kp + 5 * nc);
                round(inv_rnd,  b1, b0, kp + 6 * nc); 
                round(inv_rnd,  b0, b1, kp + 7 * nc);
                round(inv_rnd,  b1, b0, kp + 8 * nc);
                round(inv_lrnd, b0, b1, kp + 9 * nc);
    }
#elif defined(PARTIAL_UNROLL)
    {   word    rnd;

        for(rnd = 0; rnd < (cx->Nrnd >> 1) - 1; ++rnd)
        {
            round(inv_rnd, b1, b0, kp); 
            round(inv_rnd, b0, b1, kp + nc); kp += 2 * nc;
        }

        round(inv_rnd,  b1, b0, kp);
        round(inv_lrnd, b0, b1, kp + nc);
    }
#else
    {   word    rnd;

        for(rnd = 0; rnd < cx->Nrnd - 1; ++rnd)
        {
            round(inv_rnd, b1, b0, kp); 
            l_copy(b0, b1); kp += nc;
        }

        round(inv_lrnd, b0, b1, kp);
    }
#endif

    state_out(out_blk, b0);
    return aes_good;
}
