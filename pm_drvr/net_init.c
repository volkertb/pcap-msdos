/* netdrv_init.c: Initialization for network devices. */
/*
 * Written 1993,1994,1995 by Donald Becker.
 * 
 * The author may be reached as becker@cesdis.gsfc.nasa.gov or
 * C/O Center of Excellence in Space Data and Information Sciences
 * Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
 * 
 * This file contains the initialization for the "pl14+" style ethernet
 * drivers.  It should eventually replace most of drivers/net/Space.c.
 * It's primary advantage is that it's able to allocate low-memory buffers.
 * A secondary advantage is that the dangerous NE*000 netcards can reserve
 * their I/O port region before the SCSI probes start.
 * 
 * Modifications/additions by Bjorn Ekwall <bj0rn@blox.se>:
 * ethdev_index[MAX_ETH_CARDS]
 * register_netdev() / unregister_netdev()
 * 
 * Modifications by Wolfgang Walter
 * Use dev_close cleanly so we always shut things down tidily.
 * 
 * Changed 29/10/95, Alan Cox to pass sockaddr's around for mac addresses.
 * 
 * 14/06/96 - Paul Gortmaker:  Add generic eth_change_mtu() function. 
 * 24/09/96 - Paul Norton: Add token-ring variants of the netdev functions. 
 */

#include "pmdrvr.h"
#include "module.h"

/* The network devices currently exist only in the socket namespace, so these
 * entries are unused.  The only ones that make sense are
 * open  - start the ethercard
 * close - stop  the ethercard
 * ioctl - To get statistics, perhaps set the interface port (AUI, BNC, etc.)
 *
 * One can also imagine getting raw packets using
 * read & write
 * but this is probably better handled by a raw packet socket.
 * 
 * Given that almost all of these functions are handled in the current
 * socket-based scheme, putting ethercard devices in /dev/ seems pointless.
 * 
 * [Removed all support for /dev network devices. When someone adds
 * streams then by magic we get them, but otherwise they are un-needed
 * and a space waste]
 */

/* The list of used and available "eth" slots (for "eth0", "eth1", etc.)
 */
#define MAX_ETH_CARDS NUM_IRQS
static struct device *ethdev_index[MAX_ETH_CARDS];

#define dev_init_buffers(dev) ((void)0)  /* !! no need */

/* Fill in the fields of the device structure with ethernet-generic values.
 * 
 * If no device structure is passed, a new one is constructed, complete with
 * a SIZEOF_PRIVATE private data area.
 * 
 * If an empty string area is passed as dev->name, or a new structure is made,
 * a new name string is constructed.  The passed string area should be 8 bytes
 * long.
 */

struct device *init_etherdev (struct device *dev, int sizeof_priv)
{
  int i, new_device = 0;

  /* Use an existing correctly named device in dev_base list
   */
  if (dev == NULL)
  {
    int alloc_size = sizeof(struct device) + sizeof("eth%d  ") +
                     sizeof_priv + 3;
    struct device *cur_dev;
    char   pname[8];          /* Putative name for the device. */

    for (i = 0; i < MAX_ETH_CARDS; ++i)
    {
      if (ethdev_index[i] == NULL)
      {
	sprintf (pname, "eth%d", i);
	for (cur_dev = dev_base; cur_dev; cur_dev = cur_dev->next)
        {
          if (!strcmp (pname, cur_dev->name))
	  {
	    dev = cur_dev;
            dev->probe = NULL;
            dev->priv  = sizeof_priv ? k_calloc (sizeof_priv,1) : NULL;
	    goto found;
	  }
        }
      }
    }
    dev = k_calloc (alloc_size, 1);
    if (sizeof_priv)
       dev->priv = (void*) (dev + 1);
    dev->name  = sizeof_priv + (char*) (dev + 1);
    new_device = 1;
  }

found:				/* From the double loop above. */

