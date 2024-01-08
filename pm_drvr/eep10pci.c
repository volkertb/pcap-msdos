/* eep10pci.c: An EtherExpress Pro/10 PCI driver for linux */
/*
    eep10pci
        Written 1996 by John Stalba

    This software may be used and distributed according to the terms
    of the GNU Public License, incorporated herein by reference.

    This driver is for the Intel EtherExpress Pro/10 PCI card, which
    uses the Intel i82596 LAN coprocessor and the PLX9036 PCI interface.

    Sources:

    The `apricot' driver written by Mark Evans
    (which was the basis of the i82596 code), 

    the `3c59x' and the `eepro100' drivers written by Donald Becker
    (which were used for the PCI initialization code),

    and the Crynwr packet driver `eep10pkt' written by Russell Nelson
    (which was the basis for the details of the PLX9036 interface definition).

*/

static const char *version = "eep10pci.c:v0.22 10/31/96\n";

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/bios32.h>

#include <asm/processor.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#define EEP10PCI_DEBUG 1

#ifdef EEP10PCI_DEBUG
int i596_debug = EEP10PCI_DEBUG;
#else
int i596_debug = 1;
#endif


/* PLX9036 register definitions */

#define	PLXP_NODE_ADDR_REGISTER	0x40

#define	PLXP_INTERRUPT_CONTROL	0x00	/* interrupt control register */
#define	    LATCHED_INTWRITE		0x0008
#define	    INT_STAT			0x0010
#define	    LATCHED_INTREAD		0x0010
#define	    INTERRUPT_MASK_BIT		0x0020
#define	    SOFTWARE_INTERRUPT_BIT	0x0040
#define	    LAN_0_INTERRUPT_ENABLE	0x0100
#define	PLXP_USER_PINS		0x04	/* user pins register */
#define	    PLXP_USER_0			0x0001
#define	    PLXP_USER_1			0x0002
#define	    PLXP_USER_2			0x0004
#define	    PLXP_USER_3			0x0008
#define	PLXP_FLASH_CONTROL	0x08	/* flash RAM control register */
#define	PLXP_FLASH_WINDOW	0x0c	/* flash RAM window register */
#define	PLXP_LAN_CONTROL	0x10	/* EEPROM and LAN control register */
#define	    LOAD_CONFIGURATION_BIT	0x0010
#define	    LAN_0_RESET_BIT		0x0100
#define	    SOFTWARE_RESET_BIT		0x1000
#define	PLXP_MASTER_CONTROL	0x14	/* master control register */
#define	    BURST_LENGTH_BIT		0x1000

#define	PLXP_CA_OFFSET		0x20	/* offset for command attention (CA) */
#define	PLXP_PORT_OFFSET	0x24	/* offset for PORT command */
#define	    RESET_CMD			0x00
#define	    SELF_TEST_CMD		0x01
#define	    ALT_SCP_CMD			0x02
#define	    DUMP_CMD			0x03


#define MDELAY_100() \
{ \
	unsigned long timer = jiffies + ((HZ + 9)/10) + 1; \
	while ( timer > jiffies ); \
}

#define I596_NULL -1

#define CMD_EOL		0x8000	/* The last command of the list, stop. */
#define CMD_SUSP	0x4000	/* Suspend after doing cmd. */
#define CMD_INTR	0x2000	/* Interrupt after doing cmd. */

#define CMD_FLEX	0x0008	/* Enable flexible memory model */

enum commands {
	CmdNOp = 0, CmdSASetup = 1, CmdConfigure = 2, CmdMulticastList = 3,
	CmdTx = 4, CmdTDR = 5, CmdDump = 6, CmdDiagnose = 7};

#define STAT_C		0x8000	/* Set to 0 after execution */
#define STAT_B		0x4000	/* Command being executed */
#define STAT_OK		0x2000	/* Command executed ok */
#define STAT_A		0x1000	/* Command aborted */

#define	 CUC_START	0x0100
#define	 CUC_RESUME	0x0200
#define	 CUC_SUSPEND    0x0300
#define	 CUC_ABORT	0x0400
#define	 RX_START	0x0010
#define	 RX_RESUME	0x0020
#define	 RX_SUSPEND	0x0030
#define	 RX_ABORT	0x0040

#define	RU_STATUS_BITS	0x00f0
#define	RU_IDLE		0x0000
#define	RU_SUSPENDED	0x0010
#define	RU_NO_RESOURCES	0x0020
#define	RU_READY	0x0040
#define	RU_NO_RBD	0x0080
#define	RU_NO_RS_NO_RBD	0x00a0

#define	CU_STATUS_BITS	0x0700
#define	CU_IDLE		0x0000
#define	CU_SUSPENDED	0x0100
#define	CU_ACTIVE	0x0200

#define	RNR_ST_BIT	0x1000
#define	CNA_ST_BIT	0x2000
#define	FR_ST_BIT	0x4000
#define	CX_ST_BIT	0x8000

struct i596_cmd {
    unsigned short status;
    unsigned short command;
    struct i596_cmd *next;
};

#define EOF		0x8000
#define SIZE_MASK	0x3fff

struct i596_tbd {
    unsigned short size;
    unsigned short pad;
    struct i596_tbd *next;
    char *data;
};

struct tx_cmd {
    struct i596_cmd cmd;
    struct i596_tbd *tbd;
    unsigned short size;
    unsigned short pad;
    struct sk_buff *skb;
};

