/* diag-example.c: Linux Diagnostic and setup core code for PCI adapters.

   This is a diagnostic and EEPROM setup program for PCI adapters
   based on the following chips:
	VIA Rhine vt86c100 and vt3043 Ethernet controller.
   This file contains the chip-independent and example code.

   Copyright 1998-1999 by Donald Becker.
   This version released under the Gnu Public License, incorporated herein
   by reference.  Contact the author for use under other terms.

   This program must be compiled with "-O"!  See the bottom of this file
   for the suggested compile-command.

   The author may be reached as becker@cesdis.gsfc.nasa.gov.
   C/O USRA-CESDIS, Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771

   http://cesdis.gsfc.nasa.gov/linux/diag/index.html
   http://cesdis.gsfc.nasa.gov/linux/misc/NWay.html

   Common-sense licensing statement: Using any portion of this program in
   your own program means that you must give credit to the original author
   and release the resulting code under the GPL.
*/

static char *version_msg =
"via-diag.c:v1.01 9/16/99 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";
static char *usage_msg =
"Usage: etherdiag [-aEefFmqrRtvVwW] [-p <IOport>] [-A <media>] [-F <media>]\n";

#if ! defined(__OPTIMIZE__)
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

/* The following are required only with unaligned field accesses. */
#include <asm/types.h>
#include <asm/unaligned.h>

#if defined(__linux__)  &&  __GNU_LIBRARY__ == 1
#include <asm/io.h>			/* Newer libraries use <sys/io.h> instead. */
#else
#include <sys/io.h>
#endif

/* No libmii.h or libflash.h yet. */
extern int show_mii_details(long ioaddr, int phy_id);
extern int monitor_mii(long ioaddr, int phy_id);

extern int flash_show(long addr_ioaddr, long data_ioaddr);
extern int flash_dump(long addr_ioaddr, long data_ioaddr, char *filename);
extern int flash_program(long addr_ioaddr, long data_ioaddr, char *filename);
extern int (*flash_in_hook)(long addr, int offset);
extern void (*flash_out_hook)(long addr, int offset, int val);

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
	{"reset",	0, 0, 'R'},		/* Reset the transceiver. */
	{"chip-type",  1, 0, 't'},	/* Assume the specified chip type index. */
	{"test",	0, 0, 'T'},		/* Do register and SRAM test. */
	{"verbose",	0, 0, 'v'},		/* Verbose mode */
	{"version", 0, 0, 'V'},		/* Display version number */
	{"write-EEPROM", 1, 0, 'w'},/* Actually write the EEPROM with new vals */
	{ 0, 0, 0, 0 }
};

extern int  rhine_diag(int vend_id, int dev_id, long ioaddr, int part_idx);

/* Chip-specific flags. Yes, it's grungy to have the enum here. */

/* The table of known chips.
   Because of the bogus /proc/pci interface we must have both the exact
   name and a PCI vendor/device IDs.
   This table is searched in order: place specific entries followed by
   'catch-all' general entries. */
struct pcidev_entry {
	char *proc_pci_name;
	char *part_name;
	int vendor, device, device_mask;
	int flags;
	int io_size;
	int (*diag_func)(int vendor_id, int device_id, long ioaddr, int part_num);
} pcidev_tbl[] = {
	{" Ethernet controller: VIA Technologies Unknown device",
	 "VIA VT86C100A Rhine-II", 0x1106, 0x6100, 0xffff,
	 0, 128, rhine_diag},
	{" Ethernet controller: VIA Technologies Unknown device",
	 "VIA VT3043 Rhine", 0x1106, 0x3043, 0xffff,
	  0, 128, rhine_diag},
	{ 0, 0, 0, 0},
};

int verbose = 1, opt_f = 0, debug = 0;
int show_regs = 0, show_eeprom = 0, show_mii = 0;
unsigned int opt_a = 0,					/* Show-all-interfaces flag. */
	opt_restart = 0,
	opt_reset = 0,
	opt_watch = 0,
	opt_G = 0;
unsigned int opt_GPIO = 0;		/* General purpose I/O setting. */
int do_write_eeprom = 0, do_test = 0;
int nway_advertise = 0, fixed_speed = -1;
int new_default_media = -1;
/* Valid with libflash only. */
static unsigned int opt_flash_show = 0;
static char	*opt_flash_dumpfile = NULL, *opt_flash_loadfile = NULL;

static unsigned char new_hwaddr[6], set_hwaddr = 0;

