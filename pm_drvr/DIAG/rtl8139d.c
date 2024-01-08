/* rtl8139-diag.c: Diagnostics/EEPROM setup for RealTek RTL8129/8139 chips.

   This is a diagnostic and EEPROM setup program for Ethernet adapters
   based on the RealTek RTL8129 and RTL8139 chips.

   Copyright 1997-1999 by Donald Becker.
   This version released under the Gnu Public License, incorporated herein
   by reference.  Contact the author for use under other terms.

   This program must be compiled with "-O"!  See the bottom of this file
   for the suggested compile-command.

   The author may be reached as becker@cesdis.gsfc.nasa.gov.
   C/O USRA-CESDIS, Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771

   References
   http://www.realtek.com.tw/cn/cn.html
   http://cesdis.gsfc.nasa.gov/linux/misc/NWay.html
*/

static char *version_msg =
"rtl8139-diag.c:v1.01 4/30/99 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";
static char *usage_msg =
"Usage: rtl8139-diag [-aEefFmqrRtsvVwW] [-p <IOport>] [-[AF] <media>]\n";

#ifndef __OPTIMIZE__
#warning  You must compile this program with the correct options!
#warning  See the last lines of the source file.
#error You must compile this driver with "-O".
#endif
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <strings.h>
#include <errno.h>

#if defined(__linux__)  &&  __GNU_LIBRARY__ == 1
#include <asm/io.h>			/* Newer libraries use <sys/io.h> instead. */
#else
#include <sys/io.h>
/* Use   extern iopl(int level);  if your glibc does not define it. */
#endif

/* No libmii.h (yet) */
extern show_mii_details(long ioaddr, int phy_id);
extern monitor_mii(long ioaddr, int phy_id);
/* No libflash.h  */
extern int flash_show(long addr_ioaddr, long data_ioaddr);
extern int flash_dump(long addr_ioaddr, long data_ioaddr, char *filename);
extern int flash_program(long addr_ioaddr, long data_ioaddr, char *filename);

/* We should use __u8 .. __u32, but they are not always defined. */
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

struct option longopts[] = {
 /* { name  has_arg  *flag  val } */
	{"Advertise", 1, 0, 'A'},
	{"base-address", 1, 0, 'p'},
	{"show_all_registers",	0, 0, 'a'},	/* Print all registers. */
	{"help",	0, 0, 'h'},	/* Give help */
	{"show-eeprom",  0, 0, 'e'}, /* Dump EEPROM contents (-ee valid). */
	{"emergency-rewrite",  0, 0, 'E'}, /* Re-write a corrupted EEPROM.  */
	{"force-detection",  0, 0, 'f'},
	{"new-interface",  1, 0, 'F'},	/* New interface (built-in, AUI, etc.) */
	{"new-hwaddr",  1, 0, 'H'},	/* Set a new hardware address. */
	{"show-mii",  0, 0, 'm'},	/* Dump MII management registers. */
	{"port-base",  1, 0, 'p'},	/* Use the specified I/O address. */
	{"quiet",	0, 0, 'q'},		/* Decrease verbosity */
	{"Reset",		0, 0, 'R'},	/* Reset the transceiver. */
	{"chip-type",  1, 0, 't'},	/* Assume the specified chip type index. */
	{"test",	0, 0, 'T'},		/* Do register and SRAM test. */
	{"verbose",	0, 0, 'v'},		/* Verbose mode */
	{"version", 0, 0, 'V'},		/* Display version number */
	{"write-EEPROM", 1, 0, 'w'},/* Actually write the EEPROM with new vals */
	{ 0, 0, 0, 0 }
};

/* The diagnostic functions supported. */
extern int   rtl81x9_diag(int vend_id, int dev_id, int ioaddr, int part_idx);

/* The table of known chips.
   Because of the bogus /proc/pci interface we must have both the exact
   name and a PCI vendor/device IDs.
   This table is searched in order: place specific entries follwed by
   'catch-all' general entries. */
