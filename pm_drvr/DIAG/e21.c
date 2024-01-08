/* el21.c: Diagnostic program for Cabletron E2100 ethercards. */
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
*/

static char *version =
	"e21.c:v0.03 11/5/96 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <asm/io.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

/* #include "8390.h" */


#define printk printf

/* Offsets from the base_addr. */
#define E21_NIC_OFFSET	0		/* Offset to the 8390 NIC. */
#define E21_ASIC		0x10	/* The E21** series ASIC, know as PAXI. */
/* The following registers are heavy-duty magic.  Their obvious function is
   to provide the hardware station address.  But after you read from them the
   three low-order address bits of the next outb() sets a write-only internal
   register! */
#define E21_MEM_ENABLE	0x10
#define E21_MEM_BASE	0x11
#define	 E21_MEM_ON		0x05	/* Enable memory in 16 bit mode. */
#define	 E21_MEM_ON_8	0x07	/* Enable memory in	 8 bit mode. */
#define E21_IRQ_LOW		0x12	/* Low three bits of the IRQ selection. */
#define E21_IRQ_HIGH	0x14	/* High bit of the IRQ, and media select. */
#define E21_SAPROM		0x18	/* Offset to station address data. */
#define ETHERCARD_TOTAL_SIZE	0x20


extern inline void mem_on(short port, volatile char *mem_base,
												  unsigned char start_page )
{
		/* This is a little weird: set the shared memory window by doing a
		   read.  The low address bits specify the starting page. */
		mem_base[start_page];
		inb(port + E21_MEM_ENABLE);
		outb(E21_MEM_ON, port + E21_MEM_ENABLE + E21_MEM_ON);
}

extern inline void mem_off(short port)
{
		inb(port + E21_MEM_ENABLE);
		outb(0x00, port + E21_MEM_ENABLE);
}

struct option longopts[] = {
 /* { name	has_arg	 *flag	val } */
	{"base-address", 1, 0, 'p'}, /* Give help */
	{"help",	   0, 0, 'h'},	/* Give help */
	{"force",  0, 0, 'f'},		/* Force an operation, even with bad status. */
	{"irq",		   1, 0, 'i'},	/* Interrupt number */
	{"verbose",	   0, 0, 'v'},	/* Verbose mode */
	{"version",	   0, 0, 'V'},	/* Display version number */
	{ 0, 0, 0, 0 }
};


/*	Probe for E2100 series ethercards.
   
   E21xx boards have a "PAXI" located in the 16 bytes above the 8390.
   The "PAXI" reports the station address when read, and has an wierd
   address-as-data scheme to set registers when written.
*/

