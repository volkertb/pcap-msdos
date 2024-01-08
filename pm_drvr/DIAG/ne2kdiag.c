/*  ne2k-diag.c: Diagnostic program for NE2000 ethercards. */

static const char version_msg[] =
"ne2k-diag.c:v1.02 10/4/99 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";
static const char usage_msg[] =
"\t[Short] [Long arg] [Options]  - [definition]\n"
"\t-a  --show-registers      - show chip status (-aa for more)\n"
"\t-D  --debug               - describe program internal state\n"
"\t-e  --show-eeprom         - show EEPROM contents (-ee for more)\n"
"\t-p  --base-address 0xNNN  - specify card base address\n"
"\t-v  --verbose             - verbose mode\n"
"\t-w  --write               - commit the new values to the EEPROM\n"
"\t-h  --help                - display this usage message\n"
"\t-V  --version             - display version\n\n"
"\t-P  --new-address  0xNNN  - new card base address\n"
"\t-Q  --new-irq      IRQ    - new card IRQ\n"
"\t-X  --new-xcvr     N      - new transceiver index.\n"
;

/*
	Written 1993,1994,1999 by Donald Becker.
	Copyright 1994,1999 Donald Becker
	Copyright 1993 United States Government as represented by the
	Director, National Security Agency.

	This software may be used and distributed according to the terms of the
	Gnu Public License, incorporated herein	by reference.

	The author may be reached as becker@cesdis.gsfc.nasa.gov.
	C/O USRA Center of Excellence in Space Data and Information Sciences
	Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771

	This program is used to diagnose detection ("probe") problems with NE1000
	NE2000 work-alike cards.   The probe routine is very similar to that
	used in the Linux NE2000 driver, and should work with any NE2000
	clone.

	See the bottom of this file for the compile command.
	Do not forget the "-O"!

	Some of the names and comments for the #defines came from Crynwr.

	1999/Mar/18 James Bourne <jbourne@affinity-systems.ab.ca>
	Fixed -h --help and -V --version flags
	References
      RTL8019 datasheet, http://www.realtek.com.tw/cn/cn.html
*/

#ifndef __OPTIMIZE__
#warning  You must compile this program with the correct options!
#warning  See the last lines of the source file for the suggested command.
#error You must compile this driver with "-O".
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#if defined(__linux__)  &&  __GNU_LIBRARY__ == 1
#include <asm/io.h>
#else
#include <sys/io.h>
#endif



#define printk printf

#define NS_CMD	 (dev->base_addr)
#define NS_BASE	 (dev->base_addr)
#define NS_DATAPORT		0x10	/* NatSemi-defined port window offset. */
#define NE_DATAPORT		0x10	/* NatSemi-defined port window offset. */
#define NS_RESET		0x1f	/* Issue a read to reset, a write to clear. */

#define NE1SM_START_PG	0x20	/* First page of TX buffer */
#define NE1SM_STOP_PG	0x40	/* Last page +1 of RX ring */
#define NESM_START_PG	0x40	/* First page of TX buffer */
#define NESM_STOP_PG	0x80	/* Last page +1 of RX ring */

#define E8390_CMD		0x00	/* The command register (for all pages) */
#define E8390_STOP		0x01	/* Stop and reset the chip */
#define E8390_START		0x02	/* Start the chip, clear reset */
#define E8390_RREAD		0x08	/* Remote read */
#define E8390_NODMA		0x20	/* Remote DMA */
#define E8390_PAGE0		0x00	/* Select page chip registers */
#define E8390_PAGE1		0x40	/* using the two high-order bits */
#define E8390_PAGE2		0x80
#define E8390_PAGE3		0xC0	/* Page 3 is invalid on the real 8390. */

#define E8390_RXOFF 0x20		/* EN0_RXCR: Accept no packets */
#define E8390_TXOFF 0x02		/* EN0_TXCR: Transmitter off */

