/* 3c5x9setup.c: Setup program for 3Com EtherLink III ethercards.

   Copyright 1994-1999 by Donald Becker.
   This version released under the Gnu Public License, incorporated herein
   by reference.  Contact the author for use under other terms.

   This is a EEPROM setup and diagnostic program for the 3Com 3c5x9 series
   ethercards.  Most products with "EtherLink III" in the name are supported,
   including
	3c509, 3c529, 3c579  (ISA, MCA, EISA, but not PCI)
	3c556 3c562, 3c563, 3c574 and other PCMCIA (but not CardBus) cards
   The 'B' and 'C' suffix versions are supported, as are the various
   transceiver options (e.g. "-TPO")

   Instructions are at
     http://cesdis.gsfc.nasa.gov/linux/diag/index.html

   This program must be compiled with "-O"!  See the bottom of this file
   for the suggested compile-command.

   The author may be reached as becker@cesdis.gsfc.nasa.gov.
   C/O USRA-CESDIS, Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771

   References
   The 3Com EtherLink III manual, available from 3Com
   http://cesdis.gsfc.nasa.gov/linux/diag/index.html
*/

static char *version_msg =
"3c5x9setup.c:v0.05b 10/6/99 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";
static char *usage_msg =
"Usage: 3c5x9setup [-aEfFsvVw] [-p <IOport>] [-F 10baseT|10base2|AUI>] [-Q <IRQ>]\n";

#ifndef __OPTIMIZE__
#warning  You must compile this program with the correct options!
#warning  See the last lines of the source file.
#error You must compile this driver with "-O".
#endif
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <strings.h>
#include <errno.h>

#if defined(__linux__)  &&  __GNU_LIBRARY__ == 1
#include <asm/io.h>			/* Newer libraries use <sys/io.h> instead. */
#else
#include <sys/io.h>
/* Use   extern iopl(int level);  if your glibc does not define it. */
#endif

/* We should use __u8 .. __u32, but they are not always defined. */
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

struct option longopts[] = {
 /* { name  has_arg  *flag  val } */
	{"base-address", 1, 0, 'p'},
	{"new-base-address", 1, 0, 'P'},
	{"show-all-registers",	0, 0, 'a'},	/* Print all registers. */
	{"debug",			0, 0, 'D'},
	{"show-eeprom",		0, 0, 'e'}, /* Dump EEPROM contents (-ee valid). */
	{"help",			0, 0, 'h'},	/* Give help */
	{"emergency-rewrite",  0, 0, 'E'}, /* Re-write a corrupted EEPROM.  */
	{"force-detection",  0, 0, 'f'},
	{"new-interface",  1, 0, 'F'},	/* New interface (built-in, AUI, etc.) */
	{"new-IOaddress",	1, 0, 'P'},	/* New base I/O address. */
	{"new-irq",	1, 0, 'Q'},		/* New interrupt number */
	{"verbose",	0, 0, 'v'},		/* Verbose mode */
	{"version", 0, 0, 'V'},		/* Display version number */
	{"write-EEPROM", 1, 0, 'w'},/* Actually write the EEPROM with new vals */
	{ 0, 0, 0, 0 }
};

/* Offsets from base I/O address. */
#define EL3_CMD 0x0e
#define EL3_STATUS 0x0e

enum Window0 {
	Wn0EepromCmd = 10,		/* Window 0: EEPROM command register. */
	Wn0EepromData = 12,		/* Window 0: EEPROM results register. */
	IntrStatus=0x0E,			/* Valid in all windows. */
};

#define	 EEPROM_READ 0x80
#define	 EEPROM_WRITE 0x40
#define	 EEPROM_ERASE 0xC0
#define	 EEPROM_EWENB 0x30		/* Enable erasing/writing for 10 msec. */
#define	 EEPROM_EWDIS 0x00		/* Enable erasing/writing for 10 msec. */
enum Win0_EEPROM_bits {
	EEPROM_Read = 0x80, EEPROM_Busy = 0x8000,
};

#define EL3WINDOW(win_num) outw(0x0800+(win_num), ioaddr + EL3_CMD)

