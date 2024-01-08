/* atp-diag.c: Diagnostic for Realtek RTL8002/RTL8012 pocket ethernet adaptor.

   Written by Donald Becker.

    This software may only be used and distributed according to the terms
    of the GNU Public License, incorporated herein by reference.

    The author may be reached as becker@cesdis.gsfc.nasa.gov, or C/O
    Center of Excellence in Space Data and Information Sciences, Code 930.5,
    Goddard Space Flight Center, Greenbelt MD, 20771

   This is a diagnostic program for various OEM pocket ethernet adaptors
   based on the Realtek RTL8002 and RTL8012 chips.
   It doesn't test for specific hardware faults, but rather verifies the basic
   functionality of the board.
*/

static char *version =
    "atp-diag.c:v0.02 4/1/97 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <sys/time.h>
#include <linux/in.h>
#include <linux/if_ether.h>
/*#include "/usr/src/linux/net/inet/dev.h"*/
#include "atp.h"

/*  A few definitions to match the in-kernel environment. */
#define printk printf

int net_debug = 0;

struct device {
    char *name;
    short base_addr;
    unsigned char irq;
    unsigned char if_port;
    unsigned char dev_addr[6];	/* Hardware station address. */
    unsigned int tbusy:1, interrupt:1, start:1;
	void *priv;
} devs, *dev = &devs;

struct net_local lps;

/* Sequence to switch an 8012 from printer mux to ethernet mode. */
static char mux_8012[] = { 0xff, 0xf7, 0xff, 0xfb, 0xf3, 0xfb, 0xff, 0xf7,};

struct option longopts[] = {
 /* { name  has_arg  *flag  val } */
    {"base-address", 1, 0, 'p'},
    {"all",	   0, 0, 'a'},	/* Print all registers. */
    {"dump",       0, 0, 'd'},	/* Dump the first page. */
    {"examine",    1, 0, 'e'},	/* Examine a specified page. */
    {"help",       0, 0, 'h'},	/* Give help */
    {"interface",  0, 0, 'f'},	/* Interface number (built-in, AUI) */
    {"irq",	   	   1, 0, 'i'},	/* Interrupt number */
    {"reset ASIC", 0, 0, 'r'},	/* Reset the ASIC before doing anything. */
    {"verbose",    0, 0, 'v'},	/* Verbose mode */
    {"version",    0, 0, 'V'},	/* Display version number */
    {"write-EEPROM", 1, 0, 'w'},/* Write the EEPROM with the specified vals */
    { 0, 0, 0, 0 }
};

int verbose = 0;

/* Index to functions, as function prototypes. */

static void get_node_ID(struct device *dev);
static unsigned short eeprom_op(short ioaddr, unsigned int cmd);
static void hardware_init(struct device *dev);
static void write_packet(short ioaddr, int length, unsigned char *packet, int mode);
static void trigger_send(short ioaddr, int length);
static void read_block(short ioaddr, int length, unsigned char *buffer, int data_mode);
static void ram_test(short ioaddr);

int opt_f = 0;