/* Page 0 register offsets. */
#define EN0_CLDALO		0x01	/* Low byte of current local dma addr  RD */
#define EN0_STARTPG		0x01	/* Starting page of ring bfr WR */
#define EN0_CLDAHI		0x02	/* High byte of current local dma addr	RD */
#define EN0_STOPPG		0x02	/* Ending page +1 of ring bfr WR */
#define EN0_BOUNDARY	0x03	/* Boundary page of ring bfr RD WR */
#define EN0_TSR			0x04	/* Transmit status reg RD */
#define EN0_TPSR		0x04	/* Transmit starting page WR */
#define EN0_NCR			0x05	/* Number of collision reg RD */
#define EN0_TCNTLO		0x05	/* Low	byte of tx byte count WR */
#define EN0_FIFO		0x06	/* FIFO RD */
#define EN0_TCNTHI		0x06	/* High byte of tx byte count WR */
#define EN0_ISR			0x07	/* Interrupt status reg RD WR */
#define EN0_CRDALO		0x08	/* low byte of current remote dma address RD */
#define EN0_RSARLO		0x08	/* Remote start address reg 0 */
#define EN0_CRDAHI		0x09	/* high byte, current remote dma address RD */
#define EN0_RSARHI		0x09	/* Remote start address reg 1 */
#define EN0_RCNTLO		0x0a	/* Remote byte count reg WR */
#define EN0_RCNTHI		0x0b	/* Remote byte count reg WR */
#define EN0_RSR			0x0c	/* rx status reg RD */
#define EN0_RXCR		0x0c	/* RX configuration reg WR */
#define EN0_TXCR		0x0d	/* TX configuration reg WR */
#define EN0_COUNTER0	0x0d	/* Rcv alignment error counter RD */
#define EN0_DCFG		0x0e	/* Data configuration reg WR */
#define EN0_COUNTER1	0x0e	/* Rcv CRC error counter RD */
#define EN0_IMR			0x0f	/* Interrupt mask reg WR */
#define EN0_COUNTER2	0x0f	/* Rcv missed frame error counter RD */


void write_EEPROM(int port_base, int regA, int regB, int regC);
static int do_probe(int ioaddr);
static void rtl8019(long ioaddr);

int opt_a = 0, debug = 0, show_eeprom = 0, opt_F = 0, new_irq = 0;
int new_port_base = 0;
int interface = -1;
int do_write_eeprom = 0;

struct option longopts[] = {
 /* { name	has_arg	 *flag	val } */
	{"show-registers",	0, 0, 'a'},
	{"debug",			0, 0, 'D'},
	{"show-eeprom",		0, 0, 'e'},
	{"base-address",	1, 0, 'p'}, /* Base address */
	{"help",			0, 0, 'h'},	/* Give help */
	{"verbose",			0, 0, 'v'},	/* Verbose mode */
	{"version",			0, 0, 'V'},	/* Display version number */
	{"interface",		1, 0, 'F'},	/* New transceiver index (10baseT, AUI) */
	{"new-address",		1, 0, 'P'}, /* Base address */
	{"new-irq",			1, 0, 'Q'},	/* New interrupt number */
	{ 0, 					0, 0, 0 }
};


/*	Probe for various non-shared-memory ethercards.

   NEx000-clone boards have a Station Address PROM (SAPROM) in the packet
   buffer memory space.	 NE2000 clones have 0x57,0x57 in bytes 0x0e,0x0f of
   the SAPROM, while other supposed NE2000 clones must be detected by their
   SA prefix.

   Reading the SAPROM from a word-wide card with the 8390 set in byte-wide
   mode results in doubled values, which can be detected and compensated for.

   The probe is also responsible for initializing the card and filling
   in the 'dev' and 'ei_status' structures.

   We use the minimum memory size for some ethercard product lines, iff we can't
   distinguish models.	You can increase the packet buffer size by setting
   PACKETBUF_MEMSIZE.  Reported Cabletron packet buffer locations are:
		E1010	starts at 0x100 and ends at 0x2000.
		E1010-x starts at 0x100 and ends at 0x8000. ("-x" means "more memory")
		E2010	starts at 0x100 and ends at 0x4000.
		E2010-x starts at 0x100 and ends at 0xffff.	 */

