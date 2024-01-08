/*
 * bios32.c - BIOS32, PCI BIOS functions.
 *
 * $Id: bios32.c,v 1.3.2.4 1997/08/02 22:24:23 mj Exp $
 *
 * Sponsored by
 *      iX Multiuser Multitasking Magazine
 *      Hannover, Germany
 *      hm@ix.de
 *
 * Copyright 1993, 1994 Drew Eckhardt
 *      Visionary Computing
 *      (Unix and Linux consulting and custom programming)
 *      Drew@Colorado.EDU
 *      +1 (303) 786-7975
 *
 * For more information, please consult
 *
 * PCI BIOS Specification Revision
 * PCI Local Bus Specification
 * PCI System Design Guide
 *
 * PCI Special Interest Group
 * M/S HF3-15A
 * 5200 N.E. Elam Young Parkway
 * Hillsboro, Oregon 97124-6497
 * +1 (503) 696-2000
 * +1 (800) 433-5177
 *
 * Manuals are $25 each or $50 for all three, plus $7 shipping
 * within the United States, $35 abroad.
 *
 *
 * CHANGELOG :
 * Jun 17, 1994 : Modified to accommodate the broken pre-PCI BIOS SPECIFICATION
 *      Revision 2.0 present on <thys@dennis.ee.up.ac.za>'s ASUS mainboard.
 *
 * Jan 5,  1995 : Modified to probe PCI hardware at boot time by Frederic
 *     Potter, potter@cao-vlsi.ibp.fr
 *
 * Jan 10, 1995 : Modified to store the information about configured pci
 *      devices into a list, which can be accessed via /proc/pci by
 *      Curtis Varner, cvarner@cs.ucr.edu
 *
 * Jan 12, 1995 : CPU-PCI bridge optimization support by Frederic Potter.
 *      Alpha version. Intel & UMC chipset support only.
 *
 * Apr 16, 1995 : Source merge with the DEC Alpha PCI support. Most of the code
 *      moved to drivers/pci/pci.c.
 *
 * Dec 7, 1996  : Added support for direct configuration access of boards
 *      with Intel compatible access schemes (tsbogend@alpha.franken.de)
 *
 * Feb 3, 1997  : Set internal functions to static, save/restore flags
 *      avoid dead locks reading broken PCI BIOS, werner@suse.de 
 *
 * Apr 26, 1997 : Fixed case when there is BIOS32, but not PCI BIOS
 *      (mj@atrey.karlin.mff.cuni.cz)
 *
 * May 7,  1997 : Added some missing cli()'s. [mj]
 * 
 * Jun 20, 1997 : Corrected problems in "conf1" type accesses.
 *      (paubert@iram.es)
 */

#include "pmdrvr.h"
#include "module.h"
#include "bios32.h"
#include "pci.h"

/* PCI functions; refer INT 1A/AH=B4h
 */
#define PCIBIOS_PCI_FUNCTION_ID         0xB1XX
#define PCIBIOS_PCI_BIOS_PRESENT        0xB101
#define PCIBIOS_FIND_PCI_DEVICE         0xB102
#define PCIBIOS_FIND_PCI_CLASS_CODE     0xB103
#define PCIBIOS_GENERATE_SPECIAL_CYCLE  0xB106
#define PCIBIOS_READ_CONFIG_BYTE        0xB108
#define PCIBIOS_READ_CONFIG_WORD        0xB109
#define PCIBIOS_READ_CONFIG_DWORD       0xB10A
#define PCIBIOS_WRITE_CONFIG_BYTE       0xB10B
#define PCIBIOS_WRITE_CONFIG_WORD       0xB10C
#define PCIBIOS_WRITE_CONFIG_DWORD      0xB10D

#ifndef BIOS32_DEBUG
#define BIOS32_DEBUG 0
#endif

int bios32_debug = BIOS32_DEBUG;

#define PRINTK(x) do {                \
                    if (bios32_debug) \
                       printk x ;     \
                  } while (0)

/* BIOS32 signature: "_32_"
 */
#define BIOS32_SIGNATURE  (('_'<<0) + ('3'<<8) + ('2'<<16) + ('_'<<24))

/* PCI signature: "PCI "
 */
#define PCI_SIGNATURE     (('P'<<0) + ('C'<<8) + ('I'<<16) + (' '<<24))

/* PCI service signature: "$PCI"
 */
