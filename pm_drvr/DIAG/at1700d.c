/*  at1700-diag.c: Diagnostic program for the Allied Telesis AT1700.

	Written/copyright 1998 by Donald Becker.

	This software may be used and distributed according to the terms of the
	Gnu Public Lincese, incorporated herein	by reference.

	The author may be reached as becker@cesdis.gsfc.nasa.gov.
	C/O USRA Center of Excellence in Space Data and Information Sciences
	Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771

	This is a diagnostic program for the Allied Telesis AT1700, and similar
	boards built around the Fujitsu MB86965.   Don't expect it
	to test every possible hardware fault, but it does verify the basic
	functionality of the board.
*/

static char *version =
"at1700-diag.c:v0.99B 2/5/98 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <asm/io.h>
#include <asm/bitops.h>
extern iopl(int level);			/* Grrr, glibc fails the define this.. */

struct option longopts[] = {
 /* { name  has_arg  *flag  val } */
    {"base-address", 1, 0, 'p'},
    {"all",	   0, 0, 'a'},	/* Print all registers. */
    {"dump",       0, 0, 'd'},	/* Dump the first page. */
	{"show-eeprom",  0, 0, 'e'}, /* Dump EEPROM contents */
    {"help",       0, 0, 'h'},	/* Give help */
    {"interface",  0, 0, 'f'},	/* Interface number (built-in, AUI) */
    {"irq",	   	   1, 0, 'i'},	/* Interrupt number */
    {"reset ASIC", 0, 0, 'r'},	/* Reset the ASIC before doing anything. */
    {"verbose",    0, 0, 'v'},	/* Verbose mode */
    {"version",    0, 0, 'V'},	/* Display version number */
    {"write-EEPROM", 1, 0, 'w'},/* Write the EEPROM with the specified vals */
    { 0, 0, 0, 0 }
};

/* Offsets from the base address. */
enum register_offsets {
	IntrMask=2, TxMode=4, RxMode=5, CONFIG_0=6, CONFIG_1=7,
	DATAPORT=8, TX_START=10, Mode13=13,
	EEPROM_Ctrl=16, EEPROM_Data=17, IOBase0=18, IOBase1=19,
   };

const char irqmap[4] = {3, 4, 5, 9};
const int  iobasemap[8] = { 0x260, 0x280, 0x2a0, 0x240,
							0x340, 0x320, 0x380, 0x300,};

struct device {
    char *name;
    int base_addr;
    unsigned char irq;
    unsigned char if_port;
    unsigned char dev_addr[6];	/* Hardware station address. */
    unsigned int tbusy:1, interrupt:1, start:1;
	void *priv;
} devs, *dev = &devs;

int jiffies;
struct net_local {
    struct enet_statistics stats;
	long open_time;
} lps, *lp = &lps;

#define EEPROM_SIZE 16
unsigned short eeprom_contents[EEPROM_SIZE];
unsigned char fake_packet[100];
unsigned char station_addr[6];

static int show_regs = 0, show_eeprom = 0;
static int verbose = 1, opt_f = 0, debug = 1;
static int test_multicast = 0;

int do_one_board(int ioaddr);
static int read_eeprom(int ioaddr, int location);
static void eeprom_write(int ioaddr, int location, int value);
int at1700_probe1(struct device *dev, int ioaddr);
int at1700_report(int ioaddr);
void start_chip(int ioaddr);
static int net_open(struct device *dev);
static int net_close(struct device *dev);
int send_packet(int ioaddr, int length, char *buf);
int rx_packet(int ioaddr, unsigned char *rx_buffer);
void do_multicast_test(int ioaddr);
static inline unsigned ether_crc_le(int length, unsigned char *data);
void set_rx_mode(int ioaddr, unsigned char mc_addr[6], int invert);


