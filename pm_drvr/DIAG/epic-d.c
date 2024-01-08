/* epic-diag.c: Diagnostics and EEPROM setup program for the SMC EPIC-100 chip.

   This is a diagnostic and EEPROM setup program for Ethernet adapters
   based on the SMC83C170 series EPIC/100 chip, as used on the SMC EtherPowerII
   boards.

   Copyright 1997-1999 by Donald Becker.
   This version released under the Gnu Public License, incorporated herein
   by reference.  Contact the author for use under other terms.

   This program must be compiled with "-O"!  See the bottom of this file
   for the suggested compile-command.

   The author may be reached as becker@cesdis.gsfc.nasa.gov.
   C/O USRA-CESDIS, Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771

   References

   http://www.smsc.com/main/datasheets/83c171.pdf
   http://www.smsc.com/main/datasheets/83c175.pdf
   http://cesdis.gsfc.nasa.gov/linux/misc/NWay.html
   http://www.national.com/pf/DP/DP83840A.html
*/

static char *version_msg =
"epic-diag.c:v1.07 10/14/99 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";
static char *usage_msg =
"Usage: epic-diag [-aeEfFmsvVw] [-p <IOport>].\n";

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
#if defined(__linux__)  &&  __GNU_LIBRARY__ == 1
#include <asm/io.h>			/* Newer libraries use <sys/io.h> instead. */
#else
#include <sys/io.h>
#endif

/* No libmii.h or libflash.h yet. */
extern show_mii_details(long ioaddr, int phy_id);
extern monitor_mii(long ioaddr, int phy_id);

extern int flash_show(long addr_ioaddr, long data_ioaddr);
extern int flash_dump(long addr_ioaddr, long data_ioaddr, char *filename);
extern int flash_program(long addr_ioaddr, long data_ioaddr, char *filename);
extern int (*flash_in_hook)(long addr, int offset);
extern void (*flash_out_hook)(long addr, int offset, int val);

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
	{"reset",	0, 0, 'R'},		/* Reset chip. */
	{"chip-type",  1, 0, 't'},	/* Assume the specified chip type index. */
	{"test",	0, 0, 'T'},		/* Do register and SRAM test. */
	{"verbose",	0, 0, 'v'},		/* Verbose mode */
	{"version", 0, 0, 'V'},		/* Display version number */
	{"write-EEPROM", 1, 0, 'w'},/* Actually write the EEPROM with new vals */
	{ 0, 0, 0, 0 }
};

extern int    epic_diag(int vend_id, int dev_id, int ioaddr, int part_idx);

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
	{" SMC 9432 TX", "SMSC EPIC/100 83c170", 0x10B8, 0x0005, 0xffffffff,
	 0, 32, epic_diag},
	{"SMSC EPIC/C 83c175", "SMSC EPIC/C 83c175", 0x10B8, 0x0006, 0xffffffff,
	 0, 32, epic_diag},
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
#if defined(LIBFLASH)		/* Valid with libflash only. */
static unsigned int opt_flash_show = 0;
static char	*opt_flash_dumpfile = NULL, *opt_flash_loadfile = NULL;
#endif
static unsigned char new_hwaddr[6], set_hwaddr = 0;

