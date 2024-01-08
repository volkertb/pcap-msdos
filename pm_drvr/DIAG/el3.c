/* el3-diag.c: Diagnostic program for 3c509 and 3c579 ethercards.

   NOTICE: This program *must* be compiled with '-O'.
   See the compile-command at the end of the file.

	Written 1993-1997 by Donald Becker.
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
	"el3diag.c:v0.11 3/16/97 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <asm/io.h>
#include <sys/time.h>
#include <netinet/in.h>				/* For byte-sex functions. */

struct option longopts[] = {
 /* { name	has_arg	 *flag	val } */
	{"base-address", 1, 0, 'p'},
	{"all",		   0, 0, 'a'},	/* Print all registers. */
	{"help",	   0, 0, 'h'},	/* Give help */
	{"interface",  0, 0, 'f'},	/* Interface number (built-in, AUI) */
	{"irq",		   1, 0, 'i'},	/* Interrupt number */
	{"quiet",	   0, 0, 'q'},	/* Tone down verbosity mode */
	{"verbose",	   0, 0, 'v'},	/* Verbose mode */
	{"version",	   0, 0, 'V'},	/* Display version number */
	{"write-EEPROM", 1, 0, 'w'},/* Write th EEPROMS with the specified vals */
	{ 0, 0, 0, 0 }
};

#define printk printf

#ifdef EL3_DEBUG
int el3_debug = EL3_DEBUG;
#else
int el3_debug = 6;
#endif

/* Offsets from base I/O address. */
#define EL3_DATA 0x00
#define EL3_CMD 0x0e
#define	 EEPROM_READ 0x80

/* Register window 1 offsets, used in normal operation. */
#define TX_FREE 0x0C
#define TX_STATUS 0x0B
#define TX_FIFO 0x00
#define RX_FIFO 0x00

int el3_probe(void);

/* A minimal device structure so that we can use the driver transmit code
   with as few changes as possible. */
struct device {
	char *name;
	short base_addr;
	int tbusy;
	int trans_start;
} devs, *dev;

int verbose = 1;
int jiffies;
unsigned char fake_packet[100];

int el3_send_packet(void *pkt, int len);
static ushort id_read_eeprom(int id_port, int index);



int
main(int argc, char **argv)
{
	int port_base = -1, irq = -1;
	int errflag = 0, shared_mode = 0;
	int write_eeprom = 0, interface = -1, all_regs = 0;
	int c, card, longind;
	extern char *optarg;
	dev = &devs;

	while ((c = getopt_long(argc, argv, "af:i:p:qsvw", longopts, &longind))
		   != -1)
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
		case 'q': verbose--;			 break;
		case 's': shared_mode++; break;
		case 'v': verbose++;			 break;
		case 'w': write_eeprom++;		 break;
		case 'a': all_regs++;			 break;
		case '?':
			errflag++;
		}
	if (errflag) {
		fprintf(stderr, "usage:");
		return 2;
	}

	if (verbose)
		printf(version);

	el3_debug += verbose;

	dev->name = "el3";
	
	for (card = 0; card < 4; card++) {
	   printf("Looking for card %d.\n", card+1);
	   if (el3_probe() < 0)
		   break;
	}
	if (verbose > 3)
		el3_send_packet(fake_packet, 42);

	return 0;
}