#define PCI_SERVICE       (('$'<<0) + ('P'<<8) + ('C'<<16) + ('I'<<24))

/* Plug&Play service signature: "$ACF"
 */
#define PLUGnPLAY_SERVICE (('$'<<0) + ('A'<<8) + ('C'<<16) + ('F'<<24))

/* Access for created code-descriptors; readable, code seg, user, present.
 */
#define ACCESS_DESCR (0x02 | 0x08 | 0x10 | 0x80)


#include <sys/pack_on.h>

/* This is the standard structure used to identify the entry point
 * to the BIOS32 Service Directory, as documented in
 *      Standard BIOS 32-bit Service Directory Proposal
 *      Revision 0.4 May 24, 1993
 *      Phoenix Technologies Ltd.
 *      Norwood, MA
 * and the PCI BIOS specification.
 */
union bios32 {
      struct {
        DWORD  signature;    /* "_32_" */
        DWORD  entry;        /* 32 bit physical address */
        BYTE   revision;     /* Revision level, 0 */
        BYTE   length;       /* Length in paragraphs should be 1 */
        BYTE   checksum;     /* All bytes must add up to zero */
        BYTE   reserved[5];  /* Must be zero */
      } fields;
      char chars[16];
    };

#include <sys/pack_off.h>


/* Physical address of the service directory and PCI entry
 */
static __dpmi_paddr bios32_api;
static __dpmi_paddr pci_api;


/* Function table for accessing PCI configuration space
 */
struct pci_access {
       int (*find_device) (WORD, WORD, WORD, BYTE *, BYTE *);
       int (*find_class) (unsigned, WORD, BYTE *, BYTE *);
       int (*read_config_byte) (BYTE, BYTE, BYTE, BYTE *);
       int (*read_config_word) (BYTE, BYTE, BYTE, WORD *);
       int (*read_config_dword) (BYTE, BYTE, BYTE, DWORD *);
       int (*write_config_byte) (BYTE, BYTE, BYTE, BYTE);
       int (*write_config_word) (BYTE, BYTE, BYTE, WORD);
       int (*write_config_dword) (BYTE, BYTE, BYTE, DWORD);
     };

/* Pointer to selected PCI access function table
 */
static const struct pci_access *access_pci = NULL;

/* jump buffers for catching crashes inside BIOS32/PCI handlers
 */
static jmp_buf sig_buf, exc_buf;

/*
 * Returns the entry point for the given service, NULL on error
 * Call with EAX = service ("ICP$" or "FCA$"), EBX = 0
 */
static DWORD bios32_service (DWORD service, DWORD *pentry, DWORD *pbase)
{
  BYTE  return_code;    /* %al */
  DWORD base;           /* %ebx */
  DWORD length;         /* %ecx */
  DWORD entry;          /* %edx */

#if 0
  PRINTK (("bios32_service(): calling %04X:%08X, service %08X\n",
           bios32_api.selector, (unsigned)bios32_api.offset32,
           (unsigned)service));
#endif

  __asm__ __volatile__ (
           "lcall *(%%edi)"
           : "=a" (return_code), "=b" (base), "=c" (length), "=d" (entry)
           : "0" (service), "1" (0), "D" (&bios32_api) );

  switch (return_code)
  {
    case 0:
         *pentry = entry;
         *pbase  = base;
         return (1);
    case 0x80:
         PRINTK (("bios32_service(%08X): not present\n", (unsigned)service));
         return (0);
    default:
         PRINTK (("bios32_service(%08X): returned %X\n",
                  (unsigned)service, return_code));
         return (0);
  }
}


static int create_descriptor (const char *what, __dpmi_paddr *ret,
                              DWORD addr, DWORD base)
{
  WORD selector = __dpmi_allocate_ldt_descriptors (1);
  int  access   = 0;

  if (selector <= 0)
  {
    printk ("pcibios_init: failed to create selector for %s\n", what);
    return (0);
  }
  if (__dpmi_set_segment_base_address (selector, base) < 0)
  {
    printk ("pcibios_init: failed to set base for selector %X (%s)\n",
            selector, what);
    return (0);
  }

  if (__dpmi_set_segment_limit (selector, 4096-1) < 0)
  {
    printk ("pcibios_init: failed to set limit of selector %X (%s)\n",
            selector, what);
    return (0);
  }

  ret->offset32 = addr - base;
  ret->selector = selector;

  /* Set execute/user/present/readable access for selector
   */
  access = __dpmi_get_descriptor_access_rights (selector);

  PRINTK (("pcibios_init: %s pmode entry at %04X:%08X\n",
           what, ret->selector, (unsigned)ret->offset32));

  if (__dpmi_set_descriptor_access_rights (selector, access | ACCESS_DESCR) < 0)
  {
    printk ("pcibios_init: failed to set access of selector %X (%s)\n",
            selector, what);
    __dpmi_free_ldt_descriptor (selector);
    return (0);
  }
  return (1);
}

