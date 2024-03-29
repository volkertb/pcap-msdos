/*
   dmfe.c: Version 1.26

   A Davicom DM9102 fast ethernet driver for Linux. 

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 1.

   Compiler command:
   "gcc -DMODULE -D__KERNEL__ -I/usr/src/linux/net/inet -Wall 
		-Wstrict-prototypes -O6 -c dmfe.c"

   The following steps teach you how to active DM9102 board:
   1. Used the upper compiler command to compile dmfe.c
   2. insert dmfe module into kernel
   "insmod dmfe"        ;;Auto Detection Mode
   "insmod dmfe mode=0" ;;Force 10M Half Duplex
   "insmod dmfe mode=1" ;;Force 100M Half Duplex
   "insmod dmfe mode=4" ;;Force 10M Full Duplex
   "insmod dmfe mode=5" ;;Force 100M Full Duplex
   3. config a dm9102 network interface
   "ifconfig eth0 172.22.3.18"
   4. active the IP routing table
   "route add -net 172.22.3.0 eth0"
   5. Well done. Your DM9102 adapter actived now.

   Author: Sten Wang, E-mail: sten_wang@davicom.com.tw

   Date:   10/28,1998

   (C)Copyright 1997-1998 DAVICOM Semiconductor, Inc. All Rights Reserved.

   Marcelo Tosatti <marcelo@conectiva.com.br> : 
   Made it compile in 2.3 (device to net_device)
   
   Alan Cox <alan@redhat.com> :
   Removed the back compatibility support
   Reformatted, fixing spelling etc as I went
   Removed IRQ 0-15 assumption
   
   TODO
   
   Check and fix on 64bit and big endian boxes.
   Sort out the PCI latency.
   
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/version.h>

#include <linux/delay.h>
#include <asm/processor.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>


/* Board/System/Debug information/definition ---------------- */
#define PCI_DM9102_ID   0x91021282	/* Davicom DM9102 ID */
#define PCI_DM9100_ID   0x91001282	/* Davicom DM9100 ID */

#define DMFE_SUCC       0
#define DM9102_IO_SIZE  0x80
#define TX_FREE_DESC_CNT 0x1	/* Tx packet count */
#define TX_DESC_CNT     0x10	/* Allocated Tx descriptors */
#define RX_DESC_CNT     0x10	/* Allocated Rx descriptors */
#define DESC_ALL_CNT    TX_DESC_CNT+RX_DESC_CNT
#define TX_BUF_ALLOC    0x600
#define RX_ALLOC_SIZE   0x620
#define DM910X_RESET    1
#define CR6_DEFAULT     0x002c0000	/* SF, MII, HD */
#define CR7_DEFAULT     0x1a2cd
#define CR15_DEFAULT    0x06	/* TxJabber RxWatchdog */
#define TDES0_ERR_MASK  0x4302	/* TXJT, LC, EC, FUE */
#define MAX_PACKET_SIZE 1514
#define DMFE_MAX_MULTICAST 14
#define RX_MAX_TRAFFIC 0x5000
#define MAX_CHECK_PACKET 0x8000

#define DMFE_10MHF      0
#define DMFE_100MHF     1
#define DMFE_10MFD      4
#define DMFE_100MFD     5
#define DMFE_AUTO       8

#define DMFE_TIMER_WUT  jiffies+HZ*1	/* timer wakeup time : 1 second */
#define DMFE_TX_TIMEOUT HZ*2	/* tx packet time-out time */

#define DMFE_DBUG(dbug_now, msg, vaule) if (dmfe_debug || dbug_now) printk("DBUG: %s %x\n", msg, vaule)

#define DELAY_5US udelay(5)	/* udelay scale 1 usec */

#define DELAY_1US udelay(1)	/* udelay scale 1 usec */

#define SHOW_MEDIA_TYPE(mode) printk("\n<WARN> Change Speed to %sMhz %s duplex\n",mode & 1 ?"100":"10", mode & 4 ? "full":"half");


/* CR9 definition: SROM/MII */
#define CR9_SROM_READ   0x4800
#define CR9_SRCS        0x1
#define CR9_SRCLK       0x2
#define CR9_CRDOUT      0x8
#define SROM_DATA_0     0x0
#define SROM_DATA_1     0x4
#define PHY_DATA_1      0x20000
#define PHY_DATA_0      0x00000
#define MDCLKH          0x10000

#define SROM_CLK_WRITE(data, ioaddr) outl(data|CR9_SROM_READ|CR9_SRCS,ioaddr);DELAY_5US;outl(data|CR9_SROM_READ|CR9_SRCS|CR9_SRCLK,ioaddr);DELAY_5US;outl(data|CR9_SROM_READ|CR9_SRCS,ioaddr);DELAY_5US;

/* Structure/enum declaration ------------------------------- */
struct tx_desc {
	u32 tdes0, tdes1, tdes2, tdes3;
	u32 tx_skb_ptr;
	u32 tx_buf_ptr;
	u32 next_tx_desc;
	u32 reserved;
};

struct rx_desc {
	u32 rdes0, rdes1, rdes2, rdes3;
	u32 rx_skb_ptr;
	u32 rx_buf_ptr;
	u32 next_rx_desc;
	u32 reserved;
};

struct dmfe_board_info {
	u32 chip_id;		/* Chip vendor/Device ID */
	u32 chip_revesion;	/* Chip revesion */
	struct net_device *next_dev;	/* next device */

	struct pci_dev *net_dev;	/* PCI device */

	u32 ioaddr;		/* I/O base address */
	u32 cr5_data;
	u32 cr6_data;
	u32 cr7_data;
	u32 cr15_data;

/* descriptor pointer */
	unsigned char *buf_pool_ptr;	/* Tx buffer pool memory */
	unsigned char *buf_pool_start;	/* Tx buffer pool align dword */
	unsigned char *desc_pool_ptr;	/* descriptor pool memory */
	struct tx_desc *first_tx_desc;
	struct tx_desc *tx_insert_ptr;
	struct tx_desc *tx_remove_ptr;
	struct rx_desc *first_rx_desc;
	struct rx_desc *rx_insert_ptr;
	struct rx_desc *rx_ready_ptr;	/* packet come pointer */
	u32 tx_packet_cnt;	/* transmitted packet count */
	u32 rx_avail_cnt;	/* available rx descriptor count */
	u32 interval_rx_cnt;	/* rx packet count a callback time */

	u8 media_mode;		/* user specify media mode */
	u8 op_mode;		/* real work media mode */
	u8 phy_addr;
	u8 link_failed;		/* Ever link failed */
	u8 wait_reset;		/* Hardware failed, need to reset */
	u8 in_reset_state;	/* Now driver in reset routine */
	u8 rx_error_cnt;	/* recievd abnormal case count */
	u8 dm910x_chk_mode;	/* Operating mode check */
	struct timer_list timer;
	struct enet_statistics stats;	/* statistic counter */
	unsigned char srom[128];
};

enum dmfe_offsets {
	DCR0 = 0, DCR1 = 0x08, DCR2 = 0x10, DCR3 = 0x18, DCR4 = 0x20, DCR5 = 0x28,
	DCR6 = 0x30, DCR7 = 0x38, DCR8 = 0x40, DCR9 = 0x48, DCR10 = 0x50, DCR11 = 0x58,
	DCR12 = 0x60, DCR13 = 0x68, DCR14 = 0x70, DCR15 = 0x78
};