static void usage(char *prog)
{
	fprintf(stderr, "%s: %s", prog, version_msg);
	fprintf(stderr, "Usage: %s [-p 0xNNN] [-i IRQ] [-f N] [-v] [-h] [-V]\n",
			prog);
	fprintf(stderr, usage_msg);
}

int main(int argc, char *argv[])
{
	int show_version = 0, errflag = 0;
	int port_base = 0x300;
	int verbose = 0;
	int c;
	extern char *optarg;
	int option_index = 0;

	while ((c = getopt_long(argc,argv, "aDeF:i:p:P:Q:svwX:Vh",
							longopts, &option_index))
		!= -1) {
		switch (c) {
		case 'a': opt_a++; break;
		case 'D': debug++; break;
		case 'e': show_eeprom++; break;
		case 'F':
		case 'X': opt_F++; interface = atoi(optarg); break;
		case 'p': port_base = strtol(optarg, NULL, 16); break;
		case 'P': new_port_base = strtol(optarg, NULL, 16); break;
		case 'Q':
			new_irq = atoi(optarg);
			/* On ISA IRQ2 == IRQ9, with IRQ9 the proper name. */
			if (new_irq < 2 || new_irq > 15 || new_irq == 6 || new_irq == 8) {
				fprintf(stderr, "Invalid new IRQ %#x.  Valid values: "
						"3-5,7,9-15.\n", new_irq);
				errflag++;
			}
			break;
		case 'v': verbose++;	 break;
		case 'V': show_version++;		 break;
		case 'w': do_write_eeprom++;	break;
		case '?':
		case 'h':
			errflag++; break;
		}
	}
	if (errflag) {
		fprintf(stderr, usage_msg);
		return 3;
	}

	if (verbose || show_version)
		printf(version_msg);

	if (ioperm(port_base, 32, 1)) {
		perror("ne2k-diag: ioperm()");
		fprintf(stderr, "This program must be run as root.\n");
		return 2;
	}
	/* The following is needed for SLOW_DOWN_IO. */
	if (ioperm(0x80, 1, 1)) {
		perror("ioperm");
		return 1;
	}

	printf("Checking the ethercard at %#3x.\n", port_base);
	{	int regd;
		int ioaddr = port_base;

		outb_p(E8390_NODMA+E8390_PAGE1+E8390_STOP, ioaddr + E8390_CMD);
		regd = inb_p(ioaddr + 0x0d);
		printk("  Receive alignment error counter (%#x) is %2.2x\n",
			   ioaddr + 0x0d, regd);
		outb_p(0xff, ioaddr + 0x0d);
		outb_p(E8390_NODMA+E8390_PAGE0, ioaddr + E8390_CMD);
		inb_p(ioaddr + EN0_COUNTER0); /* Clear the counter by reading. */
		if (inb_p(ioaddr + EN0_COUNTER0) != 0) {
			outb(regd, ioaddr + 0x0d);	/* Restore the old values. */
			printk("  Failed initial NE2000 probe, value %2.2x.\n",
				   inb(ioaddr + EN0_COUNTER0));
		} else
			printk("  Passed initial NE2000 probe, value %2.2x.\n",
				   inb(ioaddr + EN0_COUNTER0));

	}

	do_probe(port_base);
	return 0;
}