struct i596_rfd {
    unsigned short stat;
    unsigned short cmd;
    struct i596_rfd *next;
    long rbd; 
    unsigned short count;
    unsigned short size;
    char data[1532];
};

#define RX_RING_SIZE 16
#define	TX_TIMEOUT   25
#define	TX_FULL_SIZE 16
#define	TX_NOT_FULL  1

struct i596_scb {
    unsigned short status;
    unsigned short command;
    struct i596_cmd *cmd;
    struct i596_rfd *rfd;
    unsigned long crc_err;
    unsigned long align_err;
    unsigned long resource_err;
    unsigned long over_err;
    unsigned long rcvdt_err;
    unsigned long short_err;
    unsigned short t_on;
    unsigned short t_off;
};

struct i596_iscp {
    unsigned long stat;
    struct i596_scb *scb;
};

struct i596_scp {
    unsigned long sysbus;
    unsigned long pad;
    struct i596_iscp *iscp;
};

struct i596_private {
    struct i596_scp scp;
    struct i596_iscp iscp;
    struct i596_scb scb;
    struct i596_cmd set_add;
    char eth_addr[8];
    struct i596_cmd set_conf;
    char i596_config[16];
    struct i596_cmd tdr;
    unsigned long stat;
    int last_restart;
    struct i596_rfd *rx_tail;
    struct i596_cmd *cmd_tail;
    struct i596_cmd *cmd_head;
    int cmd_backlog;
    int tx_full;
    unsigned long last_cmd;
    struct enet_statistics stats;
    struct device *next_module;
    int options;
    int media_override;
    int media;
};
#define	MEDIA_AUTO		0
#define	MEDIA_TPE		1
#define	MEDIA_BNC		2
#define	MEDIA_AUI		3

#define	TPE				0x0
#define	BNC				0x1
#define	AUI				0x2

static char *media_name[3] = { "TPE", "BNC", "AUI" };

char init_setup[] = {
	0x8E,	/* length, prefetch on */
	0xC8,	/* fifo to 8, monitor off */
	0x40,	/* don't save bad frames */
	0x2E,	/* No source address insertion, 8 byte preamble */
	0x00,	/* priority and backoff defaults */
	0x60,	/* interframe spacing */
	0x00,	/* slot time LSB */
	0xf2,	/* slot time and retries */
	0x00,	/* promiscuous mode */
	0x00,	/* collision detect */
	0x40,	/* minimum frame length */
	0xff,	
	0x00,
	0x3f	/*  multi IA */
};

static int eep10pci_scan(struct device *dev);
static int eep10pci_found_device(struct device *dev, int ioaddr, int irq,
								 int options);
static int eep10pci_probe1(struct device *dev);
static int i596_open(struct device *dev);
static int i596_start_xmit(struct sk_buff *skb, struct device *dev);
static void i596_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static int i596_close(struct device *dev);
static struct enet_statistics *i596_get_stats(struct device *dev);
static void i596_add_cmd(struct device *dev, struct i596_cmd *cmd);
static void print_eth(char *);
static void set_multicast_list(struct device *dev);

#define EEP10PCI_TOTAL_SIZE 0x30

#ifdef HAVE_DEVLIST
struct netdev_entry eep10pci_drv = 
{"eepro/10-pci", eep10pci_probe, EEP10PCI_TOTAL_SIZE, NULL};
#endif

/* per board array of initialization options */
static int options[8] = { -1, -1, -1, -1, -1, -1, -1, -1,};
static unsigned char pci_eaddr[8];


#ifdef MODULE
static int debug = -1;

/* A list of installed devices, for removing the driver module */
static struct device *root_eep10pci_dev = NULL;

int
init_module(void)
{
    int cards_found;

    if (debug >= 0)
	i596_debug = debug;
    if (i596_debug)
	printk(version);

    root_eep10pci_dev = NULL;
    cards_found = eep10pci_scan(NULL);
    return cards_found < 0 ? cards_found : 0;
}

#else
int eep10pci_probe(struct device *dev)
{
    int cards_found = 0;

    cards_found = eep10pci_scan(dev);
    if (i596_debug > 0 && cards_found)
	printk(version);

    return cards_found ? 0 : -ENODEV;
}
#endif /* !MODULE */