struct pcidev_entry {
	char *proc_pci_name;
	char *part_name;
	int vendor, device, device_mask;
	int flags;
	int io_size;
	int (*diag_func)(int vendor_id, int device_id, int ioaddr, int part_num);
} pcidev_tbl[] = {
  { "Realtek 8129", "RealTek RTL8129",
	0x10ec, 0x8129, 0xffff, 0, 128, rtl81x9_diag },
  { "Realtek 8139", "RealTek RTL8139",
	0x10ec, 0x8139, 0xffff, 0, 128, rtl81x9_diag },
  { "SMC 1211", "SMC1211TX EZCard 10/100 (RealTek RTL8139)",
	0x1113, 0x1211, 0xffff, 0, 128, rtl81x9_diag},
  { "Accton MPX5030", "Accton MPX5030 (RealTek RTL8139)",
	0x1113, 0x1211, 0xffff, 0, 128, rtl81x9_diag},
  { "Realtek", "RealTek (unknown chip type)",
	0x10ec, 0x8100, 0xff00, 0, 128, rtl81x9_diag },
  { 0, 0, 0, 0},
};

enum product_index { RTL8129, RTL8139, SMC1211, MPX5030, RTLUnknown };

int verbose = 1, opt_f = 0, debug = 0;
int show_regs = 0, show_eeprom = 0, show_mii = 0;
unsigned int opt_a = 0,					/* Show-all-interfaces flag. */
	opt_restart = 0,
	opt_reset = 0,
	opt_watch = 0;
unsigned int opt_GPIO = 0;		/* General purpose I/O setting. */
int do_write_eeprom = 0, do_test = 0;
int nway_advertise = 0, fixed_speed = -1;
int new_default_media = -1;

static int scan_proc_pci(int card_num);
static int parse_media_type(const char *capabilities);
static int get_media_index(const char *name);
int mdio_read(int ioaddr, int phy_id, int location);
void mdio_write(int ioaddr, int phy_id, int location, int value);


int
main(int argc, char **argv)
{
	int port_base = 0, chip_type = 0;
	int errflag = 0, show_version = 0;
	int emergency_rewrite = 0;
	int c, longind;
	int card_num = 0;
	extern char *optarg;

	while ((c = getopt_long(argc, argv, "#:aA:DeEfF:G:mp:qrRst:vVwW",
							longopts, &longind))
		   != -1)
		switch (c) {
		case '#': card_num = atoi(optarg);	break;
		case 'a': show_regs++; opt_a++;		break;
		case 'A': nway_advertise = parse_media_type(optarg); break;
		case 'D': debug++;			break;
		case 'e': show_eeprom++;	break;
		case 'E': emergency_rewrite++;	 break;
		case 'f': opt_f++;			break;
		case 'F': new_default_media = get_media_index(optarg);
			if (new_default_media < 0)
				errflag++;
			break;
		case 'G': opt_GPIO = strtol(optarg, NULL, 16); break;
		case 'm': show_mii++;	 break;
		case 'p':
			port_base = strtol(optarg, NULL, 16);
			break;
		case 'q': if (verbose) verbose--;		 break;
		case 'r': opt_restart++;	break;
		case 'R': opt_reset++;		break;
		case 't': chip_type = atoi(optarg);	break;
		case 'v': verbose++;		break;
		case 'V': show_version++;	break;
		case 'w': do_write_eeprom++;	break;
		case 'W': opt_watch++;		break;
#if defined(LIBFLASH)		/* Valid with libflash only. */
		case 'B': opt_flash_show++;	break;
		case 'L': opt_flash_loadfile = optarg;	break;
		case 'S': opt_flash_dumpfile = optarg;	break;
#endif
		case '?':
			errflag++;
		}
	if (errflag) {
		fprintf(stderr, usage_msg);
		return 3;
	}

	if (verbose || show_version)
		printf(version_msg);

	if (chip_type < 0
		|| chip_type >= sizeof(pcidev_tbl)/sizeof(pcidev_tbl[0]) - 1) {
		int i;
		fprintf(stderr, "Valid numeric chip types are:\n");
		for (i = 0; pcidev_tbl[i].part_name; i++) {
			fprintf(stderr, "  %d\t%s\n", i, pcidev_tbl[i].part_name);
		}
		return 3;
	}

	/* Get access to all of I/O space. */
	if (iopl(3) < 0) {
		perror("rtl8139-diag: iopl()");
		fprintf(stderr, "This program must be run as root.\n");
		return 2;
	}

	/* Try to read a likely port_base value from /proc/pci. */
	if (port_base) {
		printf("Assuming a %s adapter at %#x.\n",
			   pcidev_tbl[chip_type].part_name, port_base);
		pcidev_tbl[chip_type].diag_func(0, 0, port_base, chip_type);
	} else if ( scan_proc_pci(card_num) == 0) {
		fprintf(stderr,
				"Unable to find a recognized card in /proc/pci.\nIf there is"
				" a card in the machine, explicitly set the I/O port"
				" address\n  using '-p <ioaddr> -t <chip_type_index>'\n"
				" Use '-t -1' to see the valid chip types.\n");
		return ENODEV;
	}

	if (show_regs == 0  &&  show_eeprom == 0  &&  show_mii == 0)
		printf(" Use '-a' or '-aa' to show device registers,\n"
			   "     '-e' to show EEPROM contents,\n"
			   "  or '-m' or '-mm' to show MII management registers.\n");

	return 0;
}


