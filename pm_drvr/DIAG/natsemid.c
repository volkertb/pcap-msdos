/* natsemi-diag.c: Diagnostic and setup core code for Linux ethercards.

   This is a diagnostic and EEPROM setup program for Ethernet adapters
   based on:
	National Semiconductor DP83810 / 83815

   Copyright 1999 by Donald Becker.
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
"natsemi-diag.c:v1.01 9/19/99 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";
static char *usage_msg =
"Usage: etherdiag [-aEefFmqrRtvVwW] [-p <IOport>] [-[AF] <media>]\n";

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

extern int   tulip_diag(int vend_id, int dev_id, int ioaddr, int part_idx);
extern int symbios_diag(int vend_id, int dev_id, int ioaddr, int part_idx);
extern int hamachi_diag(int vend_id, int dev_id, int ioaddr, int part_idx);
extern int  vortex_diag(int vend_id, int dev_id, int ioaddr, int part_idx);
extern int natsemi_diag(int vend_id, int dev_id, int ioaddr, int part_idx);

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
	int (*diag_func)(int vendor_id, int device_id, int ioaddr, int part_num);
} pcidev_tbl[] = {
	{"NatSemi DP83815", "National Semiconductor DP83815",
	 0x100B, 0x0020, 0xffff, 0, 256, natsemi_diag},
#ifdef SYMBIOS_DIAG
	{" Ethernet controller: NCR Unknown device", "Symbios 53885",
	 0x1000, 0x0701, 0xffff, 0, 256, symbios_diag},
#endif
#ifdef HAMACHI_DIAG
	{" Packet Engines GNIC-II", "Packet Engines 'Hamachi' GNIC-II",
	 0x1318, 0x0911, 0xffff, 2, 1024, hamachi_diag},
#endif
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
		if (strcasecmp(name, mediamap[i].name) == 0)
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

/* The National Semiconductor DP83810 series. */

/* Offsets to the DP83815 registers.*/
enum register_offsets {
	ChipCmd=0x00, ChipConfig=0x04, EECtrl=0x08, PCIBusCfg=0x0C,
	IntrStatus=0x10, IntrMask=0x14, IntrEnable=0x18,
	TxRingPtr=0x20, TxConfig=0x24,
	RxRingPtr=0x30, RxConfig=0x34,
	WOLCmd=0x40, PauseCmd=0x44, RxFilterAddr=0x48, RxFilterData=0x4C,
	BootRomAddr=0x50, BootRomData=0x54, StatsCtrl=0x5C, StatsData=0x60,
	RxPktErrs=0x60, RxMissed=0x68, RxCRCErrs=0x64,
};

/* The diagnostics must use I/O space accesses. */
#undef readl
#undef writel
#undef writew
#define readl(addr) inl(addr)
#define writel(val, addr) outl(val, addr)
#define writew(val, addr) outw(val, addr)

/* Read the EEPROM and MII Management Data I/O (MDIO) interfaces.
   The EEPROM code is for the common 93c06/46 EEPROMs with 6 bit addresses. */

/* Delay between EEPROM clock transitions.
   No extra delay is needed with 33Mhz PCI, but future 66Mhz access may need
   a delay.  Note that pre-2.0.34 kernels had a cache-alignment bug that
   made udelay() unreliable.
   The old method of using an ISA access as a delay, __SLOW_DOWN_IO__, is
   depricated.
*/
#define eeprom_delay()	readl(ee_addr)

enum EEPROM_Ctrl_Bits {
	EE_ShiftClk=0x04, EE_DataIn=0x01, EE_ChipSelect=0x08, EE_DataOut=0x01,
};
#define EE_Write0 (EE_ChipSelect)
#define EE_Write1 (EE_ChipSelect | EE_DataOut)

/* The EEPROM commands include the alway-set leading bit. */
enum EEPROM_Cmds {
	EE_WriteCmd=(5 << 6), EE_ReadCmd=(6 << 6), EE_EraseCmd=(7 << 6),
};
#define EEPROM_SIZE 64
static u16 eeprom_contents[EEPROM_SIZE];