static int eep10pci_scan(struct device *dev)
{
    int cards_found = 0;
    
    if (pcibios_present()) {
	int pci_index;
	for (pci_index = 0; pci_index < 8; pci_index++) {
	    unsigned char pci_bus, pci_device_fn, pci_irq_line;
	    unsigned char pci_latency;
	    unsigned int pci_ioaddr;
	    unsigned short pci_command;

	    if (pcibios_find_device(PCI_VENDOR_ID_INTEL,
			PCI_DEVICE_ID_INTEL_82596,
			pci_index, &pci_bus, &pci_device_fn))
		break;
	    pcibios_read_config_byte(pci_bus, pci_device_fn,
			PCI_INTERRUPT_LINE, &pci_irq_line);
			pcibios_read_config_dword(pci_bus, pci_device_fn,
			PCI_BASE_ADDRESS_1, &pci_ioaddr);
	    /* Remove I/O space marker in bit 0. */
	    if ( !(pci_ioaddr & 1) )
		printk("i82596: I/O space marker not set in bit 0: %#x\n",
			pci_ioaddr);
	    pci_ioaddr &= ~3;
	    if (i596_debug > 2)
		printk("Found Intel i82596 PCI at I/O %#x, IRQ %d.\n",
			 pci_ioaddr, pci_irq_line);

	    /* Get and check the bus-master and latency values. */
	    pcibios_read_config_word(pci_bus, pci_device_fn,
			PCI_COMMAND, &pci_command);
	    if ( ! (pci_command & PCI_COMMAND_MASTER)) {
		printk("  PCI Master Bit has not been set! Setting...\n");
			pci_command |= PCI_COMMAND_MASTER;
		pcibios_write_config_word(pci_bus, pci_device_fn,
			PCI_COMMAND, pci_command);
	    }
	    pcibios_read_config_byte(pci_bus, pci_device_fn,
			PCI_LATENCY_TIMER, &pci_latency);
	   if (pci_latency < 10) {
		printk("  PCI latency timer (CFLT) is unreasonably low at %d."
			"  Setting to 255 clocks.\n", pci_latency);
			pcibios_write_config_byte(pci_bus, pci_device_fn,
			PCI_LATENCY_TIMER, 255);
	    } else if (i596_debug > 1)
		printk("  PCI latency timer (CFLT) is %#x.\n", pci_latency);

	    /* read the ethernet address */
	    memset(pci_eaddr, 0, 6);
	    pcibios_read_config_dword(pci_bus, pci_device_fn,
			PLXP_NODE_ADDR_REGISTER, 
			(unsigned int *)&pci_eaddr[0]);
	    pcibios_read_config_word(pci_bus, pci_device_fn,
			PLXP_NODE_ADDR_REGISTER+4,
			(unsigned short *)&pci_eaddr[4]);

	    eep10pci_found_device(dev, pci_ioaddr, pci_irq_line,
				options[cards_found]);

	    dev = 0;
	    cards_found++;
	}
    }

    return cards_found;
}


static int eep10pci_found_device(struct device *dev, int ioaddr, int irq,
								 int options)
{
struct i596_private *lp;

#ifdef MODULE
    static char devicename[9] = { 0, };
    /* Allocate and fill new device structure. */
    int dev_size = sizeof(struct device) + sizeof(struct i596_private) + 0xf;

    dev = (struct device *) kmalloc(dev_size, GFP_KERNEL);
    memset(dev, 0, dev_size);
    dev->mem_start = (int)dev + sizeof(struct device);

    /* align for scp */
    dev->priv = (void *)((dev->mem_start + 0xf) & 0xfffffff0);
    lp = (struct i596_private *)dev->priv;

    dev->name = devicename; /* An empty string. */
    dev->base_addr = ioaddr;
    dev->irq = irq;
    dev->init = eep10pci_probe1;
    lp->options = options;
    if (options >= 0)
	lp->media_override = options & 7;
    else
	lp->media_override = MEDIA_AUTO;
    ether_setup(dev);
    lp->next_module = root_eep10pci_dev;
    root_eep10pci_dev = dev;
    if (register_netdev(dev) != 0)
	return -EIO;
#else /* not a MODULE */
    if (dev) {
	dev->mem_start = (unsigned long)kmalloc(sizeof(struct i596_private)
							 + 0xf, GFP_KERNEL);
 	/* align for scp */
	dev->priv = (void *)((dev->mem_start + 0xf) & 0xfffffff0);
	memset(dev->priv, 0, sizeof(struct i596_private));
    }
    dev = init_etherdev(dev, sizeof(struct i596_private) + 0xf);
    dev->base_addr = ioaddr;
    dev->irq = irq;
    lp = (struct i596_private *)dev->priv;
    lp->options = options;
    if (options >= 0)
	lp->media_override = options & 7;
    else
	lp->media_override = MEDIA_AUTO;

    eep10pci_probe1(dev);
#endif /* MODULE */
    return 0;
}

static inline void 
wait_for_cmd_done(struct device *dev, int boguscnt, const char *id_msg)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;
    unsigned short *volatile scb_command = &(lp->scb.command);
    
    while (*scb_command)
	if (--boguscnt == 0) {
	    printk("%s: %s timeout with status %4.4x, cmd %4.4x.\n",
			dev->name, id_msg, lp->scb.status, lp->scb.command);
	    break;
    	}
}