enum dmfe_CR6_bits {
	CR6_RXSC = 0x2, CR6_PBF = 0x8, CR6_PM = 0x40, CR6_PAM = 0x80, CR6_FDM = 0x200,
	CR6_TXSC = 0x2000, CR6_STI = 0x100000, CR6_SFT = 0x200000, CR6_RXA = 0x40000000
};

/* Global variable declaration ----------------------------- */

static int dmfe_debug = 0;
static unsigned char dmfe_media_mode = 8;
static struct net_device *dmfe_root_dev = NULL;		/* First device */
static u32 dmfe_cr6_user_set = 0;

/* For module input parameter */
static int debug = 0;
static u32 cr6set = 0;
static unsigned char mode = 8;
static u8 chkmode = 1;

unsigned long CrcTable[256] =
{
	0x00000000L, 0x77073096L, 0xEE0E612CL, 0x990951BAL,
	0x076DC419L, 0x706AF48FL, 0xE963A535L, 0x9E6495A3L,
	0x0EDB8832L, 0x79DCB8A4L, 0xE0D5E91EL, 0x97D2D988L,
	0x09B64C2BL, 0x7EB17CBDL, 0xE7B82D07L, 0x90BF1D91L,
	0x1DB71064L, 0x6AB020F2L, 0xF3B97148L, 0x84BE41DEL,
	0x1ADAD47DL, 0x6DDDE4EBL, 0xF4D4B551L, 0x83D385C7L,
	0x136C9856L, 0x646BA8C0L, 0xFD62F97AL, 0x8A65C9ECL,
	0x14015C4FL, 0x63066CD9L, 0xFA0F3D63L, 0x8D080DF5L,
	0x3B6E20C8L, 0x4C69105EL, 0xD56041E4L, 0xA2677172L,
	0x3C03E4D1L, 0x4B04D447L, 0xD20D85FDL, 0xA50AB56BL,
	0x35B5A8FAL, 0x42B2986CL, 0xDBBBC9D6L, 0xACBCF940L,
	0x32D86CE3L, 0x45DF5C75L, 0xDCD60DCFL, 0xABD13D59L,
	0x26D930ACL, 0x51DE003AL, 0xC8D75180L, 0xBFD06116L,
	0x21B4F4B5L, 0x56B3C423L, 0xCFBA9599L, 0xB8BDA50FL,
	0x2802B89EL, 0x5F058808L, 0xC60CD9B2L, 0xB10BE924L,
	0x2F6F7C87L, 0x58684C11L, 0xC1611DABL, 0xB6662D3DL,
	0x76DC4190L, 0x01DB7106L, 0x98D220BCL, 0xEFD5102AL,
	0x71B18589L, 0x06B6B51FL, 0x9FBFE4A5L, 0xE8B8D433L,
	0x7807C9A2L, 0x0F00F934L, 0x9609A88EL, 0xE10E9818L,
	0x7F6A0DBBL, 0x086D3D2DL, 0x91646C97L, 0xE6635C01L,
	0x6B6B51F4L, 0x1C6C6162L, 0x856530D8L, 0xF262004EL,
	0x6C0695EDL, 0x1B01A57BL, 0x8208F4C1L, 0xF50FC457L,
	0x65B0D9C6L, 0x12B7E950L, 0x8BBEB8EAL, 0xFCB9887CL,
	0x62DD1DDFL, 0x15DA2D49L, 0x8CD37CF3L, 0xFBD44C65L,
	0x4DB26158L, 0x3AB551CEL, 0xA3BC0074L, 0xD4BB30E2L,
	0x4ADFA541L, 0x3DD895D7L, 0xA4D1C46DL, 0xD3D6F4FBL,
	0x4369E96AL, 0x346ED9FCL, 0xAD678846L, 0xDA60B8D0L,
	0x44042D73L, 0x33031DE5L, 0xAA0A4C5FL, 0xDD0D7CC9L,
	0x5005713CL, 0x270241AAL, 0xBE0B1010L, 0xC90C2086L,
	0x5768B525L, 0x206F85B3L, 0xB966D409L, 0xCE61E49FL,
	0x5EDEF90EL, 0x29D9C998L, 0xB0D09822L, 0xC7D7A8B4L,
	0x59B33D17L, 0x2EB40D81L, 0xB7BD5C3BL, 0xC0BA6CADL,
	0xEDB88320L, 0x9ABFB3B6L, 0x03B6E20CL, 0x74B1D29AL,
	0xEAD54739L, 0x9DD277AFL, 0x04DB2615L, 0x73DC1683L,
	0xE3630B12L, 0x94643B84L, 0x0D6D6A3EL, 0x7A6A5AA8L,
	0xE40ECF0BL, 0x9309FF9DL, 0x0A00AE27L, 0x7D079EB1L,
	0xF00F9344L, 0x8708A3D2L, 0x1E01F268L, 0x6906C2FEL,
	0xF762575DL, 0x806567CBL, 0x196C3671L, 0x6E6B06E7L,
	0xFED41B76L, 0x89D32BE0L, 0x10DA7A5AL, 0x67DD4ACCL,
	0xF9B9DF6FL, 0x8EBEEFF9L, 0x17B7BE43L, 0x60B08ED5L,
	0xD6D6A3E8L, 0xA1D1937EL, 0x38D8C2C4L, 0x4FDFF252L,
	0xD1BB67F1L, 0xA6BC5767L, 0x3FB506DDL, 0x48B2364BL,
	0xD80D2BDAL, 0xAF0A1B4CL, 0x36034AF6L, 0x41047A60L,
	0xDF60EFC3L, 0xA867DF55L, 0x316E8EEFL, 0x4669BE79L,
	0xCB61B38CL, 0xBC66831AL, 0x256FD2A0L, 0x5268E236L,
	0xCC0C7795L, 0xBB0B4703L, 0x220216B9L, 0x5505262FL,
	0xC5BA3BBEL, 0xB2BD0B28L, 0x2BB45A92L, 0x5CB36A04L,
	0xC2D7FFA7L, 0xB5D0CF31L, 0x2CD99E8BL, 0x5BDEAE1DL,
	0x9B64C2B0L, 0xEC63F226L, 0x756AA39CL, 0x026D930AL,
	0x9C0906A9L, 0xEB0E363FL, 0x72076785L, 0x05005713L,
	0x95BF4A82L, 0xE2B87A14L, 0x7BB12BAEL, 0x0CB61B38L,
	0x92D28E9BL, 0xE5D5BE0DL, 0x7CDCEFB7L, 0x0BDBDF21L,
	0x86D3D2D4L, 0xF1D4E242L, 0x68DDB3F8L, 0x1FDA836EL,
	0x81BE16CDL, 0xF6B9265BL, 0x6FB077E1L, 0x18B74777L,
	0x88085AE6L, 0xFF0F6A70L, 0x66063BCAL, 0x11010B5CL,
	0x8F659EFFL, 0xF862AE69L, 0x616BFFD3L, 0x166CCF45L,
	0xA00AE278L, 0xD70DD2EEL, 0x4E048354L, 0x3903B3C2L,
	0xA7672661L, 0xD06016F7L, 0x4969474DL, 0x3E6E77DBL,
	0xAED16A4AL, 0xD9D65ADCL, 0x40DF0B66L, 0x37D83BF0L,
	0xA9BCAE53L, 0xDEBB9EC5L, 0x47B2CF7FL, 0x30B5FFE9L,
	0xBDBDF21CL, 0xCABAC28AL, 0x53B39330L, 0x24B4A3A6L,
	0xBAD03605L, 0xCDD70693L, 0x54DE5729L, 0x23D967BFL,
	0xB3667A2EL, 0xC4614AB8L, 0x5D681B02L, 0x2A6F2B94L,
	0xB40BBE37L, 0xC30C8EA1L, 0x5A05DF1BL, 0x2D02EF8DL
};

