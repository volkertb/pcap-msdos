#ifndef __PMODE_MAC_DRIVER
#define __PMODE_MAC_DRIVER

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <time.h>
#include <dos.h>
#include <sys/cdefs.h>
#include <sys/swap.h>    /* intel16(), intel() */

#ifndef __BORLANDC__
#include <unistd.h>
#endif

#ifdef __DJGPP__
  #include <dpmi.h>
  #include <go32.h>
  #include <sys/farptr.h>
  #include <sys/exceptn.h>

  #define DISABLE()      __asm__ __volatile__ ("cli")
  #define ENABLE()       __asm__ __volatile__ ("sti")
  #define NOP()          __asm__ __volatile__ ("nop")
  #define STI()          ENABLE()
  #define CLI()          DISABLE()

  #if !defined(__OPTIMIZE__) && !defined(GCC_O0_DEBUG)
  #warning You must compile with atleast -O2 option
  #endif
#endif

#if defined(_MODULE)
#undef USE_SECTION_LOCKING
#endif

struct device;  /* forward */

#include "iface.h"
#include "lock.h"
#include "ioport.h"
#include "../../pcap-dos.h"
#include "../../pcap-int.h"

#define NUM_IRQS        16    /* IRQ 0-15 */

#define __LITTLE_ENDIAN
#define BITS_PER_LONG   32

#include "kmalloc.h"
#include "bitops.h"
#include "timer.h"
#include "dma.h"
#include "irq.h"
#include "printk.h"

#if defined(USE_FORTIFY)
#include "../fortify.h"    /* use Fortify malloc library */
#endif

#ifndef min
#define min(x,y)       (((x) < (y)) ? (x) : (y))
#endif

#ifndef max
#define max(x,y)       (((x) > (y)) ? (x) : (y))
#endif

#define STATIC static
#define PUBLIC

#define loBYTE(w)        (BYTE)(w)
#define hiBYTE(w)        (BYTE)((WORD)(w) >> 8)
#define DIM(array)       (sizeof(array) / sizeof(array[0]))

#define __init
#define __initdata

#define MAX_MCAST        20
#define PAGE_SHIFT       12
#define PAGE_OFFSET      0    /* 0xC0000000 for Linux */
#undef  HZ
#define HZ               1000
#define RUN_AT(x)        (jiffies + (x))
#define udelay(usec)     delay (1+(usec/1000))

#define VIRT_TO_PHYS(a)  ((DWORD)(a) + _virtual_base)
#define VIRT_TO_BUS(a)   VIRT_TO_PHYS(a)

#define PHYS_TO_VIRT(a)  (void*) ((DWORD)(a) - _virtual_base)
#define BUS_TO_VIRT(a)   PHYS_TO_VIRT(a)

#ifdef _MODULE
  #define beep()  ((void)0)
#else
  #define beep()  do {            \
                    sound (2000); \
                    delay (2);    \
                    nosound();    \
                  } while (0)
#endif

/*
 * readX/writeX() are used to access memory mapped devices.
 */
#define readb(addr)     _farpeekb (_dos_ds, (DWORD)(addr))
#define readw(addr)     _farpeekw (_dos_ds, (DWORD)(addr))
#define readl(addr)     _farpeekl (_dos_ds, (DWORD)(addr))

#define writeb(b, addr) _farpokeb (_dos_ds, (DWORD)(addr), b)
#define writew(w, addr) _farpokew (_dos_ds, (DWORD)(addr), w)
#define writel(l, addr) _farpokel (_dos_ds, (DWORD)(addr), l)

#define memset_io(adr, val, cnt)     dosmemset ((DWORD)(adr), val, cnt)
#define memcpy_fromio(buf, adr, cnt) dosmemget ((DWORD)(adr), cnt, (void*)(buf))
#define memcpy_toio(adr, buf, cnt)   dosmemput ((void*)(buf), cnt, (DWORD)(adr))

/* ditto for shared memory devices
 */
#define memcpy_from_shmem(dst, src, cnt)  memcpy_fromio (dst, src, cnt)
#define memcpy_to_shmem(dst, src, cnt)    memcpy_toio (dst, src, cnt)