static int scan_proc_pci(int card_num);
static int parse_media_type(const char *capabilities);
static int get_media_index(const char *name);
/* Other chip-specific options. */
static int LED_setting = -1;


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
	while (fgets(buffer, sizeof(buffer)-2, fp)) {
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

/* Offsets to registers, using the (ugh) SMC names. */
enum epic_registers {
  COMMAND=0, IntrStatus=4, INTMASK=8, GENCTL=0x0C, NVCTL=0x10, EECTL=0x14,
  TEST1=0x1C, CRCCNT=0x20, ALICNT=0x24, MPCNT=0x28,	/* Rx error counters. */
  MIICtrl=0x30, MIIData=0x34, MIICfg=0x38,
  LAN0=64,						/* MAC address. */
  MC0=80,						/* Multicast filter table. */
  RxCtrl=96, TxCtrl=0x70, TxSTAT=0x74,
  PRxCDAR=0x84, RxSTAT=0xA4, PTxCDAR=0xC4, TxThresh=0xDC,
};

/* Interrupt register bits, using my own meaningful names. */
enum IntrStatus {
  TxIdle=0x40000, RxIdle=0x20000,
  CntFull=0x0200, TxUnderrun=0x0100,
  TxEmpty=0x0080, TxDone=0x0020, RxError=0x0010,
  RxOverflow=0x0008, RxFull=0x0004, RxHeader=0x0002, RxDone=0x0001,
};

static int read_eeprom(long ioaddr, int location, int addr_len);
static int do_eeprom_cmd(long ioaddr, int cmd, int cmd_len);
static void write_eeprom(long ioaddr, int index, int value, int addr_len);
static int do_update(long ioaddr, int index, int addr_len,
					 const char *field_name, int old_value, int new_value);
int mdio_read(long ioaddr, int phy_id, int location);
void mdio_write(long ioaddr, int phy_id, int location, int value);
static void parse_eeprom(unsigned short *ee_data);


/* The interrupt flags. */
const char *intr_names[28] ={
	"Rx Copy Done", "Rx Header Done", "Rx Queue Empty", "Rx Buffer Overflow",
	"Rx CRC error", "Tx done", "Tx chain done", "Tx Queue empty",
	"Tx underrun", "Counter overflow", "Rx almost done",
	"Rx threshold crossed", "Fatal Interrupt summary", "PCI master abort",
	"PCI target abort", "PHY event",
	"Interrupt active", "Rx idle", "Tx idle",
	"Rx copy in progress", "Tx copy in progress", "Rx buffers empty",
	"Early Rx threshold passed", "Rx status valid", "PCI Data Parity Error",
	"PCI Address Parity Error", "PCI Master abort", "PCI Target Abort",
};
/* Non-interrupting events. */
const char *event_names[16] = {
	"Tx Abort", "Rx frame complete", "Transmit done",
};

#define EEPROM_BUF_SIZE 256

/* Last-hope recovery major boo-boos: rewrite the EEPROM with the values
   from my card (and hope I don't met you on the net...). */
unsigned short djb_epic_eeprom[EEPROM_BUF_SIZE] = {
  /* Currently invalid! */ }; 

/* Values read from the EEPROM, and the new image. */
unsigned short eeprom_contents[EEPROM_BUF_SIZE];
unsigned short new_ee_contents[EEPROM_BUF_SIZE];
#define EE_READ_CMD		(6)
#define EEPROM_SA_OFFSET	0x00

int epic_diag(int vend_id, int dev_id, int ioaddr, int part_idx)
{
	int chip_active = 0, chip_lowpower = 0;
	unsigned rx_ctrl = inl(ioaddr + RxCtrl);
	int eeprom_size, eeprom_addr_size;
	int i;

	/* It's mostly safe to examine the registers and EEPROM during
	   operation.  But warn the user, and make then pass '-f'. */
	if (opt_reset) {
		outl(0x201, ioaddr + GENCTL);
		printf("Resetting the EPIC.\n");
	} else if (inl(ioaddr + GENCTL) & 0x0008) {
		chip_lowpower = 1;
		outl(inl(ioaddr + GENCTL) & ~8, ioaddr + GENCTL); /* Wake up chip. */
	} else if ((rx_ctrl & 0x003F) != 0x0000)
		chip_active = 1;

	if (opt_restart) {
		unsigned genctrl = inl(ioaddr + GENCTL);
#if 0
		outl(0xd823, ioaddr + NVCTL);
#else
		outl(0x10, ioaddr + MIICfg);
		outl(genctrl | 0x4000, ioaddr + GENCTL);
		sleep(1);
		outl(genctrl & ~0x4000, ioaddr + GENCTL);
#endif
	}

	/* Undocumented bit that must be set for the chip to work. */
	outl(0x8, ioaddr + TEST1);

	/* Set the general purpose output LED lines.
	   Oh, and we reset the modem too. */
	if (LED_setting >= 0) {
		unsigned nvctl = inl(ioaddr + NVCTL) & ~0x70;
		/* Enable the modem as well. */
		outl(((LED_setting<<4) & 0x30) | nvctl | 0x0040, ioaddr + NVCTL);
		outl(0x4C00, ioaddr + 0x80); /* Toggle the modem reset line. */
		printf("Resetting the modem.\n");
		outl(0x4C80, ioaddr + 0x80);
		outl(0x0020, ioaddr + 0x8C); /* Enable Binary Audio */
		outl(((LED_setting<<4) & 0x30) | nvctl, ioaddr + NVCTL);
	}

	if (verbose || show_regs) {
		unsigned intr_status;

		if (chip_active && !opt_f) {
		  printf("The EPIC/100 chip appears to be active, so some registers"
				 " will not be read.\n"
				 "To see all register values use the '-f' flag.\n");
		} else
			chip_active = 0;		/* Ignore the chip status with -f */

		if (opt_a > 1) {
			/* Reading some registers hoses the chip operation. */
			char dont_read[8] = {0x00, 0x00, 0x00, 0xce,
								 0xfd, 0xed, 0x7d, 0xff};
			printf("EPIC chip registers at %#x", (int) ioaddr);
			for (i = 0; i < 0x100; i += 4) {
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
		printf(" %snterrupt sources are pending.\n",
			   (intr_status & inw(ioaddr + INTMASK)) ? "I": "No i");
		if (intr_status) {
		  for (i = 0; i < sizeof(intr_names)/sizeof(intr_names[0]); i++)
			  if (intr_status & (1<<i))
				  printf("   %s indication.\n", intr_names[i]);
		}
		printf(" Rx state is '%s', Tx state is '%s'.\n",
			   inl(ioaddr + COMMAND)&8 ? "Running" : "Stopped",
			   inl(ioaddr + COMMAND)&4 ? "Running" : "Idle");
		{
			unsigned tx_ctrl = inl(ioaddr + TxCtrl);
			unsigned last_tx = inl(ioaddr + TxSTAT);
			unsigned last_rx = inl(ioaddr + 0x64);
			static unsigned char *tx_mode[4] = {
				"half-duplex", "internal loopback",
				"external loopback", "full-duplex"};
			printf("  Transmitter: slot time %d bits, %s mode.\n",
				   ((tx_ctrl & 0xF8) + 8) << 2, tx_mode[(tx_ctrl>>1) & 3]);
			printf("  Last transmit %s, %d collisions%s.\n",
				   last_tx & 1 ? "OK" : "FAILED!!",
				   (last_tx>>8) & 0x1f,
				   last_tx & 0x80 ? ", currently deferring to traffic!":"");
			printf("  Receiver control is %4.4x, %s mode.\n", rx_ctrl,
				   rx_ctrl & 0x40 ? "monitor only" :
				   rx_ctrl & 0x20 ? "promiscuous" :
				   rx_ctrl & 8 ? "multicast" : 
				   "normal");
			printf("  The last Rx frame was %d bytes, status %x%s%s%s%s%s%s%s.\n",
				   inl(ioaddr + 0x68), last_rx,
				   last_rx&0x40 ? ", Rx disabled" : "",
				   last_rx&0x20 ? ", broadcast" : "",
				   last_rx&0x10 ? ", multicast" : "",
				   last_rx&0x08 ? ", Missed/Overflow" : "",
				   last_rx&0x04 ? ", CRC Error!" : "",
				   last_rx&0x02 ? ", Alignment Error!" : "",
				   last_rx&0x02 ? " received OK." : "" );
		}
	}

	if (opt_GPIO) {
		printf("Setting the GPIO register %8.8x.\n", opt_GPIO);
		outl(opt_GPIO, ioaddr + NVCTL);
	}

	/* Read the EEPROM. */
	/* Bring the chip out of low-power mode. */
	outl(0x4200, ioaddr + GENCTL);
	{
		int size_test = do_eeprom_cmd(ioaddr, (EE_READ_CMD << 8) << 16, 27);
		eeprom_addr_size = (size_test & 0xffe0000) == 0xffe0000 ? 8 : 6;
		eeprom_size = 1 << eeprom_addr_size;
		if (debug)
			printf("EEPROM size probe returned %#x, %d bit address.\n",
				   size_test, eeprom_addr_size);
	}
	for (i = 0; i < 64 /* Not eeprom_size! */; i++)
		eeprom_contents[i] = read_eeprom(ioaddr, i, eeprom_addr_size);

	if (set_hwaddr) {
		unsigned char sum = 0;
		const char *const field_names[] = {
			"MAC address 0/1", "MAC address 2/3", "MAC address 4/5",
			"Checksum/Board-ID"};
		memcpy(new_ee_contents, eeprom_contents, eeprom_size << 1);
		for (i = 0; i < 3; i++) {
			new_ee_contents[i + EEPROM_SA_OFFSET] =
				new_hwaddr[i*2] + (new_hwaddr[i*2+1]<<8);
			sum += new_hwaddr[i*2] + new_hwaddr[i*2+1];
		}
		sum += new_ee_contents[i]; 		/* Board ID, implicit truncation! */
		printf("Writing new MAC station address, checksum %2.2x.\n", sum);
		/* Explicit bit ops to avoid endian issues. */
		new_ee_contents[i] = (new_ee_contents[i] & 0xff) |  (-sum << 8);
		if (debug)
			printf("new_ee_contents[%d] is %4.4x.\n", i, new_ee_contents[i]);
		for (i = EEPROM_SA_OFFSET; i < EEPROM_SA_OFFSET + 5; i++)
			if (new_ee_contents[i] != eeprom_contents[i])
				do_update(ioaddr, i, eeprom_addr_size, field_names[i],
						  eeprom_contents[i], new_ee_contents[i]);
		/* Re-read all contents. */
		for (i = 0; i < eeprom_size; i++)
			eeprom_contents[i] = read_eeprom(ioaddr, i, eeprom_addr_size);
	}

	if (show_eeprom > 1) {
		unsigned short sum = 0;
		printf("EEPROM contents (size: %s):",
			   inl(ioaddr + EECTL) & 0x40 ? "64x16" : "256x16");
		for (i = 0; i < eeprom_size; i++) {
			printf("%s %4.4x", (i & 7) == 0 ? "\n " : "", eeprom_contents[i]);
			sum += eeprom_contents[i];
		}
		printf("\n The word-wide EEPROM checksum is %#4.4x.\n", sum);
	}
	/* Show the interpreted EEPROM contents. */
	if (verbose > 1 || show_eeprom)
		parse_eeprom(eeprom_contents);

	/* Show up to four (not just the on-board) PHYs. */
	if (verbose > 1 || show_mii) {
		int phys[4], phy, phy_idx = 0;
		int mii_reg;
		outl(0x0200, ioaddr + GENCTL);
		outl((inl(ioaddr + NVCTL) & ~0x003C) | 0x4800, ioaddr + NVCTL);

		phys[0] = 3;			/* Default MII address on SMC card. */
		for (phy = 1; phy < 32 && phy_idx < 4; phy++) {
			int mii_status = mdio_read(ioaddr, phy, 0);
			if (mii_status != 0xffff  && mii_status != 0x0000) {
				phys[phy_idx++] = phy;
				printf(" MII PHY found at address %d.\n", phy);
			}
		}
		if (phy_idx == 0) {
			int mii_status = mdio_read(ioaddr, 0, 1);
			if (mii_status != 0xffff  &&  mii_status != 0x0000)
				phys[phy_idx++] = 0;
			else 
				printf(" ***WARNING***: No MII transceivers found!\n");
		}
		for (phy = 0; phy < phy_idx; phy++) {
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

	if (chip_lowpower)
		outl(0x0008, ioaddr + GENCTL);
	return 0;
}


/* Serial EEPROM section. */

/* The new-style bit definitions. */
enum EEPROM_Ctrl_Bits {
	EE_ShiftClk=0x04, EE_ChipSelect=0x02, EE_DataOut=0x08, EE_DataIn=0x10,
	EE_Enb=0x03, EE_Write0=0x03, EE_Write1=0x0B,
};
/* The EEPROM commands include the always-set leading bit.
   They must be shifted by the number of address bits. */
enum EEPROM_Cmds { EE_WriteCmd=5, EE_ReadCmd=6, EE_EraseCmd=7, };

/*  EEPROM_Ctrl bits. */
#define EE_SHIFT_CLK	0x04	/* EEPROM shift clock. */
#define EE_CS			0x02	/* EEPROM chip select. */
#define EE_DATA_WRITE	0x08	/* EEPROM chip data in. */
#define EE_WRITE_0		0x03
#define EE_WRITE_1		0x0B
#define EE_DATA_READ	0x10	/* EEPROM chip data out. */
#define EE_ENB			(0x0001 | EE_CS)
#define EE_OFFSET		EECTL

/* Delay between EEPROM clock transitions. */
#define eeprom_delay(ee_addr)	inl(ee_addr)

/* This executes a generic EEPROM command, typically a write or write enable.
   It returns the data output from the EEPROM, and thus may also be used for
   reads. */
static int do_eeprom_cmd(long ioaddr, int cmd, int cmd_len)
{
	unsigned retval = 0;
	long ee_addr = ioaddr + EE_OFFSET;

	if (debug > 1)
		printf(" EEPROM op 0x%x: ", cmd);

	outl(EE_ENB | EE_SHIFT_CLK, ee_addr);

	/* Shift the command bits out. */
	do {
		short dataval = (cmd & (1 << cmd_len)) ? EE_WRITE_1 : EE_WRITE_0;
		outl(dataval, ee_addr);
		eeprom_delay(ee_addr);
		if (debug > 2)
			printf("%X", inl(ee_addr) & 15);
		outl(dataval | EE_SHIFT_CLK, ee_addr);
		eeprom_delay(ee_addr);
		retval = (retval << 1) | ((inl(ee_addr) & EE_DATA_READ) ? 1 : 0);
	} while (--cmd_len >= 0);
	outl(EE_ENB, ee_addr);

	/* Terminate the EEPROM access. */
	outl(EE_ENB & ~EE_CS, ee_addr);
	if (debug > 1)
		printf(" EEPROM result is 0x%5.5x.\n", retval);
	return retval;
}

/* Wait for the EEPROM to finish what it is doing. */
static int eeprom_busy_poll(long ee_ioaddr)
{
	int i;
	outl(EE_ChipSelect, ee_ioaddr);
	for (i = 0; i < 10000; i++)			/* Typical 2000 ticks */
		if (inl(ee_ioaddr) & EE_DataIn)
			break;
	return i;
}

/* The abstracted functions for EEPROM access. */
static int read_eeprom(long ioaddr, int location, int addr_len)
{
	return do_eeprom_cmd(ioaddr, ((EE_ReadCmd << addr_len) | location) << 16,
						 3 + addr_len + 16) & 0xffff;
}

static void write_eeprom(long ioaddr, int index, int value, int addr_len)
{
	long ee_ioaddr = ioaddr + EE_OFFSET;
	int i;

	/* Poll for previous op finished. */
	eeprom_busy_poll(ee_ioaddr);

	/* Enable programming modes. */
	do_eeprom_cmd(ioaddr, (0x4f << (addr_len-4)), 3 + addr_len);
	/* Do the actual write. */ 
	do_eeprom_cmd(ioaddr,
				  (((EE_WriteCmd<<addr_len) | index)<<16) | (value & 0xffff),
				  3 + addr_len + 16);
	i = eeprom_busy_poll(ee_ioaddr);
	if (debug)
		printf(" Write finished after %d ticks.\n", i);
	/* Disable programming.  Note: this command is not instantaneous, but
	   we check for busy before the next write. */
	do_eeprom_cmd(ioaddr, (0x40 << (addr_len-4)), 3 + addr_len);
}

static int do_update(long ioaddr, int index, int addr_len,
					 const char *field_name, int old_value, int new_value)
{
	if (old_value != new_value) {
		if (do_write_eeprom) {
			printf(" Writing new %s entry 0x%4.4x.\n",
				   field_name, new_value);
			write_eeprom(ioaddr, index, new_value, addr_len);
			if (read_eeprom(ioaddr, index, addr_len) != new_value)
				printf(" WARNING: Write of new %s entry 0x%4.4x did not "
					   "succeed!\n", field_name, new_value);
		} else
			printf(" Would write new %s entry 0x%4.4x (old value 0x%4.4x).\n",
				   field_name, new_value, old_value);
		return 1;
	}
	return 0;
}


/* Read and write the MII registers using the hardware support.
   This may also be done with a software-generated serial bit stream, but
   the hardware method should be more reliable.
   */
#define MDIO_SHIFT_CLK	0x20
#define MDIO_DATA_WRITE 0x40
#define MDIO_ENB		0x80
#define MDIO_DATA_READ	0x40
#define MII_READOP		1
#define MII_WRITEOP		2
int mdio_read(long ioaddr, int phy_id, int location)
{
	int i;

	outl((phy_id << 9) | (location << 4) | MII_READOP, ioaddr + MIICtrl);
	for (i = 10000; i > 0; i--) {
		int ctrl = inl(ioaddr + MIICtrl);
		if ((ctrl & 0x08) && debug)
			printf("MII control register returned %8.8x at tick %d.\n",
				   ctrl, 10000 - i);
		if ((ctrl & MII_READOP) == 0)
			break;
	}
	if (debug)
		printf("MII register %d:%d took %d ticks to read: %8.8x -> %8.8x.\n",
			   phy_id, location, 10000 - i, inl(ioaddr + MIICtrl),
			   inl(ioaddr + MIIData));

	return inw(ioaddr + MIIData);
}

void mdio_write(long ioaddr, int phy_id, int location, int value)
{
	int i;

	outw(value, ioaddr + MIIData);
	outl((phy_id << 9) | (location << 4) | MII_WRITEOP, ioaddr + MIICtrl);
	for (i = 10000; i > 0; i--) {
		int ctrl = inl(ioaddr + MIICtrl);
		if ((ctrl & 0x08) && debug)
			printf("MII control register returned %8.8x at tick %d.\n",
				   ctrl, 10000 - i);
		if ((ctrl & MII_WRITEOP) == 0)
			break;
	}
	if (debug)
		printf("MII register %d:%d took %d ticks to write: %8.8x -> %8.8x.\n",
			   phy_id, location, 10000 - i, inl(ioaddr + MIICtrl),
			   inl(ioaddr + MIIData));
	return;
}


static void parse_eeprom(unsigned short *eeprom)
{
	unsigned char *p = (void *)eeprom;
	int i;
	unsigned char sum = 0;

	printf("Parsing the EEPROM of a EPIC/100:\n Station Address ");
	for (i = 0; i < 5; i++)
		printf("%2.2X:", p[i]);
	printf("%2.2X.\n Board name '%.20s', revision %d.\n", p[5], p + 88, p[6]);
	for (i = 0; i < 8; i++)
		sum += p[i];
	printf("  Calculated checksum is %2.2x, %scorrect.\n", sum, sum ? "in":"");
	printf(" Subsystem ID Vendor/Device %4.4x/%4.4x.\n", eeprom[6], eeprom[7]);
	return;
}


/*
 * Local variables:
 *  compile-command: "cc -O -Wall -o epic-diag epic-diag.c"
 *  alt-compile-command: "cc -O -Wall -o epic-diag epic-diag.c -DLIBMII libmii.c"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