/* Register window 1 offsets, the window used in normal operation. */
enum Window1 {
	TX_FIFO = 0x0,  RX_FIFO = 0x0,  Wn1RxErrors = 0x4,
	Wn1RxStatus = 0x8,  Wn1Timer=0xA, Wn1TxStatus = 0xB,
	Wn1TxFree = 0xC, /* Remaining free bytes in Tx buffer. */
};

const char *intr_names[13] ={
	"Interrupt latch", "Adapter Failure", "Tx Complete", "Tx Available",
	"Rx Complete", "Rx Early Notice", "Driver Intr Request",
	"Statistics Full", "DMA Done", "Download Complete", "Upload Complete",
	"DMA in Progress", "Command in Progress",
};

/* EEPROM operation locations. */
enum eeprom_offset {
	PhysAddr01=0, PhysAddr23=1, PhysAddr45=2, ModelID=3,
	EtherLink3ID=7, IFXcvrIO=8, IRQLine=9,
	AltPhysAddr01=10, AltPhysAddr23=11, AltPhysAddr45=12,
	DriverTune=13, Checksum=15};

/* Last-hope recovery major boo-boos: rewrite the EEPROM with the values
   from my card (and hope I don't met you on the net...).
   This image is valid only for an pre-B 3c509.
*/
unsigned short djb_eeprom[16] = {
	0x0020, 0xaf0e, 0x3bc2, 0x9058, 0xbc4e, 0x0036, 0x4441, 0x6d50,
	0x0090, 0xaf00, 0x0020, 0xaf0e, 0x3bc2, 0x1310, 0x0000, 0x343c, }; 
/* Values read from the EEPROM, and the new image. */
#define EEPROM_SPACE 64
unsigned short eeprom_contents[EEPROM_SPACE];
unsigned short new_ee_contents[EEPROM_SPACE];

int verbose = 1, opt_f = 0, debug = 0;
int show_regs = 0, show_eeprom = 0;
int do_write_eeprom = 0;
int ioaddr;

const char *intrs_pending_msg = 
" This network adapter has unhandled interrupts!\n"
" This should never occur with properly configured adapter.  You may have\n"
"   a hardware interrupt conflict.\n"
" Check /proc/interrupts to verify that the interrupt is properly\n"
"   registered, and that the count is increasing.\n"
" This problem is frequently solved by moving the adapter to different IRQ.\n"
" For ISA cards, verify that the BIOS setup has assigned the IRQ line to\n"
"  ISA bus.\n"
" For PCMCIA cards, change the configuration file, typically named\n"
"  /etc/pcmcia/config.opts, to use a different IRQ.\n";

static void print_eeprom(unsigned short *eeprom_contents);
static void write_eeprom(short ioaddr, int index, int value);
static unsigned int calculate_checksum(unsigned short *values);
static int do_update(unsigned short *ee_values,
					 int index, char *field_name, int new_value);

