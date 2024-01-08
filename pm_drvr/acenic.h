#ifndef __ACENIC_H
#define __ACENIC_H

/*
 * Addressing:
 *
 * The Tigon uses 64-bit host addresses, regardless of their actual
 * length, and it expects a big-endian format. For 32 bit systems the
 * upper 32 bits of the address are simply ignored (zero), however for
 * little endian 64 bit systems (Alpha) this looks strange with the
 * two parts of the address word being swapped.
 *
 * The addresses are split in two 32 bit words for all architectures
 * as some of them are in PCI shared memory and it is necessary to use
 * readl/writel to access them.
 *
 * The addressing code is derived from Pete Wyckoff's work, but
 * modified to deal properly with readl/writel usage.
 */

typedef struct {
        DWORD addrhi;
        DWORD addrlo;
      } aceaddr;


extern __inline__ void set_aceaddr (aceaddr * aa, volatile void *addr)
{
  DWORD baddr = virt_to_bus ((void *) addr);

#if (BITS_PER_LONG == 64)
  aa->addrlo = baddr & 0xffffffff;
  aa->addrhi = baddr >> 32;
#else
  /* Don't bother setting zero every time */
  aa->addrlo = baddr;
#endif
  mb();
}

extern __inline__ void set_aceaddr_bus (aceaddr * aa, volatile void *addr)
{
  DWORD baddr = (DWORD) addr;

#if (BITS_PER_LONG == 64)
  aa->addrlo = baddr & 0xffffffff;
  aa->addrhi = baddr >> 32;
#else
  /* Don't bother setting zero every time */
  aa->addrlo = baddr;
#endif
  mb();
}


extern __inline__ void *get_aceaddr (aceaddr * aa)
{
  DWORD addr;

  mb();
#if (BITS_PER_LONG == 64)
  addr = (u64) aa->addrhi << 32 | aa->addrlo;
#else
  addr = aa->addrlo;
#endif
  return bus_to_virt (addr);
}


extern __inline__ void *get_aceaddr_bus (aceaddr * aa)
{
  DWORD addr;

  mb();
#if (BITS_PER_LONG == 64)
  addr = (u64) aa->addrhi << 32 | aa->addrlo;
#else
  addr = aa->addrlo;
#endif
  return (void*) addr;
}


struct ace_regs {
       DWORD pad0[16];          /* PCI control registers */

       DWORD HostCtrl;          /* 0x40 */
       DWORD LocalCtrl;

       DWORD pad1[2];

       DWORD MiscCfg;           /* 0x50 */

       DWORD pad2[2];

       DWORD PciState;

       DWORD pad3[2];           /* 0x60 */

       DWORD WinBase;
       DWORD WinData;

       DWORD pad4[12];          /* 0x70 */

       DWORD DmaWriteState;     /* 0xa0 */
       DWORD pad5[3];
       DWORD DmaReadState;      /* 0xb0 */

       DWORD pad6[26];

       DWORD AssistState;

       DWORD pad7[8];           /* 0x120 */

       DWORD CpuCtrl;           /* 0x140 */
       DWORD Pc;

       DWORD pad8[3];

       DWORD SramAddr;          /* 0x154 */
       DWORD SramData;

       DWORD pad9[49];

       DWORD MacRxState;        /* 0x220 */

       DWORD pad10[7];

       DWORD CpuBCtrl;          /* 0x240 */
       DWORD PcB;

       DWORD pad11[3];

       DWORD SramBAddr;         /* 0x254 */
       DWORD SramBData;

       DWORD pad12[105];

       DWORD pad13[32];         /* 0x400 */
       DWORD Stats[32];

       DWORD Mb0Hi;             /* 0x500 */
       DWORD Mb0Lo;
       DWORD Mb1Hi;
       DWORD CmdPrd;
       DWORD Mb2Hi;
       DWORD TxPrd;
       DWORD Mb3Hi;
       DWORD RxStdPrd;          /* RxStdPrd */
       DWORD Mb4Hi;
       DWORD RxJumboPrd;        /* RxJumboPrd */
       DWORD Mb5Hi;
       DWORD RxMiniPrd;
       DWORD Mb6Hi;
       DWORD Mb6Lo;
       DWORD Mb7Hi;
       DWORD Mb7Lo;
       DWORD Mb8Hi;
       DWORD Mb8Lo;
       DWORD Mb9Hi;
       DWORD Mb9Lo;
       DWORD MbAHi;
       DWORD MbALo;
       DWORD MbBHi;
       DWORD MbBLo;
       DWORD MbCHi;
       DWORD MbCLo;
       DWORD MbDHi;
       DWORD MbDLo;
       DWORD MbEHi;
       DWORD MbELo;
       DWORD MbFHi;
       DWORD MbFLo;

