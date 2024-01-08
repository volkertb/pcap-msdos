/* atlantic.c: Setup program for AT/LANTIC DP83905 ethercards. */
/*
  Copyright 1994-1998 by Donald Becker.
  This version released under the Gnu Public Lincese, incorporated herein
  by reference.  Contact the author for use under other terms.

	This is a setup and diagnostic program for the National Semiconductor
	AT/LANTIC DP83905 used in ethernet boards such as the NE2000plus.

	The author may be reached as becker@cesdis.gsfc.nasa.gov.
	C/O USRA-CESDIS, Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771

	This program must be compiled with '-O'.
	If you have unresolved references to e.g. inw(), then you haven't used -O.
	The suggested compile command is at the bottom of this file.
*/
#if 0
static char vcid[] = "$Id: atlantic.c,v 1.4 1997/01/27 22:11:31 becker Exp becker $";
#endif

static char *version =
	"atlantic.c:v1.00 1/30/98 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";
static char *usage_msg =
"Usage: atlantic [-afhNsvVwW] [-p <IOport>] [-F 10baseT|10base2|AUI>] [-Q <IRQ>]\n";

#include <unistd.h>				/* Hey, we're all POSIX here, right? */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <asm/io.h>
#include <strings.h>

struct option longopts[] = {
 /* { name	has_arg	 *flag	val } */
	{"base-address", 1, 0, 'p'}, /* Base I/O port address. */
	{"help",	   0, 0, 'h'},	/* Give help */
	{"interface",  0, 0, 'F'},	/* Set the transceiver type (built-in, AUI) */
	{"NE2000",	   1, 0, 'N'},	/* Set to NE2000 mode */
	{"new_ioport", 1, 0, 'P'},	/* Set a I/O location for the card. */
	{"irq",		   1, 0, 'Q'},	/* Set a new IRQ (interrupt) line. */
	{"wd8013",	   1, 0, 'W'},	/* Set to WD8013 mode */
	{"verbose",	   0, 0, 'v'},	/* Verbose mode */
	{"version",	   0, 0, 'V'},	/* Display version number */
	{"write-EEPROM", 1, 0, 'w'},/* Write th EEPROMS with the specified vals */
	{ 0, 0, 0, 0 }
};

/* Two functions that the new GLIBC omitted from header files.  grrr... */
extern int ioperm(unsigned long __base, unsigned long extent, int on_or_off);
extern int iopl(int level);

/* Three configuration registers are read and may be set from the EEPROM.
   They are named registers A, B and C

   RegA		NE/WD   FREAD  INT2   INT1   INT0  IOAD2  IOAD1  IOAD0
   RegB		EELOAD  BTPR   BUSE   CHRDY  IO16  GDLINK XCVR1  XCVR0
   RegC		SoftEn  ClkSel IntMde COMP   BtPR3 BtPR2  BtPR1  BtPROM0
*/
/* I/O base settings, IOAD2-0, in register A. */
static int io_base[8] = {
	0x300, 0x278, 0x240, 0x280, 0x2C0, 0x320, 0x340, 0x360};
/* The IRQ line settings, INT2-0, in register A, and a reverse map. */
static int index2irq[8] = {3, 4, 5, 9, 10, 11, 12, 15};
static int irq2index[16] = {-1, -1, 3, 0, 1, 2, -1, -1,
							-1, 3, 4, 5, 6, -1, -1, 7};
/* The transceiver settings, XCVR1-0, in register B.
   10baseT-LRT is Low Receive Threshold -- "turning the volume up" for long
   wiring runs.
*/
static char *xcvr_name[4] = {"10baseT", "Thinnet", "AUI", "10baseT (LRT)"};

int opt_f = 0;

static void show_config(int regA, int regB);
static void write_EEPROM(int dp8390_base, int regA, int regB, int regC);


/*
 I should say something here... uuhhhhmmm....
 */