int
main(int argc, char **argv)
{
	int port_base = 0x300;
	int new_interface = -1, new_irq = -1, new_ioaddr = -1;
	int errflag = 0, show_version = 0;
	int emergency_rewrite = 0;
	int show_regs = 0, opt_a = 0;
	int c, longind, i, j, saved_window;
	extern char *optarg;

	while ((c = getopt_long(argc, argv, "aDeEfF:hi:p:P:Q:svVwX:",
							longopts, &longind))
		   != -1)
		switch (c) {
		case 'a': show_regs++; opt_a++;		break;
		case 'D': debug++; break;
		case 'e': show_eeprom++;		break;
		case 'E': emergency_rewrite++;	break;
		case 'f': opt_f++; break;
		case 'F': case 'X':
			if (strncmp(optarg, "10base", 6) == 0) {
				switch (optarg[6]) {
				case 'T':  new_interface = 0; break;
				case '2':  new_interface = 3; break;
				case '5':  new_interface = 1; break;
				default: errflag++;
				}
			} else if (strcmp(optarg, "AUI") == 0)
				new_interface = 1;
			else if (optarg[0] >= '0' &&  optarg[0] <= '3'
					   &&  optarg[1] == 0)
				new_interface = optarg[0] - '0';
			else {
				fprintf(stderr, "Invalid interface specified: it must be"
						" 0..3, '10base{T,2,5}' or 'AUI'.\n");
				errflag++;
			}
			break;
		case 'Q':
			new_irq = atoi(optarg);
			if (new_irq < 3 || new_irq > 15 || new_irq == 6 || new_irq == 8) {
				fprintf(stderr, "Invalid new IRQ %#x.  Valid values: "
						"3-5,7,9-15.\n", new_irq);
				errflag++;
			}
			break;
		case 'p':
			port_base = strtol(optarg, NULL, 16);
			break;
		case 'P':
			new_ioaddr = strtol(optarg, NULL, 16);
			if (new_ioaddr < 0x200 || new_ioaddr > 0x3f0) {
				fprintf(stderr, "Invalid new I/O address %#x.  Valid range "
						"0x200-0x3f0.\n", new_ioaddr);
				errflag++;
			}
			break;
		case 'v': verbose++;		 break;
		case 'V': show_version++;		 break;
		case 'w': do_write_eeprom++;	 break;
		case '?': case 'h':
			errflag++;
		}
	if (errflag) {
		fprintf(stderr, usage_msg);
		return 3;
	}

	if (ioperm(port_base, 16, 1) < 0) {
		perror("3c5x9setup: ioperm()");
		fprintf(stderr, "This program must be run as root.\n");
		return 2;
	}

	if (verbose)
		printf(version_msg);

	ioaddr = port_base;

	saved_window = inw(ioaddr + EL3_STATUS);
	if (saved_window == 0xffff) {
		printf("No EtherLink III device exists at address 0x%X.\n", ioaddr);
		if ( ! opt_f) {
			printf("Use the '-f' option proceed anyway.\n");
			return 2;
		}
	}

	/* We can check for a stuck interrupt even while the chip is active. */
	if (verbose || opt_a) {
		unsigned intr_status = inw(ioaddr + IntrStatus);
		printf(" %snterrupt sources are pending.\n",
			   (intr_status & 0x03ff) ? "I": "No i");
		if (intr_status & 0x3ff) {
			for (i = 0; i < 13; i++)
				if (intr_status & (1<<i))
					printf("   %s indication.\n", intr_names[i]);
		}
	}
	if (!opt_f  && (saved_window & 0xe000) == 0x2000) {
		int last_intr = inb(ioaddr + Wn1Timer);
		printf("A potential 3c5*9 has been found, but it appears to still be "
			   "active.\nOnly limited information is available without "
			   "disturbing network operation.\n"
			   " Either shutdown the network, or use the '-f' flag to see all "
			   "registers.\n");
		printf("  Available Tx room %d bytes, Tx/Rx Status %4.4x / %4.4x.\n",
			   inw(ioaddr + Wn1TxFree), inb(ioaddr + Wn1TxStatus),
			   inb(ioaddr + Wn1RxStatus));
		if (last_intr != 255)
			printf("  An interrupt occured only %d ticks ago!\n", last_intr);
		if (saved_window & 0x00ff)
			printf(intrs_pending_msg);
		return 1;
	}

	EL3WINDOW(0);
	if (inw(port_base) == 0x6d50) {
		printf("3c5x9 found at %#3.3x.\n", port_base);
	} else {
		printf("3c5*9 not found at %#3.3x, status %4.4x.\n"
			   "If there is a 3c5*9 card in the machine, explicitly set the"
			   " I/O port address\n  using '-p <ioaddr>\n",
			   port_base, inw(port_base));
		if (opt_f < 2)
			return 1;
	}

	EL3WINDOW(5);
	printf(" Indication enable is %4.4x, interrupt enable is %4.4x.\n",
		   inw(ioaddr + 12), inw(ioaddr + 10));

	if (show_regs) {
		const char *statname[12] = {
			"Carrier Lost", "Heartbeat", "Tx Multiple collisions",
			"Tx Single collisions", "Tx Late collisions", "Rx FIFO overruns",
			"Tx packets", "Rx packets", "Tx deferrals",
			"(256 Rx/Tx counts)", "Rx bytes", "Tx bytes"};
		int stats[12];
		/* First read the statistics registers, which are clear-on-read. */
		EL3WINDOW(6);
		for (i = 0; i < 10; i++)
			stats[i] = inb(ioaddr + i);
		stats[10] = inw(ioaddr + 10);
		stats[11] = inw(ioaddr + 12);
		for (i = 0; i < 12; i++)
			if (stats[i])
				printf("  Event counts: %s is %d.\n", statname[i], stats[i]);
	}
						
	if (show_regs) {
		for (j = 0; j < 8; j++) {
			int i;
			printf("  Window %d:", j);
			outw(0x0800 + j, ioaddr + 0x0e);
			for (i = 0; i < 16; i+=2)
				printf(" %4.4x", inw(ioaddr + i));
			printf(".\n");
		}
	}

	EL3WINDOW(0);

	/* Read the EEPROM. */
	for (i = 0; i < EEPROM_SPACE; i++) {
		int boguscheck;
		outw(EEPROM_Read + i, ioaddr + Wn0EepromCmd);
		/* Pause for up to 162 us. for the read to take place.
		   Typical max. 175 ticks. */
		for (boguscheck = 0; boguscheck < 1620; boguscheck++)
			if ((inw(ioaddr + Wn0EepromCmd) & EEPROM_Busy) == 0)
				break;
		eeprom_contents[i] = inw(ioaddr + Wn0EepromData);
		if (show_eeprom > 2)
			printf("EEPROM read of index %d %s after %d ticks -> %4.4x.\n",
				   i, boguscheck < 1620 ? "completed" : "failed", boguscheck,
				   eeprom_contents[i]);
	}

	if (emergency_rewrite) {
		if (emergency_rewrite < 3  ||  !do_write_eeprom)
			printf(" Caution!  Last-chance EEPROM write requested.  The\n"
				   " new EEPROM values will not be written without"
				   " '-E -E -E -w' flags.\n");
		else {
			for (i = 0; i < 16; i++) {
				eeprom_contents[i] = djb_eeprom[i];
				write_eeprom(ioaddr, i, eeprom_contents[i]);
			}
		}
	}
	{
		unsigned short new_ifxcvrio = eeprom_contents[IFXcvrIO];
		unsigned short new_irqline = eeprom_contents[IRQLine];
		int something_changed = 0;

		if (new_interface >= 0)
			new_ifxcvrio = (new_interface << 14) | (new_ifxcvrio & 0x3fff);
		if (new_ioaddr > 0)
			new_ifxcvrio = ((new_ioaddr>>4) & 0x1f) | (new_ifxcvrio & 0xffe0);
		if (new_irq > 0)
			new_irqline = (new_irq << 12) | 0x0f00;

		if (do_update(eeprom_contents, IRQLine, "IRQ", new_irqline))
			something_changed++;

		if (do_update(eeprom_contents, IFXcvrIO, "transceiver/IO",
					  new_ifxcvrio))
			something_changed++;

		/* To change another EEPROM value write it here. */

		if (do_update(eeprom_contents, Checksum, "checksum",
					  calculate_checksum(eeprom_contents)))
			something_changed++;

		if (something_changed  &&  !do_write_eeprom)
			printf(" (The new EEPROM values will not be written without"
					" the '-w' flag.)\n");
	}

	if (verbose > 1 || show_eeprom) {
		print_eeprom(eeprom_contents);
	}

	EL3WINDOW(saved_window>>13);

	return 0;
}

