/* hp+.c: Diagnostic program for HP PC LAN+ (27247B and 27252A) ethercards. */
/*
	Copyright 1994 by Donald Becker.
	This version released under the Gnu Public Lincese, incorporated herein
	by reference.  Contact the author for use under other terms.

	This is a setup and diagnostic program for the Hewlett Packard PC LAN+
	ethercards, such as the HP27247B and HP27252A.

	The author may be reached as becker@cesdis.gsfc.nasa.gov.
	C/O USRA Center of Excellence in Space Data and Information Sciences
	Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771
*/

static char *version =
"hp+.c:v0.01 6/16/94 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <asm/io.h>

struct option longopts[] = {
 /* { name  has_arg  *flag  val } */
	{"base-address", 1, 0, 'p'},/* The base I/O *P*ort address. */
	{"help",       0, 0, 'h'},  /* Give help */
	{"interface",  0, 0, 'f'},  /* Transceiver type number (built-in, AUI) */
	{"irq",        1, 0, 'i'},  /* Interrupt number */
	{"ne2000",     1, 0, 'N'},  /* Switch to NE2000 mode */
	{"verbose",    0, 0, 'v'},  /* Verbose mode */
	{"version",    0, 0, 'V'},  /* Display version number */
	{"wd8013",     1, 0, 'M'},  /* Switch to shared-memory mode. */
	{"write-EEPROM", 1, 0, 'w'},/* Write the EEPROM with the specified vals */
	{ 0, 0, 0, 0 }
};

/* A few local definitions.  These mostly match the device driver
   definitions. */

#define HP_IO_EXTENT		32

#define HP_ID					0x00
#define HP_PAGING               0x02    /* See enum PageName */
#define HPP_OPTION              0x04
#define HPP_OUT_ADDR    0x08
#define HPP_IN_ADDR             0x0A
#define HP_DATAPORT             0x0c
#define NIC_OFFSET              0x10    /* Offset to the 8390 registers. */

#define HP_START_PG             0x00    /* First page of TX buffer */
#define HP_STOP_PG              0x80    /* Last page +1 of RX ring */

enum PageName { Perf_Page = 0, MAC_Page = 1, HW_Page = 2, LAN_Page = 4,
                                        ID_Page = 6 }; 

/* The values for HPP_OPTION. */
enum HP_Option {
	NICReset = 1, ChipReset = 2,    /* Active low, really UNreset. */
	EnableIRQ = 4, FakeIntr = 8, BootROMEnb = 0x10, IOEnb = 0x20,
	MemEnable = 0x40, ZeroWait = 0x80, MemDisable = 0x1000, };
/* ... and their names. */  
static char* hp_option_names[] =
{"NIC Running", "Board Running", "IRQ Enabled", "IRQ Set", "Boot ROM On",
	 "Programmed I/O Enabled", "Shared memory enabled",
	 "Zero Wait State enabled", 0, 0, 0, 0, "Memory disabled", 0, 0, 0};

static int test_shared_mem(int ioaddr, caddr_t location);
static int test_io_mem(int ioaddr);
static int do_checksum(int ioaddr);
static void block_output_io(int ioaddr, const unsigned char *buf, int count, int offset);
static void block_input_io(int ioaddr, unsigned char *buf, int count, int offset);

/*
 This is it folks...
 */

