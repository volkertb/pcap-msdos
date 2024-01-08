/* eepro100-diag.c: Diagnostic and setup program for the Intel EEPro100.

   This is a diagnostic and EEPROM setup program for the Ethernet adapters
   based on the Intel "Speedo3" chip series: the i82557, '558 and '559.
   These chips are used on the EtherExpress Pro100B, and EEPro PCI 10+.
   (Note: The EEPROM write code is deliberately disabled.)

   Copyright 1996-1999 by Donald Becker.
   This version released under the Gnu Public License, incorporated herein
   by reference.  Contact the author for use under other terms.

   This program must be compiled with "-O"!  See the bottom of this file
   for the suggested compile-command.

   The author may be reached as becker@cesdis.gsfc.nasa.gov.
   C/O USRA-CESDIS, Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771
*/

static char *version_msg =
"eepro100-diag.c:v1.01 7/8/99 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";
static char *usage_msg =
"Usage: eepro100-diag [-aEefFmqrRtsvVwW] [-p <IOport>] [-[AF] <media>]\n";

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

#include <asm/types.h>

#if defined(__linux__)  &&  __GNU_LIBRARY__ == 1
#include <asm/io.h>			/* Newer libraries use <sys/io.h> instead. */
#else
#include <sys/io.h>
#endif

/* No libmii.h (yet) */
extern int show_mii_details(long ioaddr, int phy_id);
extern int monitor_mii(long ioaddr, int phy_id);
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

extern int eepro100_diag(int vend_id, int dev_id, int ioaddr, int part_idx);

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
	int (*diag_func)(int vendor_id, int device_id, int ioaddr, int part_num);
} pcidev_tbl[] = {
	{ "Intel 82557", "Intel 82557 EtherExpressPro100B",
	  0x8086, 0x1229, 0xffff, 0, 0x20, eepro100_diag },
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
static unsigned int opt_flash_show = 0;
static char	*opt_flash_dumpfile = NULL, *opt_flash_loadfile = NULL;
static unsigned char new_hwaddr[6], set_hwaddr = 0;

static int scan_proc_pci(int card_num);
static int parse_media_type(const char *capabilities);
static int get_media_index(const char *name);


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
			/* Valid with libflash only. */
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
	fprintf(stderr, "Invalid interface specified: it must be one of\n  ");
	for (i = 0; mediamap[i].name; i++)
		fprintf(stderr, "  %s", mediamap[i].name);
	fprintf(stderr, ".\n");
	return -1;
}


/* Chip-specific section. */

/* The chip-specific section for the Intel EEPro100 diagnostic. */
static int read_eeprom(long ioaddr, int location, int addr_len);
static void write_eeprom(int ioaddr, int index, int value, int addr_len);
static int do_eeprom_cmd(long ioaddr, int cmd, int cmd_len);
int mdio_read(long ioaddr, int phy_id, int location);
void mdio_write(long ioaddr, int phy_id, int location, int value);

static void parse_eeprom(unsigned short *ee_data);
#ifdef notyet
static void write_eeprom(int ioaddr, int index, int value);
static int do_update(unsigned short *ee_values,
					 int index, char *field_name, int new_value);
#endif


/* Offsets to the various registers.
   All accesses need not be longword aligned. */
enum speedo_offsets {
	SCBStatus = 0, SCBCmd = 2,	/* Rx/Command Unit command and status. */
	SCBPointer = 4,				/* General purpose pointer. */
	SCBPort = 8,				/* Misc. commands and operands.  */
	SCBflash = 12, SCBeeprom = 14, /* EEPROM and flash memory control. */
	SCBCtrlMDI = 16,			/* MDI interface control. */
	SCBEarlyRx = 20,			/* Early receive byte count. */
};

/* The EEPROM commands include the alway-set leading bit. */
#define EE_WRITE_CMD	(5)
#define EE_READ_CMD		(6)
#define EE_ERASE_CMD	(7)

