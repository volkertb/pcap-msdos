/* myri_sbus.h: Defines for MyriCOM Gigabit Ethernet SBUS card driver.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef _MYRI_SBUS_H
#define _MYRI_SBUS_H

struct lanai_regs {
  volatile DWORD  ipf0;    /* Context zero state registers.*/
  volatile DWORD  cur0;
  volatile DWORD  prev0;
  volatile DWORD  data0;
  volatile DWORD  dpf0;
  volatile DWORD  ipf1;    /* Context one state registers.  */
  volatile DWORD  cur1;
  volatile DWORD  prev1;
  volatile DWORD  data1;
  volatile DWORD  dpf1;
  volatile DWORD  istat;    /* Interrupt status.    */
  volatile DWORD  eimask;    /* External IRQ mask.    */
  volatile DWORD  itimer;    /* IRQ timer.      */
  volatile DWORD  rtc;    /* Real Time Clock    */
  volatile DWORD  csum;    /* Checksum.      */
  volatile DWORD  dma_xaddr;  /* SBUS DMA external address.  */
  volatile DWORD  dma_laddr;  /* SBUS DMA local address.  */
  volatile DWORD  dma_ctr;  /* SBUS DMA counter.    */
  volatile DWORD  rx_dmaptr;  /* Receive DMA pointer.    */
  volatile DWORD  rx_dmalim;  /* Receive DMA limit.    */
  volatile DWORD  tx_dmaptr;  /* Transmit DMA pointer.  */
  volatile DWORD  tx_dmalim;  /* Transmit DMA limit.    */
  volatile DWORD  tx_dmalimt;  /* Transmit DMA limit w/tail.  */
  DWORD  _unused0;
  volatile BYTE  rbyte;    /* Receive byte.    */
  BYTE  _unused1[3];
  volatile WORD  rhalf;    /* Receive half-word.    */
  BYTE  _unused2[2];
  volatile DWORD  rword;    /* Receive word.    */
  volatile DWORD  salign;    /* Send align.      */
  volatile DWORD  ss_sendbyte;  /* SingleSend send-byte.  */
  volatile DWORD  ss_sendhalf;  /* SingleSend send-halfword.  */
  volatile DWORD  ss_sendword;  /* SingleSend send-word.  */
  volatile DWORD  ss_sendt;  /* SingleSend special.    */
  volatile DWORD  dma_dir;  /* DMA direction.    */
  volatile DWORD  dma_stat;  /* DMA status.      */
  volatile DWORD  timeo;    /* Timeout register.    */
  volatile DWORD  myrinet;  /* XXX MAGIC myricom thing  */
  volatile DWORD  hwdebug;  /* Hardware debugging reg.  */
  volatile DWORD  leds;    /* LED control.      */
  volatile DWORD  vers;    /* Version register.    */
  volatile DWORD  link_on;  /* Link activation reg.    */
  DWORD _unused3[0x17];
  volatile DWORD  cval;    /* Clock value register.  */
};

/* Interrupt status bits. */
#define ISTAT_DEBUG  0x80000000
#define ISTAT_HOST  0x40000000
#define ISTAT_LAN7  0x00800000
#define ISTAT_LAN6  0x00400000
#define ISTAT_LAN5  0x00200000
#define ISTAT_LAN4  0x00100000
#define ISTAT_LAN3  0x00080000
#define ISTAT_LAN2  0x00040000
#define ISTAT_LAN1  0x00020000
#define ISTAT_LAN0  0x00010000
#define ISTAT_WRDY  0x00008000
#define ISTAT_HRDY  0x00004000
#define ISTAT_SRDY  0x00002000
#define ISTAT_LINK  0x00001000
#define ISTAT_FRES  0x00000800
#define ISTAT_NRES  0x00000800
#define ISTAT_WAKE  0x00000400
#define ISTAT_OB2  0x00000200
#define ISTAT_OB1  0x00000100
#define ISTAT_TAIL  0x00000080
#define ISTAT_WDOG  0x00000040
#define ISTAT_TIME  0x00000020
#define ISTAT_DMA  0x00000010
#define ISTAT_SEND  0x00000008
#define ISTAT_BUF  0x00000004
#define ISTAT_RECV  0x00000002
#define ISTAT_BRDY  0x00000001