static int eep10pci_probe1(struct device *dev)
{
    register x;
    int ioaddr = dev->base_addr; 
    struct i596_private *lp = (struct i596_private *)dev->priv;
    int str[6], *volatile self_test_results;
    int boguscnt = 16000;   /* Timeout for set-test. */
    int i;

    /* reset i82596 chip */
    outl(RESET_CMD, ioaddr + PLXP_PORT_OFFSET);
    MDELAY_100();

    /* report card and ethernet address */
    printk("%s: Intel EtherExpress Pro/10 PCI\n%s: eaddr ", dev->name,
	dev->name);
    for (i = 0; i < 5; i++)
	printk("%2.2X:", dev->dev_addr[i] = pci_eaddr[i]);
    printk("%2.2X, I/O at %#3x, IRQ %d.\n", dev->dev_addr[i] = pci_eaddr[i],
	ioaddr, dev->irq);

    /* clear interrupt latch */
    x = inb(ioaddr + PLXP_INTERRUPT_CONTROL);
    outb(x | LATCHED_INTREAD, ioaddr + PLXP_INTERRUPT_CONTROL);

    /* perform self test */
    self_test_results = (int *) ((((int) str) + 15) & ~0xf);
    self_test_results[0] = 0;
    self_test_results[1] = -1;
    outl((int)self_test_results | SELF_TEST_CMD, ioaddr + PLXP_PORT_OFFSET);
    printk("%s: starting self test ... ", dev->name);
    MDELAY_100();
    do {
	SLOW_DOWN_IO;
    } while (self_test_results[1] == -1  &&  --boguscnt >= 0);

    if ((boguscnt < 0) || !self_test_results[0] || self_test_results[1]) {
        printk("FAILED, status %8.8x\n", self_test_results[1]);
    } else 
	printk("PASSED\n");

    /* setup PCI chip */
    outb(0xf4, ioaddr + PLXP_USER_PINS);	/* user pins in write mode */

    x = inw(ioaddr + PLXP_LAN_CONTROL);
    x &= ~SOFTWARE_RESET_BIT;
    outw(x | SOFTWARE_RESET_BIT, ioaddr + PLXP_LAN_CONTROL);
    SLOW_DOWN_IO;
    SLOW_DOWN_IO;
    outw(x, ioaddr + PLXP_LAN_CONTROL);
    SLOW_DOWN_IO;
    SLOW_DOWN_IO;

    x = inw(ioaddr + PLXP_INTERRUPT_CONTROL);
    outw(x | LAN_0_INTERRUPT_ENABLE | LATCHED_INTWRITE,
		ioaddr + PLXP_INTERRUPT_CONTROL);
    MDELAY_100();	/* delay after PLX reset */

    /* reserve I/O region for /proc/ioports reporting */
    request_region(ioaddr, EEP10PCI_TOTAL_SIZE, "eep10pci");

    /* initialize command structure info */
    lp->scb.command = 0;
    lp->scb.cmd = (struct i596_cmd *)I596_NULL;
    lp->scb.rfd = (struct i596_rfd *)I596_NULL;

    /* The EEP10PCI-specific entries in the device structure. */
    dev->open = &i596_open;
    dev->stop = &i596_close;
    dev->hard_start_xmit = &i596_start_xmit;
    dev->get_stats = &i596_get_stats;
    dev->set_multicast_list = &set_multicast_list;

    return 0;
}


static inline int
init_rx_bufs(struct device *dev, int num)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;
    int i;
    struct i596_rfd *rfd;

    lp->scb.rfd = (struct i596_rfd *)I596_NULL;

    if (i596_debug > 1) printk ("%s: init_rx_bufs %d.\n", dev->name, num);

    for (i = 0; i < num; i++)
    {
	if (!(rfd = (struct i596_rfd *)kmalloc(sizeof(struct i596_rfd),
								GFP_KERNEL)))
            break;

	rfd->stat = 0x0000;
	rfd->rbd = I596_NULL;
	rfd->count = 0;
	rfd->size = 1532;
        if (i == 0)
        {
	    rfd->cmd = CMD_EOL;
            lp->rx_tail = rfd;
        }
        else
	    rfd->cmd = 0x0000;

        rfd->next = lp->scb.rfd;
        lp->scb.rfd = rfd;
    }

    if (i != 0)
      lp->rx_tail->next = lp->scb.rfd;

    return (i);
}


static inline void
remove_rx_bufs(struct device *dev)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;
    struct i596_rfd *rfd = lp->scb.rfd;

    lp->rx_tail->next = (struct i596_rfd *)I596_NULL;

    do
    {
        lp->scb.rfd = rfd->next;
        kfree(rfd);
        rfd = lp->scb.rfd;
    }
    while (rfd != lp->rx_tail);
}


static inline void
init_i596_mem(struct device *dev)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;
    short ioaddr = dev->base_addr;
    int boguscnt = 100;

    /* change the scp address */
    outl(((int)&lp->scp) | ALT_SCP_CMD, ioaddr + PLXP_PORT_OFFSET);
    udelay(1000);

    lp->last_cmd = jiffies;

    lp->scp.sysbus = 0x00440000;	/* Linear address mode */
    lp->scp.iscp = &(lp->iscp);
    lp->iscp.scb = &(lp->scb);
    lp->iscp.stat = 0x0001;			/* busy mark = 1 */
    lp->cmd_backlog = 0;
    lp->tx_full = 0;

    lp->cmd_head = lp->scb.cmd = (struct i596_cmd *) I596_NULL;
    lp->scb.status = 0;

    /* issue channel attention */
    (void) inb(ioaddr + PLXP_CA_OFFSET);
    if (i596_debug > 2) printk("%s: starting i82596.\n", dev->name);
    MDELAY_100();

    while (*((unsigned short volatile *)(&(lp->iscp.stat)))) {
	SLOW_DOWN_IO;
	if (--boguscnt == 0) {
	    printk("%s: i82596 init timed out: status %4.4x, cmd %4.4x.\n",
				dev->name, lp->scb.status, lp->scb.command );
	    break;
    	}
    }

    lp->scb.command = 0;

    memcpy (lp->i596_config, init_setup, 14);
    lp->set_conf.command = CmdConfigure;
    i596_add_cmd(dev, &lp->set_conf);

    memcpy (lp->eth_addr, dev->dev_addr, 6);
    lp->set_add.command = CmdSASetup;
    i596_add_cmd(dev, &lp->set_add);

    lp->tdr.command = CmdTDR;
    i596_add_cmd(dev, &lp->tdr);

    wait_for_cmd_done(dev, 200, "receive unit start");

    lp->scb.command = RX_START;
    (void) inb(ioaddr + PLXP_CA_OFFSET);

    MDELAY_100();
    wait_for_cmd_done(dev, 200, "i82596 init");

    return;
}