/* Last-hope recovery major screw-ups: rewrite the EEPROM with the values
   from my card (and hope I don't met you on the net...).
   Only experts should use this, and it probably will not match your board! */
unsigned short djb_eepro100_eeprom[64] = {
	0xa000, 0x49c9, 0x87ab, 0x0000, 0x0000, 0x0101, 0x4401, 0x0000,
	0x6455, 0x2022, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0x7fcd, }; 

/* Values read from the EEPROM, and the new image. */
unsigned short eeprom_contents[256];
unsigned short new_ee_contents[256];


int eepro100_diag(int vend_id, int dev_id, int ioaddr, int part_idx)
{
	int i;
	int phy0, phy1;
	int ee_addr_size = 6;		/* Modified below. */
	int eeprom_size = 64;

	/* It's mostly safe to use the EEPro100 EEPROM and MDIO register during
	   operation.  But warn the user, and make then pass '-f'. */
	if (!opt_f  && (inw(ioaddr + SCBStatus) & 0x00fc) != 0x0000) {
		printf("A potential i82557 chip has been found, but it appears to "
			   "be active.\nEither shutdown the network, or use the"
			   " '-f' flag.\n");
		return 1;
	}

	if (verbose > 1 || show_regs) {
		static const char *tx_state[] = {
			"Idle", "Suspended", "Active", "Unknown"};
		static const char *rx_state[] = {
			"Idle", "Idle/Suspended", "No Resources", "Unknown",
			"Ready", "Broken-5",  "Broken-6",  "Broken-7", "Broken-8",
			"Suspended - no buffers", "No resources - no buffers", "Broken-11",
			"Ready, but no buffers",  "Broken-13", "Broken-14", "Broken-15",
		};
		unsigned short status;

		printf("i82557 chip registers at %#x:\n ", ioaddr);
		for (i = 0; i < 0x18; i += 4)
			printf(" %8.8x", inl(ioaddr + i));
		printf("\n");
		status = inw(ioaddr + SCBStatus);
		printf("  %snterrupt sources are pending.\n",
			   (status & 0xff00) ? "I": "No i");
		printf("   The transmit unit state is '%s'.\n",
			   tx_state[(status>>6) & 3]);
		printf("   The receive unit state is '%s'.\n",
			   rx_state[(status>>2) & 15]);
		if (status == 0x0050)
			printf("  This status is normal for an activated but idle "
				   "interface.\n");
		else
			printf("  This status is unusual for an activated interface.\n");
		if (inw(ioaddr + SCBCmd) != 0)
			printf(" The Command register has an unprocessed command "
				   "%4.4x(?!).\n",
				   inw(ioaddr + SCBCmd));
	}

	/* Read the EEPROM. */
	{
		int size_test = do_eeprom_cmd(ioaddr, (EE_READ_CMD << 8) << 16, 27);
		ee_addr_size = (size_test & 0xffe0000) == 0xffe0000 ? 8 : 6;
		eeprom_size = 1 << ee_addr_size;
		if (debug)
			printf("EEPROM size probe returned %#x, %d bit address.\n",
				   size_test, ee_addr_size);
	}
	for (i = 0; i < eeprom_size; i++)
		eeprom_contents[i] = read_eeprom(ioaddr, i, ee_addr_size);

	phy0 = eeprom_contents[6] & 0x8000 ? -1 : eeprom_contents[6] & 0x1f;
	phy1 = eeprom_contents[7] & 0x8000 ? -1 : eeprom_contents[7] & 0x1f;

	if (verbose > 2 || show_eeprom > 1) {
		unsigned short sum = 0;
		int last_row_zeros = 0;
		u16 zeros[8] = { 0, 0, 0, 0, 0, 0, 0, 0};
		printf("EEPROM contents:\n");
		for (i = 0; i < eeprom_size; i += 8) {
			int j;
			if (memcmp(eeprom_contents + i, zeros, 16) != 0) {
				last_row_zeros = 0;
				printf("  %#4.2x:", i);
				for (j = 0; j < 8; j++) {
					printf(" %4.4x", eeprom_contents[i + j]);
					sum += eeprom_contents[i + j];
				}
				printf("\n");
			} else if ( ! last_row_zeros) {
				last_row_zeros = 1;
				printf("      ...\n");
			}
		}
		if (sum == 0xBaBa)
			printf(" The EEPROM checksum is correct.\n");
		else
			printf(" *****  The EEPROM checksum is INCORRECT!  *****\n"
				   "  The checksum is 0x%2.2X, it should be 0xBABA!\n", sum);
	}

	/* The user will usually want to see the interpreted EEPROM contents. */
	if (verbose > 1 || show_eeprom)
		parse_eeprom(eeprom_contents);
	/* Warning: this is example code only!  Actual write code should
	   verify the input values, verify that the writes actually
	   occured, and never let the card get into a non-working state. */
	if (do_write_eeprom  &&  opt_G) {
		int checksum = 0;
		if (do_write_eeprom < 2)
			printf("Would write %x to the RPL-boot configuration.\n",
				   opt_GPIO);
		else {
			printf("Writing %x to the RPL-boot configuration.\n",
				   opt_GPIO);
			write_eeprom(ioaddr, 0x20, opt_GPIO, ee_addr_size);
			write_eeprom(ioaddr, 0x21, opt_GPIO >> 16, ee_addr_size);
			eeprom_contents[0x20] = read_eeprom(ioaddr, 0x20, ee_addr_size);
			eeprom_contents[0x21] = read_eeprom(ioaddr, 0x21, ee_addr_size);
		}
		for (i = 0; i < eeprom_size - 1; i++) {
			checksum += eeprom_contents[i];
		}
		if (debug)
			printf("Writing the new checksum 0x%4.4X to location %d.\n",
				   0xBABA - checksum, i);
		write_eeprom(ioaddr, i, 0xBABA - checksum, ee_addr_size);
	}

	/* Grrr, it turns out they the PHY is not always #0. */
	if (verbose > 1 || show_mii) {
		if (phy0 > 0 &&
			mdio_read(ioaddr, phy0, 0) != 0xffff  && 
			mdio_read(ioaddr, phy0, 0) != 0x0000) {
			int mii_reg;
			printf(" MII PHY #%d transceiver registers:", phy0);
			for (mii_reg = 0; mii_reg < 32; mii_reg++)
				printf("%s %4.4x", (mii_reg % 8) == 0 ? "\n " : "",
					   mdio_read(ioaddr, phy0, mii_reg));
			printf(".\n");
#ifdef LIBMII
			show_mii_details(ioaddr, phy0);
#endif
		}
		if (phy1 > 0 &&
			mdio_read(ioaddr, phy1, 0) != 0xffff  && 
			mdio_read(ioaddr, phy1, 0) != 0x0000) {
			int mii_reg;
			printf(" Alternate MII PHY (#%d) transceiver registers:", phy1);
			for (mii_reg = 0; mii_reg < 32; mii_reg++)
				printf("%s %4.4x", (mii_reg % 8) == 0 ? "\n " : "",
					   mdio_read(ioaddr, phy1, mii_reg));
			printf(".\n");
#ifdef LIBMII
			show_mii_details(ioaddr, phy1);
#endif
		}
		if (phy0 > 0 &&  nway_advertise > 0) {
			printf(" Setting the media capability advertisement register of "
				   "PHY #%d to 0x%4.4x.\n", phy0, nway_advertise | 1);
			mdio_write(ioaddr, phy0, 4, nway_advertise | 1);
		}
	}
	if (show_mii > 1) {			/* Monitor MII status */
		monitor_mii(ioaddr, phy0);
	}
	return 0;
}