/* function declaration ------------------------------------- */
int dmfe_reg_board(void);
static int dmfe_open(struct net_device *);
static int dmfe_start_xmit(struct sk_buff *, struct net_device *);
static int dmfe_stop(struct net_device *);
static struct enet_statistics *dmfe_get_stats(struct net_device *);
static void dmfe_set_filter_mode(struct net_device *);
static int dmfe_do_ioctl(struct net_device *, struct ifreq *, int);
static u16 read_srom_word(long, int);
static void dmfe_interrupt(int, void *, struct pt_regs *);
static void dmfe_descriptor_init(struct dmfe_board_info *, u32);
static void allocated_rx_buffer(struct dmfe_board_info *);
static void update_cr6(u32, u32);
static void send_filter_frame(struct net_device *, int);
static u16 phy_read(u32, u8, u8);
static void phy_write(u32, u8, u8, u16);
static void phy_write_1bit(u32, u32);
static u16 phy_read_1bit(u32);
static void parser_ctrl_info(struct dmfe_board_info *);
static void dmfe_sense_speed(struct dmfe_board_info *);
static void dmfe_process_mode(struct dmfe_board_info *);
static void dmfe_timer(unsigned long);
static void dmfe_rx_packet(struct net_device *, struct dmfe_board_info *);
static void dmfe_reused_skb(struct dmfe_board_info *, struct sk_buff *);
static void dmfe_dynamic_reset(struct net_device *);
static void dmfe_free_rxbuffer(struct dmfe_board_info *);
static void dmfe_init_dm910x(struct net_device *);
static unsigned long cal_CRC(unsigned char *, unsigned int);

/* DM910X network board routine ---------------------------- */

/*
 *	Search DM910X board, allocate space and register it
 */
 
int __init dmfe_reg_board(void)
{
	u32 pci_iobase;
	u16 dm9102_count = 0;
	u8 pci_irqline;
	static int index = 0;	/* For multiple call */
	struct dmfe_board_info *db;	/* Point a board information structure */
	int i;
	struct pci_dev *net_dev = NULL;
	struct net_device *dev;

	DMFE_DBUG(0, "dmfe_reg_board()", 0);

	if (!pci_present())
		return -ENODEV;

	index = 0;
	while ((net_dev = pci_find_class(PCI_CLASS_NETWORK_ETHERNET << 8, net_dev)))
	{
		u32 pci_id;
		u8 pci_cmd;

		index++;
		if (pci_read_config_dword(net_dev, PCI_VENDOR_ID, &pci_id) != DMFE_SUCC)
			continue;

		if (pci_id != PCI_DM9102_ID)
			continue;

		pci_iobase = net_dev->resource[0].start;
		pci_irqline = net_dev->irq;
				
		/* Enable Master/IO access, Disable memory access */
		
		pci_set_master(net_dev);
		
		pci_read_config_byte(net_dev, PCI_COMMAND, &pci_cmd);
		pci_cmd |= PCI_COMMAND_IO;
		pci_cmd &= ~PCI_COMMAND_MEMORY;
		pci_write_config_byte(net_dev, PCI_COMMAND, pci_cmd);

		/* Set Latency Timer 80h */
		
		/* FIXME: setting values > 32 breaks some SiS 559x stuff.
		   Need a PCI quirk.. */
		   
		pci_write_config_byte(net_dev, PCI_LATENCY_TIMER, 0x80);

		/* IO range and interrupt check */

		if (check_region(pci_iobase, DM9102_IO_SIZE))	/* IO range check */
			continue;

		/* Found DM9102 card and PCI resource allocated OK */
		dm9102_count++;	/* Found a DM9102 card */

		/* Init network device */
		dev = init_etherdev(NULL, 0);

		/* Allocated board information structure */
		db = (void *) (kmalloc(sizeof(*db), GFP_KERNEL | GFP_DMA));
		if(db==NULL)
			continue;	/* Out of memory */
			
		memset(db, 0, sizeof(*db));
		dev->priv = db;	/* link device and board info */
		db->next_dev = dmfe_root_dev;
		dmfe_root_dev = dev;

		db->chip_id = pci_id;	/* keep Chip vandor/Device ID */
		db->ioaddr = pci_iobase;
		pci_read_config_dword(net_dev, 8, &db->chip_revesion);

		db->net_dev = net_dev;

		dev->base_addr = pci_iobase;
		dev->irq = pci_irqline;
		dev->open = &dmfe_open;
		dev->hard_start_xmit = &dmfe_start_xmit;
		dev->stop = &dmfe_stop;
		dev->get_stats = &dmfe_get_stats;
		dev->set_multicast_list = &dmfe_set_filter_mode;
		dev->do_ioctl = &dmfe_do_ioctl;

		request_region(pci_iobase, DM9102_IO_SIZE, dev->name);

		/* read 64 word srom data */
		for (i = 0; i < 64; i++)
			((u16 *) db->srom)[i] = read_srom_word(pci_iobase, i);

		/* Set Node address */
		for (i = 0; i < 6; i++)
			dev->dev_addr[i] = db->srom[20 + i];

	}

#ifdef MODULE
	if (!dm9102_count)
		printk(KERN_WARNING "dmfe: Can't find DM910X board\n");
#endif		
	return dm9102_count ? 0 : -ENODEV;
}

/*
 *	Open the interface.
 *	The interface is opened whenever "ifconfig" actives it.
 */
 
static int dmfe_open(struct net_device *dev)
{
	struct dmfe_board_info *db = dev->priv;

	DMFE_DBUG(0, "dmfe_open", 0);

	if (request_irq(dev->irq, &dmfe_interrupt, SA_SHIRQ, dev->name, dev))
		return -EAGAIN;

	/* Allocated Tx/Rx descriptor memory */
	db->desc_pool_ptr = kmalloc(sizeof(struct tx_desc) * DESC_ALL_CNT + 0x20, GFP_KERNEL | GFP_DMA);
	if (db->desc_pool_ptr == NULL)
		return -ENOMEM;

	if ((u32) db->desc_pool_ptr & 0x1f)
		db->first_tx_desc = (struct tx_desc *) (((u32) db->desc_pool_ptr & ~0x1f) + 0x20);
	else
		db->first_tx_desc = (struct tx_desc *) db->desc_pool_ptr;

	/* Allocated Tx buffer memory */
	
	db->buf_pool_ptr = kmalloc(TX_BUF_ALLOC * TX_DESC_CNT + 4, GFP_KERNEL | GFP_DMA);
	if (db->buf_pool_ptr == NULL) {
		kfree(db->desc_pool_ptr);
		return -ENOMEM;
	}
	
	if ((u32) db->buf_pool_ptr & 0x3)
		db->buf_pool_start = (char *) (((u32) db->buf_pool_ptr & ~0x3) + 0x4);
	else
		db->buf_pool_start = db->buf_pool_ptr;

	/* system variable init */
	db->cr6_data = CR6_DEFAULT | dmfe_cr6_user_set;
	db->tx_packet_cnt = 0;
	db->rx_avail_cnt = 0;
	db->link_failed = 0;
	db->wait_reset = 0;
	db->in_reset_state = 0;
	db->rx_error_cnt = 0;

	if (chkmode && (db->chip_revesion < 0x02000030)) {
		db->dm910x_chk_mode = 1;	/* Enter the check mode */
	} else {
		db->dm910x_chk_mode = 4;	/* Enter the normal mode */
	}

	/* Initilize DM910X board */
	dmfe_init_dm910x(dev);

	/* Active System Interface */
	dev->tbusy = 0;		/* Can transmit packet */
	dev->start = 1;		/* interface ready */
	MOD_INC_USE_COUNT;

	/* set and active a timer process */
	init_timer(&db->timer);
	db->timer.expires = DMFE_TIMER_WUT;
	db->timer.data = (unsigned long) dev;
	db->timer.function = &dmfe_timer;
	add_timer(&db->timer);

	return 0;
}

