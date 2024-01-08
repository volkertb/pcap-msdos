/* 3c515-diag.c: Diagnostic program for 3c515 ethercards.

   NOTICE: This program *must* be compiled with '-O'.
   See the compile-command at the end of the file.

	Written 1993-1997 by Donald Becker.
	Copyright 1994-1997 Donald Becker
	Copyright 1993 United States Government as represented by the
	Director, National Security Agency.

	This software may be used and distributed according to the terms of the
	Gnu Public Lincese, incorporated herein	by reference.

	The author may be reached as becker@cesdis.gsfc.nasa.gov.
	C/O USRA Center of Excellence in Space Data and Information Sciences
	Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771

*/

static char *version_msg =
	"3c515-diag.c:v0.03 3/19/97 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <asm/io.h>
#include <sys/time.h>
#include <netinet/in.h>				/* For byte-sex functions. */

struct option longopts[] = {
 /* { name	has_arg	 *flag	val } */
	{"base-address", 1, 0, 'p'},
	{"all",		   0, 0, 'a'},	/* Print all registers. */
	{"help",	   0, 0, 'h'},	/* Give help */
	{"interface",  0, 0, 'f'},	/* Interface number (built-in, AUI) */
	{"irq",		   1, 0, 'i'},	/* Interrupt number */
	{"quiet",	   0, 0, 'q'},	/* Tone down verbosity mode */
	{"verbose",	   0, 0, 'v'},	/* Verbose mode */
	{"version",	   0, 0, 'V'},	/* Display version number */
	{"write-EEPROM", 1, 0, 'w'},/* Write th EEPROMS with the specified vals */
	{ 0, 0, 0, 0 }
};

#define printk printf

#ifdef EL3_DEBUG
int el3_debug = EL3_DEBUG;
#else
int el3_debug = 6;
#endif

/* Offsets from base I/O address. */
#define EL3_DATA 0x00
#define EL3_CMD 0x0e
#define	 EEPROM_READ 0x80
#define EL3WINDOW(win_num) outw(SelectWindow + (win_num), ioaddr + EL3_CMD)

/* The top five bits written to EL3_CMD are a command, the lower
   11 bits are the parameter, if applicable.
   Note that 11 parameters bits was fine for ethernet, but the new chip
   can handle FDDI length frames (~4500 octets) and now parameters count
   32-bit 'Dwords' rather than octets. */
enum vortex_cmd {
	TotalReset = 0<<11, SelectWindow = 1<<11, StartCoax = 2<<11,
	RxDisable = 3<<11, RxEnable = 4<<11, RxReset = 5<<11, RxDiscard = 8<<11,
	TxEnable = 9<<11, TxDisable = 10<<11, TxReset = 11<<11,
	FakeIntr = 12<<11, AckIntr = 13<<11, SetIntrEnb = 14<<11,
	SetStatusEnb = 15<<11, SetRxFilter = 16<<11, SetRxThreshold = 17<<11,
	SetTxThreshold = 18<<11, SetTxStart = 19<<11,
	StartDMAUp = 20<<11, StartDMADown = (20<<11)+1, StatsEnable = 21<<11,
	StatsDisable = 22<<11, StopCoax = 23<<11,};

enum Window0 {
	Wn0EepromCmd = 0x200A,		/* EEPROM command register. */
	Wn0EepromData = 0x200C,		/* EEPROM results register. */
};
enum Win0_EEPROM_bits {
	EEPROM_Read = 0x80, EEPROM_WRITE = 0x40, EEPROM_ERASE = 0xC0,
	EEPROM_EWENB = 0x30,		/* Enable erasing/writing for 10 msec. */
	EEPROM_EWDIS = 0x00,		/* Disable EWENB before 10 msec timeout. */
	EEPROM_Busy = 0x0200,		/* EEPROM access in progress. */
};
/* EEPROM locations. */
enum eeprom_offset {
	PhysAddr01=0, PhysAddr23=1, PhysAddr45=2, ModelID=3,
	EtherLink3ID=7, IFXcvrIO=8, IRQLine=9,
	NodeAddr01=10, NodeAddr23=11, NodeAddr45=12,
	DriverTune=13, Checksum=15};

enum Window3 {			/* Window 3: MAC/config bits. */
	Wn3_Config=0, Wn3_MAC_Ctrl=6, Wn3_Options=8,
};
union wn3_config {
	int i;
	struct w3_config_fields {
		unsigned int ram_size:3, ram_width:1, ram_speed:2, rom_size:2;
		int pad8:8;
		unsigned int ram_split:2, pad18:2, xcvr:3, pad21:1, autoselect:1;
		int pad24:7;
	} u;
};