static void sig_handler (int sig)
{
  if (sig == SIGSEGV || sig == SIGILL)
  {
    memcpy (&exc_buf, __djgpp_exception_state_ptr, sizeof(exc_buf));
    longjmp (sig_buf, sig);
  }
}

static void unassemble (WORD cs, DWORD eip, int num)
{
#ifdef USE_EXCEPT
  extern char *Disassemble (void **addr);  /* libexc.a (uasm.c) */
  DWORD        adr = eip;

  printk ("Disassembly:\n");
  while (num--)
  {
    char *Asm = Disassemble ((void**)&adr);
    printk ("  %02X:%08X  %s\n", cs, (unsigned)eip, Asm);
    eip = adr;
  }
#else
  ARGSUSED (cs);
  ARGSUSED (eip);
  ARGSUSED (num);
#endif
}

static int check_pcibios (void)
{
  volatile int sig;
  int      pack;            /* %%eax */
  DWORD    signature;       /* %%edx */
  BYTE     present_status;
  BYTE     major_revision;
  BYTE     minor_revision;
  DWORD    pci_entry, pci_base, addr;

  sig = setjmp (sig_buf);
  if (sig)
  {
    if (bios32_debug)
    {
      #define REG(x) (unsigned)exc_buf[0].__##x

      printk ("pcibios_init: caught %s at %X:%08X:\n"
              "reg: EAX %08X, EBX %08X, ECX %08X, EDX %08X\n"
              "     ESI %08X, EDI %08X, ESP %08X, EBP %08X\n"
              "     DS %04X, ES %04X, GS %04X, FS %04X, SS %04X, FLAGS %08X\n",
              sig == SIGSEGV ? "SIGSEGV" : "SIGILL",
              REG(cs),  REG(eip),
              REG(eax), REG(ebx), REG(ecx), REG(edx),
              REG(esi), REG(edi), REG(esp), REG(ebp),
              REG(ds),  REG(es),  REG(gs),  REG(fs),
              REG(ss),  REG(eflags));

      unassemble (REG(cs), REG(eip), 10);
      #undef REG
    }
    return (0);
  }

#if 0
  unassemble (bios32_api.selector, bios32_api.offset32, 10);
#endif

  if (!bios32_service (PCI_SERVICE,&pci_entry,&pci_base))
     return (0);

  addr = pci_base + pci_entry;

  PRINTK (("pcibios_init: service at %08Xh, (base %08Xh)\n",
           (unsigned)pci_entry, (unsigned)pci_base));

  /* Create a zero-based descriptor for accessing the PCI-API
   */
  if (!create_descriptor ("PCI", &pci_api, addr, addr))
     return (0);

  DISABLE();

  __asm__ __volatile__ (
           "    lcall *(%%edi)\n\t"
           "    jc 1f\n\t"
           "    xor %%ah, %%ah\n"
           "1:  shl $8, %%eax\n\t"
           "    movw %%bx, %%ax"
           : "=d" (signature), "=a" (pack)
           : "1" (PCIBIOS_PCI_BIOS_PRESENT), "D" (&pci_api)
           : "bx", "cx");
  ENABLE();

  present_status = (pack >> 16) & 0xFF;
  major_revision = (pack >> 8) & 0xFF;
  minor_revision = pack & 0xFF;

  if (present_status || signature != PCI_SIGNATURE)
  {
    PRINTK ((
      "pcibios_init: %s : BIOS32 Service Directory says PCI BIOS is present,\n"
      " but PCI_BIOS_PRESENT subfunction fails with present status of %Xh\n"
      " and signature of 0x%08X (%c%c%c%c).  mail drew@colorado.edu\n",
      (signature == PCI_SIGNATURE) ? "WARNING" : "ERROR",
      present_status, (unsigned)signature,
      (char)(signature >> 0), (char)(signature >> 8),
      (char)(signature >> 16),(char)(signature >> 24)));

    if (signature != PCI_SIGNATURE)
       return (0);
  }
  PRINTK (("pcibios_init: PCI BIOS revision %X.%X\n",
           major_revision, minor_revision));
  return (1);
}