int
main(int argc, char **argv)
{
    int port_base = 0x300;
	int new_irq = -1, new_iobase = -1;
    int errflag = 0, shared_mode = 0;
    int write_eeprom = 0, interface = -1, dump = 0;
	int reset_asic = 0;
    int c, longind;
    extern char *optarg;
	int ioaddr;

    while ((c = getopt_long(argc, argv, "aefF:mp:P:Q:rsdvw",
							longopts, &longind))
	   != -1)
	switch (c) {
	case 'a': show_regs++;		break;
	case 'e': show_eeprom++;	break;
	case 'f': opt_f++;			break;
	case 'F': interface = atoi(optarg); break;
	case 'Q': new_irq = atoi(optarg); break;
	case 'P': new_iobase = strtol(optarg, NULL, 16); break;
	case 'p':
	    port_base = strtol(optarg, NULL, 16);
	    break;
	case 'd': dump++; break;
	case 'm': test_multicast++; break;
	case 'r': reset_asic++; break;
	case 's': shared_mode++; break;
	case 'q': verbose--;		 break;
	case 'v': verbose++;		 break;
	case 'w': write_eeprom++;	 break;
	case '?':
	    errflag++;
	}
    if (errflag) {
		fprintf(stderr, "usage: at1700-diag [-e].\n");
		return 2;
    }

    if (verbose)
		printf(version);

    dev->name = "AT1700";
	dev->base_addr = ioaddr = port_base;
    dev->priv = lp;
    
    if (iopl(3) < 0) {
		perror("ethertest: iopl()");
		fprintf(stderr, "This program must be run as root.\n");
		return 1;
    }

	if (at1700_probe1(dev, ioaddr)) {
		printf("AT1700 not found.\n");
		if ( ! opt_f)
			return 0;
	}

	do_one_board(ioaddr);

	if (new_irq > 0 || new_iobase > 0) {
		int ioconfig = read_eeprom(ioaddr, 0);
		int new_ioconfig = ioconfig;
		int i;

		if (new_irq > 0) {
			for (i = 0; i < 4; i++)
				if (irqmap[i] == new_irq) {
					new_ioconfig &= 0x3fff;
					new_ioconfig |= i << 14;
					break;
				}
			if (i >= 4)
				fprintf(stderr, "Invalid new IRQ line, IRQ %d.\n", new_irq);
		}
		if (new_iobase > 0) {
			for (i = 0; i < 8; i++)
				if (iobasemap[i] == new_iobase) {
					new_ioconfig &= 0xf8ff;
					new_ioconfig |= i << 8;
					break;
				}
			if (i >= 8)
				fprintf(stderr, "Invalid new I/O base %#x.\n", new_iobase);
		}
		if (write_eeprom) {
			eeprom_write(ioaddr, 0, new_ioconfig);
		} else
			printf("Proposed configuration for the IRQ-I/O configuration "
				   "register is %4.4x.\n"
				   "Use '-w' to actually write this value.\n",
				   new_ioconfig);
	}

	at1700_report(ioaddr);

	if (verbose > 1) {
		net_open(dev);
		net_close(dev);
	}

	return 0;
}

int do_one_board(int ioaddr)
{
	short *sa = (short *)station_addr;
	int i;

	if (show_regs) {
		int page;
		printf("Device registers at %#x:", ioaddr);
		for (i = 0; i < 8; i+=2)
			printf(" %04x", inw(ioaddr + i));
		printf(" <paged regs>");
		for (i = 16; i < 24; i+=2)
			printf(" %04x", inw(ioaddr + i));
		printf(".\n");
		for (page = 0; page < 4; page++) {
			outb(0xe0 + (page << 2), ioaddr + CONFIG_1);
			printf("Bank %d:", page);
			for (i = 8; i < 16; i++)
				printf(" %2.2x", inb(ioaddr + i));
			printf(".\n");
		}
	}

	/* Read the EEPROM. */
	for (i = 0; i < EEPROM_SIZE; i++)
	  eeprom_contents[i] = read_eeprom(ioaddr, i);
	for(i = 0; i < 3; i++)
		sa[i] = ntohs(read_eeprom(ioaddr, 4+i));

	if (show_eeprom) {
		int i;
		printf("EEPROM contents:\n");
		for (i = 0; i < 16; i++)
			printf(" %04x", read_eeprom(ioaddr, i));
		printf("\n");

		printf("Station address in EEPROM: ");
		for(i = 0; i < 5; i++)
			printf("%2.2x:", station_addr[i]);
		printf("%2.2x.\n", station_addr[i]);
	}

	if (test_multicast)
		do_multicast_test(ioaddr);
	return 0;
}