static int do_probe(int ioaddr)
{
	int i;
	int neX000, ctron, dlink;
	unsigned char SA_prom[32];
	int wordlength = 2;
	char *name;
	int start_page, stop_page;

	struct {char value, offset; } program_seq[] = {
		{E8390_NODMA+E8390_PAGE0+E8390_STOP, E8390_CMD}, /* Select page 0*/
		{0x48,	EN0_DCFG},		/* Set byte-wide (0x48) access. */
		{0x00,	EN0_RCNTLO},	/* Clear the count regs. */
		{0x00,	EN0_RCNTHI},
		{0x00,	EN0_IMR},		/* Mask completion irq. */
		{0xFF,	EN0_ISR},
		{E8390_RXOFF, EN0_RXCR},		/* 0x20	 Set to monitor */
		{E8390_TXOFF, EN0_TXCR},		/* 0x02	 and loopback mode. */
		{32,			EN0_RCNTLO},
		{0x00,	EN0_RCNTHI},
		{0x00,	EN0_RSARLO},	/* DMA starting at 0x0000. */
		{0x00,	EN0_RSARHI},
		{E8390_RREAD+E8390_START, E8390_CMD},
	};

	/* Read the 16 bytes of station address prom, returning 1 for
	   an eight-bit interface and 2 for a 16-bit interface.
	   We must first initialize registers, similar to NS8390_init(eifdev, 0).
	   We can't reliably read the SAPROM address without this.
	   (I learned the hard way!). */
	for (i = 0; i < sizeof(program_seq)/sizeof(program_seq[0]); i++)
		outb_p(program_seq[i].value, ioaddr + program_seq[i].offset);

	for(i = 0; i < 32 /*sizeof(SA_prom)*/; i+=2) {
		SA_prom[i] = inb_p(ioaddr + NE_DATAPORT);
		SA_prom[i+1] = inb_p(ioaddr + NE_DATAPORT);
		if (SA_prom[i] != SA_prom[i+1])
			wordlength = 1;
	}

	printk("Station Address PROM    0:");
	for(i = 0; i < sizeof(SA_prom)/2; i++)
		printk(" %2.2x", SA_prom[i]);
	printk("\nStation Address PROM %#2x:", i);
	for(; i < sizeof(SA_prom); i++)
		printk(" %2.2x", SA_prom[i]);
	printk("\n");

	if (wordlength == 2) {
		/* We must set the 8390 for word mode, AND RESET IT. */
		int tmp;
		outb_p(0x49, ioaddr + EN0_DCFG);
		tmp = inb_p(ioaddr + NS_RESET);
		outb(tmp, ioaddr + NS_RESET);
		/* Un-double the SA_prom values. */
		for (i = 0; i < 16; i++)
			SA_prom[i] = SA_prom[i+i];
	}

	neX000 =  (SA_prom[14] == 0x57	&&	SA_prom[15] == 0x57);
	ctron =	 (SA_prom[0] == 0x00 && SA_prom[1] == 0x00 && SA_prom[2] == 0x1d);
	dlink =	 (SA_prom[0] == 0x00 && SA_prom[1] == 0xDE && SA_prom[2] == 0x01);

	/* Set up the rest of the parameters. */
	if (neX000 || dlink) {
		if (wordlength == 2) {
			name = dlink ? "DE200" : "NE2000";
			start_page = NESM_START_PG;
			stop_page = NESM_STOP_PG;
		} else {
			name = dlink ? "DE100" : "D-Link";
			start_page = NE1SM_START_PG;
			stop_page = NE1SM_STOP_PG;
		}
	} else if (ctron) {
		name = "Cabletron";
		start_page = 0x01;
		stop_page = (wordlength == 2) ? 0x40 : 0x20;
	} else {
		printk(" Invalid signature found, wordlength %d.\n", wordlength);
		return 1;
	}

	printk("  %s found at %#x, using start page %#x and end page %#x.\n",
		   name, ioaddr, start_page, stop_page);

	outb_p(E8390_NODMA + E8390_PAGE1, ioaddr + E8390_CMD);
	printf("The current MAC stations address is ");
	for (i = 1; i < 6; i++)
		printf("%2.2X:", inb(ioaddr + i));
	printf("%2.2X.\n", inb(ioaddr + i));
	
	if (opt_a) {
		int page; 
		for (page = 0; page < 4; page++) {
			printf("8390 page %d:", page);
			outb_p(E8390_NODMA + (page << 6), ioaddr + E8390_CMD);
			for(i = 0; i < 16; i++)
				printf(" %2.2x", inb_p(ioaddr + i));
			printf(".\n");
		}
	}

	outb_p(E8390_NODMA + E8390_PAGE0, ioaddr + E8390_CMD);
	/* Check for a RealTek 8019 chip. */
	if (inb(ioaddr + 10) == 'P'  &&  inb(ioaddr + 11) == 'p')
		rtl8019(ioaddr);

	return 0;
}