typedef struct device {
        char  *name;           /* interface name of device */
        char  *long_name;      /* long name (description)  */
        DWORD  base_addr;      /* device I/O address       */
        int    irq;            /* device IRQ number        */
        int    dma;            /* DMA channel              */
        DWORD  mem_start;      /* shared mem start         */
        DWORD  mem_end;        /* shared mem end           */
        DWORD  rmem_start;     /* shmem "recv" start       */
        DWORD  rmem_end;       /* shared "recv" end        */

        struct device *next; /* next device in list */

        /* interface service routines */
        int   (*probe)(struct device *dev);
        int   (*open) (struct device *dev);
        void  (*close)(struct device *dev);
        int   (*xmit) (struct device *dev, const void *buf, int len);
        void *(*get_stats)(struct device *dev);
        void  (*set_multicast_list)(struct device *dev);

        /* driver-to-pcap receive buffer routines */
        int   (*copy_rx_buf) (BYTE *buf, int max); /* rx-copy (pktdrvr only) */
        BYTE *(*get_rx_buf) (int len);             /* rx-buf fetch/enqueue */
        int   (*peek_rx_buf) (BYTE **buf);         /* rx-non-copy at queue */
        int   (*release_rx_buf) (BYTE *buf);       /* release after peek */

        /* ringbuffer for enqueing recv data */
        struct rx_ringbuf  queue;

        /* Low-level status flags. */
        WORD   flags;          /* interface flags          */
        BYTE   start;          /* start an operation       */
        BYTE   reentry;        /* interrupt handler active */
        DWORD  tx_busy;        /* transmitter busy flag    */

        /* interface information */
        ETHER  dev_addr;       /* hardware address         */
        ETHER  broadcast;      /* broadcast address        */
        int    addr_len;       /* length of HW-addr (6)    */
        WORD   mtu;            /* Maximum Transmit Unit    */
        void  *priv;           /* -> private data          */
        DWORD  tx_start;       /* jiffie-time of last Tx   */
        DWORD  last_rx;        /* jiffie-time of last Rx   */
        BYTE   if_port;        /* selectable AUI,TP,..     */

        /* Multicast stuff */
        int   mc_count;           /* Number of installed mcasts */
        ETHER mc_list[MAX_MCAST]; /* Multicast mac addresses    */
      } DEVICE;

/*
 * Multicast list definition
 */
struct dev_mc_list {
       struct dev_mc_list *next;
       char   dmi_addr [ETH_ALEN];
       WORD   dmi_addrlen;
       WORD   dmi_users;
     };

/*
 * Network device statistics
 */
typedef struct net_device_stats {
        DWORD  rx_packets;            /* total packets received       */
        DWORD  tx_packets;            /* total packets transmitted    */
        DWORD  rx_bytes;              /* total bytes received         */
        DWORD  tx_bytes;              /* total bytes transmitted      */
        DWORD  rx_errors;             /* bad packets received         */
        DWORD  tx_errors;             /* packet transmit problems     */
        DWORD  rx_dropped;            /* no space in Rx buffers       */
        DWORD  tx_dropped;            /* no space available for Tx    */
        DWORD  multicast;             /* multicast packets received   */

        /* detailed rx_errors: */
        DWORD  rx_length_errors;
        DWORD  rx_over_errors;        /* recv'r overrun error         */
        DWORD  rx_osize_errors;       /* recv'r over-size error       */
        DWORD  rx_crc_errors;         /* recv'd pkt with crc error    */
        DWORD  rx_frame_errors;       /* recv'd frame alignment error */
        DWORD  rx_fifo_errors;        /* recv'r fifo overrun          */
        DWORD  rx_missed_errors;      /* recv'r missed packet         */

        /* detailed tx_errors */
        DWORD  tx_aborted_errors;
        DWORD  tx_carrier_errors;
        DWORD  tx_fifo_errors;
        DWORD  tx_heartbeat_errors;
        DWORD  tx_window_errors;
        DWORD  tx_collisions;
        DWORD  tx_jabbers;
      } NET_STATS;

extern int EISA_bus, irq_debug, el3_debug, ei_debug  LOCKED_VAR;
extern int atp_debug, vortex_debug, eth16i_debug     LOCKED_VAR;
extern int ethpk_debug, bios32_debug, pcap_pkt_debug LOCKED_VAR;
extern int pci_debug, pcmcia_debug, smc_debug        LOCKED_VAR;
extern int cs89_debug, rtl8139_debug                 LOCKED_VAR;
extern int el3_max_loop                              LOCKED_VAR;

extern struct device *irq2dev_map[NUM_IRQS] LOCKED_VAR;

extern DWORD htonl (DWORD);
extern DWORD ntohl (DWORD);
extern WORD  htons (WORD);
extern WORD  ntohs (WORD);

struct device *init_etherdev (struct device *dev, int sizeof_priv);
void           ether_setup   (struct device *dev);
void           fddi_setup    (struct device *dev);

#endif /* __PMODE_MAC_DRIVER */