/* Generic (all PCI diags) code to find cards. */

static char bogus_iobase[] =
"This chip has not been assigned a valid I/O address, and will not function.\n"
" If you have warm-booted from another operating system, a complete \n"
" shut-down and power cycle may restore the card to normal operation.\n";

static char bogus_irq[] =
"This chip has not been assigned a valid IRQ, and will not function.\n"
" This must be fixed in the PCI BIOS setup.  The device driver has no way\n"
" of changing the PCI IRQ settings.\n";

static int scan_proc_bus_pci(int card_num)
{
	int card_cnt = 0, chip_idx = 0;
	int port_base;
	char buffer[514];
	unsigned int pci_bus, pci_devid, irq, pciaddr0, pciaddr1;
	int i;
	FILE *fp = fopen("/proc/bus/pci/devices", "r");

	if (fp == NULL) {
		if (debug) fprintf(stderr, "Failed to open /proc/bus/pci/devices.\n");
		return -1;
	}
	while (fgets(buffer, sizeof(buffer), fp)) {
		if (debug > 1)
			fprintf(stderr, " Parsing line -- %s", buffer);
		if (sscanf(buffer, "%x %x %x %x %x",
				   &pci_bus, &pci_devid, &irq, &pciaddr0, &pciaddr1) <= 0)
			break;
		for (i = 0; pcidev_tbl[i].vendor; i++) {
			if (pci_devid !=
				(pcidev_tbl[i].vendor << 16) + pcidev_tbl[i].device)
				continue;
			chip_idx = i;
			card_cnt++;
			port_base = pciaddr0 & ~1;
			if (card_num == 0 || card_num == card_cnt) {
				printf("Index #%d: Found a %s adapter at %#x.\n",
					   card_cnt, pcidev_tbl[chip_idx].part_name,
					   port_base);
				if (irq == 0  || irq == 255)
					printf(bogus_irq);
				if (port_base)
					pcidev_tbl[chip_idx].diag_func(0,0,port_base, i);
				else
					printf(bogus_iobase);
				break;
			}
		}
	}
	fclose(fp);
	return card_cnt;
}