/* Serial EEPROM section.
   A "bit" grungy, but we work our way through bit-by-bit :->. */
/*  EEPROM_Ctrl bits. */
#define EE_SHIFT_CLK	0x01	/* EEPROM shift clock. */
#define EE_CS			0x02	/* EEPROM chip select. */
#define EE_DATA_WRITE	0x04	/* EEPROM chip data in. */
#define EE_DATA_READ	0x08	/* EEPROM chip data out. */
#define EE_ENB			(0x4800 | EE_CS)
#define EE_WRITE_0		0x4802
#define EE_WRITE_1		0x4806
#define EE_OFFSET		SCBeeprom

/* Delay between EEPROM clock transitions. */
#define eeprom_delay()	inw(ee_addr)

static int read_eeprom(long ioaddr, int location, int addr_len)
{
	return do_eeprom_cmd(ioaddr, ((EE_READ_CMD << addr_len) | location) << 16,
						 3 + addr_len + 16) & 0xffff;
}


static void write_eeprom(int ioaddr, int index, int value, int addr_len)
{
	long ee_ioaddr = ioaddr + EE_OFFSET;
	int i;

	/* Poll for previous op finished. */
	outw(EE_ENB, ee_ioaddr);
	for (i = 0; i < 10000; i++)			/* Typical 2000 ticks */
		if (inw(ee_ioaddr) & EE_DATA_READ)
			break;
	/* Enable programming modes. */
	do_eeprom_cmd(ioaddr, (0x4f << (addr_len-4)), 3 + addr_len);
	/* Do the actual write. */ 
	do_eeprom_cmd(ioaddr,
				  (((EE_WRITE_CMD<<addr_len) | index)<<16) | (value & 0xffff),
				  3 + addr_len + 16);
	/* Poll for write finished. */
	outw(EE_ENB, ee_ioaddr);
	for (i = 0; i < 10000; i++)			/* Typical 2000 ticks */
		if (inw(ee_ioaddr) & EE_DATA_READ)
			break;
	if (debug)
		printf(" Write finished after %d ticks.\n", i);
	/* Disable programming (note: this could take extra time. */
	do_eeprom_cmd(ioaddr, (0x40 << (addr_len-4)), 3 + addr_len);
}