enum Win4_Media_bits {
	Media_SQE = 0x0008,		/* Enable SQE error counting for AUI. */
	Media_10TP = 0x00C0,	/* Enable link beat and jabber for 10baseT. */
	Media_Lnk = 0x0080,		/* Enable just link beat for 100TX/100FX. */
	Media_LnkBeat = 0x0800,
};
#define HZ 100
static struct media_table {
  char *name;
  unsigned int media_bits:16,		/* Bits to set in Wn4_Media register. */
	mask:8,				/* The transceiver-present bit in Wn3_Config.*/
	next:8;				/* The media type to try next. */
  short wait;			/* Time before we check media status. */
} media_tbl[] = {
  {	"10baseT",   Media_10TP,0x08, 3 /* 10baseT->10base2 */, (14*HZ)/10},
  { "10Mbs AUI", Media_SQE, 0x20, 8 /* AUI->default */, (1*HZ)/10},
  { "undefined", 0,			0x80, 0 /* Undefined */, 0},
  { "10base2",   0,			0x10, 1 /* 10base2->AUI. */, (1*HZ)/10},
  { "100baseTX", Media_Lnk, 0x02, 5 /* 100baseTX->100baseFX */, (14*HZ)/10},
  { "100baseFX", Media_Lnk, 0x04, 6 /* 100baseFX->MII */, (14*HZ)/10},
  { "MII",		 0,			0x40, 0 /* MII->10baseT */, (14*HZ)/10},
  { "undefined", 0,			0x01, 0 /* Undefined/100baseT4 */, 0},
  { "Default",	 0,			0xFF, 0 /* Use default */, 0},
};

/* Register window 1 offsets, used in normal operation. */
#define TX_FREE 0x0C
#define TX_STATUS 0x0B
#define TX_FIFO 0x00
#define RX_FIFO 0x00

int el3_probe(void);

/* A minimal device structure so that we can use the driver transmit code
   with as few changes as possible. */
struct device {
	char *name;
	short base_addr;
	int tbusy;
	int trans_start;
} devs, *dev;

int verbose = 1;
int jiffies;
unsigned char fake_packet[100];

int opt_f = 0;

static int corkscrew_probe(int port_base);
int el3_send_packet(void *pkt, int len);