/*
 *	Initialize DM910X board
 *	Reset DM910X board
 *	Initialize TX/Rx descriptor chain structure
 *	Send the set-up frame
 *	Enable Tx/Rx machine
 */
 
static void dmfe_init_dm910x(struct net_device *dev)
{
	struct dmfe_board_info *db = dev->priv;
	u32 ioaddr = db->ioaddr;

	DMFE_DBUG(0, "dmfe_init_dm910x()", 0);

	/* Reset DM910x board : need 32 PCI clock to complete */
	outl(DM910X_RESET, ioaddr + DCR0);
	DELAY_5US;
	outl(0, ioaddr + DCR0);

	outl(0x180, ioaddr + DCR12);	/* Let bit 7 output port */
	outl(0x80, ioaddr + DCR12);	/* Reset DM9102 phyxcer */
	outl(0x0, ioaddr + DCR12);	/* Clear RESET signal */

	/* Parser control information: Phy addr */
	parser_ctrl_info(db);
	db->media_mode = dmfe_media_mode;
	if (db->media_mode & DMFE_AUTO)
		dmfe_sense_speed(db);
	else
		db->op_mode = db->media_mode;
	dmfe_process_mode(db);

	/* Initiliaze Transmit/Receive decriptor and CR3/4 */
	dmfe_descriptor_init(db, ioaddr);

	/* Init CR6 to program DM910x operation */
	update_cr6(db->cr6_data, ioaddr);

	/* Send setup frame */
	send_filter_frame(dev, 0);

	/* Init CR5/CR7, interrupt active bit */
	outl(0xffffffff, ioaddr + DCR5);	/* clear all CR5 status */
	db->cr7_data = CR7_DEFAULT;
	outl(db->cr7_data, ioaddr + DCR7);

	/* Init CR15, Tx jabber and Rx watchdog timer */
	db->cr15_data = CR15_DEFAULT;
	outl(db->cr15_data, ioaddr + DCR15);

	/* Enable DM910X Tx/Rx function */
	db->cr6_data |= CR6_RXSC | CR6_TXSC;
	update_cr6(db->cr6_data, ioaddr);

}


/*
 *	Hardware start transmission.
 *	Send a packet to media from the upper layer.
 */
 
static int dmfe_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct dmfe_board_info *db = dev->priv;
	struct tx_desc *txptr;

	DMFE_DBUG(0, "dmfe_start_xmit", 0);

	if ((dev->tbusy == 1) && (db->tx_packet_cnt != 0))
		return 1;
	else
		dev->tbusy = 0;

	/* Too large packet check */
	if (skb->len > MAX_PACKET_SIZE) {
		printk(KERN_ERR "%s: oversized frame (%d bytes) received.\n", dev->name, (u16) skb->len);
		dev_kfree_skb(skb);
		return 0;
	}
	/* No Tx resource check, it never happen nromally */
	if (db->tx_packet_cnt >= TX_FREE_DESC_CNT) {
		printk(KERN_WARNING "%s: No Tx resource, enter xmit() again \n", dev->name);
		dev_kfree_skb(skb);
		dev->tbusy = 1;
		return -EBUSY;
	}

	/* transmit this packet */
	txptr = db->tx_insert_ptr;
	memcpy((char *) txptr->tx_buf_ptr, (char *) skb->data, skb->len);
	txptr->tdes1 = 0xe1000000 | skb->len;
	txptr->tdes0 = 0x80000000;	/* set owner bit to DM910X */

	/* Point to next transmit free descriptor */
	db->tx_insert_ptr = (struct tx_desc *) txptr->next_tx_desc;

	/* transmit counter increase 1 */
	db->tx_packet_cnt++;
	db->stats.tx_packets++;

	/* issue Tx polling command */
	outl(0x1, dev->base_addr + DCR1);

	/* Tx resource check */
	if (db->tx_packet_cnt >= TX_FREE_DESC_CNT)
		dev->tbusy = 1;

	/* Set transmit time stamp */
	dev->trans_start = jiffies;	/* saved the time stamp */

	/* free this SKB */
	dev_kfree_skb(skb);
	return 0;
}

/*
 *	Stop the interface.
 *	The interface is stopped when it is brought.
 */

static int dmfe_stop(struct net_device *dev)
{
	struct dmfe_board_info *db = dev->priv;
	u32 ioaddr = dev->base_addr;

	DMFE_DBUG(0, "dmfe_stop", 0);

	/* disable system */
	dev->start = 0;		/* interface disable */
	dev->tbusy = 1;		/* can't transmit */

	/* Reset & stop DM910X board */
	outl(DM910X_RESET, ioaddr + DCR0);
	DELAY_5US;

	/* deleted timer */
	del_timer(&db->timer);

	/* free interrupt */
	free_irq(dev->irq, dev);

	/* free allocated rx buffer */
	dmfe_free_rxbuffer(db);

	/* free all descriptor memory and buffer memory */
	kfree(db->desc_pool_ptr);
	kfree(db->buf_pool_ptr);

	MOD_DEC_USE_COUNT;

	return 0;
}

/*
 *	DM9102 insterrupt handler
 *	receive the packet to upper layer, free the transmitted packet
 */

