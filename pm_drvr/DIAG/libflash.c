/* libflash.c: Flash memory programming library.

	Copyright 1997-1999 by Donald Becker.
	This version released under the Gnu Public License, incorporated herein
	by reference.  Contact the author for use under other terms.
	The author may be reached as becker@cesdis.edu.
	C/O USRA-CESDIS, Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771

	References:
	http://www.atmel.com/atmel/products/prod9.htm
	http://www.ssti.com/products/products.html
*/
static const char version_msg[] =
"libflash.c:v2.00 7/28/99 Copyright Donald Becker, becker@cesdis.gsfc.nasa.gov\n";

/* External interface
      flash_show(), flash_dump() and flash_program()
	ADDR_IOADDR is a 32-bit I/O space register that sets the Flash ROM address.
		It is always accessed as  outl(val, ADDR_IOADDR)
	DATA_IOADDR is an 8-bit I/O register from which the Flash ROM data is read.
		It is always accessed with inb or outb  e.g.  outb(val, DATA_IOADDR)
	FILENAME is a filename where the Flash image is stored.
	The caller does any register setup required.
*/

/*  External interface to this library.  Functions and variables: */
/* Show the Flash contents on stdout. */
extern int flash_show   (long addr_ioaddr, long data_ioaddr);
/* Read or write the Flash, writing/reading FILENAME. */
extern int flash_program(long addr_ioaddr, long data_ioaddr, char *filename);
extern int flash_dump   (long addr_ioaddr, long data_ioaddr, char *filename);

extern int verbose;				/* Set to non-zero for info. */
extern int debug;				/* Currently unused. */

/* Callback hooks for accessing single words of the flash chip.
   Chips that have a I/O space 32 bit address register and do an 8 bit
   read/write register will not need this. */
int (*flash_in_hook)(long addr, int offset) = 0;
void (*flash_out_hook)(long addr, int offset, int val) = 0;

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <asm/io.h>

struct flash_id_table {
	unsigned char vendor_id, device_id;
	int flags;
	int mem_size, page_size;
	char *part_name;
} static flash_id[] = {
	{ 0x12, 0x34, 0,   8*1024,   1, "Unknown Flash EEPROM", },
	{ 0xBF, 0x07, 0, 128*1024, 128, "Silicon Storage Technology SST29EE010", },
	{ 0xBF, 0x08, 0, 128*1024, 128, "Silicon Storage Technology SST29LE010", },
	{ 0x1F, 0xDC, 0,  32*1024,  64, "Atmel AT29C256", },
	{ 0x1F, 0xBC, 0,  32*1024,  64, "Atmel AT29LV256", },
	{ 0x1F, 0x5D, 0,  64*1024, 128, "Atmel AT29C512", },
	{ 0x1F, 0x3D, 0,  64*1024, 128, "Atmel AT29LV512", },
	{ 0x1F, 0xD5, 0, 128*1024, 128, "Atmel AT29C010A", },
	{ 0x1F, 0x35, 0, 128*1024, 128, "Atmel AT29LV010A", },
	{ 0x1F, 0xDA, 0, 256*1024, 256, "Atmel AT29C020", },
	{ 0x1F, 0xBA, 0, 256*1024, 256, "Atmel AT29LV020", },
	{ 0x1F, 0x5B, 0, 512*1024, 512, "Atmel AT29C040", },
	{ 0x1F, 0x3B, 0, 512*1024, 512, "Atmel AT29LV040", },
	{ 0x1F, 0xA4, 0, 512*1024, 256, "Atmel AT29C040A", },
	{ 0x1F, 0xC4, 0, 512*1024, 256, "Atmel AT29LV040A", },
	{ 0x1F, 0x03, 0,  64*1024, 1, "Atmel AT49BV512", },
	{ 0,},
};

/* Index of internal functions. */
static int get_part_id(long addr_ioaddr, long data_ioaddr);
void sst_flash_clear(long addr_ioaddr, long data_ioaddr);
static void do_cmd(long addr_ioaddr, long data_ioaddr, int cmd);

static inline void flash_out(long addr_ioaddr, long data_ioaddr, int offset,
							 int val)
{
	if (flash_out_hook)
		(*flash_out_hook)(addr_ioaddr, offset, val);
	else if ((data_ioaddr & 0x7f) == 0x48) {
		outl(offset, addr_ioaddr);
		outl(0x3000 | val, data_ioaddr); 		/* For Tulip only. */
	} else {
		outl(offset, addr_ioaddr);
		outb(val, data_ioaddr);
	}
}
static inline int flash_in(long addr_ioaddr, long data_ioaddr, int offset)
{
	if (flash_in_hook)
		return (*flash_in_hook)(addr_ioaddr, offset);
	else if ((data_ioaddr & 0x7f) == 0x48) {
		printf("using Tulip mode");
		outl(offset, addr_ioaddr);
		outl(0x5000, data_ioaddr);				/* For Tulip only. */
		return inl(data_ioaddr) & 0xff;
	} else {
		outl(offset, addr_ioaddr);
		return inb(data_ioaddr) & 0xff;
	}
}

