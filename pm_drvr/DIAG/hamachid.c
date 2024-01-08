/* hamachi-diag.c: Diagnostic and setup for Symbios chip ethercards.

   This is a diagnostic and EEPROM setup program for Ethernet adapters
   based on the Packet Engines gigabit ethernet GNIC-II (Hamachi) 

   Copyright 1998 by Donald Becker.
   This version released under the Gnu Public Lincese, incorporated herein
   by reference.  Contact the author for use under other terms.

   This program must be compiled with "-O"!  See the bottom of this file
   for the suggested compile-command.

   The author may be reached as becker@cesdis.gsfc.nasa.gov.
   C/O USRA-CESDIS, Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771
*/

static char *version_msg =
"hamachi-diag.c:v0.02 11/6/98 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

/* User-tweakable parameters. */
#define EEPROM_SIZE 256

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <strings.h>
#include <errno.h>
#include <asm/io.h>
#ifdef LIBMII
extern show_mii_details(int ioaddr, int phy_id);
extern monitor_mii(int ioaddr, int phy_id);
#endif

int hamachi_diag(int ioaddr, int part_num);

#ifdef USE_IO
/* Use I/O operations for simplicity. */
#undef readb
#undef readw
#undef readl
#undef writeb
#undef writew
#undef writel
#define readb inb
#define readw inw
#define readl inl
#define writeb outb
#define writew outw
#define writel outl
#else
#include <sys/mman.h>
#endif


/* Offsets to the Hamachi registers.  Various sizes. */
enum hamachi_offsets {
	TxDMACtrl=0x00, TxCmd=0x04, TxStatus=0x06, TxPtr=0x08, TxCurPtr=0x10,
	RxDMACtrl=0x40, RxCmd=0x44, RxStatus=0x46, RxPtr=0x48, RxCurPtr=0x50,
	PCIClkMeas=0x060, MiscStatus=0x066, ChipRev=0x68, ChipReset=0x06B,
	LEDCtrl=0x06C, VirtualJumpers=0x06D,
	TxChecksum=0x074, RxChecksum=0x076,
	TxIntrCtrl=0x078, RxIntrCtrl=0x07C,
	InterruptEnable=0x080, InterruptClear=0x084, IntrStatus=0x088,
	EventStatus=0x08C,
	MACCnfg=0x0A0, FrameGap0=0x0A2, FrameGap1=0x0A4,
	/* See enum MII_offsets below. */
	MACCnfg2=0x0B0, RxDepth=0x0B8, FlowCtrl=0x0BC, MaxFrameSize=0x0CE,
	AddrMode=0x0D0, StationAddr=0x0D2, 
	EECmdStatus=0x0F0, EEData=0x0F1, EEAddr=0x0F2,
	FIFOcfg=0x0F8,
};

/* Offsets to the MII-mode registers. */
enum MII_offsets {
	MII_Cmd=0xA6, MII_Addr=0xA8, MII_Wr_Data=0xAA, MII_Rd_Data=0xAC,
	MII_Status=0xAE,
};

/* Bits in the interrupt status/mask registers. */
enum intr_status_bits {
	IntrRxDone=0x01, IntrRxPCIFault=0x02, IntrRxPCIErr=0x04,
	IntrTxDone=0x10, IntrTxPCIFault=0x20, IntrTxPCIErr=0x40,
	LinkChange=0x10000, NegotiationChange=0x20000, StatsMax=0x40000, };

extern int do_write_eeprom;
extern int has_mii;
extern int verbose, opt_f, debug;
extern int show_regs;
extern int show_eeprom, show_mii;
extern unsigned int opt_a,				/* Show-all-interfaces flag. */
	opt_restart,
	opt_reset,
	opt_watch;
extern int nway_advertise;
extern int fixed_speed;

/* Values read from the EEPROM. */
static unsigned char eeprom_contents[EEPROM_SIZE];

static int read_eeprom(void *mmaddr, int location);
static void write_eeprom(void *mmaddr, int index, int value);
static int mdio_read(void *mmaddr, int phy_id, int location);
static void mdio_write(void *mmaddr, int phy_id, int location, int value);



/* The interrupt flags. */
static const char *intr_names[16] ={
	"Rx DMA event", "Rx Illegal instruction",
	"PCI bus fault during receive", "PCI parity error during receive", 
	"Tx DMA event", "Tx Illegal instruction",
	"PCI bus fault during transmit", "PCI parity error during transmit", 
	"Early receive", "Carrier sense wakeup",
};

/* Non-interrupting events. */
static const char *event_names[16] = {
	"Tx Abort", "Rx frame complete", "Transmit done",
};


