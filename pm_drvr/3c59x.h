#ifndef __3C59X_H
#define __3C59X_H

/* "Knobs" that turn on special features.
 * Enable the experimental automatic media selection code
 */
#define AUTOMEDIA 1

/*
 * Allow the use of bus master transfers instead of programmed-I/O for the
 * Tx process.  Bus master transfers are always disabled by default, but
 * if this is set they may be turned on using 'options'
 */
#define VORTEX_BUS_MASTER

/* "Knobs" for adjusting internal parameters.
 * Put out somewhat more debugging messages.
 * (0 - no msg, 1 minimal msgs)
 */
#ifndef VORTEX_DEBUG
#define VORTEX_DEBUG 2
#endif

/* Number of times to check to see if the Tx FIFO has space, used in some
 *  limited cases
 */
#define WAIT_TX_AVAIL 200

/* Operational parameter that usually are not changed
 */
#define TX_TIMEOUT  40    /* Time in jiffies before concluding Tx hung */

/* The total size is twice that of the original EtherLinkIII series: the
 * runtime register window, window 1, is now always mapped in
 */
#define VORTEX_TOTAL_SIZE 0x20

#define DEMON_INDEX 5 /* Caution!  Must be consistent with above! */

/*
       Theory of Operation

I. Board Compatibility

This device driver is designed for the 3Com FastEtherLink, 3Com's PCI to
10/100baseT adapter.  It also works with the 3c590, a similar product
with only a 10Mbs interface.

II. Board-specific settings

PCI bus devices are configured by the system at boot time, so no jumpers
need to be set on the board.  The system BIOS should be set to assign the
PCI INTA signal to an otherwise unused system IRQ line.  While it's
physically possible to share PCI interrupt lines, the 1.2.0 kernel doesn't
support it.

III. Driver operation

The 3c59x series use an interface that's very similar to the previous 3c5x9
series.  The primary interface is two programmed-I/O FIFOs, with an
alternate single-contiguous-region bus-master transfer (see next).

One extension that is advertised in a very large font is that the adapters
are capable of being bus masters.  Unfortunately this capability is only for
a single contiguous region making it less useful than the list of transfer
regions available with the DEC Tulip or AMD PCnet.  Given the significant
performance impact of taking an extra interrupt for each transfer, using
DMA transfers is a win only with large blocks.

IIIC. Synchronization
The driver runs as two independent, single-threaded flows of control.  One
is the send-packet routine, which enforces single-threaded use by the
dev->tx_busy flag.  The other thread is the interrupt handler, which is
single threaded by the hardware and other software.

IV. Notes

Thanks to Cameron Spitzer and Terry Murphy of 3Com for providing both
3c590 and 3c595 boards.
The name "Vortex" is the internal 3Com project name for the PCI ASIC, and
the EISA version is called "Demon".  According to Terry these names come
from rides at the local amusement park.

The new chips support both ethernet (1.5K) and FDDI (4.5K) packet sizes!
This driver only supports ethernet packets because of the buffer allocation
limit of 4K.
*/


/* Operational definitions.
 * These are not used by other compilation units and thus are not
 * exported in a ".h" file.
 *
 * First the windows.  There are eight register windows, with the command
 * and status registers available in each.
 */
#define EL3WINDOW(win_num) outw(SelectWindow + (win_num), ioaddr + EL3_CMD)
#define EL3_CMD            0x0e
#define EL3_STATUS         0x0e

/* The top five bits written to EL3_CMD are a command, the lower
 * 11 bits are the parameter, if applicable.
 * Note that 11 parameters bits was fine for ethernet, but the new chip
 * can handle FDDI length frames (~4500 octets) and now parameters count
 * 32-bit 'Dwords' rather than octets
 */