static inline int
i596_rx(struct device *dev)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;
    int frames = 0;

    if (i596_debug > 3) printk ("%s: i596_rx()\n", dev->name);

    while ((lp->scb.rfd->stat) & STAT_C)
    {
        if (i596_debug >2) print_eth(lp->scb.rfd->data);

	if ((lp->scb.rfd->stat) & STAT_OK)
	{
	    /* a good frame */
	    int pkt_len = lp->scb.rfd->count & 0x3fff;
	    struct sk_buff *skb = dev_alloc_skb(pkt_len);

	    frames++;

	    if (skb == NULL)
	    {
		printk ("%s: i596_rx Memory squeeze, dropping packet.\n",
				dev->name);
		lp->stats.rx_dropped++;
		break;
	    }

  	    skb->dev = dev;		
	    memcpy(skb_put(skb,pkt_len), lp->scb.rfd->data, pkt_len);

	    skb->protocol=eth_type_trans(skb,dev);
	    netif_rx(skb);
	    lp->stats.rx_packets++;

	    if (i596_debug > 4) print_eth(skb->data);
	}
	else
	{
	    lp->stats.rx_errors++;
            if ((lp->scb.rfd->stat) & 0x0001) lp->stats.tx_collisions++;
	    if ((lp->scb.rfd->stat) & 0x0080) lp->stats.rx_length_errors++;
	    if ((lp->scb.rfd->stat) & 0x0100) lp->stats.rx_over_errors++;
	    if ((lp->scb.rfd->stat) & 0x0200) lp->stats.rx_fifo_errors++;
	    if ((lp->scb.rfd->stat) & 0x0400) lp->stats.rx_frame_errors++;
	    if ((lp->scb.rfd->stat) & 0x0800) lp->stats.rx_crc_errors++;
	    if ((lp->scb.rfd->stat) & 0x1000) lp->stats.rx_length_errors++;
	}

	lp->scb.rfd->stat = 0;
	lp->scb.rfd->count = 0;
	lp->scb.rfd->cmd = CMD_EOL;
	lp->rx_tail->cmd = 0;
	lp->rx_tail = lp->scb.rfd;
	lp->scb.rfd = lp->scb.rfd->next;

    }

    if (i596_debug > 3) printk ("%s: frames %d\n", dev->name, frames);

    return 0;
}


static inline void
i596_cleanup_cmd(struct device *dev)
{
    struct i596_private *lp = dev->priv;
    struct i596_cmd *ptr;

    if (i596_debug > 4) printk ("%s: i596_cleanup_cmd\n", dev->name);

    while (lp->cmd_head != (struct i596_cmd *) I596_NULL)
    {
	ptr = lp->cmd_head;

	lp->cmd_head = lp->cmd_head->next;
	lp->cmd_backlog--;

	switch ((ptr->command) & 0x7)
	{
	    case CmdTx:
	    {
		struct tx_cmd *tx_cmd = (struct tx_cmd *) ptr;
		struct sk_buff *skb = tx_cmd->skb;

		dev_kfree_skb(skb, FREE_WRITE);

		lp->stats.tx_errors++;
		lp->stats.tx_aborted_errors++;

		ptr->next = (struct i596_cmd * ) I596_NULL;
		kfree((unsigned char *)tx_cmd);
		break;
	    }
	    case CmdMulticastList:
	    {
		ptr->next = (struct i596_cmd * ) I596_NULL;
		kfree((unsigned char *)ptr);
		break;
	    }
	    default:
		ptr->next = (struct i596_cmd * ) I596_NULL;
	}
    }

    wait_for_cmd_done(dev, 200, "i596_cleanup_cmd");

    lp->scb.cmd = lp->cmd_head;
}


static inline void
i596_reset(struct device *dev, struct i596_private *lp, int ioaddr)
{
    int boguscnt;

    if (i596_debug > 4) printk ("%s: i596_reset\n", dev->name);

    wait_for_cmd_done(dev, 200, "i596_reset");

    dev->start = 0;
    dev->tbusy = 1;

    lp->scb.command = CUC_ABORT|RX_ABORT;
    (void) inb(ioaddr + PLXP_CA_OFFSET);

    /* wait for command to be accepted */
    wait_for_cmd_done(dev, 400, "i596_reset 2");

    /* wait for active flag to clear */
    boguscnt = 200;
    while (*((unsigned short volatile *)(&(lp->scb.status))) & 0x0200)
        if (--boguscnt == 0)
	{
	    printk("%s: i596_reset 3 timed out with status %4.4x, cmd %4.4x.\n",
		dev->name, lp->scb.status, lp->scb.command);
	    break;
	}

    i596_cleanup_cmd(dev);
    i596_rx(dev);

    dev->start = 1;
    dev->tbusy = 0;
    dev->interrupt = 0;
    init_i596_mem(dev);

    /* enable board interrupts */
    outb(LATCHED_INTWRITE, ioaddr + PLXP_INTERRUPT_CONTROL);

}