int
main(int argc, char *argv[])
{
	int port_base = 0x300, irq = -1;
	int errflag = 0, verbose = 0, shared_mode = 0, force = 0;
	int c, memfd, ioaddr, i;
	extern char *optarg;
	caddr_t shared_mem;
	unsigned char station_addr[6];
	int if_port = 0, dump_shared_memory = 0;

	while ((c = getopt(argc, argv, "fi:mp:svw")) != -1)
		switch (c) {
		case 'f':  force++; break;
		case 'i':
			irq = atoi(optarg);
			break;
		case 'm':
		  dump_shared_memory++;
		  break;
		case 'p':
			port_base = strtol(optarg, NULL, 16);
			break;
		case 's':  shared_mode++; break;
		case 'v':  verbose++;	 break;
		case '?':
			errflag++;
		}
	if (errflag) {
		fprintf(stderr, "usage: e21 [-fmv] [-f <port>]");
		return 2;
	}

	if (verbose)
		printf(version);

	/* Turn on I/O permissions before accessing the board registers. */
	if (ioperm(port_base, 32, 1)
		|| ioperm(0x80, 1, 1)) {				/* Needed for SLOW_DOWN_IO. */
		perror("io-perm");
		fprintf(stderr, " You must be 'root' to run hardware diagnotics.\n");
		return 1;
	}

	ioaddr = port_base;
	printf("Checking for an Cabletron E2100 series ethercard at %#3x.\n",
		   port_base);
	/* Check for 8390-like registers. */
	{
		int cmdreg;
		int oldcmdreg = inb(ioaddr);
		outb_p(0x21, ioaddr);
		cmdreg = inb(ioaddr);
		if (cmdreg != 0x21	&&	cmdreg != 0x23) {
			outb(oldcmdreg, ioaddr);			/* Restore the old values. */
			printk(" Failed initial E2100 probe.\n"
				   " The 8390 command register value was %#2.2x instead"
				   " of 0x21 of 0x23.\n"
				   "Use the '-f' flag to ignore this failure and proceed, or\n"
				   "use '-p <port>' to specify the correct base I/O address.\n",
				   cmdreg);
			if (! force) return 0;
		} else
			printk("  Passed initial E2100 probe, value %2.2x.\n", cmdreg);
	}

	printk("8390 registers:");
	for (i = 0; i < 16; i++)
		printk(" %02x", inb(ioaddr + i));
	printk("\nPAXI registers:");
	for (i = 0; i < 16; i++)
		printk(" %02x", inb(ioaddr + 16 + i));
	printk("\n");

	printf("The ethernet station address is ");
	/* Read the station address PROM.  */
	for (i = 0; i < 6; i++) {
		station_addr[i] = inb(ioaddr + E21_SAPROM + i);
		printk("%2.2x%c", station_addr[i], i < 5 ? ':' : '\n');
	}

	/* Do a media probe.  This is magic.
	   First we set the media register to the primary (TP) port. */
	inb_p(ioaddr + E21_IRQ_HIGH); 	/* Select if_port detect. */
	for(i = 0; i < 6; i++)
		if (station_addr[i] != inb(ioaddr + E21_SAPROM + i))
			if_port = 1;

	if (if_port)
	  printk("Link beat not detected on the primary transceiver.\n"
			 "  Using the secondany transceiver instead.\n");
	else
	  printk("Link beat detected on the primary transceiver.\n");
	
	/* Map the board shared memory into our address space -- this code is
	   a good example of non-kernel access to ISA shared memory space. */
	memfd = open("/dev/kmem", O_RDWR);
	if (memfd < 0) {
		perror("/dev/kmem (shared memory)");
		return 2;
	} else
	  printf("Mapped in happy.\n");
	shared_mem = mmap((caddr_t)0xd0000, 0x8000, PROT_READ|PROT_WRITE,
					  MAP_SHARED, memfd, 0xd0000);
	printf("Shared memory at %#x.\n", (int)shared_mem);
	
	inb_p(ioaddr + E21_MEM_BASE);
	inb_p(ioaddr + E21_MEM_BASE);
	outb_p(0, ioaddr + E21_ASIC + ((0xd000 >> 13) & 7));

	{
		volatile ushort *mem = (ushort*)shared_mem;
		int page;
		for (page = 0; page < 0x41; page++) {
			mem_on(ioaddr, shared_mem, page);
			mem[0] = page << 8;
			mem_off(ioaddr);
		}

		printk("Read %04x %04x.\n", mem[0], mem[1]);
		if (dump_shared_memory) {
		  for (page = 0; page < 256; page++) {
			int i;
			mem_on(ioaddr, shared_mem, page);
			printk("Page %#x:", page);
			for (i = 0; i < 16; i++)
				printk(" %04x", mem[i*128]);
			printk("\n");
			/*printk(" %04x%s", mem[0], (page & 7) == 7 ? "\n":"");*/
			mem_off(ioaddr);
		  }
		  printk("\n");
		}
		mem[0] = 0x5742;
		printk("Read %04x %04x.\n", mem[0], mem[1]);
		printk("Read %04x %04x.\n", mem[2], mem[3]);
	}
	/* do_probe(port_base);*/
	return 0;
}


/*
 * Local variables:
 *  compile-command: "gcc -Wall -O6 -N -o e21 e21.c"
 *  tab-width: 4
 *  c-indent-level: 4
 * End:
 */