static int scan_proc_pci(int card_num)
{
	int card_cnt = 0, chip_idx = 0;
	char chip_name[40];
	FILE *fp;
	int port_base;

	if ((card_cnt = scan_proc_bus_pci(card_num)) >= 0)
		return card_cnt;
	card_cnt = 0;

	fp = fopen("/proc/pci", "r");
	if (fp == NULL)
		return 0;
	{
		char buffer[514];
		int pci_bus, pci_device, pci_function, vendor_id, device_id;
		int state = 0;
		if (debug) printf("Done open of /proc/pci.\n");
		while (fgets(buffer, sizeof(buffer), fp)) {
			if (debug > 1)
				fprintf(stderr, " Parse state %d line -- %s", state, buffer);
			if (sscanf(buffer, " Bus %d, device %d, function %d",
					   &pci_bus, &pci_device, &pci_function) > 0) {
				chip_idx = 0;
				state = 1;
				continue;
			}
			if (state == 1) {
				if (sscanf(buffer, " Ethernet controller: %39[^\n]",
						   chip_name) > 0) {
					int i;
					if (debug)
						printf("Named ethernet controller %s.\n", chip_name);
					for (i = 0; pcidev_tbl[i].proc_pci_name; i++)
						if (strncmp(pcidev_tbl[i].proc_pci_name, chip_name,
									strlen(pcidev_tbl[i].proc_pci_name))
							== 0) {
							state = 2;
							chip_idx = i;
							continue;
						}
					continue;
				}
				/* Handle a /proc/pci that does not recognize the card. */
				if (sscanf(buffer, " Vendor id=%x. Device id=%x",
						   &vendor_id, &device_id) > 0) {
					int i;
					if (debug)
						printf("Found vendor 0x%4.4x device ID 0x%4.4x.\n",
							   vendor_id, device_id);
					for (i = 0; pcidev_tbl[i].vendor; i++)
						if (vendor_id == pcidev_tbl[i].vendor  &&
							(device_id & pcidev_tbl[i].device_mask)
							== pcidev_tbl[i].device)
							break;
					if (pcidev_tbl[i].vendor == 0)
						continue;
					chip_idx = i;
					state = 2;
				}
			}
			if (state == 2) {
				if (sscanf(buffer, "  I/O at %x", &port_base) > 0) {
					card_cnt++;
					state = 3;
					if (card_num == 0 || card_num == card_cnt) {
						printf("Index #%d: Found a %s adapter at %#x.\n",
							   card_cnt, pcidev_tbl[chip_idx].part_name,
							   port_base);
						if (port_base)
							pcidev_tbl[chip_idx].diag_func
								(vendor_id, device_id, port_base, chip_idx);
						else
							printf(bogus_iobase);
					}
				}
			}
		}
	}
	fclose(fp);
	return card_cnt;
}

/* Convert a text media name to a NWay capability word. */
static int parse_media_type(const char *capabilities)
{
	const char *mtypes[] = {
		"100baseT4", "100baseTx", "100baseTx-FD", "100baseTx-HD",
		"10baseT", "10baseT-FD", "10baseT-HD", 0,
	};
	int cap_map[] = { 0x0200, 0x0180, 0x0100, 0x0080, 0x0060, 0x0040, 0x0020,};
	int i;
	if (debug)
		fprintf(stderr, "Advertise string is '%s'.\n", capabilities);
	for (i = 0; mtypes[i]; i++)
		if (strcmp(mtypes[i], capabilities) == 0)
			return cap_map[i];
	if ((i = strtol(capabilities, NULL, 16)) <= 0xffff)
		return i;
	fprintf(stderr, "Invalid media advertisement '%s'.\n", capabilities);
	return 0;
}

/* Return the index of a valid media name.
   0x0800	Power up autosense (check speed only once)
   0x8000	Dynamic Autosense
*/
/* A table of media names to indices.  This matches the Digital Tulip
   SROM numbering, primarily because that is the most complete list.
   Other chips will have to map these number to their internal values.
*/
struct { char *name; int value; } mediamap[] = {
	{ "10baseT", 0 },
	{ "10base2", 1 },
	{ "AUI", 2 },
	{ "100baseTx", 3 },
	{ "10baseT-FDX", 0x204 },
	{ "100baseTx-FDX", 0x205 },
	{ "100baseT4", 6 },
	{ "100baseFx", 7 },
	{ "100baseFx-FDX", 8 },
	{ "MII", 11 },
	{ "Autosense", 0x0800 },
	{ 0, 0 },
};