static int pci_bios_find_class (unsigned class_code, WORD index,
                                BYTE *bus, BYTE *device_fn)
{
  DWORD bx, ax;

  DISABLE();
  __asm__ __volatile__ (
           "    lcall *(%%edi)\n\t"
           "    jc 1f\n\t"
           "    xor %%ah, %%ah\n"
           "1:"
           : "=b" (bx), "=a" (ax)
           : "1" (PCIBIOS_FIND_PCI_CLASS_CODE), "c" (class_code),
             "S" ((int)index), "D" (&pci_api));
  ENABLE();
  *bus       = (bx >> 8) & 0xFF;
  *device_fn = bx & 0xFF;
  return hiBYTE (ax);
}


static int pci_bios_find_device (WORD vendor, WORD device_id,
                                 WORD index,  BYTE *bus, BYTE *device_fn)
{
  WORD bx, ax;

  DISABLE();

  __asm__ __volatile__ (
           "    lcall *(%%edi)\n\t"
           "    jc 1f\n\t"
           "    xor %%ah, %%ah\n"
           "1:"
           : "=b" (bx), "=a" (ax)
           : "1" (PCIBIOS_FIND_PCI_DEVICE), "c" (device_id),
             "d" (vendor), "S" ((int)index), "D" (&pci_api));
  ENABLE();
  *bus       = (bx >> 8) & 0xFF;
  *device_fn = bx & 0xFF;
  return hiBYTE (ax);
}

static int pci_bios_read_config_byte (BYTE bus, BYTE device_fn,
                                      BYTE where, BYTE *value)
{
  DWORD ax;
  DWORD bx = (bus << 8) | device_fn;

  DISABLE();

  __asm__ __volatile__ (
           "    lcall *(%%esi)\n\t"
           "    jc 1f\n\t"
           "    xor %%ah, %%ah\n"
           "1:"
           : "=c" (*value), "=a" (ax)
           : "1" (PCIBIOS_READ_CONFIG_BYTE), "b" (bx),
             "D" ((long)where), "S" (&pci_api));
  ENABLE();
  return hiBYTE (ax);
}

static int pci_bios_read_config_word (BYTE bus, BYTE device_fn,
                                      BYTE where, WORD *value)
{
  DWORD ax;
  DWORD bx = (bus << 8) | device_fn;

  DISABLE();

  __asm__ __volatile__ (
           "    lcall *(%%esi)\n\t"
           "    jc 1f\n\t"
           "    xor %%ah, %%ah\n"
           "1:"
           : "=c" (*value), "=a" (ax)
           : "1" (PCIBIOS_READ_CONFIG_WORD), "b" (bx),
             "D" ((long)where), "S" (&pci_api));
  ENABLE();
  return hiBYTE (ax);
}

static int pci_bios_read_config_dword (BYTE bus, BYTE device_fn,
                                       BYTE where, DWORD *value)
{
  DWORD ax;
  DWORD bx = (bus << 8) | device_fn;

  DISABLE();

  __asm__ __volatile__ (
           "    lcall *(%%esi)\n\t"
           "    jc 1f\n\t"
           "    xor %%ah, %%ah\n"
           "1:"
           : "=c" (*value), "=a" (ax)
           : "1" (PCIBIOS_READ_CONFIG_DWORD), "b" (bx),
             "D" ((long)where), "S" (&pci_api));
  ENABLE();
  return hiBYTE (ax);
}

static int pci_bios_write_config_byte (BYTE bus, BYTE device_fn,
                                       BYTE where, BYTE value)
{
  DWORD ax;
  DWORD bx = (bus << 8) | device_fn;

  DISABLE();

  __asm__ __volatile__ (
           "    lcall *(%%esi)\n\t"
           "    jc 1f\n\t"
           "    xor %%ah, %%ah\n"
           "1:"
           : "=a" (ax)
           : "0" (PCIBIOS_WRITE_CONFIG_BYTE), "c" (value),
             "b" (bx), "D" ((long)where), "S" (&pci_api));
  ENABLE();
  return hiBYTE (ax);
}