int
main(int argc, char **argv)
{
	int port_base = 0x280, irq = -1;
	int errflag = 0, shared_mode = 0;
	int write_eeprom = 0, interface = -1, all_regs = 0;
	int c, card, longind;
	extern char *optarg;
	dev = &devs;

	while ((c = getopt_long(argc, argv, "afF:i:p:qsvw", longopts, &longind))
		   != -1)
		switch (c) {
		case 'f': opt_f++; break;
		case 'F':
			interface = atoi(optarg);
			break;
		case 'i':
			irq = atoi(optarg);
			break;
		case 'p':
			port_base = strtol(optarg, NULL, 16);
			break;
		case 'q': verbose--;			 break;
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
		printf(version_msg);

	/* Get access to all of I/O space. */
	if (iopl(3) < 0) {
		perror("3c515-diag: iopl()");
		fprintf(stderr, "This program must be run as root.\n");
		return 2;
	}

	el3_debug += verbose;

	dev->name = "el3";
	
	for (card = 0; card < 1; card++) {
	   printf("Looking for card at %3.3x.\n", port_base);
	   if (corkscrew_probe(port_base) < 0)
		   break;
	}
	if (verbose > 3)
		el3_send_packet(fake_packet, 42);

	return 0;
}

static int
corkscrew_probe(int port_base)
{
	int ioaddr = port_base;
	unsigned short eeprom_data[64];
	int if_port;
	int i, j;

	if ((inw(ioaddr + 0x2002) & 0x1f0) != (ioaddr & 0x1f0)) {
	  fprintf(stderr, "No ISA Fast EtherLink boards appear to be at %#3.3x.\n",
			  port_base);
	  if (! opt_f)
		return -1;
	}
	printf("\nA ISA Fast EtherLink III board was detected at I/O %#3.3x.\n",
		   ioaddr);

	for (j = 0; j < 8; j++) {
		printk("Window %d:", j);
		EL3WINDOW(j);
		for (i = 0; i < 16; i+=2)
			printk(" %4.4x", inw(ioaddr + i));
		printk(".\n");
	}
	printk("Operation registers:");
	for (i = 0; i < 16; i+=2)
	  printk(" %4.4x", inw(ioaddr + 0x10 + i));
	printk(".\n");

	printk("Boomerang registers:");
	for (i = 0; i < 16; i+=2)
	  printk(" %4.4x", inw(ioaddr + 0x400 + i));
	printk("\n  (at %#3.3x)       :", ioaddr + 0x400);
	for (i = 0; i < 16; i+=2)
	  printk(" %4.4x", inw(ioaddr + 0x410 + i));
	printk(".\n");

	printk("Corkscrew registers:");
	for (i = 0; i < 16; i+=2)
	  printk(" %4.4x", inw(ioaddr + 0x2000 + i));
	printk(".\n");

	/* Read the station address from the EEPROM. */
	if (verbose)
	  printf("EEPROM contents:");
	for (i = 0; i < 64; i++) {
		int timer;
		outw(EEPROM_Read + PhysAddr01 + i, ioaddr + Wn0EepromCmd);
		/* Pause for at least 162 us. for the read to take place. */
		for (timer = 5; timer >= 0; timer--) {
			struct timeval timeout = {0, 81};
			select(0,0,0,0,&timeout);
			if ((inw(ioaddr + Wn0EepromCmd) & EEPROM_Busy) == 0)
				break;
		}
		eeprom_data[i] = inw(ioaddr + Wn0EepromData);
		if (verbose)
		  printf("%s %4.4x", i % 16 == 0 ? "\n":"",  eeprom_data[i]);
	}

	{
		char *ram_split[] = {"5:3", "3:1", "1:1", "3:5"};
		union wn3_config config;
		unsigned available_media, default_media, autoselect;
		EL3WINDOW(3);
		available_media = inw(ioaddr + Wn3_Options);
		config.i = inl(ioaddr + Wn3_Config);
		if (verbose > 1)
			printk("  Internal config register is %4.4x, transceivers %#x.\n",
				   config.i, inw(ioaddr + Wn3_Options));
		printk("  %dK %s-wide RAM %s Rx:Tx split, %s%s interface.\n",
			   8 << config.u.ram_size,
			   config.u.ram_width ? "word" : "byte",
			   ram_split[config.u.ram_split],
			   config.u.autoselect ? "autoselect/" : "",
			   media_tbl[config.u.xcvr].name);
		if_port = config.u.xcvr;
		default_media = config.u.xcvr;
		autoselect = config.u.autoselect;
	}

	printf("   Done card at %#3.3x.\n", ioaddr);

	return 0;
}

int
el3_send_packet(void *pkt, int len)
{
	int ioaddr = dev->base_addr;

	outw(0x0801, ioaddr + EL3_CMD);
	if (el3_debug > 2) {
		printk("%s: el3_start_xmit(lenght = %d) called, status %4.4x.\n",
			   dev->name, len, inw(ioaddr + EL3_CMD));
	}

	outw(0x5800, ioaddr + EL3_CMD);
	outw(0x4800, ioaddr + EL3_CMD);

	outw(0x78ff, ioaddr + EL3_CMD); /* Allow all status bits to be seen. */
	outw(0x7098, ioaddr + EL3_CMD); /* Set interrupt mask. */

	/* Avoid timer-based retransmission conflicts. */
	dev->tbusy=1;

	/* Put out the doubleword header... */
	outw(len, ioaddr + TX_FIFO);
	outw(0x00, ioaddr + TX_FIFO);
	if (el3_debug > 4)
		printk("		Started queueing packet, FIFO room %d status %4.4x.\n",
			   inw(ioaddr + TX_FREE), inw(ioaddr+EL3_CMD));
	/* ... and the packet rounded to a doubleword. */
	outsl(ioaddr + TX_FIFO, (void *)pkt, (len + 3) >> 2);
	
	if (el3_debug > 4)
		printk("		Finished queueing packet, FIFO room remaining %d.\n",
			   inw(ioaddr + TX_FREE));
	dev->trans_start = jiffies;
	
	if (inw(ioaddr + TX_FREE) > 1536) {
		dev->tbusy=0;
	} else
		/* Interrupt us when the FIFO has room for max-sized packet. */
		outw(0x9000 + 1536, ioaddr + EL3_CMD);

	if (el3_debug > 4)
		printk("		Checking packet queue, FIFO room %d status %2.2x.\n",
			   inw(ioaddr + TX_FREE), inb(ioaddr+0xb));

	return 0;
}


/*
 * Local variables:
 *  compile-command: "cc -N -O -Wall -o 3c515-diag 3c515-diag.c"
 *  tab-width: 4
 *  c-indent-level: 4
 * End:
 */