       DWORD pad14[32];

       DWORD MacAddrHi;         /* 0x600 */
       DWORD MacAddrLo;
       DWORD InfoPtrHi;
       DWORD InfoPtrLo;
       DWORD MultiCastHi;       /* 0x610 */
       DWORD MultiCastLo;
       DWORD ModeStat;
       DWORD DmaReadCfg;
       DWORD DmaWriteCfg;       /* 0x620 */
       DWORD TxBufRat;
       DWORD EvtCsm;
       DWORD CmdCsm;
       DWORD TuneRxCoalTicks;   /* 0x630 */
       DWORD TuneTxCoalTicks;
       DWORD TuneStatTicks;
       DWORD TuneMaxTxDesc;
       DWORD TuneMaxRxDesc;     /* 0x640 */
       DWORD TuneTrace;
       DWORD TuneLink;
       DWORD TuneFastLink;
       DWORD TracePtr;          /* 0x650 */
       DWORD TraceStrt;
       DWORD TraceLen;
       DWORD IfIdx;
       DWORD IfMtu;             /* 0x660 */
       DWORD MaskInt;
       DWORD GigLnkState;
       DWORD FastLnkState;
       DWORD pad16[4];          /* 0x670 */
       DWORD RxRetCsm;          /* 0x680 */

       DWORD pad17[31];

       DWORD CmdRng[64];        /* 0x700 */
       DWORD Window[0x200];
     };

#define ACE_WINDOW_SIZE	0x800
#define ACE_JUMBO_MTU   9000
#define ACE_STD_MTU     1500
#define ACE_TRACE_SIZE  0x8000

/*
 * Host control register bits.
 */
#define IN_INT          0x01
#define CLR_INT		0x02
#define BYTE_SWAP	0x10
#define WORD_SWAP	0x20
#define MASK_INTS	0x40

/*
 * Local control register bits.
 */
#define EEPROM_DATA_IN          0x800000
#define EEPROM_DATA_OUT		0x400000
#define EEPROM_WRITE_ENABLE	0x200000
#define EEPROM_CLK_OUT		0x100000

#define EEPROM_BASE		0xa0000000

#define EEPROM_WRITE_SELECT	0xa0
#define EEPROM_READ_SELECT	0xa1

#define SRAM_BANK_512K		0x200

/*
 * Misc Config bits
 */
#define SYNC_SRAM_TIMING        0x100000

/*
 * CPU state bits.
 */
#define CPU_RESET               0x01
#define CPU_TRACE		0x02
#define CPU_PROM_FAILED		0x10
#define CPU_HALT		0x00010000
#define CPU_HALTED		0xffff0000

/*
 * PCI State bits.
 */
#define DMA_READ_MAX_4          0x04
#define DMA_READ_MAX_16		0x08
#define DMA_READ_MAX_32		0x0c
#define DMA_READ_MAX_64		0x10
#define DMA_READ_MAX_128	0x14
#define DMA_READ_MAX_256	0x18
#define DMA_READ_MAX_1K		0x1c
#define DMA_WRITE_MAX_4		0x20
#define DMA_WRITE_MAX_16	0x40
#define DMA_WRITE_MAX_32	0x60
#define DMA_WRITE_MAX_64	0x80
#define DMA_WRITE_MAX_128	0xa0
#define DMA_WRITE_MAX_256	0xc0
#define DMA_WRITE_MAX_1K	0xe0
#define MEM_READ_MULTIPLE	0x00020000
#define PCI_66MHZ		0x00080000
#define DMA_WRITE_ALL_ALIGN	0x00800000
#define READ_CMD_MEM		0x06000000
#define WRITE_CMD_MEM		0x70000000


/*
 * Mode status
 */
#define ACE_BYTE_SWAP_DATA      0x10
#define ACE_WARN		0x08
#define ACE_WORD_SWAP		0x04
#define ACE_NO_JUMBO_FRAG	0x200
#define ACE_FATAL		0x40000000