int
main(int argc, char *argv[])
{
	extern char *optarg;
	int port_base = 0x300;		/* Base address of the board. */
	int	ioaddr = 0x300;			/* Base address of the 8390 chip. */
	int new_ioport = -1, new_irq = -1, new_mode = 0;
	int errflag = 0, verbose = 0, wd_mode = 0, write_eeprom = 0;
	int opt_version = 0, opt_a = 0;
	int new_io_idx = -1, xcvr = -1;	/* I/O port, Transceiver type to set. */
	int c;
	int regA, regB, old_regA, old_regB;

	while ((c = getopt(argc, argv, "af:F:i:Q:m:Np:P:svVwW")) != -1)
		switch (c) {
		case 'a':  opt_a++; break;
		case 'f':  opt_f++; break;
		case 'F':
			if (strncmp(optarg, "10base", 6) == 0) {
				switch (optarg[6]) {
				case 'T':  xcvr = 0; break;
				case '2':  xcvr = 1; break;
				case '5':  xcvr = 2; break;
				default: errflag++;
				}
			} else if (strcmp(optarg, "AUI") == 0)
				xcvr = 2;
			else if (optarg[0] >= '0' &&  optarg[0] <= '3'
					 &&  optarg[1] == 0)
				xcvr = optarg[0] - '0';
			else {
				fprintf(stderr, "Invalid interface specified: it must be"
						" 0..3, '10base{T,2,5}' or 'AUI'.\n");
				errflag++;
			}
			break;
		case 'N':  wd_mode = 0; new_mode = 2; break;
		case 'i':
		case 'Q':
			new_irq = atoi(optarg);
			if (new_irq < 3 || new_irq > 15 || new_irq == 6 || new_irq == 8) {
				fprintf(stderr, "Invalid new IRQ %#x.  Valid values: "
						"3-5,7,9-15.\n", new_irq);
				errflag++;
			}
			break;
		case 'p':  port_base = strtol(optarg, NULL, 16);  break;
		case 'P':  new_ioport = strtol(optarg, NULL, 16);  break;
		case 's':  wd_mode++; new_mode = 1; break;
		case 'v':  verbose++;	 break;
		case 'V':  opt_version++;		 break;
		case 'w':  write_eeprom++;		 break;
		case 'W':  wd_mode++; new_mode = 1; break;
		case '?':
			errflag++;
		}
	if (errflag) {
		fprintf(stderr, usage_msg);
		return 3;
	}

	if (verbose || version)
		printf(version);

	/* Turn on access to the I/O ports. */
	if (ioperm(port_base, 32, 1) < 0) {
		perror("atlantic: io-perm");
		fprintf(stderr, "	(You must run this program with 'root' permissions.)\n");
		return 2;
	}

	/* Check the specified new I/O port value. */
	if (new_irq >= 0  &&  (new_irq >= 16  ||  irq2index[new_irq] < 0)) {
		fprintf(stderr, "The new IRQ line, %d, is invalid.\n",
				new_irq);
		return 3;
	}
	if (new_ioport >= 0) {
		int i;
		for (i = 0; i < 8; i++)
			if (new_ioport == io_base[i])
				 break;
		if (i >= 8) {
			fprintf(stderr, "The new base I/O address, %#x, is invalid.\n",
					new_ioport);
			return 3;
		}
		new_io_idx = i;
	}

	if ( ! opt_f  &&  inb(port_base) == 0xff) {
		printf("No AT/LANTIC chip found at I/O %#x.\n"
			   " Use '-f' to override if you are certain the chip is at this"
			   " I/O location.\n", port_base);
		return 1;
	}
		
	/* First find if this card is set to WD or NE mode by locating the 8390
	   registers. */
	{
		int saved_cmd = inb(port_base);
		int cntr;
		outb(0x20, port_base);
		inb(port_base + 13);	/* Clear a counter register */
		cntr = inb(port_base + 13);
		if (verbose)
			printf("  The NE2K 8390 cmd register was %2.2x, cntr %d.\n",
				   saved_cmd, cntr);
		if (cntr)
			ioaddr = port_base + 16;
		else
			ioaddr = port_base;
		outb(saved_cmd, port_base);
	}

	if (opt_a) {
		int saved_0 = inb(port_base);
		int window, i;

		printf("8390 registers at %#x (command register was %2.2x)\n",
			   ioaddr, saved_0);
		for (window = 0; window < 4; window++) {
			printf(" Window %d:", window);
			outb((window<<6) | 0x20, ioaddr);
			for(i = 0; i < 16; i++) {
				if (window == 0 && i == 6)
					printf(" **");
				else
					printf(" %2.2x", inb(ioaddr + i));
			}
			printf(".\n");
		}
		if (ioaddr != port_base) { 				/* WD emulation. */
			printf("WD8013 compatible registers: ");
			for(i = 0; i < 16; i++) {
				printf(" %2.2x", inb(port_base + i));
				fflush(stdout);
			}
			printf(".\n");
		}
	}

	printf("Reading the configuration from the AT/LANTIC at %#3x...\n",
		   port_base);
	outb(0x21, ioaddr);			/* Select Window 0 */
	old_regA = regA = inb(ioaddr + 0x0A);
	old_regB = regB = inb(ioaddr + 0x0B);

	printf("The current configuration is\n");
	show_config(regA, regB);

	/* This should never be triggered.  We see the card after all!*/
	if ((regA & 7) == 1)
		printf(" Your card is set to be configured at boot-time.\n"
			   "  Remaining with this software configuration is a bad idea"
			   " when using\n  modern systems.  The card will likely be"
			   " unintentionally activated at\n  unexpected I/O and IRQ"
			   " settings.\n" );

	if (new_mode)
		regA = (regA & ~0x80) | (new_mode == 1  ? 0x80 : 0);
	if (new_irq >= 0)
		regA = (regA & ~0x38) | (irq2index[new_irq]<<3);
	if (new_io_idx >= 0)
		regA = (regA & ~0x07) | new_io_idx;
	if (xcvr >= 0)
		regB = (regB & 0x5C) | (xcvr & 0x3);

	if (regA != old_regA || regB != old_regB || write_eeprom) {
		printf("The proposed new configuration is\n");
		show_config(regA, regB);
		if ( ! write_eeprom)
			printf(" Use '-w' to set this as the current configuration "
				   "(valid until next reset).\n"
				   " Use '-w -w' to write the settings to the EEPROM.\n"
				   " Use '-w -w -w' to do both.\n");
	}

	/* We must write the EEPROM and register B before possibly moving the
	   I/O base. */
	if (write_eeprom > 1) {
		printf("Writing the new configuration to the EEPROM...\n");
		write_EEPROM(ioaddr, regA, regB, 0x30);
		printf("Wrote the EEPROM at %#3x with 0x%2.2x 0x%2.2x 0x%2.2x.\n",
			   port_base, regA, regB, 0x30);
	}

	if (write_eeprom & 1) {
		if (regB != old_regB) {
			printf(" Setting register B to %#02x.\n", regB);
			inb(ioaddr + 0x0B);
			outb(regB, ioaddr + 0x0B);
			regB = inb(ioaddr + 0x0B);
			printf(" Register B is now %#02x:  Interface %s.\n", regB,
				   xcvr_name[regB & 0x03]);
		}
		if (regA != old_regA) {
			printf(" Setting register A to %#02x (%s mode I/O %#x IRQ %d).\n",
				   regA, regA & 0x80 ? "WD8013" : "NE2000",
				   io_base[regA & 0x07], index2irq[(regA >> 3) & 0x07] );
			inb(ioaddr + 0x0A);
			outb(regA, ioaddr + 0x0A);
			if (port_base == io_base[regA&7]) {
				ioaddr = port_base + (regA & 0x80 ? 16 : 0);
				printf(" Register A at base %x is now %#02x.\n",
					   ioaddr, inb(ioaddr + 0x0A));
			}
		}
	}

	return 0;
}