static int get_part_id(long addr_ioaddr, long data_ioaddr)
{
	int id0, id1;
	int old[2];
	int i;

	old[0] = flash_in(addr_ioaddr, data_ioaddr, 0);
	old[1] = flash_in(addr_ioaddr, data_ioaddr, 1);

#if 0
	/* Check for an immediate-write Flash by checking for programming delay.
	   However this check is falsely triggered / unreliable. */
	flash_out(addr_ioaddr, data_ioaddr, 0, old[0]);
	if (flash_in(addr_ioaddr, data_ioaddr, 0) != old[0]) {
		if (verbose)
			printf("No BIOS ROM ID, immediate-write flash (%2.2x).\n",
				   flash_in(addr_ioaddr, data_ioaddr, 0));
		for (i = 10000; i > 0; i--)
			if (flash_in(addr_ioaddr, data_ioaddr, 0) == old[0])
				break;
		return -1;
	}
#endif

	do_cmd(addr_ioaddr, data_ioaddr, 0x90);
	/* usleep(10);*/
	id0 = flash_in(addr_ioaddr, data_ioaddr, 0);
	id1 = flash_in(addr_ioaddr, data_ioaddr, 1);
#if 0  /* Test code only. */
	for (i = 0; i < 1000; i++) {
		printf("Flash ID is %x %x\r", id0, id1);
		id0 = flash_in(addr_ioaddr, data_ioaddr, 0);
		id1 = flash_in(addr_ioaddr, data_ioaddr, 1);
	}
#endif
	if (old[0] == id0  &&  old[1] == id1) {
		/* Some SST parts only respond to the two-part ID command. */
		printf("Hmmm, no response to the ID command, trying again..\n");
		do_cmd(addr_ioaddr, data_ioaddr, 0x80);
		do_cmd(addr_ioaddr, data_ioaddr, 0x60);
		usleep(10);
		id0 = flash_in(addr_ioaddr, data_ioaddr, 0);
		id1 = flash_in(addr_ioaddr, data_ioaddr, 1);
		if (old[0] == id0  &&  old[1] == id1)
			printf("ACKKK, this may not be a programmable Flash part!\n");
	}

	if (verbose > 1)
		printf(" BIOS ROM ID %2.2X %2.2X, looking up vendor...\n", id0, id1);

	/* Exit ID mode. */
	do_cmd(addr_ioaddr, data_ioaddr, 0xF0);
	usleep(10);

	for (i = 0; flash_id[i].vendor_id; i++)
		if (flash_id[i].vendor_id == id0  &&  flash_id[i].device_id == id1)
			break;
	if (flash_id[i].vendor_id == 0) {
		printf("Unknown BIOS ROM ID %2.2X %2.2X.\n", id0, id1);
		return -1;
	}
	if (verbose)
		printf("BIOS ROM ID %2.2X %2.2X, vendor %s:\n", id0, id1,
			   flash_id[i].part_name ? flash_id[i].part_name : "");
	return i;
}

int flash_show(long addr_ioaddr, long data_ioaddr)
{
	unsigned char old_inval[16], inval[16];
	int addr, did_dots = 0;
	int i, part_id, mem_size;

	part_id = get_part_id(addr_ioaddr, data_ioaddr);
	mem_size = (part_id < 0) ? 64*1024 : flash_id[part_id].mem_size;

	for (addr = 0; addr < mem_size; addr+=16) {
		for (i = 0; i < 16; i++) {
			inval[i] = flash_in(addr_ioaddr, data_ioaddr, addr + i);
		}
		if (addr == 0 || memcmp(inval, old_inval, sizeof(old_inval))) {
			printf("0x%3.3x: ", addr);
			for (i = 0; i < 16; i++)
				printf("%2.2x ", inval[i]);
			for (i = 0; i < 16; i++)
				printf("%c", isalnum(inval[i]) ? inval[i] : '.');
			printf("\n");
			memcpy(old_inval, inval, sizeof(old_inval));
			did_dots = 0;
		} else if (! did_dots) {
			did_dots = 1;
			printf("...\n");
		}
	}
	printf("\nBIOS ROM dump done.\n");
	return 0;
}