struct myri_regs {
  volatile DWORD  reset_off;
  volatile DWORD  reset_on;
  volatile DWORD  irq_off;
  volatile DWORD  irq_on;
  volatile DWORD  wakeup_off;
  volatile DWORD  wakeup_on;
  volatile DWORD  irq_read;
  DWORD _unused[0xfff9];
  volatile WORD  local_mem[0x10800];
};

/* Shared memory interrupt mask. */
#define SHMEM_IMASK_RX    0x00000002
#define SHMEM_IMASK_TX    0x00000001

/* Just to make things readable. */
#define KERNEL_CHANNEL    0

/* The size of this must be >= 129 bytes. */
struct myri_eeprom {
  DWORD    cval;
  WORD    cpuvers;
  BYTE    id[6];
  DWORD    ramsz;
  BYTE    fvers[32];
  BYTE    mvers[16];
  WORD    dlval;
  WORD    brd_type;
  WORD    bus_type;
  WORD    prod_code;
  DWORD    serial_num;
  WORD    _reserved[24];
  DWORD    _unused[2];
};

/* EEPROM bus types, only SBUS is valid in this driver. */
#define BUS_TYPE_SBUS    1

/* EEPROM CPU revisions. */
#define CPUVERS_2_3    0x0203
#define CPUVERS_3_0    0x0300
#define CPUVERS_3_1    0x0301
#define CPUVERS_3_2    0x0302
#define CPUVERS_4_0    0x0400
#define CPUVERS_4_1    0x0401
#define CPUVERS_4_2    0x0402
#define CPUVERS_5_0    0x0500

struct myri_control {
  volatile WORD  ctrl;
  volatile WORD  irqlvl;
};

/* Global control register defines. */
#define CONTROL_ROFF    0x8000  /* Reset OFF.    */
#define CONTROL_RON    0x4000  /* Reset ON.    */
#define CONTROL_EIRQ    0x2000  /* Enable IRQ's.  */
#define CONTROL_DIRQ    0x1000  /* Disable IRQ's.  */
#define CONTROL_WON    0x0800  /* Wake-up ON.    */

#define MYRI_SCATTER_ENTRIES  8
#define MYRI_GATHER_ENTRIES  16

struct myri_sglist {
  DWORD addr;
  DWORD len;
};

struct myri_rxd {
  struct myri_sglist myri_scatters[MYRI_SCATTER_ENTRIES];  /* DMA scatter list.*/
  DWORD csum;        /* HW computed checksum.    */
  DWORD ctx;
  DWORD num_sg;        /* Total scatter entries.   */
};

struct myri_txd {
  struct myri_sglist myri_gathers[MYRI_GATHER_ENTRIES]; /* DMA scatter list.  */
  DWORD num_sg;        /* Total scatter entries.   */
  WORD addr[4];        /* XXX address              */
  DWORD chan;
  DWORD len;        /* Total length of packet.  */
  DWORD csum_off;        /* Where data to csum is.   */
  DWORD csum_field;      /* Where csum goes in pkt.  */
};

#define MYRINET_MTU        8432
#define RX_ALLOC_SIZE      8448
#define MYRI_PAD_LEN       2
#define RX_COPY_THRESHOLD  128

/* These numbers are cast in stone, new firmware is needed if
 * you want to change them.
 */
#define TX_RING_MAXSIZE    16
#define RX_RING_MAXSIZE    16

#define TX_RING_SIZE       16
#define RX_RING_SIZE       16

/* GRRR... */
extern __inline__ int NEXT_RX(int num)
{
  if(++num > RX_RING_SIZE)
    num = 0;
  return num;
}

extern __inline__ int PREV_RX(int num)
{
  if(--num < 0)
    num = RX_RING_SIZE;
  return num;
}

#define NEXT_TX(num)  (((num) + 1) & (TX_RING_SIZE - 1))
#define PREV_TX(num)  (((num) - 1) & (TX_RING_SIZE - 1))

#define TX_BUFFS_AVAIL(head, tail)    \
  ((head) <= (tail) ?      \
   (head) + (TX_RING_SIZE - 1) - (tail) :  \
   (head) - (tail) - 1)

struct sendq {
  DWORD  tail;
  DWORD  head;
  DWORD  hdebug;
  DWORD  mdebug;
  struct myri_txd  myri_txd[TX_RING_MAXSIZE];
};