int at1700_probe1(struct device *dev, int ioaddr)
{
	unsigned short signature[4]         = {0xffff, 0xffff, 0x7ff7, 0xffff};
	unsigned short signature_invalid[4] = {0xffff, 0xffff, 0x7ff7, 0xdfff};
	unsigned int i;

	/* Resetting the chip doesn't reset the ISA interface, so don't bother.
	   That means we have to be careful with the register values we probe for.
	   */
	for (i = 0; i < 4; i++)
		if ((inw(ioaddr + 2*i) | signature_invalid[i]) != signature[i]) {
			if (debug > 0)
				printf("AT1700 signature match at %#x failed at offset %d"
					   " (%04x vs. %04x)\n",
					   ioaddr, i, inw(ioaddr + 2*i), signature[i]);
			if ( ! opt_f)
				return -ENODEV;
		}

	/* Switch to bank 2 and lock our I/O address. */
	outb(0xe8, ioaddr + 7);
	outb(0, ioaddr + Mode13);

#ifdef HAVE_PORTRESERVE
	/* Grab the region so we can find another board if autoIRQ fails. */
	snarf_region(ioaddr, 32);
#endif

	dev->base_addr = ioaddr;
	dev->irq = irqmap[ read_eeprom(ioaddr, 0) >> 14 ];

	printf("AT1700 at %#3x, IRQ %d, address ", ioaddr, dev->irq);

	{
		short *sa = (short *)station_addr;
		for(i = 0; i < 3; i++)
			sa[i] = ntohs(read_eeprom(ioaddr, 4+i));
		for(i = 0; i < 5; i++)
			printf("%2.2x:", station_addr[i]);
		printf("%2.2x", station_addr[i]);
	}

	dev->if_port = read_eeprom(ioaddr, 12) & 0x4000 ? 0 : 1;
	printf(" using %s.\n", dev->if_port == 0 ? "10baseT" : "150ohm STP");

	/* Set the station address in bank zero. */
	for (i = 0; i < 6; i++)
		outb(dev->dev_addr[i], ioaddr + 8 + i);

	/* Switch to bank 2 and set the multicast table to accept all. */
	outb(0xe4, ioaddr + 7);
	for (i = 0; i < 8; i++)
		outb(0xff, ioaddr + 8 + i);

	/* Set the configuration register 0 to 32K 100ns. byte-wide memory, 16 bit
	   bus access, two 4K Tx queues, and disabled Tx and Rx. */
	outb(0xda, ioaddr + CONFIG_0);

	return 0;
}

/* Initialize the AT1700 at IOADDR. */
int at1700_report(int ioaddr)
{
	int i;

	/* Switch to bank zero to read the station address. */
	printf("Station address on the chip is: ");
	outb(0xe0, ioaddr + CONFIG_1);
	for (i = 0; i < 5; i++)
		printf("%02x:", inb(ioaddr + 8 + i));
	printf("%02x ", inb(ioaddr + 8 + i));

	printf("\nMulticast filter registers as read is:");
	outb(0xe4, ioaddr + CONFIG_1);
	for (i = 8; i < 16; i++)
		printf(" %02x", inb(ioaddr + i));
	printf("\n");
	return 0;
}



/*  EEPROM_Ctrl bits. */
#define EE_SHIFT_CLK	0x40	/* EEPROM shift clock, in reg. 16. */
#define EE_CS			0x20	/* EEPROM chip select, in reg. 16. */
#define EE_DATA_WRITE	0x80	/* EEPROM chip data in, in reg. 17. */
#define EE_DATA_READ	0x80	/* EEPROM chip data out, in reg. 17. */