int main(int argc, char **argv)
{
    int port_base = 0x378, irq = -1;
    int errflag = 0, shared_mode = 0;
    int write_eeprom = 0, interface = -1, all_regs = 0, dump = 0;
	int examine = -1, reset_asic = 0, ping_test = 0;
    int c, longind;
    extern char *optarg;
	char saved_cmr2l;			/* Saved low nibble of command register 2.  */
	int ioaddr;
	int i;

    while ((c = getopt_long(argc, argv, "ade:fF:gi:p:rsvw",
							longopts, &longind))
	   != -1)
	switch (c) {
	case 'e':
	    examine = atoi(optarg);
	    break;
	case 'f': opt_f++; break;
	case 'F':
	    interface = atoi(optarg);
	    break;
	case 'i':
	    irq = atoi(optarg);
	    break;
	case 'p':
	    port_base = strtol(optarg, NULL, 16);
	    break;
	case 'd': dump++; break;
	case 'g': ping_test++; break;
	case 'r': reset_asic++; break;
	case 's': shared_mode++; break;
	case 'v': verbose++;		 break;
	case 'w': write_eeprom++;	 break;
	case 'a': all_regs++;		 break;
	case '?':
	    errflag++;
	}
    if (errflag) {
		fprintf(stderr, "usage:");
		return 2;
    }

    if (verbose)
		printf(version);

    dev->name = "ATP";
	dev->base_addr = ioaddr = port_base;
	dev->priv = &lps;
    
    if (ioperm(port_base, 4, 1) < 0
		|| ioperm(0x80, 1, 1)) { 		/* Needed for SLOW_DOWN_IO */
		perror("ethertest: ioperm()");
		fprintf(stderr, " You must be root to run this program.\n");
		return 1;
    }

	printf("AT-Lan-Tec/Realtek Pocket Ethernet adaptor test at %#x.\n",
		   dev->base_addr);

	/* Turn off the printer multiplexer on the 8012. */
	for (i = 0; i < 8; i++)
		outb(mux_8012[i], ioaddr + PAR_DATA);

	saved_cmr2l = read_nibble(ioaddr, CMR2) >> 3;
	printf(" CMR2 is %x, the device is set to page %d.\n", saved_cmr2l,
		   (saved_cmr2l>>2) & 1);
	write_reg(ioaddr, CMR2, CMR2_NULL);
	printf(" Page 0 registers:");
	for (i = 0; i < 16; i++)
		printf(" %2.2x",
			   ((read_nibble(ioaddr, i | HNib)<<1) & 0xf0)
			   | ((read_nibble(ioaddr, i)>>3) & 0x0f));
	printf("\n Page 1 registers:");
	write_reg(ioaddr, CMR2, CMR2_EEPROM);
	for (i = 0; i < 16; i++)
		printf(" %2.2x",
			   ((read_nibble(ioaddr, i | HNib)<<1) & 0xf0)
			   | ((read_nibble(ioaddr, i)>>3) & 0x0f));

	/* We are already on the EEPROM control register page. */
	printf("\n EEPROM contents:");
	for (i = 0; i < 64; i++)
		printf("%s%4.4x", i % 16 ? " " : "\n ", eeprom_op(ioaddr, EE_READ(i)));


	write_reg(ioaddr, CMR2, CMR2_NULL);

	printf("\n CMR1, Command register 1, is %02x, CMR1 is %02x.\n",
		   read_nibble(ioaddr, CMR1 | HNib)>>3, read_nibble(ioaddr, CMR1)>>3);

	write_reg_high(ioaddr, CMR1, CMR1h_RxENABLE | CMR1h_TxENABLE);	/* Enable Tx and Rx. */
    write_reg(ioaddr, CMR2, CMR2_IRQOUT);

	printf("  Station address: ");
    get_node_ID(dev);
	for (i = 0; i < 6; i++)
		printf("%02x:", dev->dev_addr[i]);
	printf("%02x.\n", dev->dev_addr[i]);

#if 0
	printf("  Setting filter table to accept-all-multicast.\n");
    write_reg(ioaddr, CMR2, 0x04); /* Switch to page 1. */
    for (i = 0; i < 8; i++)
		write_reg_byte(ioaddr, i, 0xff);
#endif

	if (opt_f) {
		/* Do the RAM test. */
		ram_test(ioaddr);

		if (all_regs) {
			printf("\n  Initializing the hardware... ");
			hardware_init(dev);
		}
		if (ping_test) {
			write_packet(ioaddr, 64, 0, 0);
			trigger_send(ioaddr, 64);
		}
		printf("\n");
	}

    write_reg(ioaddr, CMR2, saved_cmr2l);		/* Restore CMR2. */
	return 0;
}

/* Read the station address PROM, usually a word-wide EEPROM. */
static void get_node_ID(struct device *dev)
{
	short ioaddr = dev->base_addr;
	int sa_offset = 0;
	int i;
	
	write_reg(ioaddr, CMR2, CMR2_EEPROM);	  /* Point to the EEPROM control registers. */
	
	/* Some adaptors have the station address at offset 15 instead of offset
	   zero.  Check for it, and fix it if needed. */
	if (eeprom_op(ioaddr, EE_READ(0)) == 0xffff)
		sa_offset = 15;
	
	for (i = 0; i < 3; i++) {
		unsigned short station_address;
		station_address = eeprom_op(ioaddr, EE_READ(sa_offset + i));
		((unsigned short *)dev->dev_addr)[i] = ntohs(station_address);
	}
	
	write_reg(ioaddr, CMR2, CMR2_NULL);
}