static void i596_add_cmd(struct device *dev, struct i596_cmd *cmd)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;
    int ioaddr = dev->base_addr;
    unsigned long flags;

    if (i596_debug > 4) printk ("%s: i596_add_cmd\n", dev->name);

    cmd->status = 0;
    cmd->command |= (CMD_EOL|CMD_INTR);
    cmd->next = (struct i596_cmd *) I596_NULL;

    save_flags(flags);
    cli();
    if (lp->cmd_head != (struct i596_cmd *) I596_NULL)
		lp->cmd_tail->next = cmd;
    else 
    {
	lp->cmd_head = cmd;
	wait_for_cmd_done(dev, 200, "i596_add_cmd");

	lp->scb.cmd = cmd;
	lp->scb.command = CUC_START;
	(void) inb(ioaddr + PLXP_CA_OFFSET);
    }
    lp->cmd_tail = cmd;
    lp->cmd_backlog++;

    lp->cmd_head = lp->scb.cmd;
    restore_flags(flags);

    if (lp->cmd_backlog > TX_FULL_SIZE) 
    {
	int tickssofar = jiffies - lp->last_cmd;

	lp->tx_full = 1;
	if (tickssofar < TX_TIMEOUT) return;

	printk("%s: command unit timed out (bl=%d,tick=%d,jif=%lu,last=%lu).\n",
		dev->name, lp->cmd_backlog, tickssofar, jiffies, lp->last_cmd);
    }
}


static int
i596_open(struct device *dev)
{
    int i;
    struct i596_private *lp = (struct i596_private *)dev->priv;
    int ioaddr = dev->base_addr;
    register char x;

    if (i596_debug > 1)
	printk("%s: i596_open() irq %d.\n", dev->name, dev->irq);

    /* perform media selection */
    x = inb(ioaddr + PLXP_USER_PINS);
    switch (lp->media_override) {
	case MEDIA_BNC:
	    x &= ~(PLXP_USER_1 | PLXP_USER_0);
	    lp->media = BNC;
	    break;
	case MEDIA_AUI:
	    x &= ~PLXP_USER_0;
	    x |= PLXP_USER_1;
	    lp->media = AUI;
	    break;
	case MEDIA_AUTO:	/* start auto detection with TPE */
	case MEDIA_TPE:
	default:
	    x |= PLXP_USER_1 | PLXP_USER_0;
	    lp->media = TPE;
	    break;
    }
    outb(x, ioaddr + PLXP_USER_PINS);

    /* Use the now-standard shared IRQ implementation. */
    if (request_irq(dev->irq, &i596_interrupt, SA_SHIRQ, "eep10pci", dev))
		return -EAGAIN;

    if ((i = init_rx_bufs(dev, RX_RING_SIZE)) < RX_RING_SIZE)
        printk("%s: only able to allocate %d receive buffers\n", dev->name, i);

    if (i < 4)
    {
        free_irq(dev->irq, dev);
        return -EAGAIN;
    }

    dev->tbusy = 0;
    dev->interrupt = 0;
    dev->start = 1;
    MOD_INC_USE_COUNT;

    /* Initialize the 82596 memory */
    init_i596_mem(dev);

    /* enable board interrupts */
    outb(LATCHED_INTWRITE, ioaddr + PLXP_INTERRUPT_CONTROL);

    return 0;			/* Always succeed */
}


static int
i596_start_xmit(struct sk_buff *skb, struct device *dev)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;
    int ioaddr = dev->base_addr;
    struct tx_cmd *tx_cmd;

    if (i596_debug > 2) printk ("%s: i596_start_xmit\n", dev->name);

    /* Transmitter timeout, serious problems. */
    if (dev->tbusy) {
	int tickssofar = jiffies - dev->trans_start;
	if (tickssofar < 5)
	    return 1;
	printk("%s: transmit timed out, status resetting.\n",
	       dev->name);
	lp->stats.tx_errors++;
	/* Try to restart the adaptor */
	if (lp->last_restart == lp->stats.tx_packets) {
	    if (i596_debug > 1) printk ("%s: Resetting board.\n", dev->name);

	    /* Shutdown and restart */
            i596_reset(dev,lp, ioaddr);
	} else {
	    /* Issue a channel attention signal */
	    if (i596_debug > 1) printk ("%s: Kicking board.\n", dev->name);

	    lp->scb.command = CUC_START|RX_START;
	    (void) inb(ioaddr + PLXP_CA_OFFSET);

	    lp->last_restart = lp->stats.tx_packets;
	}
	dev->tbusy = 0;
	dev->trans_start = jiffies;
    }

    /* If some higher level thinks we've misses a tx-done interrupt
       we are passed NULL. n.b. dev_tint handles the cli()/sti()
       itself. */
    if (skb == NULL) {
	dev_tint(dev);
	return 0;
    }

    /* shouldn't happen */
    if (skb->len <= 0) return 0;

    if (i596_debug > 3) printk("%s: i596_start_xmit() called\n", dev->name);

    /* Block a timer-based transmit from overlapping.  This could better be
       done with atomic_swap(1, dev->tbusy), but set_bit() works as well. */
    if (set_bit(0, (void*)&dev->tbusy) != 0)
	printk("%s: Transmitter access conflict.\n", dev->name);
    else
    {
	short length = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
	dev->trans_start = jiffies;

	tx_cmd = (struct tx_cmd *) kmalloc ((sizeof (struct tx_cmd) +
			sizeof (struct i596_tbd)), GFP_ATOMIC);
	if (tx_cmd == NULL)
	{
	    printk ("%s: i596_xmit Memory squeeze, dropping packet.\n",
					dev->name);
	    lp->stats.tx_dropped++;

	    dev_kfree_skb(skb, FREE_WRITE);
	}
	else
	{
	    tx_cmd->tbd = (struct i596_tbd *) (tx_cmd + 1);
	    tx_cmd->tbd->next = (struct i596_tbd *) I596_NULL;

	    tx_cmd->cmd.command = CMD_FLEX|CmdTx;

	    tx_cmd->pad = 0;
	    tx_cmd->size = 0;
	    tx_cmd->tbd->pad = 0;
	    tx_cmd->tbd->size = EOF | length;

	    tx_cmd->skb = skb;
	    tx_cmd->tbd->data = skb->data;

	    if (i596_debug > 3) print_eth(skb->data);

	    i596_add_cmd(dev, (struct i596_cmd *)tx_cmd);

	    lp->stats.tx_packets++;
	}
    }

    if (lp->tx_full == 0)
	dev->tbusy = 0;

    return 0;
}