enum vortex_cmd {
     TotalReset     = 0<<11,
     SelectWindow   = 1<<11,
     StartCoax      = 2<<11,
     RxDisable      = 3<<11,
     RxEnable       = 4<<11,
     RxReset        = 5<<11,
     RxDiscard      = 8<<11,
     TxEnable       = 9<<11,
     TxDisable      = 10<<11,
     TxReset        = 11<<11,
     FakeIntr       = 12<<11,
     AckIntr        = 13<<11,
     SetIntrEnb     = 14<<11,
     SetStatusEnb   = 15<<11,
     SetRxFilter    = 16<<11,
     SetRxThreshold = 17<<11,
     SetTxThreshold = 18<<11,
     SetTxStart     = 19<<11,
     StartDMAUp     = 20<<11,
     StartDMADown   = (20<<11)+1,
     StatsEnable    = 21<<11,
     StatsDisable   = 22<<11,
     StopCoax       = 23<<11
   };

/* The SetRxFilter command accepts the following classes:
 */
enum RxFilter {
     RxStation   = 1,
     RxMulticast = 2,
     RxBroadcast = 4,
     RxProm      = 8
   };

/* Bits in the general status register
 */
enum vortex_status {
     IntLatch       = 0x0001,
     AdapterFailure = 0x0002,
     TxComplete     = 0x0004,
     TxAvailable    = 0x0008,
     RxComplete     = 0x0010,
     RxEarly        = 0x0020,
     IntReq         = 0x0040,
     StatsFull      = 0x0080,
     DMADone        = 1<<8,
     DMAInProgress  = 1<<11,      /* DMA controller is still busy.*/
     CmdInProgress  = 1<<12       /* EL3_CMD is still busy.*/
   };

/* Register window 1 offsets, the window used in normal operation.
 * On the Vortex this window is always mapped at offsets 0x10-0x1f
 */
enum Window1 {
     TX_FIFO  = 0x10,
     RX_FIFO  = 0x10,
     RxErrors = 0x14,
     RxStatus = 0x18,
     Timer    = 0x1A,
     TxStatus = 0x1B,
     TxFree   = 0x1C  /* Remaining free bytes in Tx buffer. */
   };

enum Window0 {
     Wn0EepromCmd  = 10,  /* Window 0: EEPROM command register. */
     Wn0EepromData = 12,  /* Window 0: EEPROM results register. */
   };

enum Win0_EEPROM_bits {
     EEPROM_Read  = 0x80,
     EEPROM_WRITE = 0x40,
     EEPROM_ERASE = 0xC0,
     EEPROM_EWENB = 0x30,    /* Enable erasing/writing for 10 msec. */
     EEPROM_EWDIS = 0x00,    /* Disable EWENB before 10 msec timeout. */
   };

/* EEPROM locations
 */
enum eeprom_offset {
     PhysAddr01   = 0,
     PhysAddr23   = 1,
     PhysAddr45   = 2,
     ModelID      = 3,
     EtherLink3ID = 7,
     IFXcvrIO     = 8,
     IRQLine      = 9,
     NodeAddr01   = 10,
     NodeAddr23   = 11,
     NodeAddr45   = 12,
     DriverTune   = 13,
     Checksum     = 15
   };

enum Window3 {      /* Window 3: MAC/config bits. */
     Wn3_Config   = 0,
     Wn3_MAC_Ctrl = 6,
     Wn3_Options  = 8
   };

union wn3_config {
      long i;
      struct w3_config_fields {
        DWORD ram_size:3,  ram_width:1;
        DWORD ram_speed:2, rom_size:2;
        int   pad8:8;
        DWORD ram_split:2, pad18:2;
        DWORD xcvr:3, pad21:1, autoselect:1;
        int   pad24:8;
      } u;
    };

enum Window4 {
     Wn4_Media = 0x0A,    /* Window 4: Various transcvr/media bits. */
   };

enum Win4_Media_bits {
     Media_SQE     = 0x0008,  /* Enable SQE error counting for AUI. */
     Media_10TP    = 0x00C0,  /* Enable link beat and jabber for 10baseT. */
     Media_Lnk     = 0x0080,  /* Enable just link beat for 100TX/100FX. */
     Media_LnkBeat = 0x0800,
   };

enum Window7 {             /* Window 7: Bus Master control. */
     Wn7_MasterAddr   = 0,
     Wn7_MasterLen    = 6,
     Wn7_MasterStatus = 12
   };

extern int vortex_options[8];

extern int tc59x_probe (struct device *dev);

#endif