/*
  An EEPROM read command starts by shifting out 0x60+address, and then
  shifting in the serial data. See the NatSemi databook for details.
 *		   ________________
 * CS : __|
 *			   ___	   ___
 * CLK: ______|	  |___|	  |
 *		 __ _______ _______
 * DI :	 __X_______X_______X
 * DO :	 _________X_______X
 */

static unsigned short eeprom_op(short ioaddr, unsigned int cmd)
{
	unsigned eedata_out = 0;
	int num_bits = EE_CMD_SIZE;
	
	while (--num_bits >= 0) {
		char outval = test_bit(num_bits, &cmd) ? EE_DATA_WRITE : 0;
		write_reg_high(ioaddr, PROM_CMD, outval | EE_CLK_LOW);
		eeprom_delay(5);
		write_reg_high(ioaddr, PROM_CMD, outval | EE_CLK_HIGH);
		eedata_out <<= 1;
		if (read_nibble(ioaddr, PROM_DATA) & EE_DATA_READ)
			eedata_out++;
		eeprom_delay(5);
	}
	write_reg_high(ioaddr, PROM_CMD, EE_CLK_LOW & ~EE_CS);
	return eedata_out;
}

/* This routine resets the hardware.  We initialize everything, assuming that
   the hardware may have been temporarily detacted. */
static void hardware_init(struct device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int ioaddr = dev->base_addr;
    int i;

	/* These should be in probe() or open() rather than here. */
	lp->addr_mode = CMR2h_Normal;
	dev->if_port = 4;

    for (i = 0; i < 6; i++)
		write_reg_byte(ioaddr, PAR0 + i, dev->dev_addr[i]);

	write_reg_high(ioaddr, CMR2, lp->addr_mode);

	if (net_debug > 2) {
		printk("%s: Open with read value %02x.\n", dev->name,
			   read_nibble(ioaddr, CMR2_h));
	}

    write_reg(ioaddr, CMR2, CMR2_IRQOUT);
    write_reg_high(ioaddr, CMR1, CMR1h_RxENABLE | CMR1h_TxENABLE);

	/* Enable the interrupt line from the serial port. */
	outb(Ctrl_SelData + Ctrl_IRQEN, ioaddr + PAR_CONTROL);

	/* Unmask the interesting interrupts. */
    write_reg(ioaddr, IMR, ISR_RxOK | ISR_TxErr | ISR_TxOK);
    write_reg_high(ioaddr, IMR, ISRh_RxErr);

	lp->tx_unit_busy = 0;
    lp->pac_cnt_in_tx_buf = 0;
	lp->saved_tx_size = 0;

	dev->tbusy = 0;
	dev->interrupt = 0;
}

static void trigger_send(short ioaddr, int length)
{
	write_reg_byte(ioaddr, TxCNT0, length & 0xff);
	write_reg(ioaddr, TxCNT1, length >> 8);
	write_reg(ioaddr, CMR1, CMR1_Xmit);
}

static void write_packet(short ioaddr, int length, unsigned char *packet, int data_mode)
{
    length = (length + 1) & ~1;		/* Round up to word length. */
    outb(EOC+MAR, ioaddr + PAR_DATA);
    if ((data_mode & 1) == 0) {
		/* Write the packet out, starting with the write addr. */
		outb(WrAddr+MAR, ioaddr + PAR_DATA);
		do {
			write_byte_mode0(ioaddr, *packet++);
		} while (--length > 0) ;
    } else {
		/* Write the packet out in slow mode. */
		unsigned char outbyte = *packet++;

		outb(Ctrl_LNibWrite + Ctrl_IRQEN, ioaddr + PAR_CONTROL);
		outb(WrAddr+MAR, ioaddr + PAR_DATA);

		outb((outbyte & 0x0f)|0x40, ioaddr + PAR_DATA);
		outb(outbyte & 0x0f, ioaddr + PAR_DATA);
		outbyte >>= 4;
		outb(outbyte & 0x0f, ioaddr + PAR_DATA);
		outb(Ctrl_HNibWrite + Ctrl_IRQEN, ioaddr + PAR_CONTROL);
		while (--length > 0)
			write_byte_mode1(ioaddr, *packet++);
    }
    /* Terminate the Tx frame.  End of write: ECB. */
    outb(0xff, ioaddr + PAR_DATA);
    outb(Ctrl_HNibWrite | Ctrl_SelData | Ctrl_IRQEN, ioaddr + PAR_CONTROL);
}