int
main(int argc, char *argv[])
{
	extern char *optarg;
	int port_base = 0x300, ioaddr, irq = -1;
	int errflag = 0, verbose = 0, mem_mode = 0, write_eeprom = 0;
	int do_version = 0;
	int xcvr = -1;				/* Transceiver type. */
	int c, i;
	unsigned int option;
	caddr_t memory_base;

	while ((c = getopt(argc, argv, "f:i:p:svVw")) != -1)
		switch (c) {
		case 'f':  xcvr = atoi(optarg); break;
		case 'i':  irq = atoi(optarg);  break;
		case 'p':  port_base = strtol(optarg, NULL, 16);  break;
		case 's':  mem_mode++; break;
		case 'v':  verbose++;			break;
		case 'V':  do_version++;		break;
		case 'w':  write_eeprom++;		break;
		case '?':
			errflag++;
		}
	if (errflag) {
		fprintf(stderr, "usage:");
		return 2;
	}

	if (verbose || do_version)
		printf(version);

	/* Turn on access to the I/O ports. */
	if (ioperm(port_base, HP_IO_EXTENT, 1)) {
		perror("io-perm");
		fprintf(stderr,
				"  (You must run this program with 'root' permissions.)\n");
		return 1;
	}
	printf("Checking the ethercard signature at %#3x:", port_base);
	ioaddr = port_base;

	for (i = 0; i < 8; i+=2)
		printf(" %4.4x", inw(ioaddr + i));

    /* Check for the HP+ signature, 50 48 0x 53. */
    if (inw(ioaddr) != 0x4850
        || (inb(ioaddr+2) & 0xf0) != 0x00
        || inb(ioaddr+3) != 0x53) {
        printf("Invalid HP PCLAN+ ethercard signature at %#x.\n", ioaddr);
        return ENODEV;
    }
    printf(" found.\n");

	option = inw(ioaddr + HPP_OPTION);
	printf("Option register is %4.4x:\n", option);
	for (i = 0; i < 16; i++)
		if (option & (1 << i))
			printf("  Option '%s' set.\n", hp_option_names[i]);

	printf("8390 registers are:");
	for(i = 16; i < 22; i++)
		printf(" %2.2x", inb(ioaddr + i));
	printf("...\n");

	do_checksum(ioaddr);

	/* Point at the Software configuration registers. */
	outw(ID_Page, ioaddr + HP_PAGING);
	printf("Software configuration:"); 
	for(i = 8; i < 16; i++)
	    printf(" %2.2x", inb(ioaddr + i));
	printf(" Model ID %4.4x.\n", inw(ioaddr + 12));

	/* Point at the Hardware configuration registers. */
	outw(HW_Page, ioaddr + HP_PAGING);
	printf("Hardware configuration:"); 
	for(i = 8; i < 16; i++)
	    printf(" %2.2x", inb(ioaddr + i));
	outb(0x0d, ioaddr + 10);
	memory_base = (caddr_t)(inw(ioaddr + 9) << 8);
	printf("\n  IRQ %d, memory address %#x, Internal Rx pages %2.2x-%2.2x.\n",
	       inb(ioaddr + 13) & 0x0f, inw(ioaddr + 9) << 8, inb(ioaddr + 14),
	       inb(ioaddr + 15));

	/* Point at the "performance" registers. */
	printf("Normal operation page :"); 
	outw(Perf_Page, ioaddr + HP_PAGING);
	for(i = 8; i < 16; i++)
	    printf(" %2.2x", inb(ioaddr + i));
	printf(".\n");

	outw(LAN_Page, ioaddr + HP_PAGING);
	printf("LAN page              :"); 
	for(i = 8; i < 16; i++)
	    printf(" %2.2x", inb(ioaddr + i));
	printf(".\n");
	outw(ID_Page, ioaddr + HP_PAGING);
	printf("ID page               :"); 
	for(i = 8; i < 16; i++)
		printf(" %2.2x", inb(ioaddr + i));
	printf(".\n");

	outw(Perf_Page, ioaddr + HP_PAGING);

	test_io_mem(ioaddr);

	if (option & MemEnable)
		test_shared_mem(ioaddr, memory_base);
	else {
		/* Ignore the EEPROM configuration, just for testing. */
		outw(option | MemEnable, ioaddr + HPP_OPTION);
		test_shared_mem(ioaddr, memory_base);
		outw(option, ioaddr + HPP_OPTION);
	}

	return 0;
}

static caddr_t shared_mem = 0;

static int map_shared_mem(caddr_t location, int extent)
{
	int memfd;

	memfd = open("/dev/kmem", O_RDWR);
	if (memfd < 0) {
		perror("/dev/kmem (shared memory)");
		return 2;
	}
	shared_mem = mmap(location, extent, PROT_READ|PROT_WRITE, MAP_SHARED,
					  memfd, (off_t)location);
	printf("Shared memory at %#x mapped to %#x.\n",
		   (int)location, (int)shared_mem);
	return 0;
}

