/* eexpress.c: Diagnostic program for Intel EtherExpress under Linux 1.0.

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
	"eexpress.c:v0.02 7/21/93 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <asm/io.h>
#include <sys/time.h>


/* Offsets from the base address. */
#define DATA_REG		0		/* Data Transfer Register. */
#define WRITE_PTR		2		/* Write Address Pointer. */
#define Read_Ptr		4		/* Read Address Pointer. */
#define CA_Ctrl			6		/* Channel Attention Control. */
#define Sel_IRQ			7		/* IRQ Select. */
#define SMB_Ptr			8		/* Shadow Memory Bank Pointer. */
#define MEM_Ctrl		11
#define MEM_Page_Ctrl	12
#define Config			13
#define EEPROM_Ctrl		14
#define ID_PORT			15

/*	EEPROM_Ctrl bits. */

#define EE_SHIFT_CLK	0x01	/* EEPROM shift clock. */
#define EE_CS			0x02	/* EEPROM chip select. */
#define EE_DATA_WRITE	0x04	/* EEPROM chip data in. */
#define EE_DATA_READ	0x08	/* EEPROM chip data out. */
#define EE_CTRL_BITS	(EE_SHIFT_CLK | EE_CS | EE_DATA_WRITE | EE_DATA_READ)
#define ASIC_RESET		0x40
#define _586_RESET		0x80

/* Delay between EEPROM clock transitions. */
#define eeprom_delay()	do { int _i = 40; while (--_i > 0) { __SLOW_DOWN_IO; }} while (0)

#define EE_WRITE_CMD	(5 << 6)
#define EE_READ_CMD		(6 << 6)
#define EE_ERASE_CMD	(7 << 6)

struct option longopts[] = {
 /* { name	has_arg	 *flag	val } */
	{"base-address", 1, 0, 'p'},
	{"all",		   0, 0, 'a'},	/* Print all registers. */
	{"help",	   0, 0, 'h'},	/* Give help */
	{"interface",  0, 0, 'f'},	/* Interface number (built-in, AUI) */
	{"irq",		   1, 0, 'i'},	/* Interrupt number */
	{"verbose",	   0, 0, 'v'},	/* Verbose mode */
	{"version",	   0, 0, 'V'},	/* Display version number */
	{"write-EEPROM", 1, 0, 'w'},/* Write th EEPROMS with the specified vals */
	{ 0, 0, 0, 0 }
};

#define printk printf

int verbose = 0;
struct device {
	char *name;
	short base_addr;
	int tbusy;
	int trans_start;
	unsigned char dev_addr[6];	/* Hardware station address. */
} devs, *dev = &devs;

unsigned char fake_packet[100];

int expprobe(short ioaddr);
void show_eeprom(short ioaddr);
int read_eeprom(int ioaddr, int location);
int check_eeprom(short ioaddr);
int reset_board(short ioaddr);


