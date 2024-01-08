/*  ne2k.c: Diagnostic program for NE2000 ethercards. */
/*
	Written 1993,1994 by Donald Becker.
	Copyright 1994 Donald Becker
	Copyright 1993 United States Government as represented by the
	Director, National Security Agency.

	This software may be used and distributed according to the terms of the
	Gnu Public Lincese, incorporated herein	by reference.

	The author may be reached as becker@cesdis.gsfc.nasa.gov.
	C/O USRA Center of Excellence in Space Data and Information Sciences
	Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771

    This diagnotic routine should work with many NE2000 clones.
	Comile this file with "gcc -Wall -O6 -N -o ne2k ne2k.c".
	Do not forget the "-O"!

    Some of the names and comments for the #defines came from Crynwr.
*/

static char *version =
	"ne2k.c:v0.01 7/21/93 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <asm/io.h>
#include <getopt.h>
/* #include "8390.h" */


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
#define E8390_PAGE2		0x80	/* Page 3 is invalid. */

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
int do_probe(int ioaddr);

struct option longopts[] = {
 /* { name	has_arg	 *flag	val } */
	{"base-address", 1, 0, 'p'}, /* Give help */
	{"help",	   0, 0, 'h'},	/* Give help */
	{"interface",  0, 0, 'f'},	/* Interface number (built-in, AUI) */
	{"irq",		   1, 0, 'i'},	/* Interrupt number */
	{"ne2000",	   1, 0, 'N'},	/* Switch to NE2000 mode */
	{"verbose",	   0, 0, 'v'},	/* Verbose mode */
	{"version",	   0, 0, 'V'},	/* Display version number */
	{ 0, 0, 0, 0 }
};


/*	Probe for various non-shared-memory ethercards.
   
   NEx000-clone boards have a Station Address PROM (SAPROM) in the packet
   buffer memory space.	 NE2000 clones have 0x57,0x57 in bytes 0x0e,0x0f of
   the SAPROM, while other supposed NE2000 clones must be detected by their
   SA prefix.

   Reading the SAPROM from a word-wide card with the 8390 set in byte-wide
   mode results in doubled values, which can be detected and compansated for.

   The probe is also responsible for initializing the card and filling
   in the 'dev' and 'ei_status' structures.

   We use the minimum memory size for some ethercard product lines, iff we can't
   distinguish models.	You can increase the packet buffer size by setting
   PACKETBUF_MEMSIZE.  Reported Cabletron packet buffer locations are:
		E1010	starts at 0x100 and ends at 0x2000.
		E1010-x starts at 0x100 and ends at 0x8000. ("-x" means "more memory")
		E2010	 starts at 0x100 and ends at 0x4000.
		E2010-x starts at 0x100 and ends at 0xffff.	 */

int
main(int argc, char *argv[])
{
	int port_base = 0x300, irq = -1;
	int errflag = 0, verbose = 0, shared_mode = 0, interface = -1;
	int c;
	extern char *optarg;

	while ((c = getopt(argc, argv, "f:i:p:svw")) != -1)
		switch (c) {
		case 'f':
			interface = atoi(optarg);
			break;
		case 'i':
			irq = atoi(optarg);
			break;
		case 'p':
			port_base = strtol(optarg, NULL, 16);
			break;
		case 's': shared_mode++; break;
		case 'v': verbose++;	 break;
		case '?':
			errflag++;
		}
	if (errflag) {
		fprintf(stderr, "usage:");
		return 2;
	}

	if (verbose)
		printf(version);
	if (ioperm(port_base, 32, 1)) {
		perror("io-perm");
		return 1;
	}
	/* The following is needed for SLOW_DOWN_IO. */
	if (ioperm(0x80, 1, 1)) {
		perror("io-perm");
		return 1;
	}
	
	printf("Checking the ethercard at %#3x.\n", port_base);
	{	int regd;
		int ioaddr = port_base;
		outb_p(E8390_NODMA+E8390_PAGE1+E8390_STOP, ioaddr + E8390_CMD);
		regd = inb_p(ioaddr + 0x0d);
		printk("  Register 0x0d (%#x) is %2.2x\n", ioaddr + 0x0d, regd);
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

int do_probe(int ioaddr)
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

	printk("8390 registers:");
	for(i = 0; i < 16; i++)
		printk(" %2.2x", inb(ioaddr + i));

	for(i = 0; i < 32 /*sizeof(SA_prom)*/; i+=2) {
		SA_prom[i] = inb_p(ioaddr + NE_DATAPORT);
		SA_prom[i+1] = inb_p(ioaddr + NE_DATAPORT);
		if (SA_prom[i] != SA_prom[i+1])
			wordlength = 1;
	}

	printk("\nSA PROM	 0:");
	for(i = 0; i < sizeof(SA_prom)/2; i++)
		printk(" %2.2x", SA_prom[i]);
	printk("\nSA PROM %#2x:", i);
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

	printk("\n	%s found at %#x, using start page %#x and end page %#x.\n",
		   name, ioaddr, start_page, stop_page);

	return 0;
}

/*
 * Local variables:
 *  compile-command: "gcc -Wall -O6 -N -o ne2k ne2k.c"
 *  tab-width: 4
 *  c-indent-level: 4
 * End:
 */