/* Serial EEPROM section. */
/* The description of EEPROM access. */
struct ee_ctrl_bits {
	int offset;
	unsigned char shift_clk,	/* Bit that drives SK (shift clock) pin */
		read_bit,				/* Mask bit for DO pin value */
		write_0, write_1,		/* Enable chip and drive DI pin with 0 / 1 */
		disable;				/* Disable chip. */
};

/* The EEPROM commands include the alway-set leading bit. */
enum EEPROM_Cmds { EE_WriteCmd=5, EE_ReadCmd=6, EE_EraseCmd=7, };

/* This executes a generic EEPROM command, typically a write or write enable.
   It returns the data output from the EEPROM, and thus may also be used for
   reads and EEPROM sizing. */
static int do_eeprom_cmd(struct ee_ctrl_bits *ee, int ioaddr, int cmd,
						 int cmd_len)
{
	int ee_addr = ioaddr + ee->offset;
	unsigned retval = 0;

	if (debug > 1)
		printf(" EEPROM op 0x%x: ", cmd);

	/* Shift the command bits out. */
	do {
		short dataval = (cmd & (1 << cmd_len)) ? ee->write_1 : ee->write_0;
		outb(dataval, ee_addr);
		if (debug > 2)
			printf("%X", inb(ee_addr) & 15);
		outb(dataval |ee->shift_clk, ee_addr);
		retval = (retval << 1) | ((inb(ee_addr) & ee->read_bit) ? 1 : 0);
	} while (--cmd_len >= 0);
	outb(ee->write_0, ee_addr);

	/* Terminate the EEPROM access. */
	outb(ee->disable, ee_addr);
	if (debug > 1)
		printf(" EEPROM result is 0x%5.5x.\n", retval);
	return retval;
}

/* Wait for the EEPROM to finish what it is doing. */
static int eeprom_busy_poll(struct ee_ctrl_bits *ee, long ioaddr)
{
	int ee_addr = ioaddr + ee->offset;
	int i;

	outb(ee->write_0, ee_addr);
	for (i = 0; i < 10000; i++)			/* Typical 2000 ticks */
		if (inb(ee_addr) & ee->read_bit)
			break;
	return i;
}

/* The abstracted functions for EEPROM access. */
static int read_eeprom(struct ee_ctrl_bits *ee, long ioaddr, int location)
{
	int addr_len = 6;
	return do_eeprom_cmd(ee, ioaddr, ((EE_ReadCmd << addr_len) | location) << 16,
						 3 + addr_len + 16) & 0xffff;
}

static void write_eeprom(struct ee_ctrl_bits *eebits, long ioaddr, int index,
						 int value)
{
	int addr_len = 6;
	int i;

	/* Poll for previous op finished. */
	eeprom_busy_poll(eebits, ioaddr);

	/* Enable programming modes. */
	do_eeprom_cmd(eebits, ioaddr, (0x4f << (addr_len-4)), 3 + addr_len);
	/* Do the actual write. */
	do_eeprom_cmd(eebits, ioaddr,
				  (((EE_WriteCmd<<addr_len) | index)<<16) | (value & 0xffff),
				  3 + addr_len + 16);
	i = eeprom_busy_poll(eebits, ioaddr);
	if (debug)
		printf(" Write finished after %d ticks.\n", i);
	/* Disable programming.  Note: this command is not instantaneous, but
	   we check for busy before the next write. */
	do_eeprom_cmd(eebits, ioaddr, (0x40 << (addr_len-4)), 3 + addr_len);
}



/* The RealTek RTL8019 specific routines. */
static const char irqmap[] = { 9, 3, 4, 5, 10, 11, 12, 15 };
static const char irq2config_map[16] = {
	-1,-1,0x00,0x10,  0x20,0x30,-1,-1,  -1,0x00,0x40,0x50, 0x60,-1,-1,0x70};
