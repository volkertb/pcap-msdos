/*
 * Simple djgpp 2.01+ test program for MAC drivers
 */

#include <signal.h>
#include <crt0.h>

#include "pmdrvr.h"
#include "8390.h"
#include "bios32.h"
#include "pci.h"

#ifdef USE_EXCEPT
#include "d:/prog/mw/except/exc.h"
#endif

struct EtherPacket {
       ETHER  src;
       ETHER  dst;
       char   data[ETH_MTU];
     };

extern int el1_probe    (struct device *dev);
extern int el2_probe    (struct device *dev);
extern int el3_probe    (struct device *dev);
extern int el16_probe   (struct device *dev);
extern int tc515_probe  (struct device *dev);
extern int tc574_probe  (struct device *dev);
extern int tc59x_probe  (struct device *dev);
extern int tc90xbc_probe(struct device *dev);
extern int eth16i_probe (struct device *dev);
extern int ethpk_probe  (struct device *dev);
extern int atp_probe    (struct device *dev);
extern int ne_probe     (struct device *dev);
extern int at1700_probe (struct device *dev);
extern int cs89x0_probe (struct device *dev);
extern int de600_probe  (struct device *dev);
extern int de620_probe  (struct device *dev);
extern int e2100_probe  (struct device *dev);
extern int es_probe     (struct device *dev);
extern int express_probe(struct device *dev);
extern int fmv18x_probe (struct device *dev);
extern int smc_probe    (struct device *dev);
extern int ultra_probe  (struct device *dev);
extern int apricot_probe(struct device *dev);
extern int rtl8139_probe(struct device *dev);

static struct device el1_dev = {
                     "elnk1",
                     "EtherLink I (3C501)",
                     0x300,      /* I/O base address */
                     0,          /* Autoprobe IRQ */
                     0,          /* ? DMA channel */
                     0xD8000,    /* Shared memory start */
                     0xDFFFF,    /*               end */
                     0,0,        /* Shared real-mem */
                     NULL,       /* no next device */
                     el1_probe
                   };

static struct device el2_dev = {
                     "elnk2",
                     "EtherLink II (3C503)",
                     0,          /* Autoprobe I/O address */
                     0,0,0,0,0,0,
                     &el1_dev,
                     el2_probe
                   };

static struct device el3_dev = {
                     "elnk3",
                     "EtherLink III (3C509)",
                     0,
                     0,0,0,0,0,0,
                     &el2_dev,
                     el3_probe
                   };

static struct device el16_dev = {
                     "elnk16",
                     "EtherLink 16 (3C507)",
                     0,
                     0,0,0,0,0,0,
                     &el3_dev,
                     el16_probe
                   };

static struct device tc515_dev = {
                     "tc515",
                     "Fast EtherLink III (3C515)",
                     0,
                     0,0,0,0,0,0,
                     &el16_dev,
                     tc515_probe
                   };
#if 0
static struct device tc574_dev = {
                     "tc574",
                     "EtherLink PCMCIA (3C574)",
                     0,
                     0,0,0,0,0,0,
                     &tc515_dev,
                     tc574_probe
                   };
#endif

static struct device tc59_dev = {
                     "tc59x",
                     "EtherLink III (3C590)",
                     0,
                     0,0,0,0,0,0,
                     &tc515_dev,
                     tc59x_probe
                   };

static struct device tc90xbc_dev = {
                     "3c90x",
                     "EtherLink PCI (3C90X)",
                     0,
                     0,0,0,0,0,0,
                     &tc59_dev,
                     tc90xbc_probe
                   };

static struct device eth16i_dev = {
                     "eth16i",
                     "EtherTeam 16i",
                     0,
                     0,0,0,0,0,0,
                     &tc90xbc_dev,
                     eth16i_probe
                   };

static struct device accton_dev = {
                     "accton",
                     "Accton EtherPocket",
                     0,
                     0,0,0,0,0,0,
                     &eth16i_dev,
                     ethpk_probe
                   };

static struct device atp_dev = {
                     "atp",
                     "ParPort Ether",
                     0,
                     0,0,0,0,0,0,
                     &accton_dev,
                     atp_probe
                   };

static struct device ne_dev = {
                     "ne",
                     "NE1000/2000 (NS8390)",
                     0,
                     0,0,0,0,0,0,
                     &atp_dev,
                     ne_probe
                   };

static struct device at17_dev = {
                     "at17",
                     "Allied Telesis 1700",
                     0,
                     0,0,0,0,0,0,
                     &ne_dev,
                     at1700_probe
                   };

static struct device cs89x0_dev = {
                     "cs89x0",
                     "Crystal Semiconductor CS89x0",
                     0,
                     0,0,0,0,0,0,
                     &at17_dev,
                     cs89x0_probe
                   };