static void print_eth(char *add)
{
    int i;

    printk ("Dest  ");
    for (i = 0; i < 6; i++)
	printk(" %2.2X", (unsigned char)add[i]);
    printk ("\n");

    printk ("Source");
    for (i = 0; i < 6; i++)
	printk(" %2.2X", (unsigned char)add[i+6]);
    printk ("\n");
    printk ("type %2.2X%2.2X\n", (unsigned char)add[12], (unsigned char)add[13]);
}


static void
i596_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    struct device *dev = (struct device *)(dev_id);
    struct i596_private *lp;
    short ioaddr;
    unsigned short status, ack_cmd = 0;
    static ecnt = 0;

    if (dev == NULL) {
	ecnt++;
	if (ecnt<100)
            printk ("i596_interrupt(): irq %d for unknown device.\n", irq);
	return;
    }

    if (i596_debug > 3) printk("%s: i596_interrupt(): irq %d\n",dev->name, irq);

    if (dev->interrupt)
	printk("%s: Re-entering the interrupt handler.\n", dev->name);

    dev->interrupt = 1;

    ioaddr = dev->base_addr;

    lp = (struct i596_private *)dev->priv;

    wait_for_cmd_done(dev, 1000, "i596 interrupt");
    status = lp->scb.status;

    if (i596_debug > 4)
	printk("%s: i596 interrupt, status %4.4x.\n", dev->name, status);

    ack_cmd = status & 0xf000;

    /* acknowledge the interrupt */
    wait_for_cmd_done(dev, 500, "i596 Interrupt Ack");
    lp->scb.command = ack_cmd;
    (void) inb(ioaddr + PLXP_CA_OFFSET);
    ack_cmd = 0;

    if ((status & CX_ST_BIT) || (status & CNA_ST_BIT))
    {
	struct i596_cmd *ptr;

	if ((i596_debug > 4) && (status & CX_ST_BIT))
	    printk("%s: i596 interrupt completed command.\n", dev->name);
	if ((i596_debug > 4) && (status & CNA_ST_BIT))
	    printk("%s: i596 interrupt command unit inactive %x.\n",
		dev->name, status & CU_STATUS_BITS);

	while ((lp->cmd_head != (struct i596_cmd *) I596_NULL) && 
					(lp->cmd_head->status & STAT_C))
	{
	    ptr = lp->cmd_head;

	    lp->cmd_head = lp->cmd_head->next;
	    lp->cmd_backlog--;
	    if ( lp->tx_full && (lp->cmd_backlog <= TX_NOT_FULL) ) {
		lp->tx_full = 0;
		dev->tbusy = 0;
	    }

	    switch ((ptr->command) & 0x7)
	    {
		case CmdTx:
		{
		    struct tx_cmd *tx_cmd = (struct tx_cmd *) ptr;
		    struct sk_buff *skb = tx_cmd->skb;

		    if ((ptr->status) & STAT_OK)
		    {
	    		if (i596_debug >2) print_eth(skb->data);
		    }
		    else
		    {
			lp->stats.tx_errors++;
			if ((ptr->status) & 0x0020)
                            lp->stats.tx_collisions++;
			if (!((ptr->status) & 0x0040))
			    lp->stats.tx_heartbeat_errors++;
			if ((ptr->status) & 0x0400)
			    lp->stats.tx_carrier_errors++;
			if ((ptr->status) & 0x0800)
                            lp->stats.tx_collisions++;
			if ((ptr->status) & 0x1000)
			    lp->stats.tx_aborted_errors++;
		    }

		    dev_kfree_skb(skb, FREE_WRITE);

		    ptr->next = (struct i596_cmd * ) I596_NULL;
		    kfree((unsigned char *)tx_cmd);
		    break;
		}
		case CmdMulticastList:
		{
		    ptr->next = (struct i596_cmd * ) I596_NULL;
		    kfree((unsigned char *)ptr);
		    break;
		}
		case CmdTDR:
		{
		    unsigned long status = *((unsigned long *) (ptr + 1));

		    if (status & 0x8000)
		    {
			if (i596_debug)
	    		    printk("%s: link ok, media is %s.\n", dev->name, 
				media_name[lp->media] );
		    }
		    else
		    {
			if (status & 0x4000)
	    		    printk("%s: Transceiver problem.\n", dev->name);
			if (status & 0x2000)
	    		    printk("%s: Termination problem.\n", dev->name);
			if (status & 0x1000)
	    		    printk("%s: Short circuit.\n", dev->name);

	    		printk("%s: (%s) Time %ld.\n", dev->name,
			    media_name[lp->media],  status & 0x07ff);

			/* TODO -- implement media autodetect here
			 * by changing media select bits and trying
			 * another TDR command.
			 */
		    }
		}
		default:
		    ptr->next = (struct i596_cmd * ) I596_NULL;
 	    }
	}
	lp->last_cmd = jiffies;

	ptr = lp->cmd_head;
	while ((ptr != (struct i596_cmd *) I596_NULL) && (ptr != lp->cmd_tail))
	{
	    ptr->command &= 0x1fff;
	    ptr = ptr->next;
	}

	if ((lp->cmd_head != (struct i596_cmd *) I596_NULL) &&
		(dev->start) &&
		((lp->scb.status & CU_STATUS_BITS) != CU_ACTIVE))
	    ack_cmd |= CUC_START;
	lp->scb.cmd = lp->cmd_head;
    }

    if ((status & RNR_ST_BIT) || (status & FR_ST_BIT))
    {
	if ((i596_debug > 4) && (status & FR_ST_BIT))
	    printk("%s: i596 interrupt received a frame.\n", dev->name);
	if ((i596_debug > 4) && (status & RNR_ST_BIT))
	    printk("%s: i596 interrupt receive unit inactive %x.\n",
		dev->name, status & 0x0070);

	i596_rx(dev);

	if (dev->start && ((lp->scb.status & RU_STATUS_BITS) != RU_READY))
	    ack_cmd |= RX_START;
    }

    /* restart CU/RU */

    if (ack_cmd)
    {
	wait_for_cmd_done(dev, 500, "i596 Interrupt");
	lp->scb.command = ack_cmd;
	(void) inb(ioaddr + PLXP_CA_OFFSET);
	SLOW_DOWN_IO;
	SLOW_DOWN_IO;
    }

    /* clear interrupt latch */
    {
        register unsigned char x;
        x = inb(ioaddr + PLXP_INTERRUPT_CONTROL);
        outb(x | LATCHED_INTREAD, ioaddr + PLXP_INTERRUPT_CONTROL);
    }

    if (i596_debug > 4)
	printk("%s: exiting interrupt.\n", dev->name);

    dev->interrupt = 0;
    return;
}