static int read_eeprom(long addr, int location)
{
	int i;
	int retval = 0;
	int ee_addr = addr + EECtrl;
	int read_cmd = location | EE_ReadCmd;
	writel(EE_Write0, ee_addr);

	/* Shift the read command bits out. */
	for (i = 10; i >= 0; i--) {
		short dataval = (read_cmd & (1 << i)) ? EE_Write1 : EE_Write0;
		writel(dataval, ee_addr);
		eeprom_delay();
		writel(dataval | EE_ShiftClk, ee_addr);
		eeprom_delay();
	}
	writel(EE_ChipSelect, ee_addr);

	for (i = 16; i > 0; i--) {
		writel(EE_ChipSelect | EE_ShiftClk, ee_addr);
		eeprom_delay();
		retval = (retval << 1) | ((readl(ee_addr) & EE_DataIn) ? 1 : 0);
		writel(EE_ChipSelect, ee_addr);
		eeprom_delay();
	}

	/* Terminate the EEPROM access. */
	writel(EE_Write0, ee_addr);
	writel(0, ee_addr);
	return retval;
}

/*  MII transceiver control section.
	The 83815 series has an internal transceiver, and we present the
	management registers as if they were MII connected. */

int mdio_read(int ioaddr, int phy_id, int location)
{
	if (phy_id == 1 && location < 32)
		return readl(ioaddr + 0x80 + (location<<2)) & 0xffff;
	else
		return 0xffff;
}

void mdio_write(int ioaddr, int phy_id, int location, int value)
{
	if (phy_id == 1 && location < 32)
		writew(value, ioaddr + 0x80 + (location<<2));
}

int natsemi_diag(int vendor_id, int device_id, int ioaddr, int part_idx)
{
	int i;

	if (verbose || show_regs) {
	  unsigned intr_status;

	  printf("NatSemi chip registers at %#x", ioaddr);
	  for (i = 0; i < 0x100; i += 4) {
		  if ((i & 0x1f) == 0)
			  printf("\n 0x%3.3X:", i);
		  printf(" %8.8x", inl(ioaddr + i));
	  }
	  printf("\n");
	  intr_status = inw(ioaddr + IntrStatus);
	  printf("  %snterrupt sources are pending (%4.4x).\n",
			 (intr_status & 0x03ff) ? "I": "No i", inl(ioaddr + IntrStatus));
	}

	for (i = 0; i < EEPROM_SIZE; i++)
		eeprom_contents[i] = read_eeprom(ioaddr, i);

	if (show_eeprom > 1) {
		printf("EEPROM contents:");
		for (i = 0; i < EEPROM_SIZE; i++) {
			if ((i&15) == 0)
				printf("\n0x%3.3x: ", i + 0x100);
			printf(" %2.2x", eeprom_contents[i]);
		}
		printf("\n");
	}
	if (show_mii) {
		int mii_reg;
		for (mii_reg = 0; mii_reg < 32; mii_reg++)
			printf("%s %4.4x", (mii_reg % 8) == 0 ? "\n  " : "",
				   mdio_read(ioaddr, 1, mii_reg));
		printf(".\n");
	}

	return 0;
}

#ifdef SYMBIOS_DIAG
/* The chip-specific section for the Symbios diagnostic. */

static int read_eeprom_serial(int ioaddr, int location);
static void write_eeprom_serial(int ioaddr, int index, int value);
static int read_eeprom(int ioaddr, int location);
static void write_eeprom(int ioaddr, int index, int value);
int mdio_read(int ioaddr, int phy_id, int location);
void mdio_write(int ioaddr, int phy_id, int location, int value);

#define EEPROM_SIZE 256

/* Offsets to the various registers.
   All accesses need not be longword aligned. */
enum yellowfin_offsets {
	TxCtrl=0x00, TxStatus=0x04, TxPtr=0x0C,
	TxIntrSel=0x10, TxBranchSel=0x14, TxWaitSel=0x18,
	RxCtrl=0x40, RxStatus=0x44, RxPtr=0x4C,
	RxIntrSel=0x50, RxBranchSel=0x54, RxWaitSel=0x58,
	EventStatus=0x80, IntrEnb=0x82, IntrClear=0x84, IntrStatus=0x86,
	ChipRev=0x8C, DMACtrl=0x90, Cnfg=0xA0, RxDepth=0xB8, FlowCtrl=0xBC,
	MII_Cmd=0xA6, MII_Addr=0xA8, MII_Wr_Data=0xAA, MII_Rd_Data=0xAC,
	MII_Status=0xAE,
	AddrMode=0xD0, StnAddr=0xD2, HashTbl=0xD8,
	EEStatus=0xF0, EECtrl=0xF1, EEAddr=0xF2, EERead=0xF3, EEWrite=0xF4,
	EEFeature=0xF5,
};

