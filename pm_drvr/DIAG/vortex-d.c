/* vortex-diag.c: Diagnostics/EEPROM setup for the 3Com Vortex series.

   This is a diagnostic and EEPROM setup program for Ethernet adapters
   based on the 3Com Vortex, Boomerang and Cyclone chips, as used on the
   3Com 3c590/595/900/905 PCI EtherLink XL adapters.

   Copyright 1997-1999 by Donald Becker.
   This version released under the Gnu Public License, incorporated herein
   by reference.  Contact the author for use under other terms.

   This program must be compiled with "-O"!  See the bottom of this file
   for the suggested compile-command.

   The author may be reached as becker@cesdis.gsfc.nasa.gov.
   C/O USRA-CESDIS, Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771

   References

   3Com Vortex Engineering Release Specification
   3Com Boomerang modifications (unreleased)
   http://cesdis.gsfc.nasa.gov/linux/misc/NWay.html
   http://www.national.com/pf/DP/DP83840.html
*/

static char *version_msg =
"vortex-diag.c:v1.09 7/28/99 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";
static char *usage_msg =
"Usage: vortex-diag [-aBEfFsvVw] [-p <IOport>].\n";

#if !defined(__OPTIMIZE__)
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
#endif

/* No libmii.h (yet) */
extern show_mii_details(int ioaddr, int phy_id);
extern monitor_mii(int ioaddr, int phy_id);
/* No libflash.h  */
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
	{"Reset",		0, 0, 'R'},	/* Reset the transceiver. */
	{"chip-type",  1, 0, 't'},	/* Assume the specified chip type index. */
	{"test",	0, 0, 'T'},		/* Do register and SRAM test. */
	{"verbose",	0, 0, 'v'},		/* Verbose mode */
	{"version", 0, 0, 'V'},		/* Display version number */
	{"write-EEPROM", 1, 0, 'w'},/* Actually write the EEPROM with new vals */
	{ 0, 0, 0, 0 }
};

extern int vortex_diag(int vendor_id, int dev_id, int ioaddr, int part_idx);

/* Chip-specific flags. Yes, it's grungy to have the enum here. */
enum { HAS_FLASH_BUG=1, ROADRUNNER=2 };

/* This table is searched in order: place specific entries followed by
   'catch-all' general entries. */