static void dmfe_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = dev_id;
	struct tx_desc *txptr;
	struct dmfe_board_info *db;
	u32 ioaddr;

	if (!dev) {
		DMFE_DBUG(1, "dmfe_interrupt() without device arg", 0);
		return;
	}
	if (dev->interrupt) {
		DMFE_DBUG(1, "dmfe_interrupt() re-entry ", 0);
		return;
	}

	/* A real interrupt coming */
	dev->interrupt = 1;	/* Lock interrupt */
	db = (struct dmfe_board_info *) dev->priv;
	ioaddr = dev->base_addr;

	DMFE_DBUG(0, "dmfe_interrupt()", 0);

	/* Disable all interrupt in CR7 to solve the interrupt edge problem */
	outl(0, ioaddr + DCR7);

	/* Got DM910X status */
	db->cr5_data = inl(ioaddr + DCR5);
	outl(db->cr5_data, ioaddr + DCR5);
	/* printk("CR5=%x\n", db->cr5_data); */

	/* Check system status */
	if (db->cr5_data & 0x2000) {
		/* A system bus error occurred */
		DMFE_DBUG(1, "A system bus error occurred. CR5=", db->cr5_data);
		dev->tbusy = 1;
		db->wait_reset = 1;		/* Need to RESET */
		outl(0, ioaddr + DCR7);		/* disable all interrupt */
		dev->interrupt = 0;		/* unlock interrupt */
		return;
	}
	/* Free the transmitted descriptor */
	txptr = db->tx_remove_ptr;
	while (db->tx_packet_cnt) {
		/* printk("tdes0=%x\n", txptr->tdes0); */
		if (txptr->tdes0 & 0x80000000)
			break;
		if ((txptr->tdes0 & TDES0_ERR_MASK) && (txptr->tdes0 != 0x7fffffff)) {
			/* printk("tdes0=%x\n", txptr->tdes0); */
			db->stats.tx_errors++;
		}
		txptr = (struct tx_desc *) txptr->next_tx_desc;
		db->tx_packet_cnt--;
	}
	db->tx_remove_ptr = (struct tx_desc *) txptr;

	if (dev->tbusy && (db->tx_packet_cnt < TX_FREE_DESC_CNT)) {
		dev->tbusy = 0;		/* free a resource */
		mark_bh(NET_BH);	/* active bottom half */
	}
	/* Received the coming packet */
	if (db->rx_avail_cnt)
		dmfe_rx_packet(dev, db);

	/* reallocated rx descriptor buffer */
	if (db->rx_avail_cnt < RX_DESC_CNT)
		allocated_rx_buffer(db);

	/* Mode Check */
	if (db->dm910x_chk_mode & 0x2) {
		db->dm910x_chk_mode = 0x4;
		db->cr6_data |= 0x100;
		update_cr6(db->cr6_data, db->ioaddr);
	}
	dev->interrupt = 0;	/* release interrupt lock */

	/* Restore CR7 to enable interrupt mask */
	
	if (db->interval_rx_cnt > RX_MAX_TRAFFIC)
		db->cr7_data = 0x1a28d;
	else
		db->cr7_data = 0x1a2cd;
	outl(db->cr7_data, ioaddr + DCR7);
}

/*
 *	Receive the come packet and pass to upper layer
 */
 
static void dmfe_rx_packet(struct net_device *dev, struct dmfe_board_info *db)
{
	struct rx_desc *rxptr;
	struct sk_buff *skb;
	int rxlen;

	rxptr = db->rx_ready_ptr;

	while (db->rx_avail_cnt) {
		if (rxptr->rdes0 & 0x80000000)	/* packet owner check */
			break;

		db->rx_avail_cnt--;
		db->interval_rx_cnt++;

		if ((rxptr->rdes0 & 0x300) != 0x300) {
			/* A packet without First/Last flag */
			/* reused this SKB */
			DMFE_DBUG(0, "Reused SK buffer, rdes0", rxptr->rdes0);
			dmfe_reused_skb(db, (struct sk_buff *) rxptr->rx_skb_ptr);
			db->rx_error_cnt++;
		} else {
			rxlen = ((rxptr->rdes0 >> 16) & 0x3fff) - 4;	/* skip CRC */

			/* A packet with First/Last flag */
			if (rxptr->rdes0 & 0x8000) {	/* error summary bit check */
				/* This is a error packet */
				/* printk("rdes0 error : %x \n", rxptr->rdes0); */
				db->stats.rx_errors++;
				if (rxptr->rdes0 & 1)
					db->stats.rx_fifo_errors++;
				if (rxptr->rdes0 & 2)
					db->stats.rx_crc_errors++;
				if (rxptr->rdes0 & 0x80)
					db->stats.rx_length_errors++;
			}
			if (!(rxptr->rdes0 & 0x8000) ||
			    ((db->cr6_data & CR6_PM) && (rxlen > 6))) {
				skb = (struct sk_buff *) rxptr->rx_skb_ptr;

				/* Received Packet CRC check need or not */
				if ((db->dm910x_chk_mode & 1) && (cal_CRC(skb->tail, rxlen) != (*(unsigned long *) (skb->tail + rxlen)))) {
					/* Found a error received packet */
					dmfe_reused_skb(db, (struct sk_buff *) rxptr->rx_skb_ptr);
					db->dm910x_chk_mode = 3;
				} else {
					/* A good packet coming, send to upper layer */
					skb->dev = dev;
					skb_put(skb, rxlen);
					skb->protocol = eth_type_trans(skb, dev);
					netif_rx(skb);	/* Send to upper layer */
					/* skb->ip_summed = CHECKSUM_UNNECESSARY; */
					dev->last_rx = jiffies;
					db->stats.rx_packets++;
				}
			} else {
				DMFE_DBUG(0, "Reused SK buffer, rdes0", rxptr->rdes0);
				dmfe_reused_skb(db, (struct sk_buff *) rxptr->rx_skb_ptr);
			}
		}

		rxptr = (struct rx_desc *) rxptr->next_rx_desc;
	}

	db->rx_ready_ptr = rxptr;
}

/*
 *	Get statistics from driver.
 */
 
static struct enet_statistics *dmfe_get_stats(struct net_device *dev)
{
	struct dmfe_board_info *db = (struct dmfe_board_info *) dev->priv;

	DMFE_DBUG(0, "dmfe_get_stats", 0);
	return &db->stats;
}

/*
 *	Set DM910X multicast address
 */
 
static void dmfe_set_filter_mode(struct net_device *dev)
{
	struct dmfe_board_info *db = dev->priv;

	DMFE_DBUG(0, "dmfe_set_filter_mode()", 0);

	if (dev->flags & IFF_PROMISC) {
		DMFE_DBUG(0, "Enable PROM Mode", 0);
		db->cr6_data |= CR6_PM | CR6_PBF;
		update_cr6(db->cr6_data, db->ioaddr);
		return;
	}
	if (dev->flags & IFF_ALLMULTI || dev->mc_count > DMFE_MAX_MULTICAST) {
		DMFE_DBUG(0, "Pass all multicast address", dev->mc_count);
		db->cr6_data &= ~(CR6_PM | CR6_PBF);
		db->cr6_data |= CR6_PAM;
		return;
	}
	DMFE_DBUG(0, "Set multicast address", dev->mc_count);
	send_filter_frame(dev, dev->mc_count);
}

/*
 *	Process the upper socket ioctl command
 */
 
static int dmfe_do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	DMFE_DBUG(0, "dmfe_do_ioctl()", 0);
	return 0;
}

/*
 *	A periodic timer routine
 *	Dynamic media sense, allocated Rx buffer...
 */
 