static int scan_proc_pci(int card_num);
static int parse_media_type(const char *capabilities);
static int get_media_index(const char *name);
/* Chip-specific options, if any, go here. */


int
main(int argc, char **argv)
{
	int port_base = 0, chip_type = 0;
	int errflag = 0, show_version = 0;
	int emergency_rewrite = 0;
	int c, longind;
	int card_num = 0;
	extern char *optarg;

	while ((c = getopt_long(argc, argv, "#:aA:DeEfF:G:mp:qrRst:vVwWH:BL:S:",
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
		case 'G': opt_G++; opt_GPIO = strtol(optarg, NULL, 16); break;
		case 'H':
			{
				int hwaddr[6], i;
				if (sscanf(optarg, "%2x:%2x:%2x:%2x:%2x:%2x",
						   hwaddr, hwaddr + 1, hwaddr + 2,
						   hwaddr + 3, hwaddr + 4, hwaddr + 5) == 6) {
					for (i = 0; i < 6; i++)
						new_hwaddr[i] = hwaddr[i];
					set_hwaddr++;
				} else
					errflag++;
				break;
			}
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
		case 'B': opt_flash_show++;	break;
		case 'L': opt_flash_loadfile = optarg;	break;
		case 'S': opt_flash_dumpfile = optarg;	break;
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
		perror("Network adapter diagnostic: iopl()");
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
			   "     '-e' to show EEPROM contents, -ee for parsed contents,\n"
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
			/* Select the I/O address. */
			port_base = pciaddr0 & 1  ?  pciaddr0 & ~1 : pciaddr1 & ~1;
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
	if (name  &&  atoi(name) >= 00)
		return atoi(name);
	fprintf(stderr, "Invalid interface specified: it must be one of\n  ");
	for (i = 0; mediamap[i].name; i++)
		fprintf(stderr, "  %s", mediamap[i].name);
	fprintf(stderr, ".\n");
	return -1;
}


/* Chip-specific section. */

/* The chip-specific section for the VIA Rhine. */

static int read_eeprom(long ioaddr, int location, int addr_len);
static void write_eeprom(long ioaddr, int index, int value, int addr_len);
int mdio_read(long ioaddr, int phy_id, int location);
void mdio_write(long ioaddr, int phy_id, int location, int value);

/* Offsets to the various registers.
   Note if accesses must be longword aligned. */
enum register_offsets {
	StationAddr=0x00, RxConfig=0x06, TxConfig=0x07, ChipCmd=0x08,
	IntrStatus=0x0C, IntrEnable=0x0E,
	MulticastFilter0=0x10, MulticastFilter1=0x14,
	RxRingPtr=0x18, TxRingPtr=0x1C,
	MIIPhyAddr=0x6C, MIIStatus=0x6D, PCIBusConfig=0x6E,
	MIICmd=0x70, MIIRegAddr=0x71, MIIData=0x72, EECtrl=0x74,
	Config=0x78, RxMissed=0x7C, RxCRCErrs=0x7E,
};

/* Bits in the interrupt status/mask registers. */
enum intr_status_bits {
	IntrRxDone=0x0001, IntrRxErr=0x0004, IntrRxEmpty=0x0020,
	IntrTxDone=0x0002, IntrTxAbort=0x0008, IntrTxUnderrun=0x0010,
	IntrPCIErr=0x0040,
	IntrStatsMax=0x0080, IntrRxEarly=0x0100, IntrMIIChange=0x0200,
	IntrRxOverflow=0x0400, IntrRxDropped=0x0800, IntrRxNoBuf=0x1000,
	IntrTxAborted=0x2000, IntrLinkChange=0x4000,
	IntrRxWakeUp=0x8000,
	IntrNormalSummary=0x0003, IntrAbnormalSummary=0xC260,
};

/* The textual names of the interrupt indications. */
static const char *intr_names[16] ={
	"Rx Done event", "Tx Done event", "Rx error", "Tx abort",
	"Tx underrun", "Out of Rx buffers", "PCI bus fault",
	"Statistics counters full", "Rx partially done", "MII status changed",
	"Rx overflow", "Rx packet dropped",
	"No Rx buffers", "Tx aborted", "Link changed", "Rx Wakeup packet",
};

/* Values read from the EEPROM, and a new image to write. */
#define EEPROM_SIZE 256
unsigned short eeprom_contents[EEPROM_SIZE];
unsigned short new_ee_contents[EEPROM_SIZE];
#define EEPROM_SA_OFFSET	0x00
#define EEPROM_CSUM_OFFSET	0	/* 0 means none. */

#ifdef LIBFLASH
#warning This diagnostic does not support flash programming.
/* Support for Flash operations. */
static int rhine_flash_in(long ioaddr, int offset) {
	return 0;
}
static void rhine_flash_out(long ioaddr, int offset, int val) {
	return;
}
#endif

/* A table for emitting the configuration of a register. */
struct config_name { int val, mask; const char*name;}
static rcvr_mode[] = {
	{0x08, 0xff, "Normal unicast"},
	{0x0C, 0xff, "Normal unicast and hashed multicast"},
	{0x1C, 0x08, "Promiscuous"},
	{0x00, 0x00, "Unknown/invalid"},
};

int rhine_diag(int vendor_id, int device_id, long ioaddr, int part_idx)
{
	int chip_active = 0;
	int rx_mode = inb(ioaddr + RxConfig);
	int ee_addr_len = 6, eeprom_size;
	int i;

	/* Always show the basic status. */
	printf("Station address ");
	for (i = 0; i < 5; i++)
		printf("%2.2x:", inb(ioaddr + StationAddr + i));
	printf("%2.2x.\n", inb(ioaddr + StationAddr + i));

	for (i = 0; rcvr_mode[i].mask; i++)
		if ((rx_mode & rcvr_mode[i].mask) == rcvr_mode[i].val) break;
	printf("  Receive mode is 0x%2.2x: %s.\n", rx_mode, rcvr_mode[i].name);

	if (verbose || show_regs) {
		unsigned intr_status;

		if (chip_active && !opt_f) {
		  printf("This device appears to be active, so some registers"
				 " will not be read.\n"
				 "To see all register values use the '-f' flag.\n");
		} else
			chip_active = 0;		/* Ignore the chip status with -f */

		if (opt_a > 1) {
			/* Reading some registers hoses the chip operation. */
			char dont_read[8] = {0x00,0x00,0x00,0xce,0xfd,0xed,0x7d,0xff};
			printf("%s chip registers at %#lx",
				   pcidev_tbl[part_idx].part_name, ioaddr);
			for (i = 0; i < pcidev_tbl[part_idx].io_size; i += 4) {
				if ((i & 0x1f) == 0)
					printf("\n 0x%3.3X:", i);
				if (chip_active && (dont_read[i>>5]) & (1<<((i>>2) & 7)))
					printf(" ********");
				else
					printf(" %8.8x", inl(ioaddr + i));
			}
			printf("\n");
		}
		intr_status = inw(ioaddr + IntrStatus);
		printf("  %snterrupt sources are pending (%4.4x).\n",
			   (intr_status & 0x03ff) ? "I": "No i", inw(ioaddr + IntrStatus));
		if (intr_status) {
			for (i = 0; i < 16; i++)
				if (intr_status & (1<<i))
					printf("   %s indication.\n", intr_names[i]);
		}
	}

	{		/* Add EEPROM sizing code here. */
		if (inb(ioaddr + EECtrl) & 0x80) {
			printf("Access to the EEPROM has been disabled (0x%2.2x).\n"
				   "  Direct reading or writing is not possible.\n",
				   inb(ioaddr + EECtrl));
			eeprom_size = 32;
			for (i = 0; i < 6; i++)
				eeprom_contents[i] = inb(ioaddr + StationAddr + i);
			for (i = 0; i < 4; i++)
				eeprom_contents[i + 0x1a] = inb(ioaddr + Config + i);
			eeprom_contents[0x18] = inb(ioaddr + PCIBusConfig);
			eeprom_contents[0x19] = inb(ioaddr + PCIBusConfig + 1);
			eeprom_contents[0x1e] = eeprom_contents[0x1f] = 0x73;
			outb(0x00, ioaddr + EECtrl);
		} else {
			eeprom_size = 1 << ee_addr_len;
			for (i = 0; i < eeprom_size; i++)
				eeprom_contents[i] = read_eeprom(ioaddr, i, ee_addr_len);
		}
	}

	if (set_hwaddr) {
		memcpy(new_ee_contents, eeprom_contents, eeprom_size << 1);
		for (i = 0; i < 3; i++)
			new_ee_contents[i + EEPROM_SA_OFFSET] =
				(new_hwaddr[i*2]<<8) + new_hwaddr[i*2+1];
		for (i = EEPROM_SA_OFFSET; i < EEPROM_SA_OFFSET + 3; i++)
			if (new_ee_contents[i] != eeprom_contents[i])
				write_eeprom(ioaddr, i, new_ee_contents[i], ee_addr_len);
#if defined(EEPROM_CSUM_OFFSET)  &&  EEPROM_CSUM_OFFSET > 0
		{			/* Recalculate the checksum. */
			unsigned short sum = 0;
			for (i = 0; i < EEPROM_CSUM_OFFSET; i++)
				sum ^= new_ee_contents[i];
			new_ee_contents[EEPROM_CSUM_OFFSET] = (sum ^ (sum>>8)) & 0xff;
			write_eeprom(ioaddr, EEPROM_CSUM_OFFSET, sum, ee_addr_len);
		}
#endif
		for (i = 0; i < eeprom_size; i++)
			eeprom_contents[i] = read_eeprom(ioaddr, i, ee_addr_len);
	}

	if (show_eeprom > 1) {
		printf("EEPROM contents%s:",
			   inb(ioaddr + EECtrl)&0x80 ? " (Assumed from chip registers)":"");
		for (i = 0; i < eeprom_size; i++) {
			if ((i&15) == 0)
				printf("\n0x%3.3x: ", i + 0x100);
			printf(" %2.2x", eeprom_contents[i]);
		}
		printf("\n");
	}
	if (show_mii) {
		int phys[4], phy, phy_idx = 0;
		for (phy = 0; phy < 32 && phy_idx < 4; phy++) {
			int mii_status = mdio_read(ioaddr, phy, 1);
			if (mii_status != 0xffff   &&  mii_status != 0x0000) {
				phys[phy_idx++] = phy;
				printf(" MII PHY found at address %d, status 0x%4.4x.\n",
					   phy, mii_status);
			}
		}
		if (phy_idx == 0)
			printf(" ***WARNING***: No MII transceivers found!\n");
		for (phy = 0; phy < phy_idx; phy++) {
			int mii_reg;
			printf(" MII PHY #%d transceiver registers:", phys[phy]);
			for (mii_reg = 0; mii_reg < 32; mii_reg++)
				printf("%s %4.4x", (mii_reg % 8) == 0 ? "\n  " : "",
					   mdio_read(ioaddr, phys[phy], mii_reg));
			printf(".\n");
		}
#ifdef LIBMII
		show_mii_details(ioaddr, phys[0]);
		if (show_mii > 1)
			monitor_mii(ioaddr, phys[0]);
#endif
	}

#ifdef LIBFLASH
	{
		flash_in_hook = rhine_flash_in;
		flash_out_hook = rhine_flash_out;
		if (opt_flash_show)
			flash_show(ioaddr, 0);
		if (opt_flash_dumpfile)
			if (flash_dump(ioaddr, 0, opt_flash_dumpfile) < 0) {
				fprintf(stderr, "Failed to save the old Flash BootROM image "
						"into file '%s'.\n", opt_flash_dumpfile);
				return 3;
			}
		if (opt_flash_loadfile)
			if (flash_program(ioaddr, 0, opt_flash_loadfile) < 0) {
				fprintf(stderr, "Failed to load the new Flash BootROM image "
						"from file '%s'.\n", opt_flash_loadfile);
				return 4;
			}
	}
#else
	if (opt_flash_loadfile  || opt_flash_dumpfile  ||  opt_flash_show)
		printf("Flash operations not configured into this program.\n");
#endif

	return 0;
}

/* Serial EEPROM section.
   A "bit" grungy, but we work our way through bit-by-bit :->. */

/* The EEPROM commands include the alway-set leading bit.
   They must be shifted by the number of address bits. */
#define EE_WRITE_CMD	(5)
#define EE_READ_CMD		(6)
#define EE_ERASE_CMD	(7)

/*  EEPROM_Ctrl bits.
    Some implementations have a data pin direction bit instead of
    separate data in and out bits.
*/
#define EE_SHIFT_CLK	0x04	/* EEPROM shift clock. */
#define EE_CS			0x08	/* EEPROM chip select. */
#define EE_DATA_WRITE	0x02	/* Data into EEPROM chip. */
#define EE_DATA_READ	0x01	/* Data out of EEPROM chip. */
/* These are generally derived from the bits above. */
#define EE_ENB			(EE_CS)
#define EE_WRITE_0		(EE_CS)
#define EE_WRITE_1		(EE_CS | EE_DATA_WRITE)

#define EE_OFFSET		EECtrl	/* Register offset in I/O space. */
/* Delay between EEPROM clock transitions. */
#define eeprom_delay(ee_addr)	inb(ee_addr)

/* This executes a generic EEPROM command, typically a write or write enable.
   It returns the data output from the EEPROM, and thus may also be used for
   reads. */
static int do_eeprom_cmd(long ioaddr, int cmd, int cmd_len)
{
	unsigned retval = 0;
	long ee_addr = ioaddr + EE_OFFSET;

	if (debug > 1)
		printf(" EEPROM op 0x%x: ", cmd);

	outb(EE_ENB | EE_SHIFT_CLK, ee_addr);

	/* Shift the command bits out. */
	do {
		short dataval = (cmd & (1 << cmd_len)) ? EE_WRITE_1 : EE_WRITE_0;
		outb(dataval, ee_addr);
		eeprom_delay(ee_addr);
		if (debug > 2)
			printf("%X", inb(ee_addr) & 15);
		outb(dataval | EE_SHIFT_CLK, ee_addr);
		eeprom_delay(ee_addr);
		retval = (retval << 1) | ((inb(ee_addr) & EE_DATA_READ) ? 1 : 0);
	} while (--cmd_len >= 0);
	outb(EE_ENB, ee_addr);

	/* Terminate the EEPROM access. */
	outb(EE_ENB & ~EE_CS, ee_addr);
	if (debug > 1)
		printf(" EEPROM result is 0x%5.5x.\n", retval);
	return retval;
}

/* The abstracted functions for EEPROM access. */
static int read_eeprom(long ioaddr, int location, int addr_len)
{
	return do_eeprom_cmd(ioaddr, ((EE_READ_CMD << addr_len) | location) << 16,
						 3 + addr_len + 16) & 0xffff;
}


static void write_eeprom(long ioaddr, int index, int value, int addr_len)
{
	long ee_ioaddr = ioaddr + EE_OFFSET;
	int i;

	/* Poll for previous op finished. */
	outb(EE_ENB, ee_ioaddr);
	for (i = 0; i < 10000; i++)			/* Typical 2000 ticks */
		if (inb(ee_ioaddr) & EE_DATA_READ)
			break;
	/* Enable programming modes. */
	do_eeprom_cmd(ioaddr, (0x4f << (addr_len-4)), 3 + addr_len);
	/* Do the actual write. */ 
	do_eeprom_cmd(ioaddr,
				  (((EE_WRITE_CMD<<addr_len) | index)<<16) | (value & 0xffff),
				  3 + addr_len + 16);
	/* Poll for write finished. */
	outb(EE_ENB, ee_ioaddr);
	for (i = 0; i < 10000; i++)			/* Typical 2000 ticks */
		if (inb(ee_ioaddr) & EE_DATA_READ)
			break;
	if (debug)
		printf(" Write finished after %d ticks.\n", i);
	/* Disable programming (note: this could take extra time. */
	do_eeprom_cmd(ioaddr, (0x40 << (addr_len-4)), 3 + addr_len);
}

/* MII - MDIO  (Media Independent Interface - Management Data I/O) accesses. */

int mdio_read(long ioaddr, int phy_id, int regnum)
{
	int boguscnt = 1024;

	if (verbose > 2)		/* Debug: 5 */
		printf(" mdio_read(%#lx, %d, %d)..", ioaddr, phy_id, regnum);
	/* Wait for a previous command to complete. */
	while ((inb(ioaddr + MIICmd) & 0x60) && --boguscnt > 0)
		;
	outb(0x00, ioaddr + MIICmd);
	outb(phy_id, ioaddr + MIIPhyAddr);
	outb(regnum, ioaddr + MIIRegAddr);
	outb(0x40, ioaddr + MIICmd);			/* Trigger read */
	boguscnt = 1024;
	while ((inb(ioaddr + MIICmd) & 0x40) && --boguscnt > 0)
		;
	return inw(ioaddr + MIIData);
}

void mdio_write(long ioaddr, int phy_id, int regnum, int value)
{
	int boguscnt = 1024;

	/* Wait for a previous command to complete. */
	while ((inb(ioaddr + MIICmd) & 0x60) && --boguscnt > 0)
		;
	outb(0x00, ioaddr + MIICmd);
	outb(phy_id, ioaddr + MIIPhyAddr);
	outb(regnum, ioaddr + MIIRegAddr);
	outw(value, ioaddr + MIIData);
	outb(0x20, ioaddr + MIICmd);			/* Trigger write. */
	return;
}

/*
 * Local variables:
 *  compile-command: "cc -O -Wall -o via-diag via-diag.c"
 *  tab-width: 4
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 */