static void show_config(int regA, int regB)
{
	printf("  Register A 0x%2.2x:  I/O base @ %#x, IRQ %d,"
		   " %s mode, %s ISA read.\n",
		   regA, io_base[regA & 0x07], index2irq[(regA >> 3) & 0x07],
		   regA & 0x80 ? "WD8013" : "NE2000",
		   regA & 0x40 ? "fast" : "normal" );
	printf("  Register B 0x%2.2x:  Interface %s.\n", regB,
		   xcvr_name[regB & 0x03]);
	printf("    Boot PROM writes are %sabled, CHRDY after %s%s.\n",
		   regB & 0x40 ? "en" : "dis",
		   regB & 0x10 ? "BALE" : "IORD/IOWR",
		   regB & 0x08 ? ", IO16 after IORD/IOWR" : "");
	if (regB & 0x20)
		printf("    Bus error detected.\n");
}

/* Write the A, B, and C configuration registers into the EEPROM at
   DP8390_BASE.
   Ideally this would be done with interrupts disabled, but we do not have
   that capability with user level code.
*/
static void write_EEPROM(int dp8390_base, int regA, int regB, int regC)
{
	int ioregb = dp8390_base + 0x0B;
	int currB = inb(ioregb) & (~0x04) ;

	outb(currB | 0x80, ioregb);
	inb(ioregb);
	outb(regA, ioregb);
	outb(regB, ioregb);
	outb(regC, ioregb);
	return;
}

/*
 * Local variables:
 *  compile-command: "gcc -Wall -O6 -o atlantic atlantic.c"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