static int pci_bios_write_config_word (BYTE bus, BYTE device_fn,
                                       BYTE where, WORD value)
{
  DWORD ax;
  DWORD bx = (bus << 8) | device_fn;

  DISABLE();

  __asm__ __volatile__ (
           "    lcall *(%%esi)\n\t"
           "    jc 1f\n\t"
           "    xor %%ah, %%ah\n"
           "1:"
           : "=a" (ax)
           : "0" (PCIBIOS_WRITE_CONFIG_WORD),
             "c" (value), "b" (bx), "D" ((long)where), "S" (&pci_api));
  ENABLE();
  return hiBYTE (ax);
}

static int pci_bios_write_config_dword (BYTE bus, BYTE device_fn,
                                        BYTE where, DWORD value)
{
  DWORD ax;
  DWORD bx = (bus << 8) | device_fn;

  DISABLE();

  __asm__ __volatile__ (
           "    lcall *(%%esi)\n\t"
           "    jc 1f\n\t"
           "    xor %%ah, %%ah\n"
           "1:"
           : "=a" (ax)
           : "0" (PCIBIOS_WRITE_CONFIG_DWORD),
             "c" (value), "b" (bx), "D" ((long)where), "S" (&pci_api));
  ENABLE();
  return hiBYTE (ax);
}

/*
 * function table for BIOS32 access
 */
static const struct pci_access pci_bios_access = {
             pci_bios_find_device,
             pci_bios_find_class,
             pci_bios_read_config_byte,
             pci_bios_read_config_word,
             pci_bios_read_config_dword,
             pci_bios_write_config_byte,
             pci_bios_write_config_word,
             pci_bios_write_config_dword
           };

/*
 * Given the vendor and device ids, find the n'th instance of that
 * device in the system.
 */
static int pci_direct_find_device (WORD vendor, WORD device_id,
                                   WORD index,  BYTE *bus, BYTE *devfn)
{
  const struct pci_dev *dev;
  WORD  curr = 0;

  for (dev = pci_devices; dev; dev = dev->next)
  {
    if (dev->vendor != vendor || dev->device != device_id)
       continue;

    if (curr == index)
    {
      *devfn = dev->devfn;
      *bus   = dev->bus->number;
      return (PCIBIOS_SUCCESSFUL);
    }
    ++curr;
  }
  return (PCIBIOS_DEVICE_NOT_FOUND);
}


/*
 * Given the class, find the n'th instance of that device
 * in the system.
 */
static int pci_direct_find_class (unsigned class_code, WORD index,
                                  BYTE *bus, BYTE *devfn)
{
  const struct pci_dev *dev;
  WORD  curr = 0;

  for (dev = pci_devices; dev; dev = dev->next)
  {
    if (dev->class != class_code)
       continue;

    if (curr == index)
    {
      *devfn = dev->devfn;
      *bus   = dev->bus->number;
      return (PCIBIOS_SUCCESSFUL);
    }
    ++curr;
  }
  return (PCIBIOS_DEVICE_NOT_FOUND);
}

/*
 * Functions for accessing PCI configuration space with type 1 accesses
 */
#define CONFIG_CMD(bus, device_fn, where) (0x80000000 | ((bus) << 16) | \
                                           ((device_fn) << 8) | ((where) & ~3))

static int pci_conf1_read_config_byte (BYTE bus, BYTE device_fn,
                                       BYTE where, BYTE *value)
{
  DISABLE();
  outl (CONFIG_CMD (bus, device_fn, where), 0xCF8);
  *value = inb (0xCFC + (where & 3));
  ENABLE();
  return (PCIBIOS_SUCCESSFUL);
}

static int pci_conf1_read_config_word (BYTE bus, BYTE device_fn,
                                       BYTE where, WORD *value)
{
  if (where & 1)  /* where must be 2n */
     return (PCIBIOS_BAD_REGISTER_NUMBER);

  DISABLE();
  outl (CONFIG_CMD (bus, device_fn, where), 0xCF8);
  *value = inw (0xCFC + (where & 2));
  ENABLE();
  return (PCIBIOS_SUCCESSFUL);
}

static int pci_conf1_read_config_dword (BYTE bus, BYTE device_fn,
                                        BYTE where, DWORD *value)
{
  if (where & 3)   /* where must be 4n */
     return (PCIBIOS_BAD_REGISTER_NUMBER);

  DISABLE();
  outl (CONFIG_CMD (bus, device_fn, where), 0xCF8);
  *value = inl (0xCFC);
  ENABLE();
  return (PCIBIOS_SUCCESSFUL);
}