static int get_media_index(const char *name)
{
	int i;
	for (i = 0; mediamap[i].name; i++)
		if (strcmp(name, mediamap[i].name) == 0)
			return i;
	fprintf(stderr, "Invalid interface specified: it must be one of\n  ");
	for (i = 0; mediamap[i].name; i++)
		fprintf(stderr, "  %s", mediamap[i].name);
	fprintf(stderr, ".\n");
	return -1;
}


/* Chip-specific section. */

/* The chip-specific section for the RealTek 8129/8139 diagnostic. */
static int read_eeprom(long ioaddr, int location);
static void parse_eeprom(unsigned short *eeprom);
#ifdef notyet
static void write_eeprom(long ioaddr, int index, int value);
static int do_update(unsigned short *ee_values,
					 int index, char *field_name, int new_value);
#endif
static void mdio_sync(long ioaddr);

const char *intr_names[16] ={
	"Rx Complete", "Rx Error", "Transmit OK", "Transmit Error",
	"Rx Buffer Overflow", "Rx Buffer Underrun", "Rx FIFO Overflow",
	"unknown-0080",
	"unknown-0100", "unknown-0200", "PCS timeout - packet too long",
	"PCI System Error",
};

/* Symbolic offsets to registers. */
enum RTL8129_registers {
	MAC0=0,						/* Ethernet hardware address. */
	MAR0=8,						/* Multicast filter. */
	TxStat0=0x10,				/* Transmit status (Four 32bit registers). */
	TxAddr0=0x20,				/* Tx descriptors (also four 32bit). */
	RxBuf=0x30, RxEarlyCnt=0x34, RxEarlyStatus=0x36,
	ChipCmd=0x37, RxBufPtr=0x38, RxBufAddr=0x3A,
	IntrMask=0x3C, IntrStatus=0x3E,
	TxConfig=0x40, RxConfig=0x44, /* Must enable Tx/Rx before writing. */
	Timer=0x48,					/* A general-purpose counter. */
	RxMissed=0x4C,				/* 24 bits valid, write clears. */
	Cfg9346=0x50, Config0=0x51, Config1=0x52,
	FlashReg=0x54, GPPinData=0x58, GPPinDir=0x59, MII_SMI=0x5A, HltClk=0x5B,
	MultiIntr=0x5C, TxSummary=0x60,
	MII_BMCR=0x62, MII_BMSR=0x64, NWayAdvert=0x66, NWayLPAR=0x68,
	NWayExpansion=0x6A,
};

/* Values read from the EEPROM, and the new image. */
#define EEPROM_SIZE 64
unsigned short eeprom_contents[EEPROM_SIZE];
unsigned short new_ee_contents[EEPROM_SIZE];