static void dmfe_timer(unsigned long data)
{
	u32 tmp_cr8;
	unsigned char tmp_cr12;
	struct net_device *dev = (struct net_device *) data;
	struct dmfe_board_info *db = (struct dmfe_board_info *) dev->priv;

	DMFE_DBUG(0, "dmfe_timer()", 0);

	/* Do reset now */
	if (db->in_reset_state)
		return;

	/* Operating Mode Check */
	if ((db->dm910x_chk_mode & 0x1) && (db->stats.rx_packets > MAX_CHECK_PACKET)) {
		db->dm910x_chk_mode = 0x4;
	}
	/* Dynamic reset DM910X : system error or transmit time-out */
	tmp_cr8 = inl(db->ioaddr + DCR8);
	if ((db->interval_rx_cnt == 0) && (tmp_cr8)) {
		db->wait_reset = 1;
		/* printk("CR8 %x, Interval Rx %x\n", tmp_cr8, db->interval_rx_cnt); */
	}
	/* Receiving Traffic check */
	if (db->interval_rx_cnt > RX_MAX_TRAFFIC)
		db->cr7_data = 0x1a28d;
	else
		db->cr7_data = 0x1a2cd;
	outl(db->cr7_data, db->ioaddr + DCR7);

	db->interval_rx_cnt = 0;

	if (db->wait_reset | (db->tx_packet_cnt &&
			      ((jiffies - dev->trans_start) > DMFE_TX_TIMEOUT)) | (db->rx_error_cnt > 3)) {
		/* printk("wait_reset %x, tx cnt %x, rx err %x, time %x\n", db->wait_reset, db->tx_packet_cnt, db->rx_error_cnt, jiffies-dev->trans_start); */
		DMFE_DBUG(0, "Warn!! Warn!! Tx/Rx moniotr step1", db->tx_packet_cnt);
		dmfe_dynamic_reset(dev);
		db->timer.expires = DMFE_TIMER_WUT;
		add_timer(&db->timer);
		return;
	}
	db->rx_error_cnt = 0;	/* Clear previous counter */

	/* Link status check, Dynamic media type change */
	tmp_cr12 = inb(db->ioaddr + DCR12);
	if (db->chip_revesion == 0x02000030) {
		if (tmp_cr12 & 2)
			tmp_cr12 = 0x0;		/* Link failed */
		else
			tmp_cr12 = 0x3;		/* Link OK */
	}
	if (!(tmp_cr12 & 0x3) && !db->link_failed) {
		/* Link Failed */
		DMFE_DBUG(0, "Link Failed", tmp_cr12);
		db->link_failed = 1;
		phy_write(db->ioaddr, db->phy_addr, 0, 0x8000);		/* reset Phy controller */
	} else if ((tmp_cr12 & 0x3) && db->link_failed) {
		DMFE_DBUG(0, "Link link OK", tmp_cr12);
		db->link_failed = 0;
		if (db->media_mode & DMFE_AUTO)
			dmfe_sense_speed(db);
		dmfe_process_mode(db);
		update_cr6(db->cr6_data, db->ioaddr);
		/* SHOW_MEDIA_TYPE(db->op_mode); */
	}
	/* reallocated rx descriptor buffer */
	if (db->rx_avail_cnt < RX_DESC_CNT)
		allocated_rx_buffer(db);

	/* Timer active again */
	db->timer.expires = DMFE_TIMER_WUT;
	add_timer(&db->timer);
}

/*
 *	Dynamic reset the DM910X board
 *	Stop DM910X board
 *	Free Tx/Rx allocated memory
 *	Reset DM910X board
 *	Re-initilize DM910X board
 */
 
static void dmfe_dynamic_reset(struct net_device *dev)
{
	struct dmfe_board_info *db = dev->priv;

	DMFE_DBUG(0, "dmfe_dynamic_reset()", 0);

	/* Enter dynamic reset route */
	db->in_reset_state = 1;

	/* Disable upper layer interface */
	dev->tbusy = 1;		/* transmit packet disable */
	dev->start = 0;		/* interface not ready */

	db->cr6_data &= ~(CR6_RXSC | CR6_TXSC);		/* Disable Tx/Rx */
	update_cr6(db->cr6_data, dev->base_addr);

	/* Free Rx Allocate buffer */
	dmfe_free_rxbuffer(db);

	/* system variable init */
	db->tx_packet_cnt = 0;
	db->rx_avail_cnt = 0;
	db->link_failed = 0;
	db->wait_reset = 0;
	db->rx_error_cnt = 0;

	/* Re-initilize DM910X board */
	dmfe_init_dm910x(dev);

	/* Restart upper layer interface */
	dev->tbusy = 0;		/* Can transmit packet */
	dev->start = 1;		/* interface ready */

	/* Leave dynamic reser route */
	db->in_reset_state = 0;
}

/*
 *	Free all allocated rx buffer 
 */

static void dmfe_free_rxbuffer(struct dmfe_board_info *db)
{
	DMFE_DBUG(0, "dmfe_free_rxbuffer()", 0);

	/* free allocated rx buffer */
	while (db->rx_avail_cnt) {
		dev_kfree_skb((void *) (db->rx_ready_ptr->rx_skb_ptr));
		db->rx_ready_ptr = (struct rx_desc *) db->rx_ready_ptr->next_rx_desc;
		db->rx_avail_cnt--;
	}
}

/*
 *	Reused the SK buffer
 */
 
static void dmfe_reused_skb(struct dmfe_board_info *db, struct sk_buff *skb)
{
	struct rx_desc *rxptr = db->rx_insert_ptr;

	if (!(rxptr->rdes0 & 0x80000000)) {
		rxptr->rx_skb_ptr = (u32) skb;
		rxptr->rdes2 = virt_to_bus(skb->tail);
		rxptr->rdes0 = 0x80000000;
		db->rx_avail_cnt++;
		db->rx_insert_ptr = (struct rx_desc *) rxptr->next_rx_desc;
	} else
		DMFE_DBUG(0, "SK Buffer reused method error", db->rx_avail_cnt);
}

/*
 *	Initialize transmit/Receive descriptor 
 *	Using Chain structure, and allocated Tx/Rx buffer
 */
 
static void dmfe_descriptor_init(struct dmfe_board_info *db, u32 ioaddr)
{
	struct tx_desc *tmp_tx;
	struct rx_desc *tmp_rx;
	unsigned char *tmp_buf;
	int i;

	DMFE_DBUG(0, "dmfe_descriptor_init()", 0);

	/* tx descriptor start pointer */
	db->tx_insert_ptr = db->first_tx_desc;
	db->tx_remove_ptr = db->first_tx_desc;
	outl(virt_to_bus(db->first_tx_desc), ioaddr + DCR4);	/* Init CR4 */

	/* rx descriptor start pointer */
	db->first_rx_desc = (struct rx_desc *)
	    ((u32) db->first_tx_desc + sizeof(struct rx_desc) * TX_DESC_CNT);
	db->rx_insert_ptr = db->first_rx_desc;
	db->rx_ready_ptr = db->first_rx_desc;
	outl(virt_to_bus(db->first_rx_desc), ioaddr + DCR3);	/* Init CR3 */

	/* Init Transmit chain */
	tmp_buf = db->buf_pool_start;
	for (tmp_tx = db->first_tx_desc, i = 0; i < TX_DESC_CNT; i++, tmp_tx++) {
		tmp_tx->tx_buf_ptr = (u32) tmp_buf;
		tmp_tx->tdes0 = 0;
		tmp_tx->tdes1 = 0x81000000;	/* IC, chain */
		tmp_tx->tdes2 = (u32) virt_to_bus(tmp_buf);
		tmp_tx->tdes3 = (u32) virt_to_bus(tmp_tx) + sizeof(struct tx_desc);
		tmp_tx->next_tx_desc = (u32) ((u32) tmp_tx + sizeof(struct tx_desc));
		tmp_buf = (unsigned char *) ((u32) tmp_buf + TX_BUF_ALLOC);
	}
	(--tmp_tx)->tdes3 = (u32) virt_to_bus(db->first_tx_desc);
	tmp_tx->next_tx_desc = (u32) db->first_tx_desc;

	/* Init Receive descriptor chain */
	for (tmp_rx = db->first_rx_desc, i = 0; i < RX_DESC_CNT; i++, tmp_rx++) {
		tmp_rx->rdes0 = 0;
		tmp_rx->rdes1 = 0x01000600;
		tmp_rx->rdes3 = (u32) virt_to_bus(tmp_rx) + sizeof(struct rx_desc);
		tmp_rx->next_rx_desc = (u32) ((u32) tmp_rx + sizeof(struct rx_desc));
	}
	(--tmp_rx)->rdes3 = (u32) virt_to_bus(db->first_rx_desc);
	tmp_rx->next_rx_desc = (u32) db->first_rx_desc;

	/* pre-allocated Rx buffer */
	allocated_rx_buffer(db);
}