/* This executes a generic EEPROM command, typically a write or write enable.
   It returns the data output from the EEPROM, and thus may also be used for
   reads. */
static int do_eeprom_cmd(long ioaddr, int cmd, int cmd_len)
{
	unsigned retval = 0;
	long ee_addr = ioaddr + EE_OFFSET;

	if (debug > 1)
		printf(" EEPROM op 0x%x: ", cmd);

	outw(EE_ENB | EE_SHIFT_CLK, ee_addr);

	/* Shift the command bits out. */
	do {
		short dataval = (cmd & (1 << cmd_len)) ? EE_WRITE_1 : EE_WRITE_0;
		outw(dataval, ee_addr);
		eeprom_delay();
		if (debug > 2)
			printf("%X", inw(ee_addr) & 15);
		outw(dataval | EE_SHIFT_CLK, ee_addr);
		eeprom_delay();
		retval = (retval << 1) | ((inw(ee_addr) & EE_DATA_READ) ? 1 : 0);
	} while (--cmd_len >= 0);
	outw(EE_ENB, ee_addr);

	/* Terminate the EEPROM access. */
	outw(EE_ENB & ~EE_CS, ee_addr);
	if (debug > 1)
		printf(" EEPROM result is 0x%5.5x.\n", retval);
	return retval;
}


/* Read and write the MII registers using the parallel MDIO interface on
   the Speedo3.  This is far less exciting than the typical bit-serial
   implementation (whew!) although we cannot control preamble generation. */

int mdio_read(long ioaddr, int phy_id, int location)
{
	int val, boguscnt = 64*10;		/* <64 usec. to complete, typ 27 ticks */
	outl(0x08000000 | (location<<16) | (phy_id<<21), ioaddr + SCBCtrlMDI);
	do {
		val = inl(ioaddr + SCBCtrlMDI);
		if (--boguscnt < 0) {
			fprintf(stderr, " mdio_read() timed out with val = %8.8x.\n", val);
		}
	} while (! (val & 0x10000000));
	return val & 0xffff;
}