int rtl81x9_diag(int vendor_id, int device_id, int ioaddr, int part_idx)
{
	int chip_active = 0;
	int i;

	/* It's mostly safe to examine the registers and EEPROM during
	   operation.  But warn the user, and make then pass '-f'. */
	if ((inb(ioaddr + ChipCmd) & 0x000C) != 0x0000)
		chip_active = 1;

	if (show_regs) {
		unsigned intr_status;

		if (chip_active && !opt_f) {
			printf("The RealTek chip appears to be active, so some registers"
				   " will not be read.\n"
				   "To see all register values use the '-f' flag.\n");
		} else
			chip_active = 0;		/* Ignore the chip status with -f */

		printf("RealTek chip registers at %#x", ioaddr);
		for (i = 0; i < 0x80; i += 4) {
			if ((i & 0x1f) == 0)
				printf("\n 0x%3.3X:", i);
			printf(" %8.8x", inl(ioaddr + i));
		}
		printf(".\n");

		intr_status = inw(ioaddr + IntrStatus);
		printf("  %snterrupt sources are pending.\n",
			   (intr_status & 0x03ff) ? "I": "No i");
		if (intr_status) {
			for (i = 0; i < 16; i++)
				if (intr_status & (1<<i))
					printf("   %s indication.\n", intr_names[i]);
		}
		{
			unsigned char cfg0 = inb(ioaddr + Config0);
			unsigned char cfg1 = inb(ioaddr + Config1);
			const char *xcvr_mode[] = {
				"MII", "an invalid transceiver", "MII/symbol",
				"4B/5B scambler"};
			printf(" The chip configuration is 0x%2.2x 0x%2.2x, %s %s-duplex"
				   " mode.\n",
				   cfg0, cfg1,
				   cfg0 & 0x20 ? "10baseT" : xcvr_mode[cfg0>>6],
				   cfg1 & 0x40 ? "full" : "half");
		}
	}

	/* Read the EEPROM. */
	for (i = 0; i < EEPROM_SIZE; i++)
		eeprom_contents[i] = read_eeprom(ioaddr, i);

	/* The user will usually want to see the interpreted EEPROM contents. */
	if (show_eeprom)
		parse_eeprom(eeprom_contents);
	if (show_eeprom > 1) {
		unsigned short sum = 0;
		printf("EEPROM contents:");
		for (i = 0; i < EEPROM_SIZE; i++) {
			printf("%s %4.4x", (i & 7) == 0 ? "\n ":"",
				   eeprom_contents[i]);
			sum += eeprom_contents[i];
		}
		printf("\n The word-wide EEPROM checksum is %#4.4x.\n", sum);
	}

	/* Show up to four (not just the on-board) PHYs. */
	if (show_mii  &&  part_idx == RTL8129) {
		int phys[4], phy, phy_idx = 0;
		mdio_sync(ioaddr);
		for (phy = 0; phy < 32 && phy_idx < 4; phy++) {
			int mii_status = mdio_read(ioaddr, phy, 0);
			if (mii_status != 0xffff) {
				phys[phy_idx++] = phy;
				printf(" MII PHY found at address %d (%4.4x).\n",
					   phy, mii_status);
			}
		}
		if (phy_idx == 0)
			printf(" ***WARNING***: No MII transceivers found!\n");
#ifdef LIBMII
		else if (show_mii > 1) {
			show_mii_details(ioaddr, phys[0]);
			if (show_mii > 2) monitor_mii(ioaddr, phys[0]);
		}
#else
		else for (phy = 0; phy < phy_idx; phy++) {
			int mii_reg;
			printf(" MII PHY #%d transceiver registers:", phys[phy]);
			for (mii_reg = 0; mii_reg < 32; mii_reg++)
				printf("%s %4.4x", (mii_reg % 8) == 0 ? "\n  " : "",
					   mdio_read(ioaddr, phys[phy], mii_reg));
			printf(".\n");
		}
#endif
	}
	if (show_mii  &&  part_idx != RTL8129) {
		printf(" The RTL8139 does not use a MII transceiver.\n"
			   " It does have internal MII-compatible registers:\n"
			   "   Basic mode control register   0x%4.4x.\n"
			   "   Basic mode status register    0x%4.4x.\n"
			   "   Autonegotiation Advertisement 0x%4.4x.\n"
			   "   Link Partner Ability register 0x%4.4x.\n"
			   "   Autonegotiation expansion     0x%4.4x.\n"
			   "   Disconnects                   0x%4.4x.\n"
			   "   False carrier sense counter   0x%4.4x.\n"
			   "   NWay test register            0x%4.4x.\n"
			   "   Receive frame error count     0x%4.4x.\n",
			   inw(ioaddr + MII_BMSR), inw(ioaddr + MII_BMCR),
			   inw(ioaddr + NWayAdvert), inw(ioaddr + NWayLPAR),
			   inw(ioaddr + NWayExpansion), inw(ioaddr + 0x6C),
			   inw(ioaddr + 0x6E), inw(ioaddr + 0x70), inw(ioaddr + 0x72));
#ifdef LIBMII
		if (show_mii > 1) {
			show_mii_details(ioaddr, -1);
			if (show_mii > 2) monitor_mii(ioaddr, 32);
		}
#endif
	}

	if (do_test) {
		printf("FIFO buffer test not yet available.\n");
	}
	return 0;
}


