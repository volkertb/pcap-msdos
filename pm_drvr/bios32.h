/*
 * BIOS32, PCI BIOS functions and defines
 * Copyright 1994, Drew Eckhardt
 * 
 * For more information, please consult 
 * 
 * PCI BIOS Specification Revision
 * PCI Local Bus Specification
 * PCI System Design Guide
 *
 * PCI Special Interest Group
 * P.O. Box 14070
 * Portland, OR 97214
 * U. S. A.
 * Phone: 800-433-5177 / +1-503-797-4207
 * Fax: +1-503-234-6762 
 * 
 * Manuals are $25 each or $50 for all three, plus $7 shipping 
 * within the United States, $35 abroad.
 */

#ifndef __BIOS32_H
#define __BIOS32_H

/*
 * Error values that may be returned by the PCI bios.  Use
 * pcibios_strerror() to convert to a printable string.
 */
#define PCIBIOS_SUCCESSFUL		0x00
#define PCIBIOS_FUNC_NOT_SUPPORTED	0x81
#define PCIBIOS_BAD_VENDOR_ID		0x83
#define PCIBIOS_DEVICE_NOT_FOUND	0x86
#define PCIBIOS_BAD_REGISTER_NUMBER	0x87
#define PCIBIOS_SET_FAILED		0x88
#define PCIBIOS_BUFFER_TOO_SMALL	0x89

int   pcibios_present (void);
int   pcibios_init    (void);
void  pcibios_exit    (void);
char *pcibios_setup   (const char *str);

int   pcibios_find_class  (unsigned class_code, WORD index,
                           BYTE *bus, BYTE *dev_fn);
int   pcibios_find_device (WORD vendor, WORD dev_id,
                           WORD index, BYTE *bus, BYTE *dev_fn);

int   pcibios_read_config_byte  (BYTE bus, BYTE dev_fn,
                                 BYTE where, BYTE *val);
int   pcibios_read_config_word  (BYTE bus, BYTE dev_fn,
                                 BYTE where, WORD *val);
int   pcibios_read_config_dword (BYTE bus, BYTE dev_fn,
                                 BYTE where, DWORD *val);
int   pcibios_write_config_byte (BYTE bus, BYTE dev_fn,
                                 BYTE where, BYTE val);
int   pcibios_write_config_word (BYTE bus, BYTE dev_fn,
                                 BYTE where, WORD val);
int   pcibios_write_config_dword(BYTE bus, BYTE dev_fn,
                                 BYTE where, DWORD val);
const char *pcibios_strerror (int error);

#endif /* BIOS32_H */
