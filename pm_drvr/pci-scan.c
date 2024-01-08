/* pci-scan.c: Linux PCI network adapter support code. */
/*
 * Originally written 1999-2001 by Donald Becker.
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License (GPL), incorporated herein by
 * reference.  Drivers interacting with these functions are derivative
 * works and thus are covered the GPL.  They must include an explicit
 * GPL notice.
 * 
 * This code provides common scan and activate functions for PCI network
 * interfaces.
 * 
 * The author may be reached as becker@scyld.com, or
 * Donald Becker
 * Scyld Computing Corporation
 * 410 Severn Ave., Suite 210
 * Annapolis MD 21403
 * 
 * Other contributers:
 */
static const char version[] = "pci-scan.c:v1.08 9/28/2001  Donald Becker "
                              "<becker@scyld.com> http://www.scyld.com/linux/drivers.html\n";

/* A few user-configurable values that may be modified when a module. */

static int debug = 1;           /* 1 normal messages, 0 quiet .. 7 verbose. */
static int min_pci_latency = 32;

#include "pmdrvr.h"
#include "bios32.h"
#include "pci.h"
#include "pci-scan.h"

#define PCI_CAPABILITY_LIST  0x34 /* Offset of first capability list entry */
#define PCI_STATUS_CAP_LIST  0x10 /* Support Capability List */
#define PCI_CAP_ID_PM        0x01 /* Power Management */

int  (*register_cb_hook) (struct drv_id_info * did);
void (*unregister_cb_hook) (struct drv_id_info * did);

/* List of registered drivers.
 */
static struct drv_id_info *drv_list;

/* List of detected PCI devices, for APM events.
 */
static struct dev_info {
       struct dev_info    *next;
       void               *dev;
       struct drv_id_info *drv_id;
       int                 flags;
     } *dev_list;

/*
 * This code is not intended to support every configuration.
 * It is intended to minimize duplicated code by providing the functions
 * needed in almost every PCI driver.
 * 
 * The "no kitchen sink" policy:
 * Additional features and code will be added to this module only if more
 * than half of the drivers for common hardware would benefit from the feature.
 */

/*
 * Ideally we would detect and number all cards of a type (e.g. network) in
 * PCI slot order.
 * But that does not work with hot-swap card, CardBus cards and added drivers.
 * So instead we detect just the each chip table in slot order.
 * 
 * This routine takes a PCI ID table, scans the PCI bus, and calls the
 * associated attach/probe1 routine with the hardware already activated and
 * single I/O or memory address already mapped.
 * 
 * This routine will later be supplemented with CardBus and hot-swap PCI
 * support using the same table.  Thus the pci_chip_tbl[] should not be
 * marked as __initdata.
 */

/*
 * Grrrr.. complex abstaction layers with negative benefit.
 */