struct recvq {
  DWORD  head;
  DWORD  tail;
  DWORD  hdebug;
  DWORD  mdebug;
  struct myri_rxd  myri_rxd[RX_RING_MAXSIZE + 1];
};

#define MYRI_MLIST_SIZE 8

struct mclist {
  DWORD maxlen;
  DWORD len;
  DWORD cache;
  struct pair {
    BYTE addr[8];
    DWORD  val;
  } mc_pairs[MYRI_MLIST_SIZE];
  BYTE bcast_addr[8];
};

struct myri_channel {
  DWORD  state;    /* State of the channel.  */
  DWORD  busy;    /* Channel is busy.    */
  struct sendq  sendq;    /* Device tx queue.    */
  struct recvq  recvq;    /* Device rx queue.    */
  struct recvq  recvqa;    /* Device rx queue acked.  */
  DWORD  rbytes;    /* Receive bytes.    */
  DWORD  sbytes;    /* Send bytes.      */
  DWORD  rmsgs;    /* Receive messages.    */
  DWORD  smsgs;    /* Send messages.    */
  struct mclist  mclist;    /* Device multicast list.  */
};

/* Values for per-channel state. */
#define STATE_WFH  0    /* Waiting for HOST.    */
#define STATE_WFN  1    /* Waiting for NET.    */
#define STATE_READY  2    /* Ready.      */

struct myri_shmem {
  BYTE  addr[8];  /* Board's address.    */
  DWORD  nchan;    /* Number of channels.    */
  DWORD  burst;    /* SBUS dma burst enable.  */
  DWORD  shakedown;  /* DarkkkkStarrr Crashesss...  */
  DWORD  send;    /* Send wanted.      */
  DWORD  imask;    /* Interrupt enable mask.  */
  DWORD  mlevel;    /* Map level.      */
  DWORD  debug[4];  /* Misc. debug areas.    */
  struct myri_channel channel;  /* Only one channel on a host.  */
};

struct myri_eth {
  /* These are frequently accessed, keep together
   * to obtain good cache hit rates.
   */
  struct myri_shmem    *shmem;    /* Shared data structures.    */
  struct myri_control    *cregs;    /* Control register space.    */
  struct recvq      *rqack;    /* Where we ack rx's.         */
  struct recvq      *rq;    /* Where we put buffers.      */
  struct sendq      *sq;    /* Where we stuff tx's.       */
  struct device      *dev;    /* Linux/NET dev struct.      */
  int        tx_old;    /* To speed up tx cleaning.   */
  struct lanai_regs    *lregs;    /* Quick ptr to LANAI regs.   */
  struct sk_buff         *rx_skbs[RX_RING_SIZE+1];/* RX skb's                   */
  struct sk_buff         *tx_skbs[TX_RING_SIZE];  /* TX skb's                   */
  struct net_device_stats    enet_stats;  /* Interface stats.           */

  /* These are less frequently accessed. */
  struct myri_regs    *regs;          /* MyriCOM register space.    */
  WORD      *lanai;    /* View 2 of register space.  */
  DWORD      *lanai3;  /* View 3 of register space.  */
  DWORD      myri_bursts;  /* SBUS bursts.               */
  struct myri_eeprom    eeprom;    /* Local copy of EEPROM.      */
  DWORD      reg_size;  /* Size of register space.    */
  DWORD      shmem_base;  /* Offset to shared ram.      */
  struct linux_sbus_device  *myri_sbus_dev;  /* Our SBUS device struct.    */
  struct myri_eth      *next_module;  /* Next in adapter chain.     */
};

/* We use this to acquire receive skb's that we can DMA directly into.
 */
#define ALIGNED_RX_SKB_ADDR(addr) \
        ((((DWORD)(addr) + (64-1)) & ~(64-1)) - (DWORD)(addr))

extern __inline__ struct sk_buff *myri_alloc_skb (DWORD length, int gfp_flags)
{
  struct sk_buff *skb = alloc_skb(length + 64, gfp_flags);

  if (skb) {
    int offset = ALIGNED_RX_SKB_ADDR(skb->data);

    if(offset)
      skb_reserve(skb, offset);
  }
  return skb;
}
#endif /* !(_MYRI_SBUS_H) */