/*
 * DMA config
 */
#define DMA_THRESH_8W		0x80

/*
 * Tuning parameters
 */
#define TICKS_PER_SEC           1000000

/*
 * Link bits
 */
#define LNK_PREF                0x00008000
#define LNK_10MB		0x00010000
#define LNK_100MB		0x00020000
#define LNK_1000MB		0x00040000
#define LNK_FULL_DUPLEX		0x00080000
#define LNK_HALF_DUPLEX		0x00100000
#define LNK_TX_FLOW_CTL_Y	0x00200000
#define LNK_NEG_ADVANCED	0x00400000
#define LNK_RX_FLOW_CTL_Y	0x00800000
#define LNK_NIC			0x01000000
#define LNK_JAM			0x02000000
#define LNK_JUMBO		0x04000000
#define LNK_ALTEON		0x08000000
#define LNK_NEG_FCTL		0x10000000
#define LNK_NEGOTIATE		0x20000000
#define LNK_ENABLE		0x40000000
#define LNK_UP			0x80000000


/*
 * Event definitions
 */
#define EVT_RING_ENTRIES        256
#define EVT_RING_SIZE	(EVT_RING_ENTRIES * sizeof(struct event))

struct event {
#ifdef __LITTLE_ENDIAN
       DWORD idx  : 12;
       DWORD code : 12;
       DWORD evt  : 8;
#else
       DWORD evt  : 8;
       DWORD code : 12;
       DWORD idx  : 12;
#endif
       DWORD pad;
     };

/*
 * Events
 */
#define E_FW_RUNNING            0x01
#define E_STATS_UPDATED		0x04

#define E_STATS_UPDATE		0x04

#define E_LNK_STATE		0x06
#define E_C_LINK_UP		0x01
#define E_C_LINK_DOWN		0x02
#define E_C_LINK_UP_FAST	0x03

#define E_ERROR			0x07
#define E_C_ERR_INVAL_CMD	0x01
#define E_C_ERR_UNIMP_CMD	0x02
#define E_C_ERR_BAD_CFG		0x03

#define E_MCAST_LIST		0x08
#define E_C_MCAST_ADDR_ADD	0x01
#define E_C_MCAST_ADDR_DEL	0x02

#define E_RESET_JUMBO_RNG	0x09


/*
 * Commands
 */
#define CMD_RING_ENTRIES        64

struct cmd {
#ifdef __LITTLE_ENDIAN
       DWORD idx  : 12;
       DWORD code : 12;
       DWORD evt  : 8;
#else
       DWORD evt  : 8;
       DWORD code : 12;
       DWORD idx  : 12;
#endif
};


#define C_HOST_STATE		0x01
#define C_C_STACK_UP		0x01
#define C_C_STACK_DOWN		0x02

#define C_FDR_FILTERING		0x02
#define C_C_FDR_FILT_ENABLE	0x01
#define C_C_FDR_FILT_DISABLE	0x02

#define C_SET_RX_PRD_IDX	0x03
#define C_UPDATE_STATS		0x04
#define C_RESET_JUMBO_RNG	0x05
#define C_ADD_MULTICAST_ADDR	0x08
#define C_DEL_MULTICAST_ADDR	0x09

#define C_SET_PROMISC_MODE	0x0a
#define C_C_PROMISC_ENABLE	0x01
#define C_C_PROMISC_DISABLE	0x02

#define C_LNK_NEGOTIATION	0x0b
#define C_C_NEGOTIATE_BOTH	0x00
#define C_C_NEGOTIATE_GIG	0x01
#define C_C_NEGOTIATE_10_100	0x02

#define C_SET_MAC_ADDR		0x0c
#define C_CLEAR_PROFILE		0x0d

#define C_SET_MULTICAST_MODE	0x0e
#define C_C_MCAST_ENABLE	0x01
#define C_C_MCAST_DISABLE	0x02

#define C_CLEAR_STATS		0x0f
#define C_SET_RX_JUMBO_PRD_IDX	0x10
#define C_REFRESH_STATS		0x11

/*
 * Descriptor flags
 */
#define BD_FLG_TCP_UDP_SUM	0x01
#define BD_FLG_IP_SUM		0x02
#define BD_FLG_END		0x04
#define BD_FLG_JUMBO		0x10
#define BD_FLG_MINI		0x1000

