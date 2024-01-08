/* pcnet-diag.c: Diagnostic and setup for AMD PCnet/PCI ethercards.

   This is a diagnostic and EEPROM setup program for the Ethernet adapters
   based on the AMD PCnet/PCI series chips.
   Note: The EEPROM setup code is not implemented.

   Copyright 1999 by Donald Becker.
   This version released under the Gnu Public License, incorporated herein
   by reference.  Contact the author for use under other terms.

   This program must be compiled with "-O"!  See the bottom of this file
   for the suggested compile-command.

   The author may be reached as becker@cesdis.gsfc.nasa.gov.
   C/O USRA-CESDIS, Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771

   http://cesdis.gsfc.nasa.gov/linux/diag/index.html
   http://cesdis.gsfc.nasa.gov/linux/misc/NWay.html
*/

static char *version_msg =
"pcnet-diag.c:v1.01 7/28/99 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";
static char *usage_msg =
"Usage: pcnet-diag [-aEefFmqrRtvVwW] [-p <IOport>] [-A <media>] [-F <media>]\n";

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
#include <asm/unaligned.h>

#if defined(__linux__)  &&  __GNU_LIBRARY__ == 1
#include <asm/io.h>
#else
#include <sys/io.h>
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

extern int tulip_diag(int vendor_id, int device_id, int ioaddr, int part_idx);
extern int pcnet_diag(int vendor_id, int device_id, int ioaddr, int part_idx);

enum pcnet_flags { PCNET_HAS_MII };

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
	{ "AMD PCnet/PCI series", "AMD PCnet32, unknown type",
	  0x1022, 0x0000, 0xffff, 0, 32, pcnet_diag },
	{ "AMD PCnet/PCI 978", "AMD PCnet-Home 79c978 Homenet",
	  0x1022, 0x2001, 0xffff, 0, 32, pcnet_diag },
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
#ifdef LIBFLASH
static unsigned int opt_flash_show = 0;
static char	*opt_flash_dumpfile = NULL, *opt_flash_loadfile = NULL;
#endif
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
			   "     '-e' to show EEPROM contents, -ee for numeric contents,\n"
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
	{ "HomePNA", 16 },
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

struct mdio_ops {
	void (*mdio_sync_func)(int ioaddr);
	int (*mdio_read_func)(int ioaddr, int phy_id, int location);
	void (*mdio_write_func)(int ioaddr, int phy_id, int location, int value);
} mdio_ops = {0, 0};

void mdio_sync(int ioaddr)
{
	if (mdio_ops.mdio_sync_func)
		mdio_ops.mdio_sync_func(ioaddr);
}

int mdio_read(int ioaddr, int phy_id, int location)
{
	if (mdio_ops.mdio_read_func)
		return mdio_ops.mdio_read_func(ioaddr, phy_id, location);
	return 0;
}

void mdio_write(int ioaddr, int phy_id, int location, int value)
{
	if (mdio_ops.mdio_write_func)
		mdio_ops.mdio_write_func(ioaddr, phy_id, location, value);
}



/* Chip-specific section. */

/* AMD names for the chip registers, using 32 bit mode. */
enum pcnet_offsets {
	AROM=0, RDP=0x10, RAP=0x14, PCnetReset=0x18, BDP=0x1C, 
};

/* PCnet/PCI Chip-specific section. */
static const char *pcnet_intr_names[16] ={
	"Initialized", "Started", "Stopped", "Tx busy",
	"Tx enabled", "Rx enabled", "Intr enable", "Interrupt flag",
	"Initialization done", "Tx done", "Rx done", "Memory error",
	"Rx Missed Frame", "Collision error", "unknown-0x4000", "Error summary",
};
static const char *pcnet_xcvrs[4] = {
	"10baseT", "HomePNA", "External MII", "undefined" };

static int  pcnet_mdio_read(int ioaddr, int phy_id, int location);
static void pcnet_mdio_write(int ioaddr, int phy_id, int location, int value);
static int  pcnet_read_eeprom(int ioaddr, int location);