/* Delay between EEPROM clock transitions. */
#define eeprom_delay()	do {} while (0);

/* The EEPROM commands include the alway-set leading bit. */
#define EE_WRITE_CMD	(5 << 6)
#define EE_READ_CMD		(6 << 6)
#define EE_ERASE_CMD	(7 << 6)

static int read_eeprom(int ioaddr, int location)
{
	int i;
	unsigned short retval = 0;
	int ee_addr = ioaddr + EEPROM_Ctrl;
	int ee_daddr = ioaddr + EEPROM_Data;
	int read_cmd = location | EE_READ_CMD;
	
	/* Shift the read command bits out. */
	for (i = 9; i >= 0; i--) {
		short dataval = (read_cmd & (1 << i)) ? EE_DATA_WRITE : 0;
		outb(EE_CS, ee_addr);
		outb(dataval, ee_daddr);
		eeprom_delay();
		outb(EE_CS | EE_SHIFT_CLK, ee_addr);	/* EEPROM clock tick. */
		eeprom_delay();
	}
	outb(EE_DATA_WRITE, ee_daddr);
	for (i = 16; i > 0; i--) {
		outb(EE_CS, ee_addr);
		eeprom_delay();
		outb(EE_CS | EE_SHIFT_CLK, ee_addr);
		eeprom_delay();
		retval = (retval << 1) | ((inb(ee_daddr) & EE_DATA_READ) ? 1 : 0);
	}

	/* Terminate the EEPROM access. */
	outb(EE_CS, ee_addr);
	eeprom_delay();
	outb(EE_SHIFT_CLK, ee_addr);
	outb(0, ee_addr);
	return retval;
}

static void eeprom_cmd(int ioaddr, int num_bits, int cmd)
{
	int ee_addr = ioaddr + EEPROM_Ctrl;
	int ee_daddr = ioaddr + EEPROM_Data;
	int i;
	/* Shift the command bits out. */
	for (i = num_bits; i >= 0; i--) {
		short dataval = (cmd & (1 << i)) ? EE_DATA_WRITE : 0;
		outb(EE_CS, ee_addr);
		outb(dataval, ee_daddr);
		eeprom_delay();
		outb(EE_CS | EE_SHIFT_CLK, ee_addr);	/* EEPROM clock tick. */
		eeprom_delay();
	}
	/* Terminate the EEPROM access. */
	outb(EE_CS, ee_addr);
	eeprom_delay();
	outb(EE_SHIFT_CLK, ee_addr);
	outb(0, ee_addr);
}
static void eeprom_write(int ioaddr, int location, int value)
{
	int cmd = ((location | EE_WRITE_CMD)<<16) | value;

	printf("Writing EEPROM location %d with %4.4x, command %#x.\n",
		   location, value, cmd);
	eeprom_cmd(ioaddr,  9, 0x130); /* Enabled writes. */
	eeprom_cmd(ioaddr, 25,  cmd);
	eeprom_cmd(ioaddr,  9, 0x100); /* Disable writes. */
}

void start_chip(int ioaddr)
{
	int i;

	/* Powerup the chip, initialize config register 1, and select bank 0. */
	outb(0xe0, ioaddr + CONFIG_1);
	outw(0x0000, ioaddr + IntrMask);

	/* Set the station address in bank zero. */
	for (i = 0; i < 6; i++)
		outb(station_addr[i], ioaddr + 8 + i);

	/* Switch to bank 2 and set the multicast table to accept all. */
	outb(0xe4, ioaddr + CONFIG_1);
#ifdef notdef
	for (i = 0; i < 8; i++)
		outb(0x80, ioaddr + 8 + i);
#endif

	/* Set the configuration register 0 to 32K 100ns. byte-wide memory, 16 bit
	   bus access, and two 4K Tx queues. */
	outb(0xda, ioaddr + CONFIG_0);
	outb(0xe8, ioaddr + CONFIG_1);
}