static void read_block(short ioaddr, int length, unsigned char *p, int data_mode)
{

	if (data_mode <= 3) { /* Mode 0 or 1 */
		outb(Ctrl_LNibRead, ioaddr + PAR_CONTROL);
		outb(length == 8  ?  RdAddr | HNib | MAR  :  RdAddr | MAR,
			 ioaddr + PAR_DATA);
		if (data_mode <= 1) { /* Mode 0 or 1 */
			do  *p++ = read_byte_mode0(ioaddr);  while (--length > 0);
		} else	/* Mode 2 or 3 */
			do  *p++ = read_byte_mode2(ioaddr);  while (--length > 0);
	} else if (data_mode <= 5)
		do      *p++ = read_byte_mode4(ioaddr);  while (--length > 0);
	else
		do      *p++ = read_byte_mode6(ioaddr);  while (--length > 0);

    outb(EOC+HNib+MAR, ioaddr + PAR_DATA);
	outb(Ctrl_SelData, ioaddr + PAR_CONTROL);
}

static void ram_test(short ioaddr)
{
	unsigned char buffer[64];
	int failedp = 0;
	int i;
	
	write_reg(ioaddr, CMR2, CMR2_RAMTEST);
	
	/* Disable then re-enable Tx and Rx to reset pointers. */
	write_reg_high(ioaddr, CMR1, CMR1h_TxRxOFF);
	write_reg_high(ioaddr, CMR1, CMR1h_RxENABLE | CMR1h_TxENABLE);
	
#ifdef new
	write_packet(ioaddr, 16, version, 4);
#else
	outb(EOC+MAR, ioaddr + PAR_DATA);
	outb(WrAddr+MAR, ioaddr + PAR_DATA);
	
	for (i = 0; i < 8; i++) {
		write_byte_mode0(ioaddr, "Mode0Mode0"[i]);
	}
	outb(WrAddr|MAR|EOC, ioaddr);
#endif
	
	/* Disable then re-enable Tx and Rx to reset pointers. */
	write_reg_high(ioaddr, CMR1, CMR1h_TxRxOFF);
	write_reg_high(ioaddr, CMR1, CMR1h_RxENABLE | CMR1h_TxENABLE);
	
	strcpy(buffer, "BUBBUB");
#ifdef new
	outb(EOC+MAR, ioaddr + PAR_DATA);
	read_block(ioaddr, 8, buffer, 4);
	printk(" Compare is %d, '%10.10s' vs '%10.10s'.\n",
		   memcmp(buffer, version, sizeof(version)), buffer, version);
	for (i = 0; i < 16; i++)
		printf(" %c", buffer[i]);
	printf(".\n");
#else
/*
	outb(Ctrl_LNibRead, ioaddr + PAR_CONTROL);
	outb(EOC+MAR, ioaddr + PAR_DATA);
	outb(RdAddr+MAR+HNib, ioaddr + PAR_DATA);
	*/
	for (i = -1; i < 10; i++) {
		ushort expected = 44;
		ushort result;
		result = read_byte_mode4(ioaddr);
		result |= read_byte_mode4(ioaddr) << 8;
		if (i >= 0  &&  result != expected) {
			printk("RAM test failed at location %d, %04x vs. %04x (read vs. wrote).\n",
				   i, result, expected);
			failedp++;
			if (failedp > 10)
				break;
		}
	}
#endif

	read_block(ioaddr, 8, buffer, 4);
	printk(" Compare is %d, '%10.10s' vs '%10.10s'.\n",
		   memcmp(buffer, version, sizeof(version)), buffer, version);

	write_reg_high(ioaddr, CMR1, CMR1h_TxRxOFF);
	write_reg(ioaddr, CMR2, CMR2_NULL);
}


/*
 * Local variables:
 *  compile-command: "gcc -U__KERNEL__ -I/usr/src/linux/drivers/net -Wall -Wstrict-prototypes -O6 -o atp-diag atp-diag.c"
 *  version-control: t
 *  kept-new-versions: 5
 *  tab-width: 4
 * End:
 */