  if (dev->name && ((dev->name[0] == '\0') || (dev->name[0] == ' ')))
  {
    for (i = 0; i < MAX_ETH_CARDS; ++i)
      if (ethdev_index[i] == NULL)
      {
	sprintf (dev->name, "eth%d", i);
	ethdev_index[i] = dev;
	break;
      }
  }

  ether_setup (dev);		/* Hmmm, should this be called here? */

  if (new_device)
  {
    /* Append the device to the device queue. */
    struct device **old_devp = &dev_base;

    while ((*old_devp)->next)
      old_devp = &(*old_devp)->next;
    (*old_devp)->next = dev;
    dev->next = 0;
  }
  return (dev);
}


void ether_setup (struct device *dev)
{
  int i;

  /* Fill in the fields of the device structure with ethernet-generic values.
   * This should be in a common file instead of per-driver.
   */
  dev_init_buffers (dev);

  /* register boot-defined "eth" devices
   */
  if (dev->name && !strncmp (dev->name, "eth", 3))
  {
    i = strtoul (dev->name + 3, NULL, 0);
    if (ethdev_index[i] == NULL)
        ethdev_index[i] = dev;
    else if (dev != ethdev_index[i])
        printk ("ether_setup: Ouch! Someone else took %s\n", dev->name);
  }

  dev->mtu      = ETH_MTU;
  dev->addr_len = ETH_ALEN;
  memset (dev->broadcast, 0xFF, ETH_ALEN);

  /* New-style flags. */
  dev->flags = IFF_BROADCAST | IFF_MULTICAST;
}


int register_netdev (struct device *dev)
{
  struct device *d = dev_base;
  int    i = MAX_ETH_CARDS;

  DISABLE();

  if (dev && dev->probe)
  {
    if (dev->name && (!dev->name[0] || dev->name[0] == ' '))
    {
      for (i = 0; i < MAX_ETH_CARDS; ++i)
	if (ethdev_index[i] == NULL)
	{
	  sprintf (dev->name, "eth%d", i);
          printk ("probing for device '%s'...\n", dev->name);
	  ethdev_index[i] = dev;
	  break;
	}
    }

    ENABLE();            /* device probes assume interrupts enabled */
    if (!(*dev->probe)(dev))
    {
      if (i < MAX_ETH_CARDS)
	ethdev_index[i] = NULL;
      return -EIO;
    }
    DISABLE();

    /* Add device to end of chain */
    if (dev_base)
    {
      while (d->next)
	d = d->next;
      d->next = dev;
    }
    else
      dev_base = dev;
    dev->next = NULL;
  }
  ENABLE();
  return (0);
}


#ifdef NOT_USED /* only for dynamically loaded modules */

void unregister_netdev (struct device *dev)
{
  int i;

  DISABLE();
  if (dev == NULL)
  {
    printk ("was NULL\n");
    return;
  }
  if (dev->start)
     printk ("ERROR '%s' is busy.\n", dev->name);

  if (dev_base == dev)
    dev_base = dev->next;
  else
  {
    struct device *d = dev_base;

    while (d && (d->next != dev))
      d = d->next;

    if (d && (d->next == dev))
       d->next = dev->next;
    else
    {
      printk ("unregister_netdev: '%s' not found\n", dev->name);
      ENABLE();
      return;
    }
  }
  for (i = 0; i < MAX_ETH_CARDS; ++i)
  {
    if (ethdev_index[i] == dev)
    {
      ethdev_index[i] = NULL;
      break;
    }
  }
  ENABLE();

  /*
   *  You can i.e use a interfaces in a route though it is not up.
   *  We call close_dev (which is changed: it will down a device even if
   *  dev->flags==0 (but it will not call dev->stop if IFF_UP
   *  is not set).
   *  This will call notifier_call_chain(&netdev_chain, NETDEV_DOWN, dev),
   *  dev_mc_discard(dev), ....
   */
  dev_close (dev);
}
#endif