static void print_eeprom(unsigned short *eeprom_contents)
{
	char *if_names[] = {"10baseT", "AUI", "undefined", "BNC"};
	u8 *p = (void *)eeprom_contents;
	u16 *ee = eeprom_contents;
	int i;

	printf(" EEPROM contents:");
	if (show_eeprom)
		for(i = 0; i < EEPROM_SPACE; i++)
			printf("%s %4.4x", i % 8 ? "" : "\n   ", eeprom_contents[i]);
	printf("\n  Model number 3c%2.2x%1.1x version %1.1x, base I/O %#x, IRQ %d, "
		   "%s port.\n",
		   eeprom_contents[ModelID] & 0x00ff,
		   eeprom_contents[ModelID] >> 12,
		   (eeprom_contents[ModelID] >> 8) & 0x000f,
		   0x200 + ((eeprom_contents[IFXcvrIO] & 0x1f) << 4),
		   eeprom_contents[IRQLine] >> 12,
		   if_names[eeprom_contents[IFXcvrIO] >> 14]);

	p = (unsigned char *)(eeprom_contents + PhysAddr01);
	printf("  3Com Node Address ");
	for (i = 0; i < 5; i++)
		printf("%2.2X:", p[i^1]);
	printf("%2.2X (used as a unique ID only).\n"
		   "  OEM Station address ",
		   p[i^1]);
	for (i = 20; i < 25; i++)
		printf("%2.2X:", p[i ^ 1]);
	printf("%2.2X (used as the ethernet address).\n", p[24]);

	/* Y2K safe.  This two-digit date is for information purposes only.
	   It is never used for computations. */
	printf("  Manufacture date (MM/DD/YY) %d/%d/%d, division %c,"
		   " product %c%c.\n", (ee[4] >> 5) & 15,
		   ee[4] & 31, ee[4] >> 9, p[10], p[12], p[13]);
	printf("  Options: %s duplex, %sable linkbeat.\n",
		   ee[13] & 0x8000 ? "force full" : "half",
		   ee[13] & 0x4000 ? "dis" : "en");

	if (calculate_checksum(eeprom_contents) != eeprom_contents[Checksum])
		printf("****CHECKSUM ERROR****: Calcuated checksum: %4.4x, "
			   "stored checksum %4.4x.\n",
			   calculate_checksum(eeprom_contents),
			   eeprom_contents[Checksum]);
	else
		printf("  The computed checksum matches the stored checksum of %4.4x.\n",
			   eeprom_contents[Checksum]);
}