int
el3_probe()
{
	static int current_tag = 0;
	static int id_port = 0x100;
	unsigned short lrs_state = 0xff, i, j;
	int ioaddr = 0x320;
	unsigned short irq;
	unsigned short eeprom_data[64];
	int if_port;

	if (current_tag == 0) {
		/* Select an open I/O location at 0x1*0 to do contention select. */
		for (id_port = 0x100; id_port < 0x200; id_port += 0x10) {
			/* Turn on I/O access permission for just this tiny region. */
			if (ioperm(id_port, 2, 1) < 0) {
				perror("EtherLink III test: ioperm()");
				fprintf(stderr, "This program must be run as root.\n");
				exit(2);
			}
			outb(0x00, id_port);
			outb(0xff, id_port);
			if (inb(id_port) & 0x01)
				break;
			/* Turn off I/O permission -- paranoia. */
			ioperm(id_port, 2, 0);
		}
		if (id_port >= 0x200) {			/* GCC optimizes this test out. */
			fprintf(stderr,
					" Error: No I/O port in the range 0x100..0x1f0"
					" available for 3c509 activation.\n");
			exit(3);
		}
	}

	printf("Generating the activation sequence on port %#3.3x for card %d.\n",
		   id_port, current_tag + 1);
	/* Check for all ISA bus boards by sending the ID sequence to the
	   ID_PORT.  We find cards past the first by setting the 'current_tag'
	   on cards as they are found.  Cards with their tag set will not
	   respond to subsequent ID sequences. */
	outb(0x00, id_port);
	outb(0x00, id_port);
	outb(0x00, id_port);
	for(i = 0; i < 255; i++) {
		if (verbose > 3) printf("	 %d %4.4x", i, lrs_state);
		outb(lrs_state, id_port);
		lrs_state <<= 1;
		lrs_state = lrs_state & 0x100 ? lrs_state ^ 0xcf : lrs_state;
	}

	/* For the first probe, clear all board's tag registers. */
	if (current_tag == 0)
		outb(0xd0, id_port);
	else				/* Otherwise kill off already-found boards. */
		outb(0xd8, id_port);

	/* Read in all EEPROM data, which does contention-select.
	   The unique station address is in the first three words, so only
	   the lowest address board will stay "on-line".
	   3Com has the station address byte-sex "backwards" the x86, but
	   in the correct order for contention-select. */
	for (i = 0; i < 16; i++) {
		eeprom_data[i] = id_read_eeprom(id_port, i);
	}

	/* Set the adaptor tag so that the next card can be found. */
	outb(0xd0 + ++current_tag, id_port);

	{
		unsigned short iobase = eeprom_data[8];
		irq = eeprom_data[9];
		if_port = iobase >> 14;
		ioaddr = 0x200 + ((iobase & 0x1f) << 4);
		dev->base_addr = ioaddr;
		dev->name = "eth0";
		irq >>= 12;
	}

	if (verbose > 1)
		printk("%s: ID sequence ended with %#2.2x.\n", dev->name, lrs_state);

	/* Activate the adaptor at the EEPROM location.
	   NOTE: Not the "current" location -- which may be different! */
	printf("Activating the card at I/O address %3.3x.\n", ioaddr);
	outb((ioaddr >> 4) | 0xe0, id_port);

	if (eeprom_data[7] != 0x6d50) {
	  fprintf(stderr,
			  "No ISA EtherLink III boards appear to be at index %d.\n",
			  current_tag);
	  return -1;
	}
	if (verbose) {
	  printf("EEPROM contents:");
	  for (i = 0; i < 16; i++)
		printf("%s %4.4x", i % 16 == 0 ? "\n":"",  eeprom_data[i]);
	}

	printf("\nAn ISA EtherLink III board was activated at I/O %#3.3x, IRQ %d.\n",
		   ioaddr, irq);

#ifdef notdef
	iobase = 0x0000;
	if (iobase == 0x0000) {
		dev->base_addr = 0x320;
		printk("%s: 3c509 has no pre-set base address, using %#x.\n",
			   dev->name, dev->base_addr);
		outb(0xf2, id_port);
	} else {
		dev->base_addr = 0x200 + ((iobase & 0x1f) << 0x10);
		outb(0xff, id_port);
	}
#endif  /* notdef */

	if (ioperm(ioaddr, 18, 1) < 0) {
		perror("ethertest: ioperm()");
		return 1;
	}

	outw(0x0800, ioaddr + 0x0e); /* Change to EL3WINDOW(0); */
	if (inw(ioaddr) != 0x6d50) {
	  fprintf(stderr,
			  "The board was activated at %#3.3x, but does not appear in"
			  " I/O space!\n"
			  "ID register at %#3.3x is %#2.2x, status register at %#3.3x"
			  " is %#2.2x.\n",
			  ioaddr, ioaddr, inw(ioaddr), ioaddr + 0x0e, inw(ioaddr + 0x0e));
  } else
	printk("%s: 3c509 found at %#3.3x.\n", dev->name, dev->base_addr);

#ifdef notdef
	for (j = 0; j < 16; j++) {
		struct timeval timeout = {0, 162};
		unsigned int eeprom_in = 0;
		outb(EEPROM_READ + j, id_port);
		select(0,0,0,0,&timeout);
		for (i = 0; i < 16; i++) {
			int eeprom = inb(id_port);
			if (el3_debug > 6) printf("%x", eeprom&0x01);
			eeprom_in = (eeprom_in << 1) + (eeprom & 0x01);
		}
		printk(" EEPROM location %2x is %4.4x\n", j, eeprom_in);
	}
	outb(0x00, id_port);
	outb(0x00, id_port);
#endif

	for (j = 0; j < 8; j++) {
		printk("Window %d:", j);
		outw(0x0800 + j, ioaddr + 0x0e);
		for (i = 0; i < 16; i+=2)
			printk(" %4.4x", inw(ioaddr + i));
		printk(".\n");
	}
	printf("   Done card %d.\n", current_tag);
	return 0;
}