static int pci_conf1_write_config_byte (BYTE bus, BYTE device_fn,
                                        BYTE where, BYTE value)
{
  DISABLE();
  outl (CONFIG_CMD (bus, device_fn, where), 0xCF8);
  outb (value, 0xCFC + (where & 3));
  ENABLE();
  return (PCIBIOS_SUCCESSFUL);
}

static int pci_conf1_write_config_word (BYTE bus, BYTE device_fn,
                                        BYTE where, WORD value)
{
  if (where & 1)   /* where must be 2n */
     return (PCIBIOS_BAD_REGISTER_NUMBER);

  DISABLE();
  outl (CONFIG_CMD (bus, device_fn, where), 0xCF8);
  outw (value, 0xCFC + (where & 2));
  ENABLE();
  return (PCIBIOS_SUCCESSFUL);
}

static int pci_conf1_write_config_dword (BYTE bus, BYTE device_fn,
                                         BYTE where, DWORD value)
{
  if (where & 3)    /* where must be 4n */
     return (PCIBIOS_BAD_REGISTER_NUMBER);

  DISABLE();
  outl (CONFIG_CMD (bus, device_fn, where), 0xCF8);
  outl (value, 0xCFC);
  ENABLE();
  return (PCIBIOS_SUCCESSFUL);
}

#undef CONFIG_CMD

/*
 * Function table for type 1
 */
static const struct pci_access pci_direct_conf1 = {
             pci_direct_find_device,
             pci_direct_find_class,
             pci_conf1_read_config_byte,
             pci_conf1_read_config_word,
             pci_conf1_read_config_dword,
             pci_conf1_write_config_byte,
             pci_conf1_write_config_word,
             pci_conf1_write_config_dword
           };

/*
 * Functions for accessing PCI configuration space with type 2 accesses
 */
#define IOADDR(devfn, where)  ((0xC000 | (((devfn) & 0x78) << 5)) + (where))
#define FUNC(devfn)           ((((devfn) & 7) << 1) | 0xF0)

static int pci_conf2_read_config_byte (BYTE bus, BYTE device_fn,
                                       BYTE where, BYTE *value)
{
  if (device_fn & 0x80)
     return (PCIBIOS_DEVICE_NOT_FOUND);

  DISABLE();
  outb (FUNC (device_fn), 0xCF8);
  outb (bus, 0xCFA);
  *value = inb (IOADDR (device_fn, where));
  outb (0, 0xCF8);
  ENABLE();
  return (PCIBIOS_SUCCESSFUL);
}

static int pci_conf2_read_config_word (BYTE bus, BYTE device_fn,
                                       BYTE where, WORD *value)
{
  if (device_fn & 0x80)
     return (PCIBIOS_DEVICE_NOT_FOUND);

  DISABLE();
  outb (FUNC (device_fn), 0xCF8);
  outb (bus, 0xCFA);
  *value = inw (IOADDR (device_fn, where));
  outb (0, 0xCF8);
  ENABLE();
  return (PCIBIOS_SUCCESSFUL);
}

static int pci_conf2_read_config_dword (BYTE bus, BYTE device_fn,
                                        BYTE where, DWORD *value)
{
  if (device_fn & 0x80)
     return (PCIBIOS_DEVICE_NOT_FOUND);

  DISABLE();
  outb (FUNC (device_fn), 0xCF8);
  outb (bus, 0xCFA);
  *value = inl (IOADDR (device_fn, where));
  outb (0, 0xCF8);
  ENABLE();
  return (PCIBIOS_SUCCESSFUL);
}

static int pci_conf2_write_config_byte (BYTE bus, BYTE device_fn,
                                        BYTE where, BYTE value)
{
  DISABLE();
  outb (FUNC (device_fn), 0xCF8);
  outb (bus, 0xCFA);
  outb (value, IOADDR (device_fn, where));
  outb (0, 0xCF8);
  ENABLE();
  return (PCIBIOS_SUCCESSFUL);
}

static int pci_conf2_write_config_word (BYTE bus, BYTE device_fn,
                                        BYTE where, WORD value)
{
  DISABLE();
  outb (FUNC (device_fn), 0xCF8);
  outb (bus, 0xCFA);
  outw (value, IOADDR (device_fn, where));
  outb (0, 0xCF8);
  ENABLE();
  return (PCIBIOS_SUCCESSFUL);
}