static int net_open(struct device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int ioaddr = dev->base_addr;
	int i;

	/* Powerup the chip, initialize config register 1, and select bank 0. */
	outb(0xe0, ioaddr + 7);

	/* Set the station address in bank zero. */
	for (i = 0; i < 6; i++)
		outb(dev->dev_addr[i], ioaddr + 8 + i);

	/* Switch to bank 2 and set the multicast table to accept all. */
	outb(0xe4, ioaddr + 7);
	for (i = 0; i < 8; i++)
		outb(0xff, ioaddr + 8 + i);

	/* Set the configuration register 0 to 32K 100ns. byte-wide memory, 16 bit
	   bus access, and two 4K Tx queues. */
	outb(0xda, ioaddr + 6);

	/* Same config 0, except enable the Rx and Tx. */
	outb(0x5a, ioaddr + 6);

	lp->open_time = jiffies;

	dev->tbusy = 0;
	dev->interrupt = 0;
	dev->start = 1;

	return 0;
}

/* The inverse routine to net_open(). */
static int net_close(struct device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int ioaddr = dev->base_addr;

	lp->open_time = 0;

	dev->tbusy = 1;
	dev->start = 0;

	/* Set configuration register 0 to disable Tx and Rx. */
	outb(0xda, ioaddr + 6);

	/* Power-down the chip.  Green, green, green! */
	outb(0x00, ioaddr + CONFIG_1);

	/* Update the statistics here. */
	return 0;
}

unsigned char rx_buffer[1536];

int send_packet(int ioaddr, int length, char *buf)
{
	int tx_start = inb(ioaddr + TX_START);
	if (verbose > 2)
		printf("Tx Status in transmit is %2.2x.\n", tx_start);
	outw(length, ioaddr + DATAPORT);
	outsw(ioaddr + DATAPORT, buf, (length + 1) >> 1);
	outb(tx_start + (0x80 | 1), ioaddr + TX_START);
	return 0;
}

int rx_packet(int ioaddr, unsigned char *rx_buffer)
{
	if ((inb(ioaddr + RxMode) & 0x40) == 0) {
		ushort status = inw(ioaddr + DATAPORT);
		ushort pkt_len = inw(ioaddr + DATAPORT);
		if ((status & 0xF0) != 0x20) {	/* There was an error. */
			return -1;
		}
		if (verbose > 2)
			printf("Rx status is %4.4x.\n", status);
		insw(ioaddr + DATAPORT, rx_buffer, (pkt_len + 1) >> 1);
		return pkt_len; 
#ifdef discard_packet
			/* Prime the FIFO and then flush the packet. */
			inw(ioaddr + DATAPORT); inw(ioaddr + DATAPORT);
			outb(0x05, ioaddr + 14);
#endif
	}
	return 0;
}