int pci_drv_register (struct drv_id_info *drv_id, void *initial_device)
{
  struct pci_dev     *pdev = NULL;
  struct pci_id_info *pci_tbl = drv_id->pci_dev_tbl;
  struct drv_id_info *drv;
  int    chip_idx, cards_found = 0;
  void  *newdev;

  /* Ignore a double-register attempt. */
  for (drv = drv_list; drv; drv = drv->next)
      if (drv == drv_id)
         return (-EBUSY);

  while ((pdev = pci_find_class (drv_id->pci_class, pdev)) != 0)
  {
    DWORD pci_id, pci_subsys_id, pci_class_rev;
    WORD  pci_command, new_command;
    int   pci_flags;
    long  pciaddr;              /* Bus address. */
    long  ioaddr = 0;           /* Mapped address for this processor. */

    pci_read_config_dword (pdev, PCI_VENDOR_ID, &pci_id);

    /* Offset 0x2c is PCI_SUBSYSTEM_ID aka PCI_SUBSYSTEM_VENDOR_ID.
     */
    pci_read_config_dword (pdev, 0x2c, &pci_subsys_id);
    pci_read_config_dword (pdev, PCI_REVISION_ID, &pci_class_rev);

    if (debug > 3)
       printk ("PCI ID %08x subsystem ID is %08x.\n", pci_id, pci_subsys_id);
    for (chip_idx = 0; pci_tbl[chip_idx].name; chip_idx++)
    {
      struct pci_id_info *chip = &pci_tbl[chip_idx];

      if ((pci_id & chip->id.pci_mask) == chip->id.pci
          && (pci_subsys_id & chip->id.subsystem_mask) == chip->id.subsystem
          && (pci_class_rev & chip->id.revision_mask) == chip->id.revision)
        break;
    }
    if (pci_tbl[chip_idx].name == 0) /* Compiled out! */
       continue;

    pci_flags = pci_tbl[chip_idx].pci_flags;
    pciaddr = pdev->base_address[(pci_flags >> 4) & 7];
    if (debug > 2)
       printk ("Found %s at PCI address 0x%x, mapped IRQ %d.\n",
               pci_tbl[chip_idx].name, pciaddr, pdev->irq);

    if (!(pci_flags & PCI_UNUSED_IRQ) && (pdev->irq == 0 || pdev->irq == 255))
    {
      if (pdev->bus->number == 32) /* Broken CardBus activation. */
           printk ("Resources for CardBus device '%s' have"
                   " not been allocated.\nActivation has been delayed.\n",
                   pci_tbl[chip_idx].name);
      else printk ("PCI device '%s' was not assigned an IRQ.\n"
                   "It will not be activated.\n", pci_tbl[chip_idx].name);
      continue;
    }

    if (pci_flags & PCI_BASE_ADDRESS_SPACE_IO)
       ioaddr = pciaddr & PCI_BASE_ADDRESS_IO_MASK;

    if (!(pci_flags & PCI_NO_ACPI_WAKE))
       acpi_wake (pdev);

    pci_read_config_word (pdev, PCI_COMMAND, &pci_command);
    new_command = pci_command | (pci_flags & 7);
    if (pci_command != new_command)
    {
      printk ("  The PCI BIOS has not enabled the"
              " device at %d/%d!  Updating PCI command %04x->%04x.\n",
              pdev->bus->number, pdev->devfn, pci_command, new_command);
      pci_write_config_word (pdev, PCI_COMMAND, new_command);
    }

    newdev = (*drv_id->probe1) (pdev, initial_device, ioaddr, pdev->irq,
                                chip_idx, cards_found);
    if (newdev == NULL)
       continue;

    initial_device = 0;
    cards_found++;
    if (pci_flags & PCI_COMMAND_MASTER)
    {
      pci_set_master (pdev);
      if (!(pci_flags & PCI_NO_MIN_LATENCY))
      {
        BYTE pci_latency;

        pci_read_config_byte (pdev, PCI_LATENCY_TIMER, &pci_latency);
        if (pci_latency < min_pci_latency)
        {
          printk ("  PCI latency timer (CFLT) is "
                  "unreasonably low at %d.  Setting to %d clocks.\n", pci_latency, min_pci_latency);
          pci_write_config_byte (pdev, PCI_LATENCY_TIMER, min_pci_latency);
        }
      }
    }
    {
      struct dev_info *devp = k_malloc (sizeof(struct dev_info));

      if (!devp)
         continue;
      devp->next   = dev_list;
      devp->dev    = newdev;
      devp->drv_id = drv_id;
      dev_list     = devp;
    }
  }

  if (((drv_id->flags & PCI_HOTSWAP) && register_cb_hook &&
      (*register_cb_hook) (drv_id) == 0) || cards_found)
  {
    drv_id->next = drv_list;
    drv_list = drv_id;
    return (0);
  }
  return (-ENODEV);
}

void pci_drv_unregister (struct drv_id_info *drv_id)
{
  struct drv_id_info **drvp;
  struct dev_info    **devip = &dev_list;

  if (unregister_cb_hook)
    (*unregister_cb_hook) (drv_id);

  for (drvp = &drv_list; *drvp; drvp = &(*drvp)->next)
      if (*drvp == drv_id)
      {
        *drvp = (*drvp)->next;
        break;
      }

  while (*devip)
  {
    struct dev_info *thisdevi = *devip;

    if (thisdevi->drv_id == drv_id)
    {
      *devip = thisdevi->next;
      k_free (thisdevi);
    }
    else
      devip = &(*devip)->next;
  }
}