static struct device de600_dev = {
                     "de600",
                     "D-Link DE-600 pocket adapter",
                     0,
                     0,0,0,0,0,0,
                     &cs89x0_dev,
                     de600_probe
                   };

static struct device de620_dev = {
                     "de620",
                     "D-Link DE-620 pocket adapter",
                     0,
                     0,0,0,0,0,0,
                     &de600_dev,
                     de620_probe
                   };

static struct device smc_dev = {
                     "smc9194",
                     "SMC 9000 Series",
                     0,
                     0,0,0,0,0,0,
                     &de620_dev,
                     smc_probe
                   };

static struct device rtl8139_dev = {
                     "rtl8139",
                     "RealTek PCI",
                     0,
                     0,0,0,0,0,0,
                     &smc_dev,
                     rtl8139_probe     /* dev->probe routine */
                   };

const struct device *dev_base = &rtl8139_dev; /* list of EtherNet devices */

static void init_tests (void);
static void transmit   (struct device *dev, int num);
static void show_stats (struct device *dev);
static void Shutdown   (int sig);

int EISA_bus = 0;

int main (void)
{
  struct device *dev;

  init_tests();

  for (dev = dev_base; dev; dev = dev->next)
  {
    fprintf (stderr, "Probing for %s..", dev->long_name);
    if (!(*dev->probe)(dev))
    {
      fprintf (stderr, "not found\n\n");
      continue;
    }

    /* Receive broadcast, multicast and everything else..
     */
    dev->flags |= (IFF_ALLMULTI | IFF_PROMISC);

    fprintf (stderr, "found. Opening card..\n");
    if (!(*dev->open)(dev))
       fprintf (stderr, "failed.\n");
    else
    {
      transmit (dev, 1);
      transmit (dev, 2);
    }
    (*dev->close)(dev);
    show_stats (dev);
    _printk_flush();
  }
  return (0);
}

static void init_tests (void)
{
  signal (SIGINT, Shutdown);

#ifdef USE_SECTION_LOCKING
  lock_sections();
#endif

#ifdef USE_EXCEPT
  InstallExcHandler (NULL);
#endif

  _printk_init (32*1024, NULL);
  rtl8139_debug = 6;
  el3_debug = 6;
  ei_debug  = 6;
  atp_debug = 6;
  vortex_debug = 6;
  ethpk_debug  = 6;
  eth16i_debug = 1;
  bios32_debug = 2;
  pci_debug    = 2;
  pci_init();
}

static void transmit (struct device *dev, int num)
{
  struct EtherPacket eth;

  fprintf (stderr, "Transmitting %d..\n", num);
  if (!isatty(fileno(stderr)))
     fprintf (stdout, "Transmitting %d..\n", num);

  memcpy (&eth.src, &dev->dev_addr, sizeof(eth.src));
  memset (&eth.dst, 0xFF, sizeof(eth.dst));
  memset (&eth.data, 0, sizeof(eth.data));

  if (!(*dev->xmit)(dev,&eth,sizeof(eth)))
       fprintf (stderr, "failed.\n");
  else fprintf (stderr, "okay.\n");

  sleep (2);        /* lets test the IRQ handler */
  _printk_flush();
}

static void show_stats (struct device *dev)
{
#if 1
  NET_STATS *s = (NET_STATS*) (*dev->get_stats)(dev);

  fprintf (stderr,
           "Tx: packets %lu, bytes %lu, errors %lu, aborts %lu\n"
           "    carrier %lu, hearbeat %lu, win err %lu, collisions %lu\n",
           s->tx_packets, s->tx_bytes, s->tx_errors, s->tx_aborted_errors,
           s->tx_carrier_errors, s->tx_heartbeat_errors, s->tx_window_errors,
           s->tx_collisions);

  fprintf (stderr,
           "Rx packets  %lu, bytes %lu, dropped %lu, errors %lu\n"
           "   FIFO err %lu, overruns %lu, framing %lu, len err %lu, CRC err %lu\n",
           s->rx_packets, s->rx_bytes, s->rx_dropped, s->rx_errors,
           s->rx_fifo_errors, s->rx_over_errors, s->rx_frame_errors,
           s->rx_length_errors, s->rx_crc_errors);
#else
  pcap_stats_detail (dev);
#endif
}

static void Shutdown (int sig)
{
  int irq;

  signal (sig, SIG_DFL);

  for (irq = 0; irq < NUM_IRQS; irq++)
      free_irq (irq);

  _printk_safe = 1;
  printk ("Got ^C\n");
  _printk_flush();
  exit (0);
}