int hamachi_diag(int ioaddr, int part_num)
{
	void *mmaddr;
	int i;

	if (verbose)
		printf(version_msg);
	{
		extern long pci_mem_addr;
		int memfd;
		if (debug)
			printf("Hamachi mapped to PCI memory at %lx.\n", pci_mem_addr);
		memfd = open("/dev/kmem", O_RDWR);
		if (memfd < 0) {
			perror("/dev/kmem (shared memory)");
			return 2;
		}
		mmaddr = mmap(0, 0x400, PROT_READ|PROT_WRITE,
					  MAP_SHARED, memfd, pci_mem_addr);
		if (debug)
			printf("PCI memory space mmapped to %#x.\n", (int)mmaddr);
	}
	if (show_regs) {
		printf("Hamachi chip registers at %#x", ioaddr);
		for (i = 0; i < 0x100; i += 4) {
			if ((i & 0x1f) == 0)
				printf("\n 0x%3.3X:", i);
			printf(" %8.8x", readl(mmaddr + i));
		}
		printf("\n");
	}
	{
		unsigned intr_status, event_status;

		printf("Station address ");
		for (i = 0; i < 5; i++)
			printf("%2.2x:", readb(mmaddr + StationAddr + i));
		printf("%2.2x.\n", readb(mmaddr + StationAddr + i));

		intr_status = readw(mmaddr + IntrStatus);
		printf("  %snterrupt sources are pending (%4.4x).\n",
			   (intr_status & 0x03ff) ? "I": "No i", intr_status);
		if (intr_status) {
			for (i = 0; i < 16; i++)
				if (intr_status & (1<<i))
					printf("   %s indication.\n", intr_names[i]);
		}
		event_status = readw(mmaddr + EventStatus);
		if (event_status) {
			for (i = 0; i < 3; i++)
				if (event_status & (1<<i))
					printf("   %s event.\n", event_names[i]);
		}
	}

	for (i = 0; i < EEPROM_SIZE; i++)
		eeprom_contents[i] = read_eeprom(mmaddr, i);

	if (show_eeprom) {
		printf("EEPROM contents:");
		for (i = 0; i < (show_eeprom > 1 ? EEPROM_SIZE : 0x20); i++) {
			if ((i&15) == 0)
				printf("\n0x%3.3x: ", i);
			printf(" %2.2x", eeprom_contents[i]);
		}
		printf("\n");
	}
	if ((has_mii && verbose) || show_mii) {
		int phys[4], phy, phy_idx = 0;
		for (phy = 0; phy < 32 && phy_idx < 4; phy++) {
			int mii_status = mdio_read(mmaddr, phy, 1);
			if (mii_status != 0xffff  && 
				mii_status != 0x0000) {
				phys[phy_idx++] = phy;
				printf(" MII PHY found at address %d, status 0x%4.4x.\n",
					   phy, mii_status);
			}
		}
	}
	/* Special purpose diagnostics. */
	{
		printf("Mac test register 1 is %x.\n", readl(mmaddr + 0xc0));
		writel(0x00040000, mmaddr + 0xC0);
		printf("Mac test register is %x.\n", readl(mmaddr + 0xc4));
	}
	return 0;
}

static int read_eeprom(void *ioaddr, int location)
{
	int bogus_cnt = 1000;		/* Typical ticks: 48 */

	writew(location, ioaddr + EEAddr);
	writeb(0x02, ioaddr + EECmdStatus);
	while ((readb(ioaddr + EECmdStatus) & 0x40)  && --bogus_cnt > 0)
		;
	if (debug > 5)
		printf("   EEPROM status is %2.2x after %d ticks.\n",
			   (int)readb(ioaddr + EECmdStatus), 1000- bogus_cnt);
	return readb(ioaddr + EEData);
}

static void write_eeprom(void *mmaddr, int location, int value)
{
	int bogus_cnt = 1000;

	while ((readb(mmaddr + EECmdStatus) & 0x40)  && --bogus_cnt > 0)
		;
	/* Enable writing. */
	writeb(0x05, mmaddr + EECmdStatus);
	writeb(value, mmaddr + EEData);
	writew(location, mmaddr + EEAddr);
	while ((readb(mmaddr + EECmdStatus) & 0x40)  && --bogus_cnt > 0)
		;
	writeb(0x01, mmaddr + EECmdStatus);		/* Do the write. */
	while ((readb(mmaddr + EECmdStatus) & 0x40)  && --bogus_cnt > 0)
		;
	writeb(0x04, mmaddr + EECmdStatus);		/* Disable writing. */
	return;
}

/* MII Managemen Data I/O accesses.
   These routines assume the MDIO controller is idle, and do not exit until
   the command is finished. */

int mdio_read(void *mmaddr, int phy_id, int location)
{
	int i = 10000;

	if (verbose > 2)		/* Debug: 5 */
		printf(" mdio_read(%p, %d, %d)..", mmaddr, phy_id, location);
	writew((phy_id<<8) + location, mmaddr + MII_Addr);
	writew(1, mmaddr + MII_Cmd);
	while (--i > 0)
		if ((readw(mmaddr + MII_Status) & 1) == 0)
			break;
	return readw(mmaddr + MII_Rd_Data);
}

void mdio_write(void *mmaddr, int phy_id, int location, int value)
{
	int i = 10000;

	writew((phy_id<<8) + location, mmaddr + MII_Addr);
	writew(value, mmaddr + MII_Wr_Data);

	/* Wait for the command to finish. */
	while (--i > 0)
		if ((readw(mmaddr + MII_Status) & 1) == 0)
			break;
	return;
}



/*
 * Local variables:
 *  compile-command: "cc -O -Wall -o etherdiag diag-dispatch.c hamachi-diag.c"
 *  compile-command-2: "cc -O -Wall -o hamachi-diag hamachi-diag.c"
 *  alt-compile-command: "cc -O -Wall -o hamachi-diag hamachi-diag.c -DLIBMII libmii.c"
 *  tab-width: 4
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 */