/*
 *	Update CR6 vaule
 *	Firstly stop DM910X , then written value and start
 */
 
static void update_cr6(u32 cr6_data, u32 ioaddr)
{
	u32 cr6_tmp;

	cr6_tmp = cr6_data & ~0x2002;	/* stop Tx/Rx */
	outl(cr6_tmp, ioaddr + DCR6);
	DELAY_5US;
	outl(cr6_data, ioaddr + DCR6);
	cr6_tmp = inl(ioaddr + DCR6);
	/* printk("CR6 update %x ", cr6_tmp); */
}

/*
 *	Send a setup frame
 *	This setup frame initilize DM910X addres filter mode
 */
 
static void send_filter_frame(struct net_device *dev, int mc_cnt)
{
	struct dmfe_board_info *db = dev->priv;
	struct dev_mc_list *mcptr;
	struct tx_desc *txptr;
	u16 *addrptr;
	u32 *suptr;
	int i;

	DMFE_DBUG(0, "send_filetr_frame()", 0);

	txptr = db->tx_insert_ptr;
	suptr = (u32 *) txptr->tx_buf_ptr;

	/* broadcast address */
	*suptr++ = 0xffff;
	*suptr++ = 0xffff;
	*suptr++ = 0xffff;

	/* Node address */
	addrptr = (u16 *) dev->dev_addr;
	*suptr++ = addrptr[0];
	*suptr++ = addrptr[1];
	*suptr++ = addrptr[2];

	/* fit the multicast address */
	for (mcptr = dev->mc_list, i = 0; i < mc_cnt; i++, mcptr = mcptr->next) {
		addrptr = (u16 *) mcptr->dmi_addr;
		*suptr++ = addrptr[0];
		*suptr++ = addrptr[1];
		*suptr++ = addrptr[2];
	}

	for (; i < 14; i++) {
		*suptr++ = 0xffff;
		*suptr++ = 0xffff;
		*suptr++ = 0xffff;
	}

	/* prepare the setup frame */
	db->tx_packet_cnt++;
	dev->tbusy = 1;
	txptr->tdes1 = 0x890000c0;
	txptr->tdes0 = 0x80000000;
	db->tx_insert_ptr = (struct tx_desc *) txptr->next_tx_desc;

	update_cr6(db->cr6_data | 0x2000, dev->base_addr);
	outl(0x1, dev->base_addr + DCR1);
	update_cr6(db->cr6_data, dev->base_addr);
	dev->trans_start = jiffies;

}

/*
 *	Allocate rx buffer,
 *	Allocate as many Rx buffers as possible.
 */
static void allocated_rx_buffer(struct dmfe_board_info *db)
{
	struct rx_desc *rxptr;
	struct sk_buff *skb;

	rxptr = db->rx_insert_ptr;

	while (db->rx_avail_cnt < RX_DESC_CNT) {
		if ((skb = alloc_skb(RX_ALLOC_SIZE, GFP_ATOMIC)) == NULL)
			break;
		rxptr->rx_skb_ptr = (u32) skb;
		rxptr->rdes2 = virt_to_bus(skb->tail);
		rxptr->rdes0 = 0x80000000;
		rxptr = (struct rx_desc *) rxptr->next_rx_desc;
		db->rx_avail_cnt++;
	}

	db->rx_insert_ptr = rxptr;
}

/*
 *	Read one word data from the serial ROM
 */
 
static u16 read_srom_word(long ioaddr, int offset)
{
	int i;
	u16 srom_data = 0;
	long cr9_ioaddr = ioaddr + DCR9;

	outl(CR9_SROM_READ, cr9_ioaddr);
	outl(CR9_SROM_READ | CR9_SRCS, cr9_ioaddr);

	/* Send the Read Command 110b */
	SROM_CLK_WRITE(SROM_DATA_1, cr9_ioaddr);
	SROM_CLK_WRITE(SROM_DATA_1, cr9_ioaddr);
	SROM_CLK_WRITE(SROM_DATA_0, cr9_ioaddr);

	/* Send the offset */
	for (i = 5; i >= 0; i--) {
		srom_data = (offset & (1 << i)) ? SROM_DATA_1 : SROM_DATA_0;
		SROM_CLK_WRITE(srom_data, cr9_ioaddr);
	}

	outl(CR9_SROM_READ | CR9_SRCS, cr9_ioaddr);

	for (i = 16; i > 0; i--) {
		outl(CR9_SROM_READ | CR9_SRCS | CR9_SRCLK, cr9_ioaddr);
		DELAY_5US;
		srom_data = (srom_data << 1) | ((inl(cr9_ioaddr) & CR9_CRDOUT) ? 1 : 0);
		outl(CR9_SROM_READ | CR9_SRCS, cr9_ioaddr);
		DELAY_5US;
	}

	outl(CR9_SROM_READ, cr9_ioaddr);
	return srom_data;
}

/*
 *	Parser Control media block to get Phy address
 */
 
static void parser_ctrl_info(struct dmfe_board_info *db)
{
	int i;
	char *sdata = db->srom;
	unsigned char count;

	/* point to info leaf0 */
	count = *(sdata + 33);

	/* Point to First media block */
	sdata += 34;
	for (i = 0; i < count; i++) {
		if (*(sdata + 1) == 1) {
			db->phy_addr = *(sdata + 2);
			break;
		}
		sdata += ((unsigned char) *(sdata) & 0x7f) + 1;
	}

	if (i >= count) {
		printk("Can't found Control Block\n");
		db->phy_addr = 1;
	}
}

/*
 *	Auto sense the media mode
 */
 
static void dmfe_sense_speed(struct dmfe_board_info *db)
{
	int i;
	u16 phy_mode;

	for (i = 1000; i; i--) {
		DELAY_5US;
		phy_mode = phy_read(db->ioaddr, db->phy_addr, 1);
		if ((phy_mode & 0x24) == 0x24)
			break;
	}

	if (i) {
		phy_mode = phy_read(db->ioaddr, db->phy_addr, 17) & 0xf000;
		/* printk("Phy_mode %x ",phy_mode); */
		switch (phy_mode) {
		case 0x1000:
			db->op_mode = DMFE_10MHF;
			break;
		case 0x2000:
			db->op_mode = DMFE_10MFD;
			break;
		case 0x4000:
			db->op_mode = DMFE_100MHF;
			break;
		case 0x8000:
			db->op_mode = DMFE_100MFD;
			break;
		default:
			db->op_mode = DMFE_100MHF;
			DMFE_DBUG(1, "Media Type error, phy reg17", phy_mode);
			break;
		}
	} else {
		db->op_mode = DMFE_100MHF;
		DMFE_DBUG(0, "Link Failed :", phy_mode);
	}
}

/*
 *	Process op-mode
 *	AUTO mode : PHY controller in Auto-negotiation Mode
 *	Force mode: PHY controller in force mode with HUB
 *	N-way force capability with SWITCH
 */
 
