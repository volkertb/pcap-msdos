/*
 * linux/drivers/net/am79c961.h
 */

#ifndef _LINUX_am79c961a_H
#define _LINUX_am79c961a_H

/* use 0 for production, 1 for verification, >2 for debug. debug flags: */
#define DEBUG_TX	 2
#define DEBUG_RX	 4
#define DEBUG_INT	 8
#define DEBUG_IC	16
#ifndef NET_DEBUG
#define NET_DEBUG 	0
#endif

#define NET_UID		0
#define NET_RDP		0x10
#define NET_RAP		0x12
#define NET_RESET	0x14
#define NET_IDP		0x16

/*
 * RAP registers
 */
#define CSR0		0
#define CSR0_INIT	0x0001
#define CSR0_STRT	0x0002
#define CSR0_STOP	0x0004
#define CSR0_TDMD	0x0008
#define CSR0_TXON	0x0010
#define CSR0_RXON	0x0020
#define CSR0_IENA	0x0040
#define CSR0_INTR	0x0080
#define CSR0_IDON	0x0100
#define CSR0_TINT	0x0200
#define CSR0_RINT	0x0400
#define CSR0_MERR	0x0800
#define CSR0_MISS	0x1000
#define CSR0_CERR	0x2000
#define CSR0_BABL	0x4000
#define CSR0_ERR	0x8000

#define CSR3		3
#define CSR3_EMBA	0x0008
#define CSR3_DXMT2PD	0x0010
#define CSR3_LAPPEN	0x0020
#define CSR3_IDONM	0x0100
#define CSR3_TINTM	0x0200
#define CSR3_RINTM	0x0400
#define CSR3_MERRM	0x0800
#define CSR3_MISSM	0x1000
#define CSR3_BABLM	0x4000
#define CSR3_MASKALL	0x5F00

#define LADRL		8
#define LADRM1		9
#define LADRM2		10
#define LADRH		11
#define PADRL		12
#define PADRM		13
#define PADRH		14

#define MODE		15
#define MODE_DISRX	0x0001
#define MODE_DISTX	0x0002
#define MODE_LOOP	0x0004
#define MODE_DTCRC	0x0008
#define MODE_COLL	0x0010
#define MODE_DRETRY	0x0020
#define MODE_INTLOOP	0x0040
#define MODE_PORT0	0x0080
#define MODE_DRXPA	0x2000
#define MODE_DRXBA	0x4000
#define MODE_PROMISC	0x8000

#define BASERXL		24
#define BASERXH		25
#define BASETXL		30
#define BASETXH		31

#define POLLINT		47

#define SIZERXR		76
#define SIZETXR		78

#define RMD_ENP		0x0100
#define RMD_STP		0x0200
#define RMD_CRC		0x0800
#define RMD_FRAM	0x2000
#define RMD_ERR		0x4000
#define RMD_OWN		0x8000

#define TMD_ENP		0x0100
#define TMD_STP		0x0200
#define TMD_MORE	0x1000
#define TMD_ERR		0x4000
#define TMD_OWN		0x8000

#define TST_RTRY	0x0200
#define TST_LCAR	0x0400
#define TST_LCOL	0x1000
#define TST_UFLO	0x4000

struct dev_priv {
    struct enet_statistics stats;
    unsigned long	rxbuffer[RX_BUFFERS];
    unsigned long	txbuffer[TX_BUFFERS];
    unsigned char	txhead;
    unsigned char	txtail;
    unsigned char	rxhead;
    unsigned char	rxtail;
    unsigned long	rxhdr;
    unsigned long	txhdr;
};

extern int	am79c961_probe (struct net_device *dev);
static int	am79c961_probe1 (struct net_device *dev);
static int	am79c961_open (struct net_device *dev);
static int	am79c961_sendpacket (struct sk_buff *skb, struct net_device *dev);
static void	am79c961_interrupt (int irq, void *dev_id, struct pt_regs *regs);
static void	am79c961_rx (struct net_device *dev, struct dev_priv *priv);
static void	am79c961_tx (struct net_device *dev, struct dev_priv *priv);
static int	am79c961_close (struct net_device *dev);
static struct enet_statistics *am79c961_getstats (struct net_device *dev);
static void	am79c961_setmulticastlist (struct net_device *dev);

#endif