/* The interrupt flags for a Yellowfin. */
static const char *intr_names[16] ={
	"Rx DMA event", "Rx Illegal instruction",
	"PCI bus fault during receive", "PCI parity error during receive", 
	"Tx DMA event", "Tx Illegal instruction",
	"PCI bus fault during transmit", "PCI parity error during transmit", 
	"Early receive", "Carrier sense wakeup",
};
/* Non-interrupting events. */
const char *event_names[16] = {
	"Tx Abort", "Rx frame complete", "Transmit done",
};
/* Values read from the EEPROM, and the new image. */
unsigned char eeprom_contents[EEPROM_SIZE];
unsigned short new_ee_contents[EEPROM_SIZE];

int symbios_diag(int vendor_id, int device_id, int ioaddr, int part_idx)
{
	int i;

	if (verbose || show_regs) {
	  unsigned intr_status, event_status;

	  printf("Station address ");
	  for (i = 0; i < 5; i++)
		  printf("%2.2x:", inb(ioaddr + StnAddr + i));
	  printf("%2.2x.\n", inb(ioaddr + StnAddr + i));

	  printf("Yellowfin chip registers at %#x", ioaddr);
	  for (i = 0; i < 0x100; i += 4) {
		  if ((i & 0x1f) == 0)
			  printf("\n 0x%3.3X:", i);
		  printf(" %8.8x", inl(ioaddr + i));
	  }
	  printf("\n");
	  intr_status = inw(ioaddr + IntrStatus);
	  printf("  %snterrupt sources are pending (%4.4x).\n",
			 (intr_status & 0x03ff) ? "I": "No i", inw(ioaddr + IntrStatus));
	  if (intr_status) {
		  for (i = 0; i < 16; i++)
			  if (intr_status & (1<<i))
				  printf("   %s indication.\n", intr_names[i]);
	  }
	  event_status = inw(ioaddr + EventStatus);
	  if (event_status) {
		  for (i = 0; i < 3; i++)
			  if (event_status & (1<<i))
				  printf("   %s event.\n", event_names[i]);
	  }
	}

	if (0)
		eeprom_contents[0] = read_eeprom(ioaddr, 0);
	else
		for (i = 0; i < EEPROM_SIZE; i++)
			eeprom_contents[i] = read_eeprom(ioaddr, i+0x100);

	if (show_eeprom > 1) {
		printf("EEPROM contents:");
		for (i = 0; i < EEPROM_SIZE; i++) {
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
			if (mii_status != 0xffff  && 
				mii_status != 0x0000) {
				phys[phy_idx++] = phy;
				printf(" MII PHY found at address %d, status 0x%4.4x.\n",
					   phy, mii_status);
			}
		}
	}

	return 0;
}

/* Reading a serial EEPROM is a "bit" grungy, but we work our way through:->.*/
/* This code is a "nasty" timing loop, but PC compatible machines are
   *supposed* to delay an ISA-compatible period for the SLOW_DOWN_IO macro.  */
#define eeprom_delay()	do { int _i = 3; while (--_i > 0) { __SLOW_DOWN_IO; }} while (0)

/*  EEPROM_Ctrl bits. */
#define EE_SHIFT_CLK	0x02	/* EEPROM shift clock. */
#define EE_CS			0x00	/* EEPROM chip select. */
#define EE_DATA_WRITE	0x01	/* EEPROM chip data in. */
#define EE_WRITE_0		0x00
#define EE_WRITE_1		0x01
#define EE_DATA_READ	0x01	/* EEPROM chip data out. */
#define EE_ENB			0

/* The EEPROM commands include the alway-set leading bit. */
#define EE_WRITE_CMD	(5 << 6)
#define EE_READ_CMD		(6 << 6)
#define EE_ERASE_CMD	(7 << 6)