void mdio_write(long ioaddr, int phy_id, int location, int value)
{
	int val, boguscnt = 64*10;		/* <64 usec. to complete, typ 27 ticks */
	outl(0x04000000 | (location<<16) | (phy_id<<21) | value,
		 ioaddr + SCBCtrlMDI);
	do {
		val = inl(ioaddr + SCBCtrlMDI);
		if (--boguscnt < 0) {
			fprintf(stderr, " mdio_write() timed out with val = %8.8x.\n",
					val);
		}
	} while (! (val & 0x10000000));
	return;
}

#ifndef LIBMII
int monitor_mii(long ioaddr, int phy_id)
{
	int i;
	unsigned short new_1, baseline_1 = mdio_read(ioaddr, phy_id, 1);
	if (baseline_1 == 0xffff) {
		fprintf(stderr, "No MII transceiver present to monitor.\n");
		return -1;
	}
	printf("  Baseline value of MII status register is %4.4x.\n",
		   baseline_1);
	for (i = 0; i < 60; i++) {
		new_1 = mdio_read(ioaddr, phy_id, 1);
		if (new_1 != baseline_1) {
			printf("  MII status register changed to %4.4x.\n", new_1);
			baseline_1 = new_1;
		}
		sleep(1);
	}
	return 0;
}
#endif  /* not LIBMII */


/* PHY media interface chips. */
static const char *phys[] = {
	"None", "i82553-A/B", "i82553-C", "i82503",
	"DP83840", "80c240", "80c24", "i82555",
	"unknown-8", "unknown-9", "DP83840A", "unknown-11",
	"unknown-12", "unknown-13", "unknown-14", "unknown-15", };
enum phy_chips { NonSuchPhy=0, I82553AB, I82553C, I82503, DP83840, S80C240,
					 S80C24, I82555, DP83840A=10, };
static const char is_mii[] = { 0, 1, 1, 0, 1, 1, 0, 1 };
const char *connectors[] = {" RJ45", " BNC", " AUI", " MII"};


static void parse_eeprom(unsigned short *eeprom)
{
	unsigned char dev_addr[6];
	int i, j;
	int phy0 = eeprom[6] & 0x8000 ? -1 : eeprom_contents[6] & 0x1f;
	int phy1 = eeprom[7] & 0x8000 ? -1 : eeprom_contents[7] & 0x1f;

	printf("Intel EtherExpress Pro 10/100 EEPROM contents:\n"
		   "  Station address ");
	for (j = 0, i = 0; i < 3; i++) {
		dev_addr[j++] = eeprom[i];
		dev_addr[j++] = eeprom[i] >> 8;
	}
	for (i = 0; i < 5; i++)
		printf("%2.2X:", dev_addr[i]);
	printf("%2.2X.\n", dev_addr[i]);
	if ((eeprom[3] & 0x03) != 3)
		printf("  Receiver lock-up bug exists. (The driver work-around *is* "
			   "implemented.)\n");
	printf("  Board assembly %4.4x%2.2x-%3.3d, Physical connectors present:",
		   eeprom[8], eeprom[9]>>8, eeprom[9] & 0xff);
	for (i = 0; i < 4; i++)
		if (eeprom[5] & (1<<i))
			printf(connectors[i]);
	printf("\n  Primary interface chip %s PHY #%d.\n",
		   phys[(eeprom[6]>>8)&7], phy0);
	if (((eeprom[6]>>8) & 0x3f) == DP83840)
		printf("  Transceiver-specific setup is required for the DP83840"
			   " transceiver.\n");
	if (eeprom[7] & 0x0700)
		printf("    Secondary interface chip %s, PHY %d.\n",
			   phys[(eeprom[7]>>8)&7], phy1);

	return;
}


/*
 * Local variables:
 *  compile-command: "cc -O -Wall -o eepro-diag eepro100-diag.c"
 *  alt-compile-command: "cc -O -Wall -o eepro-diag eepro100-diag.c -DLIBMII libmii.c"
 *  tab-width: 4
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 */