/*
 * Ring Control block flags
 */
#define RCB_FLG_TCP_UDP_SUM	0x01
#define RCB_FLG_IP_SUM		0x02
#define RCB_FLG_VLAN_ASSIST	0x10
#define RCB_FLG_COAL_INT_ONLY	0x20
#define RCB_FLG_IEEE_SNAP_SUM	0x80
#define RCB_FLG_EXT_RX_BD	0x100
#define RCB_FLG_RNG_DISABLE	0x200

/*
 * TX ring
 */
#define TX_RING_ENTRIES	128
#define TX_RING_SIZE	(TX_RING_ENTRIES * sizeof(struct tx_desc))
#define TX_RING_BASE	0x3800

struct tx_desc {
       aceaddr addr;
       DWORD   flagsize;
#if 0
      /* This is in PCI shared mem and must be accessed with readl/writel
       * real layout is:
       */
#if __LITTLE_ENDIAN
       WORD  flags;
       WORD  size;
       WORD  vlan;
       WORD  reserved;
#else
       WORD  size;
       WORD  flags;
       WORD  reserved;
       WORD  vlan;
#endif
#endif
       DWORD vlanres;
};


#define RX_STD_RING_ENTRIES	512
#define RX_STD_RING_SIZE	(RX_STD_RING_ENTRIES * sizeof(struct rx_desc))

#define RX_JUMBO_RING_ENTRIES	256
#define RX_JUMBO_RING_SIZE	(RX_JUMBO_RING_ENTRIES *sizeof(struct rx_desc))

#define RX_MINI_RING_ENTRIES	1024
#define RX_MINI_RING_SIZE	(RX_MINI_RING_ENTRIES *sizeof(struct rx_desc))

#define RX_RETURN_RING_ENTRIES	2048
#define RX_RETURN_RING_SIZE	(RX_MAX_RETURN_RING_ENTRIES * \
				 sizeof(struct rx_desc))

struct rx_desc {
  aceaddr addr;

#ifdef __LITTLE_ENDIAN
  WORD size;
  WORD idx;
#else
  WORD idx;
  WORD size;
#endif

#ifdef __LITTLE_ENDIAN
  WORD flags;
  WORD type;
#else
  WORD type;
  WORD flags;
#endif

#ifdef __LITTLE_ENDIAN
  WORD tcp_udp_csum;
  WORD ip_csum;
#else
  WORD ip_csum;
  WORD tcp_udp_csum;
#endif

#ifdef __LITTLE_ENDIAN
  WORD vlan;
  WORD err_flags;
#else
  WORD err_flags;
  WORD vlan;
#endif

  DWORD reserved;
  DWORD opague;
};


/*
 * This struct is shared with the NIC firmware.
 */
struct ring_ctrl
{
  aceaddr rngptr;
#ifdef __LITTLE_ENDIAN
  WORD flags;
  WORD max_len;
#else
  WORD max_len;
  WORD flags;
#endif
  DWORD pad;
};

struct ace_mac_stats
{
  DWORD excess_colls;
  DWORD coll_1;
  DWORD coll_2;
  DWORD coll_3;
  DWORD coll_4;
  DWORD coll_5;
  DWORD coll_6;
  DWORD coll_7;
  DWORD coll_8;
  DWORD coll_9;
  DWORD coll_10;
  DWORD coll_11;
  DWORD coll_12;
  DWORD coll_13;
  DWORD coll_14;
  DWORD coll_15;
  DWORD late_coll;
  DWORD defers;
  DWORD crc_err;
  DWORD underrun;
  DWORD crs_err;
  DWORD pad[3];
  DWORD drop_ula;
  DWORD drop_mc;
  DWORD drop_fc;
  DWORD drop_space;
  DWORD coll;
  DWORD kept_bc;
  DWORD kept_mc;
  DWORD kept_uc;
};

struct ace_info {
       union {
         DWORD stats[256];
       } s;
       struct ring_ctrl evt_ctrl;
       struct ring_ctrl cmd_ctrl;
       struct ring_ctrl tx_ctrl;
       struct ring_ctrl rx_std_ctrl;
       struct ring_ctrl rx_jumbo_ctrl;
       struct ring_ctrl rx_mini_ctrl;
       struct ring_ctrl rx_return_ctrl;
       aceaddr          evt_prd_ptr;
       aceaddr          rx_ret_prd_ptr;
       aceaddr          tx_csm_ptr;
       aceaddr          stats2_ptr;
     };