static int read_eeprom_serial(int ioaddr, int location)
{
	unsigned short retval = 0;
	int ee_addr = ioaddr + 0x9e;
	int ee_dir = ioaddr + 0x9f;
	int read_cmd = 0xA1;
	int i;

	outb(0xf8, ee_dir);
	/* Shift the read command bits out. */
	for (i = 8; i >= 0; i--) {
		short dataval = (read_cmd & (1 << i)) ? EE_DATA_WRITE : 0;
		outb(EE_ENB | dataval, ee_addr);
		eeprom_delay();
		printf("%d", inb(ee_addr) & 7);
		outb(EE_ENB | dataval | EE_SHIFT_CLK, ee_addr);
		eeprom_delay();
		printf("%d", inb(ee_addr) & 7);
		outb(EE_ENB | dataval, ee_addr);	/* Finish EEPROM a clock tick. */
		eeprom_delay();
	}
	outb(0xfd, ee_dir);
	printf(" ");
	
	for (i = 16; i > 0; i--) {
		outb(EE_ENB | EE_SHIFT_CLK, ee_addr);
		eeprom_delay();
		retval = (retval << 1) | ((inb(ee_addr) & EE_DATA_READ) ? 1 : 0);
		printf("%d", inb(ee_addr) & 3);
		outb(EE_ENB, ee_addr);
		eeprom_delay();
	}

	printf("\n");
	/* Terminate the EEPROM access. */
	outb(0xff, ee_dir);
	outb(EE_ENB & ~EE_CS, ee_addr);
	return retval;
}

static void write_eeprom_serial(int ioaddr, int index, int value)
{
	int i;
	int ee_addr = ioaddr + 0x9e;
	int ee_dir = ioaddr + 0x9f;
	int cmd = ((index | EE_WRITE_CMD)<< 16) | value;
	
	outb(0xfc, ee_dir);
	
	/* Shift the command bits out. */
	for (i = 26; i >= 0; i--) {
		short dataval = (cmd & (1 << i)) ? EE_DATA_WRITE : 0;
		outb(EE_ENB | dataval, ee_addr);
		eeprom_delay();
		outb(EE_ENB | dataval | EE_SHIFT_CLK, ee_addr);
		eeprom_delay();
	}
	outb(EE_ENB, ee_addr);
	
	/* Terminate the EEPROM access. */
	outb(0xff, ee_dir);
}

static int read_eeprom(int ioaddr, int location)
{
	int bogus_cnt = 1000;

	outb(location, ioaddr + EEAddr);
	outb(0x30 | ((location >> 8) & 7), ioaddr + EECtrl);
	while ((inb(ioaddr + EEStatus) & 0x80)  && --bogus_cnt > 0)
		;
	if (inb(ioaddr + EEStatus) || debug)
		printf("EEStatus is %2.2x after %d ticks, %d is %x.\n",
			   inb(ioaddr + EEStatus), 1000-bogus_cnt, location,
			   inb(ioaddr + EERead));
	return inb(ioaddr + EERead);
}

static void write_eeprom(int ioaddr, int location, int value)
{
	int bogus_cnt = 1000;

	outb(1, ioaddr + EEFeature);	/* Enable writing. */
	outb(value, ioaddr + EEWrite);
	outb(location, ioaddr + EEAddr);
	outb(0x20 | ((location >> 8) & 7), ioaddr + EECtrl);
	while ((inb(ioaddr + EEStatus) & 0x80)  && --bogus_cnt > 0)
		;
	outb(0, ioaddr + EEFeature);	/* Disable writing. */
	printf("EEStatus is %2.2x after %d ticks, %d is now %x.\n",
		   inb(ioaddr + EEStatus), 999-bogus_cnt, location,
		   read_eeprom(ioaddr, location));
	return;
}

/* MII Managemen Data I/O accesses.
   These routines assume the MDIO controller is idle, and do not exit until
   the command is finished. */

int mdio_read(int ioaddr, int phy_id, int location)
{
	int i = 10000;

	if (verbose > 2)		/* Debug: 5 */
		printf(" mdio_read(%#x, %d, %d)..", ioaddr, phy_id, location);
	outw((phy_id<<8) + location, ioaddr + MII_Addr);
	outw(1, ioaddr + MII_Cmd);
	while (--i > 0)
		if ((inw(ioaddr + MII_Status) & 1) == 0)
			break;
	return inw(ioaddr + MII_Rd_Data);
}

void mdio_write(int ioaddr, int phy_id, int location, int value)
{
	int i = 10000;

	outw((phy_id<<8) + location, ioaddr + MII_Addr);
	outw(value, ioaddr + MII_Wr_Data);

	/* Wait for the command to finish. */
	while (--i > 0)
		if ((inw(ioaddr + MII_Status) & 1) == 0)
			break;
	return;
}
#endif /* SYMBIOS_DIAG */

/*
 * Local variables:
 *  compile-command: "cc -O -Wall -o natsemi-diag natsemi-diag.c"
 *  tab-width: 4
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 */