int
el3_send_packet(void *pkt, int len)
{
	int ioaddr = dev->base_addr;

	outw(0x0801, ioaddr + EL3_CMD);
	if (el3_debug > 2) {
		printk("%s: el3_start_xmit(lenght = %d) called, status %4.4x.\n",
			   dev->name, len, inw(ioaddr + EL3_CMD));
	}

	outw(0x5800, ioaddr + EL3_CMD);
	outw(0x4800, ioaddr + EL3_CMD);

	outw(0x78ff, ioaddr + EL3_CMD); /* Allow all status bits to be seen. */
	outw(0x7098, ioaddr + EL3_CMD); /* Set interrupt mask. */

	/* Avoid timer-based retransmission conflicts. */
	dev->tbusy=1;

	/* Put out the doubleword header... */
	outw(len, ioaddr + TX_FIFO);
	outw(0x00, ioaddr + TX_FIFO);
	if (el3_debug > 4)
		printk("		Started queueing packet, FIFO room %d status %4.4x.\n",
			   inw(ioaddr + TX_FREE), inw(ioaddr+EL3_CMD));
	/* ... and the packet rounded to a doubleword. */
	outsl(ioaddr + TX_FIFO, (void *)pkt, (len + 3) >> 2);
	
	if (el3_debug > 4)
		printk("		Finished queueing packet, FIFO room remaining %d.\n",
			   inw(ioaddr + TX_FREE));
	dev->trans_start = jiffies;
	
	if (inw(ioaddr + TX_FREE) > 1536) {
		dev->tbusy=0;
	} else
		/* Interrupt us when the FIFO has room for max-sized packet. */
		outw(0x9000 + 1536, ioaddr + EL3_CMD);

	if (el3_debug > 4)
		printk("		Checking packet queue, FIFO room %d status %2.2x.\n",
			   inw(ioaddr + TX_FREE), inb(ioaddr+0xb));

	return 0;
}

/* Read a word from the EEPROM when in the ISA ID probe state. */
static ushort id_read_eeprom(int id_port, int index)
{
	int bit, word = 0;

	/* Issue read command, and pause for at least 162 us. for it to complete.
	   Assume extra-fast 16Mhz bus. */
	outb(EEPROM_READ + index, id_port);

	/* Pause for at least 162 us. for the read to take place. */
	usleep(162);

	for (bit = 15; bit >= 0; bit--)
		word = (word << 1) + (inb(id_port) & 0x01);

	if (verbose > 3)
		printk("  3c509 EEPROM word %d %#4.4x.\n", index, word);

	return word;
}

/*
 * Local variables:
 *  compile-command: "cc -N -O -Wall -o el3-diag el3.c"
 *  tab-width: 4
 *  c-indent-level: 4
 * End:
 */