static int test_shared_mem(int ioaddr, caddr_t location)
{
	int option_reg = inw(ioaddr + HPP_OPTION);

	block_output_io(ioaddr, "Zero page info ZZZeerooo", 20, 0);

	map_shared_mem(location, 0x8000);
	
	outw(Perf_Page, ioaddr + HP_PAGING);
	outw(0x0000, ioaddr + HPP_IN_ADDR);

	outw(option_reg & ~(MemDisable + BootROMEnb), ioaddr + HPP_OPTION);

	printf("The option register is %4.4x, shared memory value is %8.8x"
		   " %8.8x (%10.30s).\n",
		   inw(ioaddr + HPP_OPTION), ((int*)shared_mem)[0],
		   ((int*)shared_mem)[0], (char*)shared_mem);

	outw(0x0000, ioaddr + HPP_OUT_ADDR);
	((int*)shared_mem)[0] = 0xC01dBeef , 
	outw(0x0000, ioaddr + HPP_IN_ADDR);
	printf("The shared memory value is %8.8x.\n", ((int*)shared_mem)[0]);

	outw(option_reg, ioaddr + HPP_OPTION);

	return 0;
}

static int test_io_mem(int ioaddr)
{
	int i;

	{
		unsigned char buff[40];
		block_output_io(ioaddr, "Rx buffer wrap is faulty!", 20, 0x7ff6);
		block_output_io(ioaddr, "Zero page write worked.", 24, 0);
		block_output_io(ioaddr, "wrap is working.", 20, 0x0c00);
		block_input_io(ioaddr, buff, 24, 0);
		printf("Buffer input at 0 is '%20.30s'.\n", buff);
		block_input_io(ioaddr, buff, 30, 0x7ff6);
		printf("Buffer input at 0x7ff6 is '%15.30s'.\n", buff);
		block_input_io(ioaddr, buff, 20, 12*256);
		printf("Rx Buffer values are");
		for (i = 0; i < 20; i++)
			printf(" %2.2x", buff[i]);
		printf("\n");
	}

	printf("Tx region 0 is:");
	outw(0x0000, ioaddr + HPP_IN_ADDR);
	for(i = 0; i < 10; i++)
		printf(" %2.2x", inw(ioaddr + HP_DATAPORT));
	printf("...\n");
	printf("Tx region 1 is:");
	outw(0x0600, ioaddr + HPP_IN_ADDR);
	for(i = 0; i < 10; i++)
		printf(" %2.2x", inw(ioaddr + HP_DATAPORT));
	printf("...\n");
	printf("Rx region is:");
	outw(0xc00, ioaddr + HPP_IN_ADDR);
	for(i = 0; i < 20; i++)
	    printf(" %2.2x", inb(ioaddr + HP_DATAPORT));
	printf(".\n        ");
	outw(0xc00, ioaddr + HPP_IN_ADDR);
	for(i = 0; i < 10; i++)
	    printf(" %4.4x", inw(ioaddr + HP_DATAPORT));
	printf(".\n        ");
	outw(0xc00, ioaddr + HPP_IN_ADDR);
	for(i = 0; i < 5; i++)
	    printf(" %8.8x", inl(ioaddr + HP_DATAPORT));
	printf(".\n");

	return 0;
}

static int do_checksum(int ioaddr)
{
	unsigned char checksum = 0;
	int i;

	/* Retrieve and checksum the station address. */
	outw(MAC_Page, ioaddr + HP_PAGING);

	printf("Ethernet address      :");
	for(i = 0; i < 8; i++) {
	    unsigned char inval = inb(ioaddr + 8 + i);
	    if (i < 7)
	        checksum += inval;
	    printf(" %2.2x", inval);
	}

	if (checksum != 0xff) {
	    printf(" bad checksum %2.2x.\n", checksum);
	    return ENODEV;
	}
	printf(" good checksum.\n");

	return 0;
}

static void
block_output_io(int ioaddr, const unsigned char *buf, int count, int offset)
{
	    outw(offset, ioaddr + HPP_OUT_ADDR);
	    outsl(ioaddr + HP_DATAPORT, buf, (count+3)>>2);
	    return;
}
static void
block_input_io(int ioaddr, unsigned char *buf, int count, int offset)
{
	outw(offset, ioaddr + HPP_IN_ADDR);
	insw(ioaddr + HP_DATAPORT, buf, count>>1);
	if (count & 0x01)
		buf[count-1] = inb(ioaddr + HP_DATAPORT);
	return;
}

/*
 * Local variables:
 *  compile-command: "gcc -Wall -O6 -o hp+ hp+.c"
 * tab-width: 4
 * c-indent-level: 4
 * End:
 */
