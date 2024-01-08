
/* Operational parameter that usually are not changed.
 */

/* The Vortex size is twice that of the original EtherLinkIII series: the
 * runtime register window, window 1, is now always mapped in.
 * The Boomerang size is twice as large as the Vortex -- it has additional
 * bus master control registers.
 */
#define VORTEX_TOTAL_SIZE     0x20
#define BOOMERANG_TOTAL_SIZE  0x40

/*
 * Theory of Operation
 * 
 * I. Board Compatibility
 * 
 * This device driver is designed for the 3Com FastEtherLink and FastEtherLink
 * XL, 3Com's PCI to 10/100baseT adapters.  It also works with the 10Mbs
 * versions of the FastEtherLink cards.  The supported product IDs are
 * 3c590, 3c592, 3c595, 3c597, 3c900, 3c905
 * 
 * The related ISA 3c515 is supported with a separate driver, 3c515.c, included
 * with the kernel source or available from
 * cesdis.gsfc.nasa.gov:/pub/linux/drivers/3c515.html
 * 
 * II. Board-specific settings
 * 
 * PCI bus devices are configured by the system at boot time, so no jumpers
 * need to be set on the board.  The system BIOS should be set to assign the
 * PCI INTA signal to an otherwise unused system IRQ line.
 * 
 * The EEPROM settings for media type and forced-full-duplex are observed.
 * The EEPROM media type should be left at the default "autoselect" unless using
 * 10base2 or AUI connections which cannot be reliably detected.
 * 
 * III. Driver operation
 * 
 * The 3c59x series use an interface that's very similar to the previous 3c5x9
 * series.  The primary interface is two programmed-I/O FIFOs, with an
 * alternate single-contiguous-region bus-master transfer (see next).
 * 
 * The 3c900 "Boomerang" series uses a full-bus-master interface with separate
 * lists of transmit and receive descriptors, similar to the AMD LANCE/PCnet,
 * DEC Tulip and Intel Speedo3.  The first chip version retains a compatible
 * programmed-I/O interface that has been removed in 'B' and subsequent board
 * revisions.
 * 
 * One extension that is advertised in a very large font is that the adapters
 * are capable of being bus masters.  On the Vortex chip this capability was
 * only for a single contiguous region making it far less useful than the full
 * bus master capability.  There is a significant performance impact of taking
 * an extra interrupt or polling for the completion of each transfer, as well
 * as difficulty sharing the single transfer engine between the transmit and
 * receive threads.  Using DMA transfers is a win only with large blocks or
 * with the flawed versions of the Intel Orion motherboard PCI controller.
 * 
 * The Boomerang chip's full-bus-master interface is useful, and has the
 * currently-unused advantages over other similar chips that queued transmit
 * packets may be reordered and receive buffer groups are associated with a
 * single frame.
 * 
 * With full-bus-master support, this driver uses a "RX_COPYBREAK" scheme.
 * Rather than a fixed intermediate receive buffer, this scheme allocates
 * full-sized skbuffs as receive buffers.  The value RX_COPYBREAK is used as
 * the copying breakpoint: it is chosen to trade-off the memory wasted by
 * passing the full-sized skbuff to the queue layer for all frames vs. the
 * copying cost of copying a frame to a correctly-sized skbuff.
 * 
 * IIIC. Synchronization
 * The driver runs as two independent, single-threaded flows of control.  One
 * is the send-packet routine, which enforces single-threaded use by the
 * dev->tbusy flag.  The other thread is the interrupt handler, which is single
 * threaded by the hardware and other software.
 * 
 * IV. Notes
 * 
 * Thanks to Cameron Spitzer and Terry Murphy of 3Com for providing development
 * 3c590, 3c595, and 3c900 boards.
 * The name "Vortex" is the internal 3Com project name for the PCI ASIC, and
 * the EISA version is called "Demon".  According to Terry these names come
 * from rides at the local amusement park.
 * 
 * The new chips support both ethernet (1.5K) and FDDI (4.5K) packet sizes!
 * This driver only supports ethernet packets because of the skbuff allocation
 * limit of 4K.
 */

/* This table drives the PCI probe routines.  It's mostly boilerplate in all
 * of the drivers, and will likely be provided by some future kernel.
 */
enum pci_flags_bit {
     PCI_USES_IO = 1,
     PCI_USES_MEM = 2,
     PCI_USES_MASTER = 4,
     PCI_ADDR0 = 0x10 << 0,
     PCI_ADDR1 = 0x10 << 1,
     PCI_ADDR2 = 0x10 << 2,
     PCI_ADDR3 = 0x10 << 3,
   };

struct pci_id_info {
       const char *name;
       WORD        vendor_id, device_id, device_id_mask, flags;
       int         drv_flags, io_size;
       struct device *(*probe1) (struct device *dev,
                                 int pci_bus, int pci_devfn, long ioaddr,
                                 int irq, int chip_idx, int fnd_cnt);
     };

enum { IS_VORTEX = 1,
       IS_BOOMERANG = 2,
       IS_CYCLONE = 4,
       HAS_PWR_CTRL = 0x10,
       HAS_MII = 0x20,
       HAS_NWAY = 0x40,
       HAS_CB_FNS = 0x80,
     };

extern int debug_3c575;

extern int tc90x_probe (struct device *dev);