/*
 * Search PCI configuration space for the specified capability registers.
 * Return the index, or 0 on failure.
 */
int pci_find_capability (struct pci_dev *pdev, int findtype)
{
  WORD pci_status, cap_type;
  BYTE pci_cap_idx;
  int  cap_idx;

  pci_read_config_word (pdev, PCI_STATUS, &pci_status);
  if (!(pci_status & PCI_STATUS_CAP_LIST))
     return (0);

  pci_read_config_byte (pdev, PCI_CAPABILITY_LIST, &pci_cap_idx);
  cap_idx = pci_cap_idx;
  for (cap_idx = pci_cap_idx; cap_idx; cap_idx = (cap_type >> 8) & 0xff)
  {
    pci_read_config_word (pdev, cap_idx, &cap_type);
    if ((cap_type & 0xff) == findtype)
       return (cap_idx);
  }
  return (0);
}


/* Change a device from D3 (sleep) to D0 (active).
 * Return the old power state.
 * This is more complicated than you might first expect since most cards
 * forget all PCI config info during the transition!
 */
int acpi_wake (struct pci_dev *pdev)
{
  DWORD base[5], romaddr;
  WORD  pci_command, pwr_command;
  BYTE  pci_latency, pci_cacheline, irq;
  int   i, pwr_cmd_idx = pci_find_capability (pdev, PCI_CAP_ID_PM);

  if (pwr_cmd_idx == 0)
     return (0);

  pci_read_config_word (pdev, pwr_cmd_idx + 4, &pwr_command);
  if ((pwr_command & 3) == 0)
     return (0);

  pci_read_config_word (pdev, PCI_COMMAND, &pci_command);
  for (i = 0; i < 5; i++)
      pci_read_config_dword (pdev, PCI_BASE_ADDRESS_0 + i * 4, &base[i]);
  pci_read_config_dword (pdev, PCI_ROM_ADDRESS, &romaddr);
  pci_read_config_byte (pdev, PCI_LATENCY_TIMER, &pci_latency);
  pci_read_config_byte (pdev, PCI_CACHE_LINE_SIZE, &pci_cacheline);
  pci_read_config_byte (pdev, PCI_INTERRUPT_LINE, &irq);

  pci_write_config_word (pdev, pwr_cmd_idx + 4, 0x0000);
  for (i = 0; i < 5; i++)
      if (base[i])
         pci_write_config_dword (pdev, PCI_BASE_ADDRESS_0 + i * 4, base[i]);
  pci_write_config_dword (pdev, PCI_ROM_ADDRESS, romaddr);
  pci_write_config_byte (pdev, PCI_INTERRUPT_LINE, irq);
  pci_write_config_byte (pdev, PCI_CACHE_LINE_SIZE, pci_cacheline);
  pci_write_config_byte (pdev, PCI_LATENCY_TIMER, pci_latency);
  pci_write_config_word (pdev, PCI_COMMAND, pci_command | 5);
  return (pwr_command & 3);
}

int acpi_set_pwr_state (struct pci_dev *pdev, enum acpi_pwr_state new_state)
{
  WORD pwr_command;
  int  pwr_cmd_idx = pci_find_capability (pdev, PCI_CAP_ID_PM);

  if (pwr_cmd_idx == 0)
     return (0);

  pci_read_config_word (pdev, pwr_cmd_idx + 4, &pwr_command);
  if ((pwr_command & 3) == ACPI_D3 && new_state != ACPI_D3)
     acpi_wake (pdev);           /* The complicated sequence. */

  pci_write_config_word (pdev, pwr_cmd_idx + 4, (pwr_command & ~3) | new_state);
  return (pwr_command & 3);
}