int flash_dump(long addr_ioaddr, long data_ioaddr, char *filename)
{
	char wbuf[4096];
	int fd, i, offset, cnt;
	int	part_id = get_part_id(addr_ioaddr, data_ioaddr);
	int mem_size = (part_id < 0) ? 8*1024 : flash_id[part_id].mem_size;

	printf("Saving the boot flash ROM into file '%s'...", filename);
	fflush(stdout);
	if ((fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0444)) < 0) {
		perror("flash_dump");
		return -1;
	}
	for (offset = 0; offset < mem_size; offset += cnt) {
		int blk_size = sizeof(wbuf) < 128*1024 - offset
			? sizeof(wbuf) : 128*1024 - offset;
		for (i = 0; i < blk_size; i++) {
			wbuf[i] = flash_in(addr_ioaddr, data_ioaddr, offset + i);
		}
		cnt = write(fd, wbuf, i);
		if (cnt < 0) {
			perror("save_flash");
			return -1;
		}
	}
	printf("done.\n");
	return 0;
}

/* Bulk erase the flash ROM. */
void sst_flash_clear(long addr_ioaddr, long data_ioaddr)
{
	do_cmd(addr_ioaddr, data_ioaddr, 0x80);
	do_cmd(addr_ioaddr, data_ioaddr, 0x10);
	usleep(20000);				/* 20 msec. */
}

int flash_program(long addr_ioaddr, long data_ioaddr, char *filename)
{
	unsigned char rbuf[1024];
	int fd, i;
	int offset = 0, eofflag = 0;
	int	part_id = get_part_id(addr_ioaddr, data_ioaddr);
	int page_size = part_id > 0 ? flash_id[part_id].page_size : 128;

	if (part_id < 0) {
		fprintf(stderr, "  This is an unknown flash chip, which cannot be "
				"programmed.\n");
		return -1;
	}

	if ((fd = open(filename, O_RDONLY)) < 0) {
		perror("flash_program");
		return -1;
	}
	do {
		int rcnt, cnt = 0;
		/* The flash image should be in a file, and we should never
		   encounter short reads.  But it's a disaster if we do, so we guard
		   against it. */
		do {
			rcnt = read(fd, rbuf + cnt, page_size - cnt);
			if (rcnt < 0) {
				perror("flash_load");
				fprintf(stderr, "read(%d,%p,%d)\n",
						fd, rbuf + cnt, page_size - cnt);
				return -1;
			} else if (rcnt == 0) {
				eofflag++;
				break;
			}
			cnt += rcnt;
		} while (page_size - cnt > 0);

		if (cnt == 0  &&  eofflag)
			break;

		/* At this point rbuf[] is either a full block, or the tail of
		   data to be written.  We cannot allow blocks to span the 128 byte
		   flash pages. */
		do_cmd(addr_ioaddr, data_ioaddr, 0xA0);
		for (i = 0; i < cnt; i++) {
			flash_out(addr_ioaddr, data_ioaddr, offset + i, rbuf[i]);
		}
		if (verbose) {
			printf("\rWriting a block of %d bytes at offset %#x..",
				   cnt, offset);
			if (verbose > 1)
				printf("%2.2x %2.2x %2.2x %2.2x %2.2x .",
					   rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4]);
			fflush(stdout);
		}
		/* Pause for 200usec. to allow the write to start. */
		usleep(200);
		/* And then poll for the write to complete, look for stable read. */
		for (i = 0; i < 100000; i++)
			if (flash_in(addr_ioaddr, data_ioaddr, 0) ==
 				flash_in(addr_ioaddr, data_ioaddr, 0))
				break;
		if (verbose > 2)
			printf("Write completed after %d ticks.\n", i);
		/* Verify the write. */
		for (i = 0; i < cnt; i++) {
			unsigned char val = flash_in(addr_ioaddr, data_ioaddr, offset + i);
			if (val != rbuf[i]) {
				fprintf(stderr, "\n Flash ROM write failed at offset %d, "
						"0x%2.2x vs. 0x%2.2x.\n", offset + i, val, rbuf[i]);
				return -1;
			}
		}
		offset += cnt;
	} while (! eofflag  &&  offset < 128*1024);

	printf("\rFinished programming %d bytes into the flash ROM.\n",
		   offset);
	return 0;
}

static void do_cmd(long addr_ioaddr, long data_ioaddr, int cmd)
{
	flash_out(addr_ioaddr, data_ioaddr, 0x5555, 0xAA);
	flash_out(addr_ioaddr, data_ioaddr, 0x2AAA, 0x55);
	flash_out(addr_ioaddr, data_ioaddr, 0x5555, cmd);
	/* Alternate command is simply
	   flash_out(addr_ioaddr, data_ioaddr, 0, cmd); */
}

/*
 * Local variables:
 *  compile-command: "cc -O -Wall -c libflash.c"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