static void write_eeprom(short ioaddr, int index, int value)
{
	int timer;
	/* Verify that the EEPROM is idle. */
	for (timer = 1620; inw(ioaddr + Wn0EepromCmd) & 0x8000;)
		if (--timer < 0)
			goto error_return;
	outw(EEPROM_EWENB, ioaddr + Wn0EepromCmd);
	usleep(60);
	outw(EEPROM_ERASE + index, ioaddr + Wn0EepromCmd);
	usleep(60);
	outw(EEPROM_EWENB, ioaddr + Wn0EepromCmd);
	usleep(60);
	outw(value, ioaddr + Wn0EepromData);
	outw(EEPROM_WRITE + index, ioaddr + Wn0EepromCmd);
	for (timer = 16000; inw(ioaddr + Wn0EepromCmd) & 0x8000;)
		if (--timer < 0)
			goto error_return;
	if (debug)
		fprintf(stderr, "EEPROM wrote index %d with 0x%4.4x after %d ticks!\n",
				index, value, 16000-timer);
	return;
error_return:
	fprintf(stderr, "Failed to write EEPROM location %d with 0x%4.4x!\n",
			index, value);
}

/* Calculate the EEPROM checksum.
   The checksum for the fixed values is returned in the high byte.
   The checksum for the programmable variables is in the low the byte.
   */

static unsigned int
calculate_checksum(unsigned short *values)
{
	int fixed_checksum = 0, var_checksum = 0;
	int i;

	for (i = 0; i <= 14; i++) {				/* Note: 14 (loc. 15 is the sum) */
		if (i == IFXcvrIO || i == IRQLine || i == DriverTune)
			var_checksum ^= values[i];
		else
			fixed_checksum ^= values[i];
	}
	return ((fixed_checksum ^ (fixed_checksum << 8)) & 0xff00) |
		((var_checksum ^ (var_checksum >> 8)) & 0xff);
}

static int do_update(unsigned short *ee_values,
					 int index, char *field_name, int new_value)
{
	if (ee_values[index] != new_value) {
		if (do_write_eeprom) {
			printf("Writing new %s entry 0x%4.4x.\n",
				   field_name, new_value);
			write_eeprom(ioaddr, index, new_value);
		} else
			printf(" Would write new %s entry 0x%4.4x (old value 0x%4.4x).\n",
				   field_name, new_value, ee_values[index]);
		ee_values[index] = new_value;
		return 1;
	}
	return 0;
}


/*
 * Local variables:
 *  compile-command: "cc -O -Wall -o 3c5x9setup 3c5x9setup.c"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