static int pci_conf2_write_config_dword (BYTE bus, BYTE device_fn,
                                         BYTE where, DWORD value)
{
  DISABLE();
  outb (FUNC (device_fn), 0xCF8);
  outb (bus, 0xCFA);
  outl (value, IOADDR (device_fn, where));
  outb (0, 0xCF8);
  ENABLE();
  return (PCIBIOS_SUCCESSFUL);
}

#undef IOADDR
#undef FUNC

/*
 * Function table for type 2
 */
static const struct pci_access pci_direct_conf2 = {
             pci_direct_find_device,
             pci_direct_find_class,
             pci_conf2_read_config_byte,
             pci_conf2_read_config_word,
             pci_conf2_read_config_dword,
             pci_conf2_write_config_byte,
             pci_conf2_write_config_word,
             pci_conf2_write_config_dword
           };

static const struct pci_access *check_direct_pci (void)
{
  const struct pci_access *ret = NULL;
  DWORD tmp;

  DISABLE();

  /* check if configuration type 1 works
   */
  outb (1, 0xCFB);
  tmp = inl (0xCF8);
  outl (0x80000000, 0xCF8);
  if (inl(0xCF8) == 0x80000000)
  {
    outl (tmp, 0xCF8);
    PRINTK (("pcibios_init: Using configuration type 1\n"));
    ret = &pci_direct_conf1;
    goto quit;
  }
  outl (tmp, 0xCF8);

  /* check if configuration type 2 works
   */
  outb (0, 0xCFB);
  outb (0, 0xCF8);
  outb (0, 0xCFA);
  if (inb(0xCF8) == 0 && inb(0xCFB) == 0)
  {
    PRINTK (("pcibios_init: Using configuration type 2\n"));
    ret = &pci_direct_conf2;
  }
  else
  {
    PRINTK (("pcibios_init: Not supported chipset for direct PCI access !\n"));
    ret = NULL;
  }

quit:
  ENABLE();
  return (ret);
}

/*
 * Access defined pcibios functions via the function table
 */
int pcibios_present (void)
{
  return (access_pci ? 1 : 0);
}

int pcibios_find_class (unsigned class_code, WORD index, BYTE *bus, BYTE *device_fn)
{
  if (access_pci && access_pci->find_class)
     return (*access_pci->find_class) (class_code, index, bus, device_fn);

  return (PCIBIOS_FUNC_NOT_SUPPORTED);
}

int pcibios_find_device (WORD vendor, WORD device_id,
                         WORD index, BYTE *bus, BYTE *device_fn)
{
  if (access_pci && access_pci->find_device)
     return (*access_pci->find_device) (vendor, device_id, index, bus, device_fn);

  return (PCIBIOS_FUNC_NOT_SUPPORTED);
}

int pcibios_read_config_byte (BYTE bus, BYTE device_fn, BYTE where, BYTE *value)
{
  if (access_pci && access_pci->read_config_byte)
     return (*access_pci->read_config_byte) (bus, device_fn, where, value);

  return (PCIBIOS_FUNC_NOT_SUPPORTED);
}

int pcibios_read_config_word (BYTE bus, BYTE device_fn, BYTE where, WORD *value)
{
  if (access_pci && access_pci->read_config_word)
     return (*access_pci->read_config_word) (bus, device_fn, where, value);

  return (PCIBIOS_FUNC_NOT_SUPPORTED);
}

int pcibios_read_config_dword (BYTE bus, BYTE device_fn, BYTE where, DWORD *value)
{
  if (access_pci && access_pci->read_config_dword)
     return (*access_pci->read_config_dword) (bus, device_fn, where, value);

  return (PCIBIOS_FUNC_NOT_SUPPORTED);
}

int pcibios_write_config_byte (BYTE bus, BYTE device_fn, BYTE where, BYTE value)
{
  if (access_pci && access_pci->write_config_byte)
     return (*access_pci->write_config_byte) (bus, device_fn, where, value);

  return (PCIBIOS_FUNC_NOT_SUPPORTED);
}

int pcibios_write_config_word (BYTE bus, BYTE device_fn, BYTE where, WORD value)
{
  if (access_pci && access_pci->write_config_word)
     return (*access_pci->write_config_word) (bus, device_fn, where, value);

  return (PCIBIOS_FUNC_NOT_SUPPORTED);
}