void do_multicast_test(int ioaddr)
{
	unsigned char mc_addr[6], tx_buffer[60];
	unsigned char rx_buffer[1500];
	unsigned char mc_filter[8];		 /* Multicast hash filter */
	int bit_to_test;
	int rx_length;
	int i;

	printf("\nRunning a multicast filter test.\n");
	start_chip(ioaddr);

	memcpy(tx_buffer+6, station_addr, 6);
	*(short *)(tx_buffer+12) = htons(0x0800);

	for (bit_to_test = 0; bit_to_test < 64; bit_to_test++) {
		/* Set the multicast filter. */
		memset(mc_filter, 0x00, sizeof(mc_filter));
		set_bit(bit_to_test, mc_filter);
		outb(0xda, ioaddr + CONFIG_0); /* Must turn off to set MC. */
		outb(0xe4, ioaddr + CONFIG_1);
		for (i = 0; i < 8; i++)
			outb(mc_filter[i], ioaddr + 8 + i);

		outb(0, ioaddr + TxMode);	/* Set loopback. */
		outb(2, ioaddr + RxMode);	/* Set normal Rx mode. */
		outb(0xe8, ioaddr + CONFIG_1); /* Set to operational bank 2 */
		outb(0x5a, ioaddr + CONFIG_0); /* Start chip. */

		/* Make up a multicast address. */
		mc_addr[0] = 0xff; mc_addr[1] = 0; mc_addr[2] = 0;
		mc_addr[3] = 0; mc_addr[4] = 0; mc_addr[5] = 0;

		for (i = 0; i < 0x1000; i++) {
			unsigned int crc;
			int j;
			mc_addr[4] = i>>8;
			mc_addr[5] = i;
			crc = ether_crc_le(ETH_ALEN, mc_addr);
			memcpy(tx_buffer, mc_addr, 6);
			/* Flush the Rx queue. */
			rx_length = rx_packet(ioaddr, rx_buffer);
			if (rx_length)
				printf("Flushing Rx frame of length %d.\n", rx_length);
			send_packet(ioaddr, 60, tx_buffer);
			usleep(1000);
			for (j = 40000; j > 0; j--)
				if (inb(ioaddr) & 0x80)
					break;
			if (verbose > 3)
				printf("Tx done at %d ticks.\n", 40000-j);
			for (j = 40; j > 0; j--)
				if ((inb(ioaddr + RxMode) & 0x40) == 0)
					break;
			rx_length =	rx_packet(ioaddr, rx_buffer);
			if ( (bit_to_test == crc>>26) ==  (rx_length <= 0)) {
				printf("MC mismatch on %#x CRC %8.8x (%d) %s ",
					   i, crc, crc >> 26,
					   memcmp(tx_buffer, rx_buffer, 60) ? " mismatch " :"");
				printf("%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x "
					   "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x "
					   "%2.2x%2.2x.\n",
					   rx_buffer[0], rx_buffer[1], rx_buffer[2],
					   rx_buffer[3], rx_buffer[4], rx_buffer[5],
					   rx_buffer[6], rx_buffer[7], rx_buffer[8],
					   rx_buffer[9], rx_buffer[10], rx_buffer[11],
					   rx_buffer[12], rx_buffer[13]);
			}
		}
	}

	printf("Multicast filter test finished.\n\n");
}

/* The little-endian AUTODIN II ethernet CRC calculation.
   N.B. Do not use for bulk data, use a table-based routine instead.
   This is common code and should be moved to net/core/crc.c */
static unsigned const ethernet_polynomial_le = 0xedb88320U;
static inline unsigned ether_crc_le(int length, unsigned char *data)
{
	unsigned int crc = 0xffffffff;	/* Initial value. */
	while(--length >= 0) {
		unsigned char current_octet = *data++;
		int bit;
		for (bit = 8; --bit >= 0; current_octet >>= 1) {
			if ((crc ^ current_octet) & 1) {
				crc >>= 1;
				crc ^= ethernet_polynomial_le;
			} else
				crc >>= 1;
		}
	}
	return crc;
}

void set_rx_mode(int ioaddr, unsigned char mc_addr[6], int invert)
{
	unsigned char mc_filter[8];		 /* Multicast hash filter */
	int saved_bank = inb(ioaddr + CONFIG_1);
	int i;

	for (i = 0; i < 8; i++)
		mc_filter[i] = 0;
	set_bit(ether_crc_le(ETH_ALEN, mc_addr) & 0x3f, mc_filter);
	if (invert)
		for (i = 0; i < 8; i++)
			mc_filter[i] ^= 0xff;

	/* Switch to bank 1 and set the multicast table. */
	outb(0xe4, ioaddr + CONFIG_1);
	for (i = 0; i < 8; i++)
		outb(mc_filter[i], ioaddr + 8 + i);
	outb(saved_bank, ioaddr + CONFIG_1);

	outb(0, ioaddr + TxMode);	/* Set loopback. */
	outb(2, ioaddr + RxMode);	/* Set normal Rx mode. */

	return;
}


/*
 * Local variables:
 *  compile-command: "cc -O -Wall -o at1700-diag at1700-diag.c"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