int
main(int argc, char **argv)
{
	int port_base = 0x300, irq = -1;
	int errflag = 0, shared_mode = 0;
	int write_eeprom = 0, interface = -1, all_regs = 0;
	int c, longind;
	extern char *optarg;

	while ((c = getopt_long(argc, argv, "af:i:p:svw", longopts, &longind))
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

	dev->name = "eexpress";
	
	if (ioperm(0x80, 2, 1) < 0
		|| ioperm(port_base, 18, 1) < 0) {
		perror("ethertest: ioperm()");
		return 1;
	}

	reset_board(port_base);

	expprobe(port_base);

	if (verbose) {
		show_eeprom(port_base);
	}
	check_eeprom(port_base);

	return 0;
}

int expmagic(short ioaddr)
{
	short id_addr = ioaddr + ID_PORT;
	unsigned short sum = 0;
	int i;
	for (i = 4; i > 0; i--) {
		short id_val = inb(id_addr);
		sum |= (id_val >> 4) << ((id_val & 3) << 2);
	}
	return sum;
}

int expprobe(short ioaddr)
{
	unsigned short station_addr[3];
	unsigned short sum = expmagic(ioaddr);
	short i;

	if (sum != 0xbaba) {
		printf("Probe failed ID checksum, expected 0xbaba, got %#04x.\n", sum);
		return 1;
	}

	printf("EtherExpress found at %#x, station address", ioaddr);

	/* The station address is stored backwards in the EEPROM, reverse after reading. */
	station_addr[0] = read_eeprom(ioaddr, 2);
	station_addr[1] = read_eeprom(ioaddr, 3);
	station_addr[2] = read_eeprom(ioaddr, 4);
	for (i = 0; i < 6; i++) {
		dev->dev_addr[i] = ((unsigned char*)station_addr)[5-i];
		printf(" %02x", dev->dev_addr[i]);
	}

	{
		char irqmap[] = {0, 9, 3, 4, 5, 10, 11, 0};
		short irqval = read_eeprom(ioaddr, 0) >> 13;
		printf(", IRQ %d\n", irqmap[irqval & 7]);
	}
	return 0;
}

void
show_eeprom(short ioaddr)
{
	int j;

	for (j = 0; j < 16; j++) {
		printk(" EEPROM location %2x is %4.4x\n", j, read_eeprom(ioaddr, j));
	}
}

int
read_eeprom(int ioaddr, int location)
{
	int i;
	unsigned short retval = 0;
	short ee_addr = ioaddr + EEPROM_Ctrl;
	int read_cmd = location | EE_READ_CMD;
	short ctrl_val = inb(ee_addr) & ~ASIC_RESET & ~EE_CTRL_BITS;

	if (verbose > 2) printk(" EEPROM reg @ %x (%2x) ", ee_addr, ctrl_val);

	ctrl_val |= EE_CS;
	outb(ctrl_val, ee_addr);

	/* Shift the read command bits out. */
	for (i = 8; i >= 0; i--) {
		short outval = (read_cmd & (1 << i)) ? ctrl_val | EE_DATA_WRITE : ctrl_val;
		if (verbose > 3) printf("%x ", outval);
		outb(outval, ee_addr);
		outb(outval | EE_SHIFT_CLK, ee_addr);	/* Give the EEPROM a clock tick. */
		eeprom_delay();
		outb(outval, ee_addr);	/* Give the EEPROM a clock tick. */
		eeprom_delay();
	}
	if (verbose > 3) printf(" ");
	outb(ctrl_val, ee_addr);

	for (i = 16; i > 0; i--) {
		outb(ctrl_val | EE_SHIFT_CLK, ee_addr);	 eeprom_delay();
		retval = (retval << 1) | ((inb(ee_addr) & EE_DATA_READ) ? 1 : 0);
		outb(ctrl_val, ee_addr);  eeprom_delay();
	}

	/* Terminate the EEPROM access. */
	ctrl_val &= ~EE_CS;
	outb(ctrl_val | EE_SHIFT_CLK, ee_addr);
	eeprom_delay();
	outb(ctrl_val, ee_addr);
	eeprom_delay();

	return retval;
}

int
check_eeprom(short ioaddr)
{
	int i;
	unsigned short sum = 0;

	for (i = 0; i < 64; i++)
		sum += read_eeprom(ioaddr, i);
	printk("EEPROM checksum is %#04x, which is %s.\n", sum,
		   sum == 0xbaba ? "correct" : "bad (correct value is 0xbaba)" );
	return sum != 0xbaba;
}

int
reset_board(short ioaddr)
{
	int boguscnt = 1000;

	outb(ASIC_RESET, ioaddr + EEPROM_Ctrl);
	outb(0x00, ioaddr + EEPROM_Ctrl);
	while (--boguscnt > 0)
		if (expmagic(ioaddr) == 0xbaba) {
			if (verbose > 1)
				printk("Exited reset after %d checks.\n", 1000 - boguscnt);
			return 0;
		}

	printk("Failed to find the board after a reset.\n");
	return 1;
}


/*
 * Local variables:
 *  compile-command: "cc -N -O -Wall -o eexpress eexpress.c"
 *  c-indent-level: 4
 *  tab-width: 4
 * End:
 */