static int
i596_close(struct device *dev)
{
    int ioaddr = dev->base_addr;
    struct i596_private *lp = (struct i596_private *)dev->priv;
    int boguscnt = 200;

    dev->start = 0;
    dev->tbusy = 1;

    if (i596_debug > 1)
	printk("%s: Shutting down ethercard, status was %4.4x.\n",
	       dev->name, lp->scb.status);

    lp->scb.command = CUC_ABORT|RX_ABORT;
    (void) inb(ioaddr + PLXP_CA_OFFSET);

    i596_cleanup_cmd(dev);

    while (*((unsigned short volatile *)(&(lp->scb.status))))
	if (--boguscnt == 0)
	{
	    printk("%s: close timed out with status %4.4x, cmd %4.4x.\n",
		   dev->name, lp->scb.status, lp->scb.command);
	    break;
    	}
    free_irq(dev->irq, dev);
    remove_rx_bufs(dev);
    MOD_DEC_USE_COUNT;

    return 0;
}

static struct enet_statistics *
i596_get_stats(struct device *dev)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;

    return &lp->stats;
}


/*
 *	Set or clear the multicast filter for this adaptor.
 */
 
static void set_multicast_list(struct device *dev)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;
    struct i596_cmd *cmd;

    if (i596_debug > 1)
	printk ("%s: set multicast list %d\n", dev->name, dev->mc_count);

    if (dev->mc_count > 0) 
    {
	struct dev_mc_list *dmi;
	char *cp;
	cmd = (struct i596_cmd *) kmalloc(sizeof(struct i596_cmd)+2+
						dev->mc_count*6, GFP_ATOMIC);
	if (cmd == NULL)
	{
	    printk ("%s: set_multicast Memory squeeze.\n", dev->name);
	    return;
	}
	cmd->command = CmdMulticastList;
	*((unsigned short *) (cmd + 1)) = dev->mc_count * 6;
	cp=((char *)(cmd + 1))+2;
	for(dmi=dev->mc_list;dmi!=NULL;dmi=dmi->next)
	{
	    memcpy(cp, dmi,6);
	    cp+=6;
	}
	print_eth (((char *)(cmd + 1)) + 2);
	i596_add_cmd(dev, cmd);
    }
    else
    {
	if (lp->set_conf.next != (struct i596_cmd * ) I596_NULL) 
	    return;
	if (dev->mc_count == 0 && !(dev->flags&(IFF_PROMISC|IFF_ALLMULTI)))
	{
	    if(dev->flags&IFF_ALLMULTI)
		dev->flags|=IFF_PROMISC;
	    lp->i596_config[8] &= ~0x01;
	}
	else
	    lp->i596_config[8] |= 0x01;

	i596_add_cmd(dev, &lp->set_conf);
    }
}


#ifdef MODULE
void
cleanup_module(void)
{
    struct device *next_dev;

    while (root_eep10pci_dev) {
	next_dev = 
		((struct i596_private *)root_eep10pci_dev->priv)->next_module;
    	unregister_netdev(root_eep10pci_dev);

	release_region(root_eep10pci_dev->base_addr, EEP10PCI_TOTAL_SIZE);
	kfree(root_eep10pci_dev);
	root_eep10pci_dev = next_dev;
    }
}
#endif /* MODULE */

/*
 * Local variables:
 *  compile-command: "gcc -DMODULE -D__KERNEL__ -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O6 -m486 -c eep10pci.c"
 * End:
 */