/* Serial EEPROM section. */

/*  EEPROM_Ctrl bits. */
#define EE_SHIFT_CLK	0x04	/* EEPROM shift clock. */
#define EE_CS			0x08	/* EEPROM chip select. */
#define EE_DATA_WRITE	0x02	/* EEPROM chip data in. */
#define EE_WRITE_0		0x00
#define EE_WRITE_1		0x02
#define EE_DATA_READ	0x01	/* EEPROM chip data out. */
#define EE_ENB			(0x80 | EE_CS)

/* Delay between EEPROM clock transitions.
   No extra delay is needed with 33Mhz PCI, but 66Mhz may change this.
 */

#define eeprom_delay()	inl(ee_addr)

/* The EEPROM commands include the alway-set leading bit. */
#define EE_WRITE_CMD	(5 << 6)
#define EE_READ_CMD		(6 << 6)
#define EE_ERASE_CMD	(7 << 6)

static int read_eeprom(long ioaddr, int location)
{
	int i;
	unsigned retval = 0;
	long ee_addr = ioaddr + Cfg9346;
	int read_cmd = location | EE_READ_CMD;

	outb(EE_ENB & ~EE_CS, ee_addr);
	outb(EE_ENB, ee_addr);

	/* Shift the read command bits out. */
	for (i = 10; i >= 0; i--) {
		int dataval = (read_cmd & (1 << i)) ? EE_DATA_WRITE : 0;
		outb(EE_ENB | dataval, ee_addr);
		eeprom_delay();
		outb(EE_ENB | dataval | EE_SHIFT_CLK, ee_addr);
		eeprom_delay();
	}
	outb(EE_ENB, ee_addr);
	eeprom_delay();

	for (i = 16; i > 0; i--) {
		outb(EE_ENB | EE_SHIFT_CLK, ee_addr);
		eeprom_delay();
		retval = (retval << 1) | ((inb(ee_addr) & EE_DATA_READ) ? 1 : 0);
		outb(EE_ENB, ee_addr);
		eeprom_delay();
	}

	/* Terminate the EEPROM access. */
	outb(~EE_CS, ee_addr);
	return retval;
}

#ifdef not_yet
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
#endif /* notyet */


/* MII serial management: mostly bogus for now. */
/* Read and write the MII management registers using software-generated
   serial MDIO protocol.
   The maximum data clock rate is 2.5 Mhz.  The minimum timing is usually
   met by back-to-back PCI I/O cycles, but we insert a delay to avoid
   "overclocking" issues. */
#define MDIO_DIR		0x80
#define MDIO_DATA_OUT	0x04
#define MDIO_DATA_IN	0x02
#define MDIO_CLK		0x01
#define MDIO_WRITE0 (MDIO_DIR)
#define MDIO_WRITE1 (MDIO_DIR | MDIO_DATA_OUT)

#define mdio_delay()	inl(mdio_addr)

/* A map from MII-like registers to the RTL8139 equivalent. */
static char mii_2_8139_map[8] = {
	MII_BMCR, MII_BMSR, 0, 0, NWayAdvert, NWayLPAR, NWayExpansion, 0 };

