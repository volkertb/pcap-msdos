/*
 * drivers/pci/pci.c
 *
 * PCI services that are built on top of the BIOS32 service.
 *
 * Copyright 1993, 1994, 1995 Drew Eckhardt, Frederic Potter,
 * David Mosberger-Tang
 *
 * Apr 12, 1998 : Fixed handling of alien header types. [mj]
 */

#include "pmdrvr.h"
#include "bios32.h"
#include "pci.h"

#define PRINTK(x) do {             \
                    if (pci_debug) \
                       printk x ;  \
                  } while (0)


/*
 * Convert some of the configuration space registers of the device at
 * address (bus,devfn) into a string (possibly several lines each).
 * The configuration string is stored starting at buf[len].  If the
 * string would exceed the size of the buffer (SIZE), 0 is returned.
 */
static int sprint_dev_config (struct pci_dev *dev, char *buf, int size)
{
  DWORD       base, l, class_rev;
  unsigned    bus, devfn, last_reg;
  WORD        vendor, device, status;
  BYTE        bist, latency, min_gnt, max_lat, hdr_type;
  int         reg, len = 0;
  const char *str;

  bus   = dev->bus->number;
  devfn = dev->devfn;

  pcibios_read_config_byte (bus, devfn, PCI_HEADER_TYPE, &hdr_type);
  pcibios_read_config_dword(bus, devfn, PCI_CLASS_REVISION, &class_rev);
  pcibios_read_config_word (bus, devfn, PCI_VENDOR_ID, &vendor);
  pcibios_read_config_word (bus, devfn, PCI_DEVICE_ID, &device);
  pcibios_read_config_word (bus, devfn, PCI_STATUS, &status);
  pcibios_read_config_byte (bus, devfn, PCI_BIST, &bist);
  pcibios_read_config_byte (bus, devfn, PCI_LATENCY_TIMER, &latency);
  pcibios_read_config_byte (bus, devfn, PCI_MIN_GNT, &min_gnt);
  pcibios_read_config_byte (bus, devfn, PCI_MAX_LAT, &max_lat);
  if (len + 80 > size)
     return (-1);

  len += sprintf (buf+len, "  Bus %2d, device %3d, function %2d:\n",
                  bus, PCI_SLOT(devfn), PCI_FUNC(devfn));

  if (len + 80 > size)
     return (-1);

  len += sprintf (buf+len, "    %s: %s %s (rev %d).\n      ",
                  pci_strclass (class_rev >> 8),
                  pci_strvendor (vendor),
                  pci_strdev (vendor, device),
                  (unsigned)class_rev & 0xff);

  if (!pci_lookup_dev (vendor, device))
     len += sprintf (buf+len, "Vendor id=%x. Device id=%x.\n      ",
                     vendor, device);

  str = 0;              /* to keep gcc shut... */

  switch (status & PCI_STATUS_DEVSEL_MASK)
  {
    case PCI_STATUS_DEVSEL_FAST:
         str = "Fast devsel.  ";
         break;
    case PCI_STATUS_DEVSEL_MEDIUM:
         str = "Medium devsel.  ";
         break;
    case PCI_STATUS_DEVSEL_SLOW:
         str = "Slow devsel.  ";
         break;
  }
  if (len + strlen (str) > size)
     return (-1);

  len += sprintf (buf+len, str);

  if (status & PCI_STATUS_FAST_BACK)
  {
#define fast_b2b_capable "Fast back-to-back capable.  "
    if (len + strlen(fast_b2b_capable) > size)
       return (-1);

    len += sprintf (buf+len, fast_b2b_capable);
#undef fast_b2b_capable
  }

  if (bist & PCI_BIST_CAPABLE)
  {
#define BIST_capable     "BIST capable.  "
    if (len + strlen(BIST_capable) > size)
       return (-1);

    len += sprintf (buf+len, BIST_capable);
#undef BIST_capable
  }

  /* !! pcibios_fixup() should correct this
   */
  if (dev->irq > 0 && dev->irq < 255)
  {
    if (len + 40 > size)
       return (-1);

    len += sprintf (buf+len, "IRQ %d.  ", dev->irq);
  }

  if (dev->master)
  {
    if (len + 80 > size)
       return (-1);

    len += sprintf (buf+len, "Master Capable.  ");
    if (latency)
         len += sprintf (buf+len, "Latency=%d.  ", latency);
    else len += sprintf (buf+len, "No bursts.  ");

    if (min_gnt)
       len += sprintf (buf+len, "Min Gnt=%d.", min_gnt);
    if (max_lat)
       len += sprintf (buf+len, "Max Lat=%d.", max_lat);
  }

  switch (hdr_type & 0x7f)
  {
    case 0:
         last_reg = PCI_BASE_ADDRESS_5;
         break;
    case 1:
         last_reg = PCI_BASE_ADDRESS_1;
         break;
    default:
         last_reg = 0;
  }

  for (reg = PCI_BASE_ADDRESS_0; reg <= last_reg; reg += 4)
  {
    if (len + 40 > size)
       return (-1);

    pcibios_read_config_dword (bus, devfn, reg, &l);
    base = l;
    if (!base)
       continue;

    if (base & PCI_BASE_ADDRESS_SPACE_IO)
       len += sprintf (buf+len, "\n      I/O at 0x%lx.",
                       base & PCI_BASE_ADDRESS_IO_MASK);
    else
    {
      const char *pref, *type = "unknown";

      if (base & PCI_BASE_ADDRESS_MEM_PREFETCH)
           pref = "P";
      else pref = "Non-p";

      switch (base & PCI_BASE_ADDRESS_MEM_TYPE_MASK)
      {
        case PCI_BASE_ADDRESS_MEM_TYPE_32:
             type = "32 bit";
             break;
        case PCI_BASE_ADDRESS_MEM_TYPE_1M:
             type = "20 bit";
             break;
        case PCI_BASE_ADDRESS_MEM_TYPE_64:
             type = "64 bit";
             /* read top 32 bit address of base addr: */
             reg += 4;
             pcibios_read_config_dword (bus, devfn, reg, &l);
             base |= ((uint64) l) << 32;
             break;
      }
      len += sprintf (buf+len,
                      "\n      %srefetchable %s memory at 0x%lx.",
                      pref, type, base & PCI_BASE_ADDRESS_MEM_MASK);
    }
  }
  len += sprintf (buf+len, "\n");
  return (len);
}

/*
 * Print list of PCI devices.
 */
static int print_pci_list (void)
{
  struct pci_dev *dev;
  int    num = 0;

  printf ("PCI devices found:\n");

  for (dev = pci_devices; dev; dev = dev->next)
  {
    char buf[512];
    int  len;

    buf[0] = '\0';
    len = sprint_dev_config (dev, buf, sizeof(buf));
    printf ("%s%s", buf, len < 0 ? "truncated..\n\n" : "\n");
    num++;
  }
  return (num);
}

int main (void)
{
#ifdef USE_SECTION_LOCKING
  lock_sections();
#endif

  _printk_init (32*1024, NULL);
  bios32_debug = 2;
  pci_debug    = 2;

  printk ("Probing PCI hardware.\n");

  if (!pci_init())
     return (0);

  print_pci_list();
  return (1);
}