struct pcidev_entry {
	char *proc_pci_name;
	char *part_name;
	int vendor, device, device_mask;
	int flags;
	int io_size;
	int (*diag_func)(int vendor_id, int device_id, int ioaddr, int part_num);
} pcidev_tbl[] = {
	{"3Com Vortex/Boomerag/Cyclone", "3Com Generic Vortex/Boomerag/Cyclone",
	 0x10B7, 0x9999, 0xffff, 0, 256, vortex_diag},
	{"3Com Vortex (rev ", "3Com 3c590 Vortex 10Mbps",
	 0x10B7, 0x5900, 0xffff, 0, 256, vortex_diag},
	{"3Com EISA Vortex", "3c592 EISA 10mbps Demon/Vortex",
	 0x10B7, 0x5920, 0xffff, 0, 256, vortex_diag},
	{"3Com Vortex (rev ", "3Com 3c595 Vortex 10/100baseTx",
	 0x10B7, 0x5950, 0xffff, 0, 256, vortex_diag},
	{"3Com Vortex (rev ", "3Com 3c595 Vortex 10/100baseT4",
	 0x10B7, 0x5951, 0xffff, 0, 256, vortex_diag},
	{"3Com Vortex (rev ", "3Com 3c595 Vortex 10/100baseT-MII",
	 0x10B7, 0x5952, 0xffff, 0, 256, vortex_diag},
	{"3Com EISA Vortex", "3c597 EISA Fast Demon/Vortex",
	 0x10B7, 0x5970, 0xffff, 0, 256, vortex_diag},
	{"3Com 3C900", "3c900 Boomerang 10baseT",
	 0x10B7, 0x9000, 0xffff, 0, 256, vortex_diag},
	{"3Com 3C900", "3c900 Boomerang 10Mbps Combo",
	 0x10B7, 0x9001, 0xffff, 0, 256, vortex_diag},
	{"3Com 3C900", "3c900 Cyclone 10Mbps TPO",
	 0x10B7, 0x9004, 0xffff, 0, 256, vortex_diag},
	{"3Com 3C900", "3c900 Cyclone 10Mbps Combo",
	 0x10B7, 0x9005, 0xffff, 0, 256, vortex_diag},
	{"3Com 3C900", "3c900 Cyclone 10Mbps TPC",
	 0x10B7, 0x9006, 0xffff, 0, 256, vortex_diag},
	{"3Com 3C905", "3c905 Boomerang 100baseTx",
	 0x10B7, 0x9050, 0xffff, 0, 256, vortex_diag},
	{"3Com 3C905", "3c905 Boomerang 100baseT4",
	 0x10B7, 0x9051, 0xffff, 0, 256, vortex_diag},
	{"3Com 3C905", "3c905B Cyclone 100baseTx",
	 0x10B7, 0x9055, 0xffff, HAS_FLASH_BUG, 256, vortex_diag},
	{"3Com 3C905BNC", "3c905B Cyclone 10/100/BNC",
	 0x10B7, 0x9058, 0xffff, HAS_FLASH_BUG, 256, vortex_diag},
	{"3Com 3C905B-FX", "3c905B Cyclone 100baseFx",
	 0x10B7, 0x905A, 0xffff, HAS_FLASH_BUG, 256, vortex_diag},
	{"3Com 3C905C", "3c905C Tornado 100baseTx",
	 0x10B7, 0x9200, 0xff00, HAS_FLASH_BUG, 256, vortex_diag},
	{"3Com 3C980", "3c980 Cyclone, server edition",
	 0x10B7, 0x9800, 0xffff, HAS_FLASH_BUG, 256, vortex_diag},
	{"3Com 3cSOHO100-TX", "3cSOHO100-TX Hurricane",
	 0x10B7, 0x7646, 0xff00, HAS_FLASH_BUG, 256, vortex_diag},
	{"3c555 Laptop Hurricane",	"3c555 Laptop Hurricane",
	 0x10B7, 0x5055, 0xffff, HAS_FLASH_BUG, 256, vortex_diag},
	{"3Com 3CCFE556", "3ccfe556 Roadrunner PCMCIA",
	 0x0101, 0x0556, 0xffff, ROADRUNNER, 32, vortex_diag},
	{ 0, 0, 0, 0},
};

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
static unsigned int opt_flash_show = 0;
static char	*opt_flash_dumpfile = NULL, *opt_flash_loadfile = NULL;
static int opt_dma_diag = 0;
static unsigned char new_hwaddr[6], set_hwaddr = 0;
static unsigned set_ee_rom = 0;

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

	while ((c = getopt_long(argc, argv, "#:aA:DeEfF:G:mp:PqrRst:vVwWgH:BL:S:",
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
		case 'g': opt_dma_diag++;	break;
		case 'G': opt_GPIO = strtol(optarg, NULL, 16); break;
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
		case 'P': set_ee_rom++; break;
		case 'q': if (verbose) verbose--;		 break;
		case 'r': opt_restart++;	break;
		case 'R': opt_reset++;		break;
		case 't': chip_type = atoi(optarg);	break;
		case 'v': verbose++;		break;
		case 'V': show_version++;	break;
		case 'w': do_write_eeprom++;	break;
		case 'W': opt_watch++;		break;
			/* libflash only. */
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
		perror("vortex-diag: iopl()");
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
/* A table of names to indices. */
struct { char *name; int value; } mediamap[] = {
	{ "10baseT", 0 },
	{ "10base2", 1 },
	{ "AUI", 2 },
	{ "100baseTx", 3 },
	{ "10baseT-FDX", 4 },
	{ "100baseTx-FDX", 5 },
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
	if (name)
		return atoi(name);
	fprintf(stderr, "Invalid interface specified: it must be one of\n  ");
	for (i = 0; mediamap[i].name; i++)
		fprintf(stderr, "  %s", mediamap[i].name);
	fprintf(stderr, ".\n");
	return -1;
}


/* Chip-specific section. */

/* The 3Com Vortex/Boomerang/Cyclone designs. */
static int read_eeprom(long ioaddr, int addrsize, int location);
static void write_eeprom(int ioaddr, int addrsize, int index, int value);
static int do_update(int ioaddr, int addrlen, 
					 unsigned short *ee_values, unsigned short *old_ee_values);
static void mdio_sync(int ioaddr);
int mdio_read(int ioaddr, int phy_id, int location);
void mdio_write(int ioaddr, int phy_id, int location, int value);

static void parse_eeprom(unsigned short *ee_data);
static void show_dma_state(long ioaddr);
int vortex_flash_in(long addr, int offset);
void vortex_flash_out(long addr, int offset, int val);

#define EL3WINDOW(win_num) outw(SelectWindow + (win_num), ioaddr + EL3_CMD)
#define EL3_CMD 0x0e

enum vortex_cmd {
	TotalReset = 0<<11, SelectWindow = 1<<11, StartCoax = 2<<11,
	RxDisable = 3<<11, RxEnable = 4<<11, RxReset = 5<<11,
	UpStall = 6<<11, UpUnstall = (6<<11)+1,
	DownStall = (6<<11)+2, DownUnstall = (6<<11)+3,
	RxDiscard = 8<<11, TxEnable = 9<<11, TxDisable = 10<<11, TxReset = 11<<11,
	FakeIntr = 12<<11, AckIntr = 13<<11, SetIntrEnb = 14<<11,
	SetStatusEnb = 15<<11, SetRxFilter = 16<<11, SetRxThreshold = 17<<11,
	SetTxThreshold = 18<<11, SetTxStart = 19<<11,
	StartDMAUp = 20<<11, StartDMADown = (20<<11)+1, StatsEnable = 21<<11,
	StatsDisable = 22<<11, StopCoax = 23<<11,};

/* Bits in the general status register. */
enum vortex_status {
	IntLatch = 0x0001, AdapterFailure = 0x0002, TxComplete = 0x0004,
	TxAvailable = 0x0008, RxComplete = 0x0010, RxEarly = 0x0020,
	IntReq = 0x0040, StatsFull = 0x0080,
	DMADone = 1<<8, DownComplete = 1<<9, UpComplete = 1<<10,
	DMAInProgress = 1<<11,			/* DMA controller is still busy.*/
	CmdInProgress = 1<<12,			/* EL3_CMD is still busy.*/
};
const char *intr_names[13] ={
	"Interrupt latch", "Adapter Failure", "Tx Complete", "Tx Available",
	"Rx Complete", "Rx Early Notice", "Driver Intr Request",
	"Statistics Full", "DMA Done", "Download Complete", "Upload Complete",
	"DMA in Progress", "Command in Progress",
};

enum Window0 {
	Wn0EepromCmd = 10,		/* Window 0: EEPROM command register. */
	Wn0EepromData = 12,		/* Window 0: EEPROM results register. */
	IntrStatus=0x0E,			/* Valid in all windows. */
};
#define MAX_EEPROM_SIZE 256
/* In undocumented bogusness, these value depend on the EEPROM size. */
enum Win0_EEPROM_cmds {
	EEPROM_Read = 2, EEPROM_WRITE = 1, EEPROM_ERASE = 3,
	EEPROM_EWENB = 0xC,		/* Enable erasing/writing for 10 msec. */
	EEPROM_EWDIS = 0x0,		/* Disable EWENB before 10 msec timeout. */
};

enum Window3 {			/* Window 3: MAC/config bits. */
	Wn3_Config=0, Wn3_MaxPktSz=4, Wn3_MAC_Ctrl=6, Wn3_Options=8,
};
enum Window4 {		/* Window 4: Xcvr/media bits. */
	Wn4_FIFODiag = 4, Wn4_NetDiag = 6, Wn4_PhysicalMgmt=8, Wn4_Media = 10,
};
/* Media names in Wn3_Config bits 24:20. */
const char *el_medianames[16] ={
	"10baseT", "10Mbs AUI", "undefined", "10base2",
	"100baseTX", "100baseFX", "MII", "undefined",
	"Autonegotiate", "MII-External", "undefined-10", "undefined-11",
	"undefined-12", "undefined-13", "undefined-14", "undefined-15",
};

/* Values read from the EEPROM, and the new image. */
unsigned short eeprom_contents[MAX_EEPROM_SIZE];
unsigned short new_ee_contents[MAX_EEPROM_SIZE];

int vortex_diag(int vendor_id, int dev_id, int ioaddr, int part_idx)
{
	int chip_active = 0;
	int saved_window = inw(ioaddr + EL3_CMD) >> 13;
	int internal_config;
	int eeaddrlen, eesize;
	int i;

	/* It's mostly safe to examine the registers and EEPROM during
	   operation.  But warn the user, and make then pass '-f'. */
#ifdef notdef
	if ((inl(ioaddr + EL3_CMD) & 0xE000) == 0xE000)
		chip_active = 1;
#endif

	if (opt_a) {
		int io_size = pcidev_tbl[part_idx].io_size;
		int j;

		if (!opt_f) {
			printf("The Vortex chip may be active, so FIFO registers"
				   " will not be read.\n"
				   "To see all register values use the '-f' flag.\n");
		} else
			chip_active = 0;		/* Ignore the chip status with -f */

		printf("Initial window %d, registers values by window:\n",
			   saved_window);
		for (j = 0; j < 8; j++) {
			printf("  Window %d:", j);
			outw(SelectWindow + j, ioaddr + EL3_CMD);
			for (i = 0; i < 16; i+=2) {
				if (j == 1 && i < 4 && ! opt_f)
					printf(" FIFO");
				else
					printf(" %4.4x", inw(ioaddr + i));
			}
			printf(".\n");
		}
		printf("Vortex chip registers at %#x", ioaddr);
		for (i = 0x10;
			 i < (io_size > 0x40 ? 0x40 : io_size);
			 i += 4) {
			if ((i & 0x0f) == 0)
				printf("\n  0x%3.3X:", ioaddr + i);
			if (! opt_f) {
				if (i == 0x10)
					printf(" **FIFO**");
				else if (i == 0x1c)
					printf(" *STATUS*");
				else
					printf(" %8.8x", inl(ioaddr + i));
			} else
				printf(" %8.8x", inl(ioaddr + i));
		}
		printf("\n");
	}
	if (opt_dma_diag)
		show_dma_state(ioaddr);
	EL3WINDOW(3);
	internal_config = inl(ioaddr + Wn3_Config);
	if (verbose > 1 || opt_a) {
		unsigned intr_status = inw(ioaddr + IntrStatus);
		EL3WINDOW(5);
		printf(" Indication enable is %4.4x, interrupt enable is %4.4x.\n",
			   inw(ioaddr + 12), inw(ioaddr + 10));
		printf(" %snterrupt sources are pending.\n",
			   (intr_status & 0x03ff) ? "I": "No i");
		if (intr_status) {
			for (i = 0; i < 13; i++)
				if (intr_status & (1<<i))
					printf("   %s indication.\n", intr_names[i]);
		}
		{
			const char *medias[] = {"100baseT4", "100baseTx", "100baseFx",
			"10baseT", "10base2", "AUI", "MII", "", "10baseFL"};
			int MediaOptions, MacCtrl;
			EL3WINDOW(3);
			MediaOptions = inw(ioaddr + Wn3_Options);
			printf(" Transceiver/media interfaces available: ");
			for (i = 0; i < 8; i++)
				if (MediaOptions & 1<<i)
					printf(" %s", medias[i]);
			printf("%s.\n", (MediaOptions&0xE010)==0x0010 ? "10baseFL" : "");
			printf("Transceiver type in use:  %s.\n",
				   el_medianames[(internal_config >> 20) & 15]);
			MacCtrl = inw(ioaddr + Wn3_MAC_Ctrl);
			printf(" MAC settings: %s-duplex%s%s%s.\n",
				   MacCtrl & 0x020 ? "full":"half",
				   MacCtrl & 0x040 ? ", Large packets permitted":"",
				   MacCtrl & 0x100 ? ", 802.1Q flow control":"",
				   MacCtrl & 0x200 ? ", VLT VLAN enabled":"");
			if (inw(ioaddr + Wn3_MaxPktSz) != 1514)
				printf("Maximum packet size is %d.\n",
					   inw(ioaddr + Wn3_MaxPktSz));
		}
		EL3WINDOW(2);
		printf(" Station address set to %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x.\n",
			   inb(ioaddr + 0), inb(ioaddr + 1), inb(ioaddr + 2),
			   inb(ioaddr + 3), inb(ioaddr + 4), inb(ioaddr + 5));
		/* This register only exists on certain Cyclone chips. */
		printf(" Configuration options %4.4x.\n", inw(ioaddr + 14));
	}
	/* Read the EEPROM. */
	EL3WINDOW(0);

	outw(0x5555, ioaddr + Wn0EepromData);
	if (read_eeprom(ioaddr, 6, 0) != 0x5555) {
		if (debug)
			printf(" EEPROM Address length is 6 bits (%4.4x).\n",
				   read_eeprom(ioaddr, 6, 0));
		eeaddrlen = 6;
	} else {
		int val = read_eeprom(ioaddr, 6, 0);
		int e6 = read_eeprom(ioaddr, 6, 0);
		int e8 = read_eeprom(ioaddr, 8, 0);
		if (1 || debug)
			printf(" EEPROM address sizing read returned %4.4x/%4.4x/%4.4x.\n",
				   e6, e8, val);
		eeaddrlen = (e6 != 0xffff) ? 8 : 6;
	}
	eesize = 1 << eeaddrlen;

	if (debug)
		printf(" EEPROM address size is %d bits.\n", eeaddrlen);
	for (i = 0; i < eesize; i++)
		eeprom_contents[i] = read_eeprom(ioaddr, eeaddrlen, i);

	if (set_hwaddr) {
		unsigned short sum = 0;
		memcpy(new_ee_contents, eeprom_contents, eesize << 1);
		for (i = 0; i < 3; i++)
			new_ee_contents[i + 10] = (new_hwaddr[i*2]<<8) + new_hwaddr[i*2+1];
		/* Recalculate the checksum: Cyclone only! */
		for (i = 0; i < 0x1A; i++)
			sum ^= new_ee_contents[i];
		new_ee_contents[0x20] = (sum ^ (sum>>8)) & 0xff;
		do_update(ioaddr, eeaddrlen, new_ee_contents, eeprom_contents);
		for (i = 0; i < eesize; i++)
			eeprom_contents[i] = read_eeprom(ioaddr, eeaddrlen, i);
	}

	if (set_ee_rom) {
		unsigned short sum = 0;
		memcpy(new_ee_contents, eeprom_contents, eesize << 1);
		new_ee_contents[9] = 0x3001;
		/* Recalculate the checksum: Cyclone only! */
		for (i = 0; i < 0x1B; i++)
			sum ^= new_ee_contents[i];
		new_ee_contents[0x20] = (sum ^ (sum>>8)) & 0xff;
		printf("Setting the EEPROM BIOS ROM field to %4.4x, new checksum "
			   "%2.2x.\n", new_ee_contents[9], new_ee_contents[0x20]);
		do_update(ioaddr, eeaddrlen, new_ee_contents, eeprom_contents);
		for (i = 0; i < eesize; i++)
			eeprom_contents[i] = read_eeprom(ioaddr, eeaddrlen, i);
	}
	if (verbose + show_eeprom > 2) {
		unsigned short sum = 0;
		printf("EEPROM contents (%d words):", eesize);
		for (i = 0; i < eesize; i++) {
			if (i % 8 == 0)
				printf("\n 0x%3.3x:", i);
			printf(" %4.4x", eeprom_contents[i]);
			sum += eeprom_contents[i];
		}
		printf("\n The word-wide EEPROM checksum is %#4.4x.\n", sum);
	}

	/* The user will usually want to see the interpreted EEPROM contents. */
	if (verbose > 1 || show_eeprom) {
		parse_eeprom(eeprom_contents);
	}

	/* Show up to four (not just the on-board) PHYs. */
	if (verbose > 1 || show_mii) {
		int phys[4], phy, phy_idx = 0;
		int mii_reg;
		int saved_media_options;
		phys[0] = 24;			/* Default for most 3Com products. */
		EL3WINDOW(3);
		/* Turn on the MII transceiver for some cards. */
		saved_media_options = inw(ioaddr + Wn3_Options);
		outw((saved_media_options & 0x1ff) | 0x8000, ioaddr + Wn3_Options);

		EL3WINDOW(4);
		mdio_sync(ioaddr);
		for (phy = 1; phy <= 32 && phy_idx < 4; phy++) {
			int mii_status = mdio_read(ioaddr, phy & 0x1f, 1);
			if (mii_status != 0xffff  &&  mii_status != 0) {
				phys[phy_idx++] = phy&0x1f;
				printf(" MII PHY found at address %d, status %4.4x.\n",
					   phy & 0x1f, mii_status);
			}
		}
		if (phy_idx == 0)
			printf(" ***WARNING***: No MII transceivers found!\n");
		for (phy = 0; phy < phy_idx; phy++) {
			printf(" MII PHY %d at #%d transceiver registers:",
				   phy, phys[phy]);
			for (mii_reg = 0; mii_reg < 32; mii_reg++)
				printf("%s %4.4x", (mii_reg % 8) == 0 ? "\n  " : "",
					   mdio_read(ioaddr, phys[phy], mii_reg));
			printf(".\n");
		}
		if (opt_reset) {
			printf("Resetting the transceiver...\n");
			mdio_write(ioaddr, phys[phy], 0, 0x8000);
		}
		if (phy_idx  &&  nway_advertise > 0) {
			printf(" Setting the media capability advertisement register of "
				   "PHY #%d to 0x%4.4x.\n", phys[0], nway_advertise | 1);
			mdio_write(ioaddr, phys[0], 4, nway_advertise | 1);
		}
		if (opt_restart) {
			printf("Restarting negotiation...\n");
			mdio_write(ioaddr, phys[0], 0, 0x0000);
			mdio_write(ioaddr, phys[0], 0, 0x1200);
		}
		/* To force 100baseTx-HD do  mdio_write(ioaddr, phys[0], 0, 0x2000); */
		if (fixed_speed >= 0) {
			int reg0_val = 0;
			reg0_val |= (fixed_speed & 0x0180) ? 0x2000 : 0;
			reg0_val |= (fixed_speed & 0x0140) ? 0x0100 : 0;
			printf("Setting the speed to \"fixed\", %4.4x.\n", reg0_val);
			mdio_write(ioaddr, phys[0], 0, reg0_val);
		}

#ifdef LIBMII
		if (show_mii > 1)
			show_mii_details(ioaddr, phys[0]);
		if (opt_watch || show_mii > 2)
			monitor_mii(ioaddr, phys[0]);
#endif
		EL3WINDOW(3);
		outw(saved_media_options, ioaddr + Wn3_Options);
	}

#ifdef LIBFLASH
	/* First, work around a chip buglet: the media must be set to EXT MII
	   to read the flash on one Hurricane rev. */
	/* Turn on the MII transceiver for some cards. */
	{
		if (pcidev_tbl[part_idx].flags & HAS_FLASH_BUG) {
			EL3WINDOW(3);
			outl((internal_config & ~0x00f00000)|0x00600000,
				 ioaddr + Wn3_Config);
		}
		EL3WINDOW(0);
		flash_in_hook = vortex_flash_in;
		flash_out_hook = vortex_flash_out;
		if (opt_flash_show)
			flash_show(ioaddr, 0);
		if (opt_flash_dumpfile)
			if (flash_dump(ioaddr, 0, opt_flash_dumpfile) < 0) {
				fprintf(stderr, "Failed to save the old Flash BootROM image "
						"into file '%s'.\n", opt_flash_dumpfile);
				EL3WINDOW(3);
				outl(internal_config, ioaddr + Wn3_Config);
				EL3WINDOW(saved_window);
				return 3;
			}
		if (opt_flash_loadfile)
			if (flash_program(ioaddr, 0, opt_flash_loadfile) < 0) {
				fprintf(stderr, "Failed to load the new Flash BootROM image "
						"from file '%s'.\n", opt_flash_loadfile);
				EL3WINDOW(3);
				outl(internal_config, ioaddr + Wn3_Config);
				EL3WINDOW(saved_window);
				return 4;
			}
		if (pcidev_tbl[part_idx].flags & HAS_FLASH_BUG) {
			EL3WINDOW(3);
			outl(internal_config, ioaddr + Wn3_Config);
		}
	}
#else
	if (opt_flash_loadfile  || opt_flash_dumpfile  ||  opt_flash_show)
		printf("Flash operations not configured into this program.\n");
#endif

	if (do_test) {
		int bogus_cnt = 10000;
		/* Run the built-in-self-test of the SRAM buffer.  This is
		   valid only on the boomerang, but we run it unconditionally. */
		EL3WINDOW(4);
		outw(0x0004, ioaddr + Wn4_FIFODiag);
		while ((inw(ioaddr + Wn4_FIFODiag) & 0x0020) == 0  &&  --bogus_cnt > 0)
			inw(ioaddr + Wn4_FIFODiag);
		if (inw(ioaddr + Wn4_FIFODiag) & 0x0020)
			printf("SRAM buffer test %s\n",
				   inw(ioaddr + Wn4_FIFODiag) & 0x0010 ? "failed!" : "passed");
		else
			printf("INTERNAL FAILURE -- SRAM buffer test did not complete!\n");
	}
	EL3WINDOW(saved_window);
	return 0;
}


/* Serial EEPROM section. */
static int read_eeprom(long ioaddr, int addrlen, int location)
{
	int timer;

	outw((2 << addrlen) + location, ioaddr + Wn0EepromCmd);
 	/* Wait for the read to take place, worst-case 162 us. */
	for (timer = 1620; timer >= 0; timer--) {
		if ((inw(ioaddr + Wn0EepromCmd) & 0x8000) == 0)
			break;
	}
	if (debug > 2)
		fprintf(stderr, "  EEPROM read completed in %d ticks, %4.4x.\n",
				1620-timer, inw(ioaddr + Wn0EepromData));
	return inw(ioaddr + Wn0EepromData);
}

static void write_eeprom(int ioaddr, int addrlen, int index, int value)
{
	int timer;
	/* Verify that the EEPROM is idle. */
	for (timer = 1620; inw(ioaddr + Wn0EepromCmd) & 0x8000;)
		if (--timer < 0)
			goto error_return;
	/* Enable writing: EEPROM_EWENB | 110000.... */
	outw(3 << (addrlen-2), ioaddr + Wn0EepromCmd);
	for (timer = 400; inw(ioaddr + Wn0EepromCmd) & 0x8000;) {
		if (--timer < 0)
			goto error_return;
	}
	if (debug)
		fprintf(stderr, "EEPROM write enable took %d ticks!\n", 400 - timer);
	outw((EEPROM_ERASE << addrlen) + index, ioaddr + Wn0EepromCmd);
	for (timer = 16000; inw(ioaddr + Wn0EepromCmd) & 0x8000;)
		if (--timer < 0) {
			fprintf(stderr, "EEPROM failed to erase index %d!\n", index);
			return;
		}
	if (debug)
		fprintf(stderr, "EEPROM erased index %d after %d ticks!\n",
				index, 16000-timer);
	outw(3 << (addrlen-2), ioaddr + Wn0EepromCmd);
	for (timer = 400; inw(ioaddr + Wn0EepromCmd) & 0x8000;) {
		if (--timer < 0)
			goto error_return;
	}
	if (debug)
		fprintf(stderr, "EEPROM write enable took %d ticks!\n", 400-timer);
	outw(value, ioaddr + Wn0EepromData);
	outw((EEPROM_WRITE << addrlen) + index, ioaddr + Wn0EepromCmd);
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

static int do_update(int ioaddr, int addrlen, 
					 unsigned short *ee_values, unsigned short *old_ee_values)
{
	int i;

	EL3WINDOW(0);
	if (debug)
		printf("Comparing %d words (address length %d) in do_update().\n",
			   1<<addrlen, addrlen);
	for (i = 0; i < (1<<addrlen); i++) {
		if (ee_values[i] != old_ee_values[i]) {
			if (do_write_eeprom) {
				if (verbose)
					printf("Writing an EEPROM word offset %d value 0x%4.4x.\n",
						   i, ee_values[i]);
				write_eeprom(ioaddr, addrlen, i, ee_values[i]);
			} else
				printf(" Would write new %d entry 0x%4.4x (old value 0x%4.4x).\n", 
					   i, ee_values[i], old_ee_values[i]);
		}
	}
	outw(EEPROM_EWDIS << addrlen, ioaddr + Wn0EepromCmd);
	for (i = 400; i > 0 && inw(ioaddr + Wn0EepromCmd) & 0x8000; i--)
		;
	return 0;
}


/* Read and write the MII registers using software-generated serial
   MDIO protocol.  It is just different enough from the EEPROM protocol
   to not share code.  The maxium data clock rate is 2.5 Mhz, which is met
   by the PCI I/O access timing. */
#define mdio_delay(mdio_addr)	inw(mdio_addr)

#define MDIO_SHIFT_CLK	0x01
#define MDIO_DIR_WRITE	0x04
#define MDIO_DATA_WRITE0 (0x00 | MDIO_DIR_WRITE)
#define MDIO_DATA_WRITE1 (0x02 | MDIO_DIR_WRITE)
#define MDIO_DATA_READ	0x02
#define MDIO_ENB_IN		0x00

static int mii_preamble_required = 1;
static void mdio_sync(int ioaddr)
{
	int mdio_addr = ioaddr + Wn4_PhysicalMgmt;
	int i;
	/* Establish sync by sending at least 32 logic ones. */ 
	for (i = 32; i >= 0; i--) {
		outw(MDIO_DATA_WRITE1, mdio_addr);
		mdio_delay(mdio_addr);
		outw(MDIO_DATA_WRITE1 | MDIO_SHIFT_CLK, mdio_addr);
		mdio_delay(mdio_addr);
	}
}
int mdio_read(int ioaddr, int phy_id, int location)
{
	int i;
	int read_cmd = (0xf6 << 10) | (phy_id << 5) | location;
	unsigned int retval = 0;
	int mdio_addr = ioaddr + Wn4_PhysicalMgmt;

	if (verbose > 2)		/* Debug: 5 */
		printf(" mdio_read(%#x, %d, %d)..", ioaddr, phy_id, location);
	if (mii_preamble_required)
		mdio_sync(ioaddr);
	/* Shift the read command bits out. */
	for (i = 14; i >= 0; i--) {
		int dataval = (read_cmd&(1<<i)) ? MDIO_DATA_WRITE1 : MDIO_DATA_WRITE0;
		if (verbose > 3)		/* Debug: 5 */
			printf("%d", (read_cmd & (1 << i)) ? 1 : 0);

		outw(dataval, mdio_addr);
		mdio_delay(mdio_addr);
		outw(dataval | MDIO_SHIFT_CLK, mdio_addr);
		if (verbose > 3) printf(" %x", (inw(mdio_addr) >> 16) & 0x0f);
		mdio_delay(mdio_addr);
	}
	if (verbose > 3) printf("-> %x", (inw(mdio_addr) >> 16) & 0x0f);

	if (verbose > 3)		/* Debug: 5 */
		printf(" \n");

	/* Read the two transition, 16 data, and wire-idle bits. */
	for (i = 19; i > 0; i--) {
		outw(MDIO_ENB_IN, mdio_addr);
		mdio_delay(mdio_addr);
		retval = (retval << 1) | ((inw(mdio_addr) & MDIO_DATA_READ) ? 1 : 0);
		outw(MDIO_ENB_IN | MDIO_SHIFT_CLK, mdio_addr);
		mdio_delay(mdio_addr);
		if (verbose > 3) printf(" %x", (inw(mdio_addr) >> 16) & 0x0f);
	}
	if (verbose > 2)		/* Debug: 5 */
		printf(" %4.4x.\n", retval);

	/* Avoid buggy 3c905B MDIO implementation. */
	return (retval & 0x20000) == 0x00000 ? retval>>1 & 0x1ffff : 0;
}

void mdio_write(int ioaddr, int phy_id, int location, int value)
{
	int write_cmd = 0x50020000 | (phy_id << 23) | (location << 18) | value;
	int mdio_addr = ioaddr + Wn4_PhysicalMgmt;
	int i;

	if (verbose > 2)		/* Debug: 5 */
		printf(" mdio_write(%#x, %d, %d, %4.4x)..",
			   ioaddr, phy_id, location, value);
	if (mii_preamble_required)
		mdio_sync(ioaddr);

	/* Shift the command bits out. */
	for (i = 31; i >= 0; i--) {
		int dataval = (write_cmd&(1<<i)) ? MDIO_DATA_WRITE1 : MDIO_DATA_WRITE0;
		outw(dataval, mdio_addr);
		mdio_delay(mdio_addr);
		outw(dataval | MDIO_SHIFT_CLK, mdio_addr);
		mdio_delay(mdio_addr);
	}
	/* Leave the interface idle. */
	for (i = 1; i >= 0; i--) {
		outw(MDIO_ENB_IN, mdio_addr);
		mdio_delay(mdio_addr);
		outw(MDIO_ENB_IN | MDIO_SHIFT_CLK, mdio_addr);
		mdio_delay(mdio_addr);
	}

	return;
}

int vortex_flash_in(long addr, int offset)
{
	outl(offset, addr + 4);
	return inb(addr + 8) & 0xff;
}
void vortex_flash_out(long addr, int offset, int val)
{
	outl(offset, addr + 4);
	outb(val, addr + 8);
}


static void parse_eeprom(unsigned short *eeprom)
{
	unsigned char *p = (void *)eeprom;
	u16 *ee = eeprom;
	int i, sum = 0;

	printf("Parsing the EEPROM of a 3Com Vortex/Boomerang:\n");
	if (eeprom[0] == 0x10b7) {
		printf(" The CardBus product ID is %4.4x %4.4x.\n",
			   eeprom[0], eeprom[1]);
		p += 0x60;
		ee += 0x30;
	}
	printf(" 3Com Node Address ");
	for (i = 0; i < 5; i++)
		printf("%2.2X:", p[i^1]);
	printf("%2.2X (used as a unique ID only).\n", p[i^1]);
	printf(" OEM Station address %2.2x", p[1 + 20]);
	for (i = 1; i < 6; i++)
		printf(":%2.2X", p[(i^1) + 20]);
	printf(" (used as the ethernet address).\n");
	printf(" Manufacture date (MM/DD/YYYY) %d/%d/%d, division %c,"
		   " product %c%c.\n", ((p[8]>>5) & 7) + ((p[9]<<3) & 8),
		   p[8] & 31, (p[9]>>1) + 1900, p[10], p[12], p[13]);
	printf("Options: %s.\n",
		   ee[13] & 0x8000 ? "force full-duplex" : "none");
	for (i = 0; i < 0x16; i++)
		sum ^= ee[i];
	printf("  Vortex format checksum is %scorrect (%4.4x vs. %4.4x).\n",
		   ((sum ^ (sum>>8)) & 0xff) == ee[0x17] ? "" : "in",
		   (sum ^ (sum>>8)) & 0xff, ee[0x17]);
	for ( ; i < 0x1A; i++)
		sum ^= ee[i];
	sum ^= sum>>8;
	printf("  Cyclone format checksum is %scorrect (%#2.2x vs. %#2.2x).\n",
		   (sum  & 0xff) == (ee[0x20] & 0xff) ? "" : "in",
		   sum & 0xff, ee[0x20] & 0xff);
	for (sum = 0, i = 0; i < 0x20*2; i++)
		sum ^= ((unsigned char *)ee)[i];
	printf("  Hurricane format checksum is %scorrect (%#2.2x vs. %#2.2x).\n",
		   sum == (ee[0x20] & 0xff) ? "" : "in",
		   sum, ee[0x20] & 0xff);
	return;
}

static void show_dma_state(long ioaddr)
{
	int dma_ctrl = inl(ioaddr + 0x20);
	printf("  DMA control register is %8.8x.\n", dma_ctrl);
	if (dma_ctrl & 0x0080) {
		outw(DownStall, ioaddr + EL3_CMD);
		printf("   DMA control register is %8.8x (during Tx Stall).\n",
			   inl(ioaddr + 0x20));
		outw(DownUnstall, ioaddr + EL3_CMD);
	}
	printf("   Tx list starts at %8.8x.\n",
		   inl(ioaddr + 0x24));
	printf("   Tx FIFO thresholds: min. burst %d bytes, "
		   "priority with %d bytes to empty.\n",
		   inb(ioaddr + 0x2A)<<5, inl(ioaddr + 0x2C)<<5);
	printf("   Tx poll period %d0 ns.\n",
		   inb(ioaddr + 0x2D)<<5);
	printf("   Tx maximum burst recorded %d.\n",
		   inw(ioaddr + 0x78));
}


/*
 * Local variables:
 *  compile-command: "cc -O -Wall -o vortex-diag vortex-diag.c -DLIBMII libmii.c"
 *  alt-compile-command: "cc -O -Wall -o vortex-diag vortex-diag.c"
 *  flash-compile-command: "cc -O -Wall -o vortex-diag vortex-diag.c -DLIBMII libmii.c  -DLIBFLASH libflash.c"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