static void dmfe_process_mode(struct dmfe_board_info *db)
{
	u16 phy_reg;

	/* Full Duplex Mode Check */
	db->cr6_data &= ~CR6_FDM;	/* Clear Full Duplex Bit */
	if (db->op_mode & 0x4)
		db->cr6_data |= CR6_FDM;

	if (!(db->media_mode & DMFE_AUTO)) {	/* Force Mode Check */
		/* User force the media type */
		phy_reg = phy_read(db->ioaddr, db->phy_addr, 5);
		/* printk("Nway phy_reg5 %x ",phy_reg); */
		if (phy_reg & 0x1) {
			/* parter own the N-Way capability */
			phy_reg = phy_read(db->ioaddr, db->phy_addr, 4) & ~0x1e0;
			switch (db->op_mode) {
			case DMFE_10MHF:
				phy_reg |= 0x20;
				break;
			case DMFE_10MFD:
				phy_reg |= 0x40;
				break;
			case DMFE_100MHF:
				phy_reg |= 0x80;
				break;
			case DMFE_100MFD:
				phy_reg |= 0x100;
				break;
			}
			phy_write(db->ioaddr, db->phy_addr, 4, phy_reg);
		} else {
			/* parter without the N-Way capability */
			switch (db->op_mode) {
			case DMFE_10MHF:
				phy_reg = 0x0;
				break;
			case DMFE_10MFD:
				phy_reg = 0x100;
				break;
			case DMFE_100MHF:
				phy_reg = 0x2000;
				break;
			case DMFE_100MFD:
				phy_reg = 0x2100;
				break;
			}
			phy_write(db->ioaddr, db->phy_addr, 0, phy_reg);
		}
	}
}

/*
 *	Write a word to Phy register
 */
 
static void phy_write(u32 iobase, u8 phy_addr, u8 offset, u16 phy_data)
{
	u16 i;
	u32 ioaddr = iobase + DCR9;

	/* Send 33 synchronization clock to Phy controller */
	for (i = 0; i < 35; i++)
		phy_write_1bit(ioaddr, PHY_DATA_1);

	/* Send start command(01) to Phy */
	phy_write_1bit(ioaddr, PHY_DATA_0);
	phy_write_1bit(ioaddr, PHY_DATA_1);

	/* Send write command(01) to Phy */
	phy_write_1bit(ioaddr, PHY_DATA_0);
	phy_write_1bit(ioaddr, PHY_DATA_1);

	/* Send Phy addres */
	for (i = 0x10; i > 0; i = i >> 1)
		phy_write_1bit(ioaddr, phy_addr & i ? PHY_DATA_1 : PHY_DATA_0);

	/* Send register addres */
	for (i = 0x10; i > 0; i = i >> 1)
		phy_write_1bit(ioaddr, offset & i ? PHY_DATA_1 : PHY_DATA_0);

	/* written trasnition */
	phy_write_1bit(ioaddr, PHY_DATA_1);
	phy_write_1bit(ioaddr, PHY_DATA_0);

	/* Write a word data to PHY controller */
	for (i = 0x8000; i > 0; i >>= 1)
		phy_write_1bit(ioaddr, phy_data & i ? PHY_DATA_1 : PHY_DATA_0);
}

/*
 *	Read a word data from phy register
 */
 
static u16 phy_read(u32 iobase, u8 phy_addr, u8 offset)
{
	int i;
	u16 phy_data;
	u32 ioaddr = iobase + DCR9;

	/* Send 33 synchronization clock to Phy controller */
	for (i = 0; i < 35; i++)
		phy_write_1bit(ioaddr, PHY_DATA_1);

	/* Send start command(01) to Phy */
	phy_write_1bit(ioaddr, PHY_DATA_0);
	phy_write_1bit(ioaddr, PHY_DATA_1);

	/* Send read command(10) to Phy */
	phy_write_1bit(ioaddr, PHY_DATA_1);
	phy_write_1bit(ioaddr, PHY_DATA_0);

	/* Send Phy addres */
	for (i = 0x10; i > 0; i = i >> 1)
		phy_write_1bit(ioaddr, phy_addr & i ? PHY_DATA_1 : PHY_DATA_0);

	/* Send register addres */
	for (i = 0x10; i > 0; i = i >> 1)
		phy_write_1bit(ioaddr, offset & i ? PHY_DATA_1 : PHY_DATA_0);

	/* Skip transition state */
	phy_read_1bit(ioaddr);

	/* read 16bit data */
	for (phy_data = 0, i = 0; i < 16; i++) {
		phy_data <<= 1;
		phy_data |= phy_read_1bit(ioaddr);
	}

	return phy_data;
}

/*
 *	Write one bit data to Phy Controller
 */
 
static void phy_write_1bit(u32 ioaddr, u32 phy_data)
{
	outl(phy_data, ioaddr);	/* MII Clock Low */
	DELAY_1US;
	outl(phy_data | MDCLKH, ioaddr);	/* MII Clock High */
	DELAY_1US;
	outl(phy_data, ioaddr);	/* MII Clock Low */
	DELAY_1US;
}

/*
 *	Read one bit phy data from PHY controller
 */
 
static u16 phy_read_1bit(u32 ioaddr)
{
	u16 phy_data;

	outl(0x50000, ioaddr);
	DELAY_1US;
	phy_data = (inl(ioaddr) >> 19) & 0x1;
	outl(0x40000, ioaddr);
	DELAY_1US;

	return phy_data;
}

/*
 *	Calculate the CRC valude of the Rx packet
 */
 
static unsigned long cal_CRC(unsigned char *Data, unsigned int Len)
{
	unsigned long Crc = 0xffffffff;

	while (Len--) {
		Crc = CrcTable[(Crc ^ *Data++) & 0xFF] ^ (Crc >> 8);
	}

	return ~Crc;

}

#ifdef MODULE

MODULE_AUTHOR("Sten Wang, sten_wang@davicom.com.tw");
MODULE_DESCRIPTION("Davicom DM910X fast ethernet driver");
MODULE_PARM(debug, "i");
MODULE_PARM(mode, "i");
MODULE_PARM(cr6set, "i");
MODULE_PARM(chkmode, "i");

/*	Description: 
 *	when user used insmod to add module, system invoked init_module()
 *	to initilize and register.
 */
 
int init_module(void)
{
	DMFE_DBUG(0, "init_module() ", debug);

	if (debug)
		dmfe_debug = debug;	/* set debug flag */
	if (cr6set)
		dmfe_cr6_user_set = cr6set;

	switch (mode) {
	case 0:
	case 1:
	case 4:
	case 5:
		dmfe_media_mode = mode;
		break;
	default:
		dmfe_media_mode = 8;
		break;
	}

	return dmfe_reg_board();	/* search board and register */
}

/*
 *	Description: 
 *	when user used rmmod to delete module, system invoked clean_module()
 *	to un-register device.
 */
 
void cleanup_module(void)
{
	struct net_device *next_dev;

	DMFE_DBUG(0, "clean_module()", 0);

	while (dmfe_root_dev) {
		next_dev = ((struct dmfe_board_info *) dmfe_root_dev->priv)->next_dev;
		unregister_netdev(dmfe_root_dev);
		release_region(dmfe_root_dev->base_addr, DM9102_IO_SIZE);
		kfree(dmfe_root_dev->priv);	/* free board information */
		kfree(dmfe_root_dev);	/* free device structure */
		dmfe_root_dev = next_dev;
	}
	DMFE_DBUG(0, "clean_module() exit", 0);
}

#endif				/* MODULE */