/*
 * struct ace_skb holding the rings of skb's. This is an awful lot of
 * pointers, but I don't see any other smart mode to do this in an
 * efficient manner ;-(
 */
struct ace_skb {
       struct sk_buff *tx_skbuff[TX_RING_ENTRIES];
       struct sk_buff *rx_std_skbuff[RX_STD_RING_ENTRIES];
       struct sk_buff *rx_mini_skbuff[RX_MINI_RING_ENTRIES];
       struct sk_buff *rx_jumbo_skbuff[RX_JUMBO_RING_ENTRIES];
     };


/*
 * Struct private for the AceNIC.
 *
 * Elements are grouped so variables used by the tx handling goes
 * together, and will go into the same cache lines etc. in order to
 * avoid cache line contention between the rx and tx handling on SMP.
 *
 * Frequently accessed variables are put at the beginning of the
 * struct to help the compiler generate better/shorter code.
 */
struct ace_private
{
  struct ace_skb *skb;
  struct ace_regs *regs;        /* register base */
  int version, fw_running, fw_up, link;
  int promisc, mcast_all;

  /* The send ring is located in the shared memory window
   */
  struct ace_info *info;
  struct tx_desc *tx_ring;
  DWORD  tx_prd, tx_full, tx_ret_csm;
  struct timer_list timer;

  DWORD std_refill_busy __attribute__ ((aligned (L1_CACHE_BYTES)));
  DWORD mini_refill_busy, jumbo_refill_busy;
  atomic_t cur_rx_bufs, cur_mini_bufs, cur_jumbo_bufs;
  DWORD rx_std_skbprd, rx_mini_skbprd, rx_jumbo_skbprd;
  DWORD cur_rx;
  struct tq_struct immediate;
  int bh_pending, jumbo;
  struct rx_desc rx_std_ring[RX_STD_RING_ENTRIES] __attribute__ ((aligned (L1_CACHE_BYTES)));
  struct rx_desc rx_jumbo_ring[RX_JUMBO_RING_ENTRIES];
  struct rx_desc rx_mini_ring[RX_MINI_RING_ENTRIES];
  struct rx_desc rx_return_ring[RX_RETURN_RING_ENTRIES];
  struct event evt_ring[EVT_RING_ENTRIES];
  volatile DWORD evt_prd __attribute__ ((aligned (L1_CACHE_BYTES)));
  volatile DWORD rx_ret_prd __attribute__ ((aligned (L1_CACHE_BYTES)));
  volatile DWORD tx_csm __attribute__ ((aligned (L1_CACHE_BYTES)));
  unsigned char *trace_buf;
  struct pci_dev *pdev;
  struct net_device *next;
  WORD pci_command;
  u8 pci_latency;
  char name[24];
  struct net_device_stats stats;
};

/*
 * Prototypes
 */
static int ace_init (struct net_device *dev, int board_idx);
static void ace_load_std_rx_ring (struct ace_private *ap, int nr_bufs);
static void ace_load_mini_rx_ring (struct ace_private *ap, int nr_bufs);
static void ace_load_jumbo_rx_ring (struct ace_private *ap, int nr_bufs);
static int ace_flush_jumbo_rx_ring (struct net_device *dev);
static void ace_interrupt (int irq, void *dev_id, struct pt_regs *regs);
static int ace_load_firmware (struct net_device *dev);
static int ace_open (struct net_device *dev);
static int ace_start_xmit (struct sk_buff *skb, struct net_device *dev);
static int ace_close (struct net_device *dev);
static void ace_timer (DWORD data);
static void ace_bh (struct net_device *dev);
static void ace_dump_trace (struct ace_private *ap);
static void ace_set_multicast_list (struct net_device *dev);
static int ace_change_mtu (struct net_device *dev, int new_mtu);

#ifdef SKB_RECYCLE
extern int ace_recycle (struct sk_buff *skb);
#endif
static int ace_ioctl (struct net_device *dev, struct ifreq *ifr, int cmd);
static int ace_set_mac_addr (struct net_device *dev, void *p);
static struct net_device_stats *ace_get_stats (struct net_device *dev);
static u8 read_eeprom_byte (struct ace_regs *regs, DWORD offset);

#endif     /* _ACENIC_H_ */