int pcnet_diag(int vendor_id, int device_id, int ioaddr, int part_idx)
{
	u32 saved_RAP = inl(ioaddr + RAP);
	u8 mac_addr[8];
	int i;

	/* Note: this chip only works on little-endian anyway. */
	((u32*)mac_addr)[0] = inl(ioaddr + AROM);
	((u32*)mac_addr)[1] = inl(ioaddr + AROM + 4);
	printf(" Station MAC address is %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x.\n",
		   mac_addr[0], mac_addr[1], mac_addr[2],
		   mac_addr[3], mac_addr[4], mac_addr[5]);

	if (show_regs) {
		int num_regs = pcidev_tbl[part_idx].io_size;
		u32 csr0;

		/* Assume the chip is in 32 bit mode, but assert just in case. */
		outl(0, ioaddr + RAP);
		outl(0, ioaddr + RDP);
		csr0 = inl(ioaddr + RDP);
		printf("The chip is %s.\n"
			   "The transmitter is %s%s, the receiver is %s.\n",
			   csr0&4 ? "stopped" : csr0&2 ? "started" :
			   csr0&1 ? ", initialization has completed" : "initializing(?)",
			   csr0&0x10 ? "running":"stopped", csr0&8 ? " (busy)":"",
			   csr0&0x20 ? "running":"stopped");
		if (csr0 & 0x0080) {
			printf("Interrupt sources are pending!\n");
			for (i = 0; i < 15; i++)
				if (csr0 & (1<<i))
					printf("   %s indication.\n", pcnet_intr_names[i]);
		} else
			printf(" No interrupt sources are pending.\n");

		outl(0x49, ioaddr + RAP);
		printf(" Using the %s transceiver.\n", 
			   pcnet_xcvrs[inl(ioaddr + BDP) & 3]);
		if (show_regs > 1) {
			printf("%s chip registers at %#x:",
				   pcidev_tbl[part_idx].part_name, ioaddr);
			for (i = 0; i < num_regs; i += 4)
				printf("%s %8.8x", (i % 32) == 0 ? "\n " : "",
					   inl(ioaddr + i));
			printf("\n");
			printf(" Control and Status Registers:");
			for (i = 0; i < 128; i ++) {
				outl(i, ioaddr + RAP);
				printf("%s %8.8x", (i % 8) == 0 ? "\n  " : "",
					   inl(ioaddr + RDP));
			}
			printf("\nBus Configuration Registers:");
			for (i = 0; i < 64; i ++) {
				outl(i, ioaddr + RAP);
				printf("%s %4.4x", (i % 8) == 0 ? "\n  " : "",
					   inl(ioaddr + BDP) & 0xffff);
			}
			printf("\n");
		}
	}

	if (show_eeprom) {
		u16 eeprom_contents[64];
		u8 checksum = 1;

		printf("EEPROM contents:");
		for (i = 0; i < 64; i++)
			printf("%s %4.4x", (i & 7) == 0 ? "\n ":"",
				   eeprom_contents[i] = pcnet_read_eeprom(ioaddr, i));
		for (i = 0; i < 82; i++)
			checksum += ((u8*)eeprom_contents)[i];
		printf("\nEEPROM checksum is %2.2x (%s).\n", checksum,
			   checksum ? "INCORRECT" : "correct");
	}

	if (show_mii) {
		int phys[4], phy, phy_idx = 0;

		mdio_ops.mdio_read_func = pcnet_mdio_read;
		mdio_ops.mdio_write_func = pcnet_mdio_write;
		mdio_sync(ioaddr);
		for (phy = 0; phy < 32 && phy_idx < 4; phy++) {
			int mii_status = mdio_read(ioaddr, phy, 1);
			if (mii_status != 0xffff  &&
				mii_status != 0x0000) {
				phys[phy_idx++] = phy;
				printf(" MII PHY found at address %d, status 0x%4.4x.\n",
					   phy, mii_status);
			}
		}
		if (phy_idx) {
			if (nway_advertise > 0) {
				printf(" Setting the media capability advertisement register"
					   " of PHY #%d to 0x%4.4x.\n",
					   phys[0], nway_advertise | 1);
				mdio_write(ioaddr, phys[0], 4, nway_advertise | 1);
			}
			if (opt_restart) {
				printf("Restarting negotiation...\n");
				mdio_write(ioaddr, phys[0], 0, 0x0000);
				mdio_write(ioaddr, phys[0], 0, 0x1200);
			}
		}
		if (phy_idx == 0)
			printf(" ***WARNING***: No MII transceivers found!\n");
#ifdef LIBMII
		else {
			if (show_mii > 1)
				show_mii_details(ioaddr, phys[0]);
			if (opt_watch || show_mii > 2)
				monitor_mii(ioaddr, phys[0]);
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

	outl(saved_RAP, ioaddr + RAP);

	return 0;
}

/* MII management register interaction is trivial on the PCnet. */
static int pcnet_mdio_read(int ioaddr, int phy_id, int location)
{
	outl(33, ioaddr + RAP);
	outl((phy_id<<5) + location, ioaddr + BDP);
	outl(34, ioaddr + RAP);
	return inl(ioaddr + BDP) & 0xffff;
}
static void pcnet_mdio_write(int ioaddr, int phy_id, int location, int value)
{
	outl(33, ioaddr + RAP);
	outl((phy_id<<5) + location, ioaddr + BDP);
	outl(34, ioaddr + RAP);
	outl(value, ioaddr + BDP);
	return;
}

/* The PCnet has a typical software-serial EEPROM interface. */
#define eeprom_delay()	inl(ee_addr)

/*  EEPROM_Ctrl bits. */
#define EE_DATA_READ	0x01	/* Data bit to/from EEPROM. */
#define EE_SHIFT_CLK	0x02	/* EEPROM shift clock. */
#define EE_CS			0x04	/* EEPROM chip select. */
#define EE_WRITE_0		0x14
#define EE_WRITE_1		0x15
#define EE_ENB			(0x10 | EE_CS)

/* The EEPROM commands include the alway-set leading bit. */
#define EE_ADDR_SZ		6
#define EE_WRITE_CMD	(5<<EE_ADDR_SZ)
#define EE_READ_CMD		(6<<EE_ADDR_SZ)
#define EE_ERASE_CMD	(7<<EE_ADDR_SZ)
#define EE_CMD_SZ		(4+6)	/* command-length + ADDR_SZ */

static int pcnet_read_eeprom(int ioaddr, int location)
{
	int i;
	unsigned short retval = 0;
	int ee_addr = ioaddr + BDP;
	int read_cmd = location | EE_READ_CMD;

	outl(19, ioaddr + RAP);

	outl(EE_ENB & ~EE_CS, ee_addr);
	outl(EE_ENB, ee_addr);

	/* Shift the read command bits out. */
	for (i = EE_CMD_SZ; i >= 0; i--) {
		short dataval = (read_cmd & (1 << i)) ? EE_WRITE_1 : EE_WRITE_0;
		outl(dataval, ee_addr);
		eeprom_delay();
		outl(dataval | EE_SHIFT_CLK, ee_addr);
		eeprom_delay();
	}
	outl(EE_ENB, ee_addr);

	for (i = 16; i > 0; i--) {
		outl(EE_ENB | EE_SHIFT_CLK, ee_addr);
		eeprom_delay();
		retval = (retval << 1) | ((inl(ee_addr) & EE_DATA_READ) ? 1 : 0);
		outl(EE_ENB, ee_addr);
		eeprom_delay();
	}

	/* Terminate the EEPROM access. */
	outl(EE_ENB & ~EE_CS, ee_addr);
	if (debug > 2)
		printf(" EEPROM value at %d is %5.5x.\n", location, retval);
	return retval;
}


/*
 * Local variables:
 *  compile-command: "cc -O -Wall -Wstrict-prototypes -o pcnet-diag pcnet-diag.c `[ -f libmii.c ] && echo -DLIBMII libmii.c`"
 *  simple-compile-command: "cc -O -o pcnet-diag pcnet-diag.c"
 *  tab-width: 4
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 */