static const int iomap[] = {
	0x300, 0x320, 0x340, 0x360,  0x380, 0x3A0, 0x3C0, 0x3E0,
	0x200, 0x220, 0x240, 0x260,  0x280, 0x2A0, 0x2C0, 0x2E0 };
static const char *const xcvr_modes[4] = {
	"10baseT or coax, selected on 10baseT link beat",
	"10baseT with link test disabled", "10base5 / AUI", "10base2"};
struct ee_ctrl_bits rtl_ee_tbl = {0x01,  0x04, 0x01, 0x88, 0x8A, 0x00 };
#define EEPROM_SIZE 64

static void rtl8019(long ioaddr)
{
	int config0, config1, config2, config3, eeword0, eeword1;
	int i;

	/* The rtl8019-specific registers are on page 3. */  
	outb(E8390_NODMA+E8390_PAGE3, ioaddr + E8390_CMD);
	/* We do not use symbolic names for the Realtek-specific registers. */
	config0 = inb(ioaddr + 3);
	config1 = inb(ioaddr + 4);
	config2 = inb(ioaddr + 5);
	config3 = inb(ioaddr + 6);
	printf("This is a RTL8019%s chip in jumper%s%s mode.\n",
		   config0 & 0xC0 ? "" : "AS", config0 & 0x08 ? "ed" : "less",
		   (config0 & 0x08) == 0 && (config3 & 0x80) ? " PnP" :"");
	printf("  The current settings are: IRQ %d (%sabled, I/O address 0x%x,\n",
		   irqmap[(config1>>4) & 7], config1 & 0x80 ? "en" : "dis",
		   iomap[config1 & 15]);
	printf("     The transceiver is set to %s,\n"
		   "     The chip is set to %s duplex.\n",
		   xcvr_modes[config2 >> 6], config3 & 0x40 ? "full" : "half");
	eeword0 = read_eeprom(&rtl_ee_tbl, ioaddr, 0);
	eeword1 = read_eeprom(&rtl_ee_tbl, ioaddr, 1);
	config1 = eeword0;
	config2 = eeword0 >> 8;
	config3 = eeword1;
	printf("  The EEPROM settings are: IRQ %d, I/O address 0x%x, %sPnP mode.\n"
		   "     The transceiver is set to %s,\n"
		   "     The chip is set to %s duplex.\n",
		   irqmap[(config1>>4) & 7], iomap[config1 & 15],
		   (config3 & 0x80) ? "" :"non-",
		   xcvr_modes[config2 >> 6], config3 & 0x40 ? "full" : "half");
	if (show_eeprom) {
		unsigned short sum = 0, val;
		int i;
		printf("EEPROM contents:");
		for (i = 0; i < EEPROM_SIZE; i++) {
			val = read_eeprom(&rtl_ee_tbl, ioaddr, i);
			printf("%s %4.4x", (i & 7) == 0 ? "\n ":"", val);
			sum += val;
		}
		printf("\n The word-wide EEPROM checksum is %#4.4x.\n", sum);
	}
	if (opt_F  ||  new_irq > 0  ||  new_port_base > 0) {
		int newval0 = eeword0;
		if (opt_F  &&  interface >= 0  &&  interface < 4) {
			newval0 &= 0x3fff;
			newval0 |= interface << 14;
		}
		if (new_irq > 0 && new_irq < 16 && irq2config_map[new_irq] > 0) {
			newval0 &= 0xff8f;
			newval0 |= irq2config_map[new_irq];
		}

		for (i = 0; i < 16; i++)
			if (iomap[i] == new_port_base) {
				newval0 &= 0xfff0;
				newval0 |= i;
				break;
			}
		if (do_write_eeprom) {
			printf(" Writing 0x%4.4x to EEPROM word 0.\n", newval0);
			write_eeprom(&rtl_ee_tbl, ioaddr, 0, newval0);
		} else
			printf(" Would write 0x%4.4x to EEPROM word 0 with '-w'.\n",
				   newval0);
	}
}

/*
 * Local variables:
 *  compile-command: "gcc -Wall -O6 -o ne2k-diag ne2k-diag.c"
 *  tab-width: 4
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 */