/* Syncronize the MII management interface by shifting 32 one bits out. */
static void mdio_sync(long mdio_addr)
{
	int i;

	for (i = 32; i >= 0; i--) {
		outb(MDIO_WRITE1, mdio_addr);
		mdio_delay();
		outb(MDIO_WRITE1 | MDIO_CLK, mdio_addr);
		mdio_delay();
	}
	return;
}
int mdio_read(int ioaddr, int phy_id, int location)
{
	long mdio_addr = ioaddr + MII_SMI;
	int mii_cmd = (0xf6 << 10) | (phy_id << 5) | location;
	int retval = 0;
	int i;

	if (phy_id == 32) {			/* Really a 8139.  Use internal registers. */
		return location < 8 && mii_2_8139_map[location] ?
			inw(ioaddr + mii_2_8139_map[location]) : 0;
	}
	mdio_sync(mdio_addr);
	/* Shift the read command bits out. */
	for (i = 15; i >= 0; i--) {
		int dataval = (mii_cmd & (1 << i)) ? MDIO_DATA_OUT : 0;

		if (verbose > 3)		/* Debug: 5 */
			printf("%d", (mii_cmd & (1 << i)) ? 1 : 0);

		outb(MDIO_DIR | dataval, mdio_addr);
		mdio_delay();
		outb(MDIO_DIR | dataval | MDIO_CLK, mdio_addr);
		if (verbose > 4) printf(" %x ", (inb(mdio_addr) & 0x0f));
		mdio_delay();
	}
	if (verbose > 3) printf("-> %x", inb(mdio_addr) & 0x0f);

	/* Read the two transition, 16 data, and wire-idle bits. */
	for (i = 19; i > 0; i--) {
		outb(0, mdio_addr);
		mdio_delay();
		retval = (retval << 1) | ((inb(mdio_addr) & MDIO_DATA_IN) ? 1 : 0);
		outb(MDIO_CLK, mdio_addr);
		mdio_delay();
		if (verbose > 3) printf("%x", inb(mdio_addr) & 0x0f);
	}
	if (verbose > 2)
		printf("  MII read of %d:%d -> %4.4x.\n", phy_id, location, retval);
	return (retval>>1) & 0xffff;
}

void mdio_write(int ioaddr, int phy_id, int location, int value)
{
	long mdio_addr = ioaddr + MII_SMI;
	int mii_cmd = (0x5002 << 16) | (phy_id << 23) | (location<<18) | value;
	int i;

	if (phy_id == 32) {			/* Really a 8139.  Use internal registers. */
		if (location < 8 && mii_2_8139_map[location])
 			outw(value, ioaddr + mii_2_8139_map[location]);
		return;
	}
	mdio_sync(mdio_addr);

	/* Shift the command bits out. */
	for (i = 31; i >= 0; i--) {
		int dataval = (mii_cmd & (1 << i)) ? MDIO_WRITE1 : MDIO_WRITE0;
		outb(dataval, mdio_addr);
		mdio_delay();
		outb(dataval | MDIO_CLK, mdio_addr);
		if (verbose > 4) printf(" %x ", (inb(mdio_addr) & 0x0f));
		mdio_delay();
	}
	/* Clear out extra bits. */
	for (i = 2; i > 0; i--) {
		outb(0, mdio_addr);
		mdio_delay();
		outb(MDIO_CLK, mdio_addr);
		mdio_delay();
	}
	return;
}

static void parse_eeprom(unsigned short *eeprom)
{
	unsigned char *p = (void *)eeprom;
	int i, sum = 0;

	printf("Parsing the EEPROM of a RealTek chip:\n"
		   "  PCI IDs -- Vendor %#4.4x, Device %#4.4x, Subsystem %#4.4x.\n"
		   "  PCI timer settings -- minimum grant %d, maximum latency %d.\n"
		   "  General purpose pins --  direction 0x%2.2x  value 0x%2.2x.\n"
		   "  Station Address ",
		   eeprom[1], eeprom[2], eeprom[3], p[10], p[11], p[13], p[12] );
	for (i = 14; i < 19; i++)
		printf("%2.2X:", p[i]);
	printf("%2.2X.\n"
		   "  Configuration register 0/1 -- 0x%2.2x / 0x%2.2x.\n",
		   p[i], p[21], p[22]);
	for (i = 0; i < 24; i++)
		sum += p[i];
	printf(" EEPROM active region checksum is %4.4x.\n", sum);
	return;
}


/*
 * Local variables:
 *  compile-command: "cc -O -Wall -o rtl8139-diag rtl8139-diag.c `[ -f libmii.c ] && echo -DLIBMII libmii.c`"
 *  simple-compile-command: "cc -O -Wall -o rtl8139-diag rtl8139-diag.c"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