int pcibios_write_config_dword (BYTE bus, BYTE device_fn, BYTE where, DWORD value)
{
  if (access_pci && access_pci->write_config_dword)
     return (*access_pci->write_config_dword) (bus, device_fn, where, value);

  return (PCIBIOS_FUNC_NOT_SUPPORTED);
}

const char *pcibios_strerror (int error)
{
  static char buf[80];

  switch (error)
  {
    case PCIBIOS_SUCCESSFUL:
         return ("SUCCESSFUL");

    case PCIBIOS_FUNC_NOT_SUPPORTED:
         return ("FUNC_NOT_SUPPORTED");

    case PCIBIOS_BAD_VENDOR_ID:
         return ("SUCCESSFUL");

    case PCIBIOS_DEVICE_NOT_FOUND:
         return ("DEVICE_NOT_FOUND");

    case PCIBIOS_BAD_REGISTER_NUMBER:
         return ("BAD_REGISTER_NUMBER");

    case PCIBIOS_SET_FAILED:
         return ("SET_FAILED");

    case PCIBIOS_BUFFER_TOO_SMALL:
         return ("BUFFER_TOO_SMALL");

    default:
         sprintf (buf, "UNKNOWN RETURN 0x%x", error);
         return (buf);
  }
}

char *pcibios_setup (const char *str)
{
  (void)str;
  return (NULL);
}

void pcibios_exit (void)
{
#if 1
  PRINTK (("pcibios_exit\n"));
#else
  if (bios32_debug)
     fprintf (stderr, "pcibios_exit\n");
#endif

  if (bios32_api.selector > 0)
  {
    if (__dpmi_free_ldt_descriptor (bios32_api.selector))
       PRINTK (("pcibios_exit: Failed to free BIOS32 descriptor\n"));
    bios32_api.selector = 0;
  }

  if (pci_api.selector > 0)
  {
    if (__dpmi_free_ldt_descriptor (pci_api.selector))
       PRINTK (("pcibios_exit: Failed to free PCI descriptor\n"));
    pci_api.selector = 0;
  }
}

int pcibios_init (void)
{
  BYTE  sum;
  DWORD addr;
  int   i, length;

  /* Follow the standard procedure for locating the BIOS32 Service
   * directory by scanning the permissible address range from
   * 0xE0000 through 0xFFFFF for a valid BIOS32 structure. Blocks
   * must be aligned on paragraphs.
   */
  for (addr = 0xE0000; addr <= 0xFFFF0; addr += 16)
  {
    union bios32 check;
    void (*old_sigsegv)(int);
    void (*old_sigill)(int);

    dosmemget ((DWORD)addr, sizeof(check), &check);
    if (check.fields.signature != BIOS32_SIGNATURE)
       continue;

    length = 16 * check.fields.length;
    if (!length)
       continue;

    sum = 0;
    for (i = 0; i < length; ++i)
        sum += check.chars[i];
    if (sum != 0)
       continue;

    if (check.fields.revision != 0)
    {
      PRINTK (("pcibios_init: unsupported revision %d at %08X\n",
               check.fields.revision, (unsigned)addr));
      continue;
    }

    PRINTK (("pcibios_init: BIOS32 Service Directory structure at %08Xh\n",
            (unsigned)addr));

    if (bios32_api.selector == 0)
    {
      if (check.fields.entry >= 0x100000)
      {
        PRINTK (("pcibios_init: entry in high memory (%08Xh), trying "
                 "direct PCI access\n", (unsigned)check.fields.entry));
        access_pci = check_direct_pci();
      }
      else
      {
        if (!create_descriptor("BIOS32", &bios32_api, check.fields.entry, addr))
           return (0);
      }
    }

    /* We need to trap SIGSEGV/SIGILL in case BIOS32/PCI pmode-handler
     * contains bogus code. E.g. WinNT's virtual-PCI handler is filled
     * with nulls (ADD [EAX],AX ?)
     */
    old_sigsegv = signal (SIGSEGV, sig_handler);
    old_sigill  = signal (SIGILL, sig_handler);
       
    if (check_pcibios())
       access_pci = &pci_bios_access;

    signal (SIGSEGV, old_sigsegv);
    signal (SIGILL, old_sigill);
    return (1);
  }

  PRINTK (("pcibios_init: BIOS32 Service Directory not found\n"));
  return (0);
}

