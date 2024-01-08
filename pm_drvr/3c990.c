/*
 * YOU SHOULD CAREFULLY READ THE FOLLOWING TERMS AND CONDITIONS BEFORE 
 * INSTALLING AND USING THIS PRODUCT, 
 * THE USE OF WHICH IS LICENSED BY 3COM CORPORATION ("3COM") 
 * and others as set forth below for your USE ONLY AS SET FORTH BELOW. 
 * DOWNLOADING, INSTALLING OR OTHERWISE USING ANY PART OF THE SOFTWARE OR 
 * DOCUMENTATION INDICATES THAT YOU ACCEPT THESE TERMS AND CONDITIONS.  
 * IF YOU DO NOT AGREE TO THE TERMS AND CONDITIONS OF THIS AGREEMENT, 
 * DO NOT DOWNLOAD, INSTALL OR OTHERWISE USE THE SOFTWARE OR DOCUMENTATION. 
 * AND IF YOU HAVE RECEIVED THE SOFTWARE AND DOCUMENTATION ON PHYSICAL MEDIA, 
 * RETURN THE ENTIRE PRODUCT WITH THE SOFTWARE AND DOCUMENTATION UNUSED TO THE
 * SUPPLIER WHERE YOU OBTAINED IT.
 
 * This driver (3c990.c) has been written to work with the 3cr990 product line
 * of network cards, manufactured by 3Com Corp.
 * This driver is not intended for any other product line, including the 3c59x 
 * or 3C90x product lines (although drivers with both of these names,
 * and for both of these product lines, are available ). 
 *
 * It does not work with the 2.3 kernel; only the 2.0 and 2.2 at this point.
 * The driver contains no support now for architectures other than IA-32 bit.
 *
 * To force the media selection, use the command line argument force=X, 
 * where X denotes the selection as follows:
 * 0=10(megabit)Half(Duplex); 1=10Full; 2=100Half; 3=100Full; 4=auto (default)
 * e.g. insmod 3c990.o force=1   
 * ... should give you 10 megabit full duplex forced action.
 *
 * You may contact 3Com for updates and information (regarding this driver) at:
 * http://support.3com.com/infodeli/tools/nic/3c990.htm
 *
 * You may request or submit driver modifications at:
 * http://support.3com.com/infodeli/tools/nic/linuxrequest.htm
 *
 * If you would like more information about compiling the driver than is 
 * available at the bottom of this file, you may choose to reference:
 * http://cesdis.gsfc.nasa.gov/linux/misc/modules.html
 * and/or you may reference the readme file for the 3c90x driver.
 *
 * License to "tc990image"
 * The binary image "tc990image" is licensed to users by 3Com Corporation, 
 * with intended use for 3Com's Network Interface Card models 3CR990-x.
 * (Where x indicates any extension model number.)
 * LICENSE:  3Com grants you a nonexclusive, nontransferable 
 * (except as specified herein) license to use the tc990image in conjunction 
 * with your use of 3Com's Network Interface Card models 3CR990-x as specified 
 * above. You are not permitted to lease, rent, distribute or sublicense 
 * (except as specified herein) the tc990image or to use the tc990image or in 
 * any other unauthorized manner.  Further, no license is granted to you in the
 * human readable code of the Software (source code).  Except as provided 
 * below, this Agreement does not grant you any rights to patents, copyrights, 
 * trade secrets, trademarks, or any other rights with respect to the Software 
 * or Documentation.
 * Subject to the restrictions set forth herein, the tc990image is licensed to 
 * be used on any workstation or any network server owned by or leased to you, 
 * for your internal use, provided that the tc990image is used only in 
 * connection with this 3Com product.  You may reproduce and provide one (1) 
 * copy of the tc990image for each such workstation or network server on which 
 * the Software is used as permitted hereunder.  Otherwise, the Software and 
 * Documentation may be copied only as essential for backup or archive purposes
 * in support of your use of the Software as permitted hereunder.  Each copy 
 * of the tc990image must contain 3Com's and its licensors' proprietary rights 
 * and copyright notices in the same form as on the original.  You agree not to
 * remove or deface any portion of any legend provided on any licensed program 
 * or documentation delivered to you under this Agreement.
 * ASSIGNMENT;   You may transfer the tc990image and the licenses granted 
 * herein to another party in the same country in which you obtained it if the 
 * other party agrees in writing to accept and be bound by the terms and 
 * conditions of this Agreement. If you transfer the tc990image, you must at 
 * the same time either transfer all copies of the Software and Documentation 
 * to the party or you must destroy any copies not transferred. Except as set 
 * forth above, you may not assign or transfer your rights.
 *
 * NO REVERSE ENGINEERING:  Modification, reverse engineering, reverse 
 * compiling, or disassembly of the tc990image is expressly prohibited. 
 * However, if you are a European Union ("EU") resident, information necessary 
 * to achieve interoperability of the tc990image with other programs within the
 * meaning of the EU Directive on the Legal Protection of Computer Programs is 
 * available to you from 3Com upon written request.
 * TRADE SECRETS; TITLE:  You acknowledge and agree that the structure, 
 * sequence and organization of the tc990image are the valuable trade secrets 
 * of 3Com and its suppliers. You agree to hold such trade secrets in 
 * confidence. You further acknowledge and agree that ownership of, and title 
 * to, the tc990image and all subsequent copies thereof regardless of the form 
 * or media are held by 3Com.
 *
 * License to this driver
 * Some material in this driver has been adopted from pci-skeleton.c, 
 * a Linux PCI network adapter skeleton device driver. 
 * pci-skeleton.c written in 1998-1999 by Donald Becker.
 * The author of pci-skeleton.c may be reached as becker@scyld.com.
 * More information about Becker is available at http://www.scyld.com/
 *
 * Except as otherwise provided, This software and pci-skeleton.c, may be used 
 * and distributed according to the terms of the GNU Public License (GPL), 
 * incorporated herein by reference.
 *
 * UNITED STATES GOVERNMENT LEGENDS:  The portions of this Driver developed by 
 * 3Com, and the tc990image ("Software") is commercial in nature and developed 
 * solely at private expense.  The Software is delivered as 
 * "Commercial Computer Software" as defined in DFARS 252.227-7014 (June 1995) 
 * or as a commercial item as defined in FAR 2.101(a) and as such is provided 
 * with only such rights as are provided herein.  Technical data is provided 
 * with limited rights only as provided in DFAR 252.227-7015 (Nov. 1995) or 
 * FAR 52.227-14 (June 1987), whichever is applicable.
 *
 * TERM AND TERMINATION:  The licenses to the Software granted hereunder are 
 * perpetual unless terminated earlier as specified below or otherwise 
 * provided. You may terminate the licenses and this Agreement at any time by 
 * destroying the Software and Documentation together with all copies and 
 * merged portions in any form. The licenses and this Agreement will also 
 * terminate immediately if you fail to comply with any term or condition of 
 * this Agreement. Upon such termination you agree to destroy the Software and 
 * Documentation, together with all copies and merged portions in any form.
 *
 * LIMITED WARRANTIES AND LIMITATION OF LIABILITY: This driver and  tc990image 
 * are provided with no warranty, including warranty of fitness for any 
 * particular purpose.  3Com assumes no liability for any damages or injuries 
 * resulting from use of the driver or tc990image file.
 * GOVERNING LAW:   This License shall be governed by the laws of the State of 
 * California, U.S.A. excluding its conflicts of laws principles and excluding 
 * the United Nations Convention on Contracts for the 
 * International Sale of Goods.
 * SEVERABILITY:  In the event any provision of this License is found to be 
 * invalid, illegal or unenforceable, the validity, legality and enforceability
 * of any of the remaining provisions shall not in any way be affected or 
 * impaired and a valid, legal and enforceable provision of similar intent and 
 * economic impact shall be substituted therefor.
 * ENTIRE AGREEMENT:  This Agreement sets forth the entire understanding and 
 * agreement between you and 3Com and supersedes all prior agreements, whether 
 * written or oral, with respect to the Software and Documentation, and may be 
 * amended only in a writing signed by both parties.
 * Should you have any questions concerning this Agreement or if you desire to 
 * contact 3Com for any reason, please contact the 3Com subsidiary serving your
 * country, or write: 
 * 3Com Corporation, Customer Support Information, 
 * 5400 Bayfront Plaza, Santa Clara, CA  95052
 */

#include "pmdrvr.h"
#include "module.h"
#include "bios32.h"
#include "pci.h"

#if 0  // not yet

#define tc990_IMAGE_SIZE 0x10000
#include "3c990img.h"
#define IMPLEMENT_3C990
#include "3c990.h"

#undef  STATIC
#define STATIC                  /* for.map-file */

static const char *version = "3Com 3c990.c v1.0.0b 10/2000  \n";

static struct pci_id_info pci_tbl[] = {
  { "3Com 10/100 PCI NIC w/3XP (3CR990-TX-95)", 0x9902 },
  { "3Com 10/100 PCI NIC w/3XP (3CR990-TX-97)", 0x9903 },
  { "3Com 10/100 PCI NIC w/3XP (3C990B-TX-M)", 0x9904 },
  { "3Com 10/100 PCI Server NIC w/3XP (3CR990SVR95)", 0x9908 },
  { "3Com 10/100 PCI Server NIC w/3XP (3CR990SVR97)", 0x9909 },
  { "3Com 10/100 PCI Server NIC w/3XP (3C990BSVR)", 0x990A },
  { NULL, }
};

struct tc990_shared {
       HOST_INIT_S  init;
       DWORD        slack;
       HOST_VAR_S   var;
       RX_FREE_DSC  rxFree[RX_ENTRIES];
       RX_DSC       rxLo[RX_ENTRIES], rxHi[RX_ENTRIES];
       CMD_DSC      cmd[CMD_ENTRIES];
       RESPONSE_DSC rsp[RESPONSE_ENTRIES];
     };

struct tc990_ring_info {
       tc990_RING RxHiRing, RxLoRing, RxBuffRing; /* receive information */
       TX_RING    TxHiRing, TxLoRing;             /* send information */
       tc990_RING CmdRing, RspRing;               /* command ring information */
     };

struct tc990_private {
       struct tc990_shared    *shared;
       DWORD                   shared_size;
       DWORD                   shared_dma_addr;

       struct tc990_ring_info  ring_info;

       BYTE                   *DownloadPageVirtual;
       DWORD                   DownloadPagePhysical;
       DWORD                   ringPhysical;
       DWORD                   BufferPhysical;
       DWORD                   slot_memory_size;
       DWORD                   slot_physical;
       const char             *product_name;

       struct SLOT            *slot;
       struct device          *next_module; /* Link for devices of this type */
       struct net_device_stats stats;
       struct wait_queue      *wait_q;
       struct pci_dev         *pdev;

       WORD                    chip_id;
       BYTE                    pci_bus, pci_devfn;
       BYTE                    tx_full;
       DWORD                   index;
       DWORD                   resources_reserved;
     };

static void update_tx_ring_index (DWORD * writePUW);
static struct device *tc990probe (struct device *dev, long ioaddr, WORD irq, WORD chp_idx, WORD fnd_cnt);

static int   tc990_open (struct device *dev);
static void  tc990_interrupt (int irq);
static int   tc990_close (struct device *dev);
static int   tc990_start_tx (struct device *dev, const void *buf, int len);
static void *tc990_get_stats (struct device *dev);

static void stats_handler (struct tc990_private *np);
static void release_buf_to_firmware (struct device *dev, DWORD slot_index);
static void process_receive (struct device *dev, tc990_RING * pRxRing,
                             volatile DWORD *regRxReadUL,
                             volatile DWORD *regRxWriteUL);

static WORD calculate_buffer_checksum (WORD * Buffer, DWORD Count);
static WORD download_boot_record (struct device *dev, WORD process);
static WORD download_runtime_image (struct device *dev);
static WORD init_ring_zone (struct device *dev);
static WORD issue_command (struct device *dev, WORD command, WORD parameter1,
                          DWORD parameter2, DWORD parameter3, WORD *return1,
                          DWORD *return2, DWORD *return3, BYTE wait);

static BYTE process_response (struct tc990_private *np,
                              DWORD response_read_index, WORD command);

static void free_resources (struct device *dev);

static void process_send_complete (struct device *dev, TX_RING *pTxRing,
                                   volatile DWORD *xmitReadIndex);
static void tc990_set_rx_mode (struct device *dev);

int  tc990_init_module (void);
void tc990_cleanup_module (void);

/* A list of our installed devices, for removing the driver module.
 */
static struct device *root_net_dev = NULL;

int tc990_probe (struct device *dev)
{
  struct pci_dev *pdev = 0;
  WORD   pci_command;
  WORD   chip_idx, irq = 0;
  WORD   cards_found = 0;
  WORD   vendor, device;
  BYTE   cacheSz;
  BYTE   pci_latency;
  BYTE   rev;
  long   pciaddr, ioaddr;

  while ((pdev = pci_find_class (PCI_CLASS_NETWORK_ETHERNET << 8, pdev)) != NULL)
  {
    vendor = pdev->vendor;
    device = pdev->device;

    if (vendor != tc990_VENDOR_ID)
       continue;

    for (chip_idx = 0; pci_tbl[chip_idx].device_id; chip_idx++)
        if (device == pci_tbl[chip_idx].device_id)
           break;

    if (pci_tbl[chip_idx].device_id == 0)
       continue;

    pci_read_config_byte (pdev, PCI_CLASS_REVISION, &rev);

    pdev = pci_find_slot (pci_bus, pci_device_fn);
    pciaddr = pdev->base_address[1];
    irq = pdev->irq;

    ioaddr = pciaddr & PCI_BASE_ADDRESS_MEM_MASK;

    /* Read the cache line size from the PCI space.
     */
    pci_read_config_byte (pdev, PCI_CACHE_LINE_SIZE, &cacheSz);

    /* Cache line size is in DWORDS, calculate bytes.
     */
    cacheSz *= 4;

    /* Check the cache line size
     */
    if ((cacheSz % 0x10) || (!cacheSz))
    {
      printk ("Cacheline size not proper.\n");
      cacheSz = 0x20;  /* default:Use the paragraph boundary for alignment. */
    }

    /* Here the Command reg is read and modified if necessary
     */
    pci_read_config_word (pdev, PCI_COMMAND, &pci_command);
    if ((pci_command & (PCI_COMMAND_MASTER | PCI_COMMAND_INVALIDATE)) !=
         (PCI_COMMAND_MASTER | PCI_COMMAND_INVALIDATE))
       pci_write_config_byte (pdev, PCI_COMMAND, pci_command);

    dev = tc990probe (dev, ioaddr, irq, chip_idx, cards_found);

    if (!dev)
       break;

    /* PCI latency check (set) */
    pci_read_config_byte (pdev, PCI_LATENCY_TIMER, &pci_latency);
    if (pci_latency < min_pci_latency[tc990_index])
    {
      printk ("  PCI latency timer (CFLT) is "
              "unreasonably low at %d. Setting to %d clocks.\n",
              pci_latency, min_pci_latency[tc990_index]);

      pci_write_config_byte (pdev, PCI_LATENCY_TIMER,
                             min_pci_latency[tc990_index]);

      /* Save the pdev variable
       */
      ((struct tc990_private*) (dev->priv))->pdev = pdev;
      ((struct tc990_private*) (dev->priv))->index = tc990_index++;
    }
    dev = 0;
    cards_found++;
  }
  return (cards_found);
}


static struct device *tc990probe (struct device *dev, long ioaddr,
                                  WORD irq, WORD chip_id, WORD card_idx)
{
  struct tc990_private *np;
  WORD   retval;
  WORD   stationAddressHi = 0;
  DWORD  stationAddressLo = 0;
  DWORD  i;

  dev = init_etherdev (dev, sizeof(struct tc990_private));

  dev->base_addr = ioaddr;
  dev->irq = irq;

  /* Make certain the descriptor lists are aligned.
   */
  np = (void*) (((DWORD)k_calloc(sizeof(*np), 1) + 15) & ~15);
  if (!np)
  {
    free_resources (dev);
    return (NULL);
  }

  dev->priv = np;

  np->shared = (struct tc990_shared*) pci_alloc_consistent (
               sizeof(struct tc990_shared), &np->shared_dma_addr);
  if (!np->shared)
  {
    free_resources (dev);
    return (NULL);
  }

  np->shared_size = sizeof (struct tc990_shared);
  memset (np->shared, 0, sizeof (struct tc990_shared));

  np->resources_reserved |= TC990_SHARED_MEMORY_ALLOCATED;

  retval = init_ring_zone (dev);
  if (retval)
  {
    free_resources (dev);
    return (NULL);
  }

  np->chip_id = chip_id;

  /* The chip-specific entries in the device structure.
   */
  dev->open  = tc990_open;
  dev->xmit  = tc990_start_tx;
  dev->close = tc990_close;
  dev->get_stats          = tc990_get_stats;
  dev->set_multicast_list = tc990_set_rx_mode;

  if ((retval = download_runtime_image (dev)) != 0)
  {
    free_resources (dev);
    return (NULL);
  }

  if ((retval = download_boot_record (dev, tc990_WAITING_FOR_BOOT)) != 0)
  {
    free_resources (dev);
    return (NULL);
  }

  /* Set the maximum packet size for the firmware
   */
  retval = issue_command (dev, tc990_CMD_MAX_PKT_SIZE_WRITE, 0x800, 0, 0, NULL, NULL, NULL, 1);
  retval |= issue_command (dev, 69, 0, 0, 0, NULL, NULL, NULL, 0);
  if (retval)
  {
    free_resources (dev);
    return (NULL);
  }

  /* Read the station address from the adapter.
   */
  retval = issue_command (dev, tc990_CMD_STATION_ADR_READ, 0, 0, 0,
                          &stationAddressHi, &stationAddressLo, NULL, 1);
  if (retval)
  {
    free_resources (dev);
    return (NULL);
  }

  dev->dev_addr[0] = ((BYTE*) (&stationAddressHi))[1];
  dev->dev_addr[1] = ((BYTE*) (&stationAddressHi))[0];
  dev->dev_addr[2] = ((BYTE*) (&stationAddressLo))[3];
  dev->dev_addr[3] = ((BYTE*) (&stationAddressLo))[2];
  dev->dev_addr[4] = ((BYTE*) (&stationAddressLo))[1];
  dev->dev_addr[5] = ((BYTE*) (&stationAddressLo))[0];

  printk ("%s: %s at %x, ", dev->name, pci_tbl[chip_id].name, ioaddr);

  for (i = 0; i < 5; i++)
      printk ("%02X:", dev->dev_addr[i]);

  printk ("%02X, IRQ %d.\n", dev->dev_addr[i], irq);

  np->next_module = root_net_dev;
  root_net_dev = dev;

  /* Update the resources reserved
   */
  np->resources_reserved |= TC990_DEVICE_IN_ROOT_CHAIN;
  return (dev);
}


static int tc990_open (struct device *dev)
{
  struct tc990_private *np = (struct tc990_private *) dev->priv;
  long   ioaddr = dev->base_addr;
  WORD   retval = 0;

  /* MUST set irq2dev_map first, because IRQ may come
   * before request_irq() returns.
   */
  irq2dev_map [dev->irq] = dev;
  if (!request_irq (dev->irq, tc990_interrupt))
  {
    irq2dev_map [dev->irq] = NULL;
    return (0);
  }

  np->resources_reserved |= TC990_IRQ_RESERVED;

  dev->tx_busy = 0;
  dev->reentry = 0;
  dev->start = 1;

  if (force[np->index] < 0 || force[np->index] > 4)
      printk ("3c990: Bad value -- force=%x. Autonegotiation default "
              "used instead. \n", force[np->index]);
  else
    retval = issue_command (dev, tc990_CMD_XCVR_SELECT, force[np->index],
                            0, 0, NULL, NULL, NULL, 0);

  retval |= issue_command (dev, tc990_CMD_TX_ENABLE, 0, 0, 0,
                           NULL, NULL, NULL, 1);
  retval |= issue_command (dev, tc990_CMD_RX_ENABLE, 0, 0, 0,
                           NULL, NULL, NULL, 1);
  if (retval)
     return (0);

  writel (tc990_ENABLE_ALL_INT, ioaddr + tc990_INT_ENABLE_REG);
  writel (tc990_UNMASK_ALL_INT, ioaddr + tc990_INT_MASK_REG);

  netif_start_queue (dev);
  return (1);
}


static int tc990_start_tx (device *dev, const void *buf, int len)
{
  struct tc990_private *np = (struct tc990_private *) dev->priv;
  TX_RING *pTxRing = &np->ring_info.TxLoRing;
  DWORD    addr;
  DWORD    xmitReadIndex;
  DWORD    xmitWriteIndex;
  DWORD    xmitDescriptorSpace;
  TX_DESC *pXmitWritePtr;
  TX_FRAME_DESC *pFramePtr;

  /* Block a timer-based transmit from overlapping.  This could better
   * be done with atomic_swap(1, dev->tx_busy), but this works as well
   */
  if (dev->tx_busy)
     return (0);

  xmitReadIndex  = pTxRing->LastReadUL;
  xmitWriteIndex = pTxRing->LastWriteUL;

  if (xmitWriteIndex >= xmitReadIndex)
       xmitDescriptorSpace = (sizeof(TX_DESC) * TX_ENTRIES) - (xmitWriteIndex - xmitReadIndex);
  else xmitDescriptorSpace = xmitReadIndex - xmitWriteIndex;

  if (xmitDescriptorSpace < (4 * sizeof (TX_DESC)))
  {
    np->tx_full = 1;
    dev->tx_busy = 1;
    dev->start = 0;
    return (0);
  }

  pXmitWritePtr = (TX_DESC*) (pTxRing->RingBase + xmitWriteIndex);
  pFramePtr = (TX_FRAME_DESC*) pXmitWritePtr;

  pFramePtr->frU.frFlagsS.frFlagsUC = FRAME_TYPE_PKT_HDR;
  pFramePtr->pktPtrLoUL = (DWORD) buf;
  pFramePtr->pktPtrHiUL = 0;

  update_tx_ring_index (&xmitWriteIndex);
  pXmitWritePtr = (TX_DESC*) (pTxRing->RingBase + xmitWriteIndex);

  pXmitWritePtr->fragLenUW      = len;
  pXmitWritePtr->fragFlagsUC    = FRAME_TYPE_FRAG_HDR | HOST_DESC_VALID;
  pXmitWritePtr->fragReservedUC = 0;
  pXmitWritePtr->fragSpareUL    = 0;

  addr = VIRT_TO_BUS (buf);
  pXmitWritePtr->fragHostAddrLoUL = addr;
  pXmitWritePtr->fragHostAddrHiUL = 0;

  pFramePtr->frU.frFlagsS.frFlagsUC |= HOST_DESC_VALID;
  pFramePtr->frU.frFlagsS.frNumDescUC = 1;
  pFramePtr->frU.frFlagsS.frPktLen = (WORD) skb->len;
  update_tx_ring_index (&xmitWriteIndex);

  pTxRing->LastWriteUL = xmitWriteIndex;
  writel (pTxRing->LastWriteUL, dev->base_addr + pTxRing->WriteRegister);

  dev->tx_busy = 0;
  dev->tx_start = jiffies;
  return (1);
}

static void tc990_interrupt (int irq)
{
  struct device        *dev = irq2dev_map[irq];
  struct tc990_private *np;

  volatile DWORD intr_status;
  volatile DWORD *rxReadIndex;
  volatile DWORD *rxWriteIndex;
  volatile DWORD *xmitReadIndex;

  long ioaddr = dev->base_addr;
  np = (struct tc990_private *) dev->priv;

  /* mask all the interrupts
   */
  writel (tc990_MASK_ALL_INT, ioaddr + tc990_INT_MASK_REG);

  intr_status = readl (ioaddr + tc990_INT_STATUS_REG);

  while (1)
  {  
    writel (intr_status, ioaddr + tc990_INT_STATUS_REG);
    if (!intr_status)
       break;

    rxReadIndex = &np->shared->var.hvWriteS.regRxHiReadUL;
    rxWriteIndex = &np->shared->var.hvReadS.regRxHiWriteUL;
    if (*rxReadIndex != *rxWriteIndex)
       process_receive (dev, &np->ring_info.RxHiRing, rxReadIndex, rxWriteIndex);

    /* Check the Low ring
     */
    rxReadIndex = &np->shared->var.hvWriteS.regRxLoReadUL;
    rxWriteIndex = &np->shared->var.hvReadS.regRxLoWriteUL;
    if (*rxReadIndex != *rxWriteIndex)
       process_receive (dev, &np->ring_info.RxLoRing, rxReadIndex, rxWriteIndex);

    /* check the transmit ring
     */
    xmitReadIndex = &np->shared->var.hvReadS.regTxLoReadUL;

    if (np->ring_info.TxLoRing.LastReadUL != *xmitReadIndex)
       process_send_complete (dev, &np->ring_info.TxLoRing, xmitReadIndex);

    /* check the response ring
     */
    if (np->shared->var.hvWriteS.regRespReadUL != np->shared->var.hvReadS.regRespWriteUL)
       process_response (np, np->shared->var.hvWriteS.regRespReadUL, 0);

    intr_status = readl (ioaddr + tc990_INT_STATUS_REG);
  }

  /* unmask all the interrupts */
  writel (tc990_UNMASK_ALL_INT, ioaddr + tc990_INT_MASK_REG);
}

static void process_receive (struct device *dev, tc990_RING *pRxRing,
                             volatile DWORD *regRxReadUL, volatile DWORD *regRxWriteUL)
{
  struct sk_buff       *skb;
  struct tc990_private *np = (struct tc990_private *) dev->priv;
  RX_DSC *rxDescriptor;
  DWORD   slotIndex, rxReadIndex = *regRxReadUL, writeVal = *regRxWriteUL;
  DWORD   rxBufferSize = ETHERNET_MAXIMUM_FRAME_SIZE + 4;
  WORD    count = 0;

  while (rxReadIndex != writeVal)
  {
    rxDescriptor = (RX_DSC *) (pRxRing->RingBase + rxReadIndex);
    slotIndex = rxDescriptor->virtual_addr_lo;

    if (!rxDescriptor->RxStatus)
    {
      skb = (struct sk_buff *) np->slot[slotIndex].skb;
      if (skb)
      {
        /* Add the pci_unmap code here */

        skb_put (skb, rxDescriptor->FrameLength);
        dev->last_rx = jiffies;
      }
    }

    UPDATE_INDEX (rxReadIndex, sizeof (RX_DSC), RX_ENTRIES);
    writeVal = *regRxWriteUL;
    count++;
  }

  /* Refill buffer slots
   */
  rxReadIndex = *regRxReadUL;

  while (rxReadIndex != writeVal)
  {
    /* Give this receive descriptor back to firmware.
     */
    rxDescriptor = (RX_DSC *) (pRxRing->RingBase + rxReadIndex);
    slotIndex = rxDescriptor->virtual_addr_lo;

    UPDATE_INDEX (rxReadIndex, sizeof(RX_DSC), RX_ENTRIES);
    *regRxReadUL = rxReadIndex;

    if ((skb = dev_alloc_skb (rxBufferSize)) == NULL)
    {
      printk ("3c990: Memory squeeze (alloc rx buff failed) Danger!\n");
      return;
    }
    skb->dev = dev;

    if (tc990_debug > 2)
    {
      printk ("*A:Data %x; tail %x; ", skb->data, skb->tail); //##
      printk ("Head %x; end %x \n", skb->head, skb->end); //##
    }

    /* Save the information in the slot.
     */
    np->slot[slotIndex].virtual_addr_lo = (DWORD) skb->tail;
    np->slot[slotIndex].virtual_addr_hi = 0;
    np->slot[slotIndex].skb = (DWORD) skb;

    /* Add the pci unmap code
     */
    np->slot[slotIndex].physical_addr_lo = VIRT_TO_BUS (np->slot[slotIndex].virtual_addr_lo);

    np->slot[slotIndex].physical_addr_hi = 0;
    np->slot[slotIndex].buffer_length = rxBufferSize;
    release_buf_to_firmware (dev, slotIndex);

    if (tc990_debug > 2)
    {
      printk ("*B:Data %x; tail %x=%x; ", skb->data, np->slot[slotIndex].virtual_addr_lo, skb->tail);
      printk ("Head %x; end %x \n", skb->head, skb->end);
    }
  }
}

static void release_buf_to_firmware (struct device *dev, DWORD slot_index)
{
  struct tc990_private *np = (struct tc990_private *) dev->priv;
  RX_FREE_DSC *rxFreeDescriptor;
  HOST_VAR_S  *pHostVar;

  pHostVar = (HOST_VAR_S*) BUS_TO_VIRT ((DWORD)np->shared->init.hostVarsPS);

  rxFreeDescriptor = (RX_FREE_DSC*) (np->ring_info.RxBuffRing.RingBase +
                                     pHostVar->hvWriteS.regRxBuffWriteUL);

  rxFreeDescriptor->physical_addr_lo = np->slot[slot_index].physical_addr_lo;
  rxFreeDescriptor->physical_addr_hi = np->slot[slot_index].physical_addr_hi;
  rxFreeDescriptor->virtual_addr_lo = slot_index;
  rxFreeDescriptor->virtual_addr_hi = 0;

  UPDATE_INDEX (pHostVar->hvWriteS.regRxBuffWriteUL, sizeof(RX_FREE_DSC), RX_ENTRIES);
}


static BYTE process_response (struct tc990_private *np,
                              DWORD responseReadIndex, WORD Command)
{
  RESPONSE_DSC *responseDesc;

  while (responseReadIndex != np->shared->var.hvReadS.regRespWriteUL)
  {
    responseDesc = (RESPONSE_DSC*) (np->ring_info.RspRing.RingBase + responseReadIndex);

    if (Command == responseDesc->nrml.Command)
       return (tc990_STATUS_CMD_FOUND);

    if (responseDesc->nrml.Command == tc990_CMD_READ_STATS)
    {
      stats_handler (np);
      responseReadIndex = np->shared->var.hvWriteS.regRespReadUL;
      return (0);
    }
    if (responseDesc->nrml.Flags & RESPONSE_DSC_ERROR_SET)
       printk ("tc990: Response error bit set \n");
    UPDATE_INDEX (np->shared->var.hvWriteS.regRespReadUL,
                  sizeof(RESPONSE_DSC), RESPONSE_ENTRIES);
    responseReadIndex = np->shared->var.hvWriteS.regRespReadUL;
  }
  return (0);
}


static void process_send_complete (struct device *dev, TX_RING *pTxRing,
                                   volatile DWORD *xmitReadIndex)
{
  struct sk_buff       *skb;
  struct tc990_private *np = (struct tc990_private *) dev->priv;
  TX_FRAME_DESC        *pXmitReadPtr;

  while (pTxRing->LastReadUL != *xmitReadIndex)
  {
    pXmitReadPtr = (TX_FRAME_DESC *) (pTxRing->RingBase + pTxRing->LastReadUL);

    if (pXmitReadPtr->frU.frFlagsS.frFlagsUC & FRAME_TYPE_PKT_HDR)
    {
      skb = (struct sk_buff *) pXmitReadPtr->pktPtrLoUL;
      /* Add the PCI unmap code here */
      dev_kfree_skb_irq (skb);
    }
    update_tx_ring_index (&pTxRing->LastReadUL);
  }

  if (np->tx_full)
  {
    np->tx_full = 0;
    netif_wake_queue (dev);     /* Attention: under a spinlock.  --SAW */
  }
}

static void stats_handler (struct tc990_private *np)
{
  RESPONSE_DSC *responseDescriptor;

  responseDescriptor = (RESPONSE_DSC*) (np->ring_info.RspRing.RingBase +
                                        np->shared->var.hvWriteS.regRespReadUL);

  if (responseDescriptor->nrml.Flags & RESPONSE_DSC_ERROR_SET)
  {
    printk ("3c990: Stats error flag set.\n");
    return;
  }

  np->stats.tx_packets = responseDescriptor->tx.pkt;
  np->stats.tx_bytes = responseDescriptor->tx.byte;

  UPDATE_INDEX (np->shared->var.hvWriteS.regRespReadUL, sizeof(RESPONSE_DSC),
                RESPONSE_ENTRIES);
  responseDescriptor = (RESPONSE_DSC*) (np->ring_info.RspRing.RingBase +
                       np->shared->var.hvWriteS.regRespReadUL);

  UPDATE_INDEX (np->shared->var.hvWriteS.regRespReadUL, sizeof(RESPONSE_DSC),
                RESPONSE_ENTRIES);

  responseDescriptor = (RESPONSE_DSC*) (np->ring_info.RspRing.RingBase +
                       np->shared->var.hvWriteS.regRespReadUL);

  np->stats.tx_carrier_errors = responseDescriptor->txCrit.carrierLost;
  np->stats.tx_collisions = responseDescriptor->txCrit.multCollision;
  np->stats.tx_errors = responseDescriptor->txCrit.carrierLost;

  UPDATE_INDEX (np->shared->var.hvWriteS.regRespReadUL, sizeof(RESPONSE_DSC),
                RESPONSE_ENTRIES);

  responseDescriptor = (RESPONSE_DSC*) (np->ring_info.RspRing.RingBase +
                       np->shared->var.hvWriteS.regRespReadUL);

  np->stats.rx_packets = responseDescriptor->txRx.rxPkt;
  np->stats.rx_bytes = responseDescriptor->txRx.rxByte;

  UPDATE_INDEX (np->shared->var.hvWriteS.regRespReadUL, sizeof(RESPONSE_DSC),
                RESPONSE_ENTRIES);
  responseDescriptor = (RESPONSE_DSC*) (np->ring_info.RspRing.RingBase +
                       np->shared->var.hvWriteS.regRespReadUL);

  np->stats.rx_fifo_errors = responseDescriptor->rx.fifoOverrun;
  np->stats.rx_errors = responseDescriptor->rx.fifoOverrun;
  np->stats.rx_errors += responseDescriptor->rx.badSSD;
  np->stats.rx_errors += responseDescriptor->rx.crcError;
  np->stats.rx_crc_errors = responseDescriptor->rx.crcError;

  UPDATE_INDEX (np->shared->var.hvWriteS.regRespReadUL, sizeof(RESPONSE_DSC),
                RESPONSE_ENTRIES);
  responseDescriptor = (RESPONSE_DSC*) (np->ring_info.RspRing.RingBase +
                       np->shared->var.hvWriteS.regRespReadUL);
  np->stats.rx_length_errors = responseDescriptor->rxGeneral.oversize;

  UPDATE_INDEX (np->shared->var.hvWriteS.regRespReadUL, sizeof(RESPONSE_DSC),
                RESPONSE_ENTRIES);
  responseDescriptor = (RESPONSE_DSC*) (np->ring_info.RspRing.RingBase +
                       np->shared->var.hvWriteS.regRespReadUL);

  UPDATE_INDEX (np->shared->var.hvWriteS.regRespReadUL, sizeof(RESPONSE_DSC),
                RESPONSE_ENTRIES);
  responseDescriptor = (RESPONSE_DSC*) (np->ring_info.RspRing.RingBase +
                       np->shared->var.hvWriteS.regRespReadUL);

  wake_up_interruptible (&np->wait_q);
}

static void *tc990_get_stats (struct device *dev)
{
  struct tc990_private *np = (struct tc990_private *) dev->priv;
  WORD   errVal = 0;

  if (netif_running (dev))
  {
    DISABLE();
    errVal = issue_command (dev, (WORD) tc990_CMD_READ_STATS, 0, 0, 0,
                            NULL, NULL, NULL, 0);
    if (errVal)
       printk ("3c990: Stats command gone BAD.");
    ENABLE();
  }
  return (void*) &np->stats;
}


static void tc990_set_rx_mode (struct device *dev)
{
  WORD  i, j;
  BYTE  ThisByte;
  BYTE  Address[6];
  DWORD hashFilterArray[(tc990_MULTICAST_BITS + 1) / 32];
  DWORD hashBit, count;
  DWORD filter = 0;
  DWORD Crc, Carry;

  if (dev->flags & IFF_PROMISC)
  {                             /* Set promiscuous. log net taps */
    printk ("%s: Promiscuous mode enabled.\n", dev->name);
    filter |= tc990_RX_FILT_PROMISCUOUS;
  }
  else if (dev->flags & IFF_ALLMULTI)
  {
    filter |= tc990_RX_FILT_DIRECTED;
    filter |= tc990_RX_FILT_ALL_MULTICAST;
    filter |= tc990_RX_FILT_BROADCAST;
  }
  else if (dev->flags & IFF_MULTICAST)
  {
    filter |= tc990_RX_FILT_DIRECTED;
    filter |= tc990_RX_FILT_HASH_MULTICAST;
    filter |= tc990_RX_FILT_BROADCAST;

    /* SetMulticastAddressList on the adapter.
     * Clear all bits   
     */
    memset (hashFilterArray, 0, sizeof (hashFilterArray));

    /* Calculate hash bit for eash address and stuff in bit array
     */
    for (count = 0; count < dev->mc_count; count++)
    {
      /* CalculateHashBits
       * Compute CRC for the address value
       */
      Crc = 0xffffffff;
      for (i = 0; i < 6; i++)
      {
        ThisByte = Address[i];
        for (j = 0; j < 8; j++)
        {
          Carry = ((Crc & 0x80000000) ? 1 : 0) ^ (ThisByte & 0x01);
          Crc <<= 1;
          ThisByte >>= 1;
          if (Carry)
             Crc = (Crc ^ POLYNOMIAL) | Carry;
        }
      }
      /* filter bit position
       */
      hashBit = (WORD) (Crc & tc990_MULTICAST_BITS);
      hashFilterArray[hashBit / 32] |= (1 << hashBit % 32);
    }
    i = issue_command (dev, (WORD) tc990_CMD_MCAST_HASH_MASK_WRITE,
                       (WORD) 2, hashFilterArray[0], hashFilterArray[1],
                       NULL, NULL, NULL, 0);
  }
  else
  {
    filter |= tc990_RX_FILT_DIRECTED;
    filter |= tc990_RX_FILT_BROADCAST;
  }
  i = issue_command (dev, (WORD) tc990_CMD_RX_FILT_WRITE, (WORD)filter,
                     0, 0, NULL, NULL, NULL, 0);
}

static void tc990_close (struct device *dev)
{
  WORD errVal;
  long ioaddr = dev->base_addr;

  dev->tx_busy = 1;
  dev->start = 0;

  /* Disable interrupts by clearing the interrupt mask.
   */
  writel (tc990_MASK_ALL_INT, ioaddr + tc990_INT_MASK_REG);

  /* Stop the chip's Tx and Rx processes.
   */
  errVal = issue_command (dev, tc990_CMD_TX_DISABLE, 0, 0, 0, NULL, NULL, NULL, 1);
  errVal |= issue_command (dev, tc990_CMD_RX_DISABLE, 0, 0, 0, NULL, NULL, NULL, 1);
  if (errVal)
     printk ("3c990: Unaccounted for failure of command to network adapter. DANGER!");

  free_irq (dev->irq, dev);
  irq2dev_map[dev->irq] = NULL;
}


static void free_resources (struct device *dev)
{
  struct tc990_private *np = (struct tc990_private*) dev->priv;
  WORD   index;

  printk ("3c990: There appear to be inadequate resources at this time "
          "and on this machine for this driver.  Killing further "
          "initialization.\n");
  if (!np)
     return;

  unregister_netdev (dev);

  if (np->resources_reserved & TC990_DEVICE_IN_ROOT_CHAIN)
     root_net_dev = np->next_module;

  if (np->resources_reserved & TC990_SLOT_MEMORY_ALLOCATED)
  {
    /* Free all the receive buffers */
    if (np->slot)
    {
      for (index = 0; index < RX_ENTRIES - 1; index++)
      {
        if (np->slot[index].skb)
           k_free ((void*)np->slot[index].skb);

        np->slot[index].virtual_addr_lo = 0;
        np->slot[index].physical_addr_lo = 0;
      }
    }
    k_free (np->slot);
  }

  if (np->resources_reserved & TC990_SHARED_MEMORY_ALLOCATED)
     k_free (np->shared);

  k_free (np);
  k_free (dev);
}


static WORD issue_command (struct device *dev, WORD Command, WORD Parameter1,
                          DWORD Parameter2, DWORD Parameter3, WORD *Return1,
                          DWORD *Return2, DWORD *Return3, BYTE WaitForResponse)
{
  struct tc990_private *np = (struct tc990_private *) dev->priv;
  CMD_DSC              *commandDescriptor;
  RESPONSE_DSC         *responseDescriptor;
  DWORD count, responseStatus;
  long  ioaddr = dev->base_addr;

  commandDescriptor = (CMD_DSC*) (np->ring_info.CmdRing.RingBase +
                      np->ring_info.CmdRing.LastWriteUL);
  memset (commandDescriptor, 0, sizeof(CMD_DSC));

  if (WaitForResponse)          /* Check if wait for response is needed. */
      /* Set the flag indicating the need for a response
       * to the command.
       */
       responseStatus = CMD_DSC_RESPONSE_NEEDED;
  else responseStatus = CMD_DSC_RESPONSE_NOT_NEEDED;

  commandDescriptor->Command        = Command;
  commandDescriptor->num_descriptor = 0;
  commandDescriptor->Parameter1     = Parameter1;
  commandDescriptor->Parameter2     = Parameter2;
  commandDescriptor->Parameter3     = Parameter3;
  commandDescriptor->Flags          = CMD_DSC_FRAME_VALID | responseStatus |
                                      CMD_DSC_TYPE_CMD_FRAME;
  commandDescriptor->SequenceNo = 0x11;

  UPDATE_INDEX (np->ring_info.CmdRing.LastWriteUL, sizeof(CMD_DSC), CMD_ENTRIES);

  writel (np->ring_info.CmdRing.LastWriteUL, ioaddr + tc990_HostTo3XP_COMM_2_REG);

  if (WaitForResponse)
  {                             /* Check for response */
    for (count = 0; count < tc990_WAIT_COUNTER; count++)
    {
      /* If firmware has posted the response then break from loop
       */
      if (np->shared->var.hvWriteS.regRespReadUL != np->shared->var.hvReadS.regRespWriteUL)
      {
        if (tc990_CMD_READ_STATS == commandDescriptor->Command)
        {
          process_response (np, np->shared->var.hvWriteS.regRespReadUL, 0);
          return (0);
        }
        if (tc990_STATUS_CMD_FOUND == process_response (np,
            np->shared->var.hvWriteS.regRespReadUL, (WORD)commandDescriptor->Command))
           break;
      }
      udelay (50);
    }
    /* If no response for tc990_WAIT_COUNTER, return failure.
     */
    if (count >= tc990_WAIT_COUNTER)
       return (1);

    responseDescriptor = (RESPONSE_DSC*) (np->ring_info.RspRing.RingBase +
                         np->shared->var.hvWriteS.regRespReadUL);
    if (responseDescriptor->nrml.Flags & RESPONSE_DSC_ERROR_SET)
       return (1);

    /* Command is successful, pass the response results back.
     */
    if (Return1)
       *Return1 = (WORD) responseDescriptor->nrml.Parameter1;
    if (Return2)
       *Return2 = responseDescriptor->nrml.Parameter2;
    if (Return3)
       *Return3 = responseDescriptor->nrml.Parameter3;
    UPDATE_INDEX (np->shared->var.hvWriteS.regRespReadUL, sizeof (RESPONSE_DSC), RESPONSE_ENTRIES);
  }
  return (0);
}

static WORD reset_adapter (struct device *dev)
{
  DWORD count;
  long  ioaddr = dev->base_addr;

  writel (tc990_RESET_ALL, ioaddr + tc990_SOFT_RESET_REG);
  udelay (10);     /* Give the board some time to adjust */

  writel (0, ioaddr + tc990_SOFT_RESET_REG);
  for (count = 0; count < tc990_WAIT_COUNTER; count++)
  {
    if (readl (ioaddr + tc990_3XP2HOST_COMM_0_REG) == tc990_WAITING_FOR_HOST_REQUEST)
       break;
    udelay (50);
  }
  if (count >= tc990_WAIT_COUNTER)
    return (1);
  return (0);

}


static WORD download_boot_record (struct device *dev, WORD process)
{
  struct tc990_private *np = (struct tc990_private *) dev->priv;
  DWORD  val, counter;
  long   ioaddr = dev->base_addr;

  /* Check if the adapter status is waiting for boot.
   */
  for (counter = 0; counter < tc990_WAIT_COUNTER; counter++)
  {
    val = readl (ioaddr + tc990_3XP2HOST_COMM_0_REG);
    if (val == process)
      break;
    udelay (50);
  }

  if (tc990_WAIT_COUNTER == counter)
    return (21);

  /* register the ring with the firmware
   */
  writel (0, ioaddr + tc990_HostTo3XP_COMM_2_REG);
  writel (np->ringPhysical, ioaddr + tc990_HostTo3XP_COMM_1_REG);

  writel (tc990_BOOTCMD_REG_BOOT_RECORD, ioaddr + tc990_HostTo3XP_COMM_0_REG);

  /* wait for the firmware to pick up the command.
   */
  for (counter = 0; counter < tc990_WAIT_COUNTER; counter++)
  {
    val = readl (ioaddr + tc990_3XP2HOST_COMM_0_REG);
    if (val != process)
       break;
    udelay (50);
  }

  if (tc990_WAIT_COUNTER == counter)
     return (22);

  /* clear the tx and cmd ring write registers
   */
  writel (tc990_NULL_CMD, ioaddr + tc990_HostTo3XP_COMM_1_REG);
  writel (tc990_NULL_CMD, ioaddr + tc990_HostTo3XP_COMM_2_REG);
  writel (tc990_NULL_CMD, ioaddr + tc990_HostTo3XP_COMM_3_REG);
  writel (tc990_NULL_CMD, ioaddr + tc990_HostTo3XP_COMM_0_REG);

  return (0);
}

static WORD init_ring_zone (struct device *dev)
{
  struct tc990_private *np = (struct tc990_private*) dev->priv;
  struct sk_buff       *skb;
  RX_FREE_DSC          *rxFreeDescriptor;
  DWORD  physicalLow = (DWORD) np->shared_dma_addr;
  WORD   index;
  WORD   rxBufferSize = ETHERNET_MAXIMUM_FRAME_SIZE + 4;

  np->ringPhysical = physicalLow;

  /* initialize the BootRecord and HostRingReadWrite registers
   */
  np->shared->init.hostZeroWordUL = physicalLow + sizeof (HOST_INIT_S);
  np->shared->init.hostZeroWordHiUL = 0;

  physicalLow += sizeof (HOST_INIT_S) + sizeof (DWORD);
  np->shared->init.hostVarsPS = (HOST_VAR_S *) physicalLow;
  np->shared->init.hostVarsHiUL = 0;

  physicalLow += sizeof (HOST_VAR_S);
  np->ring_info.TxLoRing.RingBase = (DWORD) BUS_TO_VIRT (physicalLow);
  np->shared->init.hostTxLoStartUL = physicalLow;
  np->shared->init.hostTxLoStartHiUL = 0;
  np->shared->init.hostTxLoSizeUL = TX_ENTRIES * sizeof (TX_DESC);

  physicalLow += np->shared->init.hostTxLoSizeUL;
  np->ring_info.TxHiRing.RingBase = (DWORD) BUS_TO_VIRT (physicalLow);
  np->shared->init.hostTxHiStartUL = physicalLow;
  np->shared->init.hostTxHiStartHiUL = 0;
  np->shared->init.hostTxHiSizeUL = TX_ENTRIES * sizeof (TX_DESC);

  physicalLow += np->shared->init.hostTxHiSizeUL;

  /* Save the address of RxLo descriptor ring.
   * This ring is filled by f/w.
   */
  np->ring_info.RxLoRing.RingBase = (DWORD) BUS_TO_VIRT (physicalLow);
  np->shared->init.hostRxLoStartUL = physicalLow;
  np->shared->init.hostRxLoStartHiUL = 0;
  np->shared->init.hostRxLoSizeUL = RX_ENTRIES * sizeof (RX_DSC);

  physicalLow += np->shared->init.hostRxLoSizeUL;

  /* Save the address of RxHi descriptor ring.
   * This ring is filled by f/w.
   */
  np->ring_info.RxHiRing.RingBase = (DWORD) BUS_TO_VIRT (physicalLow);
  np->shared->init.hostRxHiStartUL = physicalLow;
  np->shared->init.hostRxHiStartHiUL = 0;
  np->shared->init.hostRxHiSizeUL = RX_ENTRIES * sizeof (RX_DSC);

  physicalLow += np->shared->init.hostRxHiSizeUL;

  /* Save the address of Rx buffer ring.
   * This ring is filled by driver.
   */
  np->ring_info.RxBuffRing.RingBase = (DWORD) BUS_TO_VIRT (physicalLow);
  np->shared->init.hostRxFreeStartUL = physicalLow;
  np->shared->init.hostRxFreeStartHiUL = 0;
  np->shared->init.hostRxFreeSizeUL = RX_ENTRIES * sizeof (RX_FREE_DSC);

  physicalLow += np->shared->init.hostRxFreeSizeUL;

  /* Save the command and response buffers
   */
  np->ring_info.CmdRing.RingBase = (DWORD) BUS_TO_VIRT (physicalLow);
  np->shared->init.hostCtrlStartUL = physicalLow;
  np->shared->init.hostCtrlStartHiUL = 0;
  np->shared->init.hostCtrlSizeUL = CMD_ENTRIES * sizeof (CMD_DSC);

  physicalLow += np->shared->init.hostCtrlSizeUL;
  np->ring_info.RspRing.RingBase = (DWORD) BUS_TO_VIRT (physicalLow);
  np->shared->init.hostRespStartUL = physicalLow;
  np->shared->init.hostRespStartHiUL = 0;
  np->shared->init.hostRespSizeUL = RESPONSE_ENTRIES * sizeof (RESPONSE_DSC);

  np->shared->var.hvWriteS.regRxBuffWriteUL = (RX_ENTRIES-1) * sizeof(RX_FREE_DSC);

  /* Initialize counters  (from ResetRingStructures)
   */
  np->ring_info.RxLoRing.LastWriteUL = 0;
  np->ring_info.RxHiRing.LastWriteUL = 0;
  np->ring_info.RxBuffRing.LastWriteUL = 0;
  np->ring_info.TxLoRing.LastWriteUL = 0;
  np->ring_info.TxHiRing.LastWriteUL = 0;
  np->ring_info.CmdRing.LastWriteUL = 0;

  np->ring_info.TxLoRing.LastReadUL = 0;
  np->ring_info.TxHiRing.LastReadUL = 0;
  np->ring_info.TxLoRing.WriteRegister = tc990_HostTo3XP_COMM_3_REG;
  np->ring_info.TxHiRing.WriteRegister = tc990_HostTo3XP_COMM_1_REG;
  np->ring_info.TxLoRing.PacketPendingNo = 0;
  np->ring_info.TxHiRing.PacketPendingNo = 0;

  np->ring_info.TxLoRing.NoEntries = TX_ENTRIES;
  np->ring_info.TxHiRing.NoEntries = TX_ENTRIES;
  np->ring_info.CmdRing.NoEntries = CMD_ENTRIES;
  np->ring_info.RspRing.NoEntries = RESPONSE_ENTRIES;

  np->slot_memory_size = ((RX_ENTRIES - 1) * (sizeof (struct SLOT)));

  np->slot = pci_alloc_consistent (np->slot_memory_size, &np->slot_physical);
  if (!np->slot)
    return 50;

  memset (np->slot, 0, np->slot_memory_size);
  np->resources_reserved |= TC990_SLOT_MEMORY_ALLOCATED;

  /* Alloc slots
   * Put the addresses of all the receive buffers in the receive buffer
   * descriptor ring.    
   */
  for (index = 0; index < RX_ENTRIES - 1; index++)
  {
    if ((skb = dev_alloc_skb (rxBufferSize)) == NULL)
    {
      printk ("3c990: Allocate rx skb failed.\n");
      printk ("Try reducing RX_ENRIES value and recompile\n");
      return (52);
    }
    skb->dev = dev;

    /* Save the information in the slot.
     */
    np->slot[index].virtual_addr_lo = (DWORD) skb->tail;
    np->slot[index].virtual_addr_hi = 0;
    np->slot[index].skb = (DWORD) skb;
    np->slot[index].physical_addr_lo = VIRT_TO_BUS ((void*)np->slot[index].virtual_addr_lo);

    np->slot[index].physical_addr_hi = 0;
    np->slot[index].buffer_length = rxBufferSize;
  }

  /* Indicate all the buffers are free.
   */
  rxFreeDescriptor = (RX_FREE_DSC *) np->ring_info.RxBuffRing.RingBase;
  for (index = 0; index < RX_ENTRIES - 1; index++)
  {
    rxFreeDescriptor->physical_addr_lo = np->slot[index].physical_addr_lo;
    rxFreeDescriptor->physical_addr_hi = np->slot[index].physical_addr_hi;
    rxFreeDescriptor->virtual_addr_lo = index;
    rxFreeDescriptor->virtual_addr_hi = 0;
    rxFreeDescriptor++;
  }
  return (0);
}

static WORD download_runtime_image (struct device *dev)
{
  struct tc990_private *np = (struct tc990_private *) dev->priv;
  tc990_FILE_HEADER    *fileHeader;
  tc990_FILE_SECTION   *fileSection;
  DWORD  index;
  DWORD  count, value, oldIntMask, oldIntEnable;
  DWORD  tempLength, thisSectionHeaderLocation, thisSectionDataLocation;
  DWORD  start_address, numBytesInThisSection, checksum;
  DWORD  sections;
  long   ioaddr = dev->base_addr;
  void  *sectionDataBuffer;

  np->DownloadPageVirtual = pci_alloc_consistent (tc990_IMAGE_SIZE,
                                                  &np->DownloadPagePhysical);
  if (!np->DownloadPageVirtual)
     return (1);

  fileHeader = (tc990_FILE_HEADER *) tc990image;

  if (fileHeader->tagID[0] != 'T' || fileHeader->tagID[1] != 'Y' ||
      fileHeader->tagID[2] != 'P' || fileHeader->tagID[3] != 'H' ||
      fileHeader->tagID[4] != 'O' || fileHeader->tagID[5] != 'O' ||
      fileHeader->tagID[6] != 'N')
  {
    k_free (np->DownloadPageVirtual);
    return (1);
  }

  if (reset_adapter (dev))
  {
    pci_free_consistent (np->DownloadPageVirtual);
    return (1);
  }

  udelay (5000);
  oldIntEnable = readl (ioaddr + tc990_3XP2HOST_COMM_0_REG);

  /* Mask the interrupts and enable the interrupts.
   */
  oldIntEnable = readl (ioaddr + tc990_INT_ENABLE_REG);
  writel (oldIntEnable | tc990_3XP_COMM_INT0, ioaddr + tc990_INT_ENABLE_REG);
  oldIntMask = readl (ioaddr + tc990_INT_MASK_REG);
  writel (oldIntMask | tc990_3XP_COMM_INT0, ioaddr + tc990_INT_MASK_REG);

  for (count = 0; count < tc990_WAIT_COUNTER; count++)
  {
    value = readl (ioaddr + tc990_3XP2HOST_COMM_0_REG);
    if (value == tc990_WAITING_FOR_HOST_REQUEST)
       break;
    udelay (50);
  }

  if (count == tc990_WAIT_COUNTER)
  {
    pci_free_consistent (np->DownloadPageVirtual);
    return (1);
  }

  udelay (5000);

  /* Acknowledge the status
   */
  writel (tc990_3XP_COMM_INT0, ioaddr + tc990_INT_STATUS_REG);
  writel (fileHeader->ExecuteAddress, ioaddr + tc990_HostTo3XP_COMM_1_REG);
  writel (tc990_BOOTCMD_RUNTIME_IMAGE, ioaddr + tc990_HostTo3XP_COMM_0_REG);

  thisSectionHeaderLocation = sizeof (struct _tc990_FILE_HEADER);

  sections = fileHeader->NumSections;

  for (index = 0; index < sections; index++)
  {  
    fileSection = (tc990_FILE_SECTION *) (tc990image + thisSectionHeaderLocation);

    numBytesInThisSection = fileSection->num_bytes;
    start_address = fileSection->start_address;
    thisSectionDataLocation = sizeof (tc990_FILE_SECTION);

    while (numBytesInThisSection)
    {
      /* wait for 3cr990 to be ready to accept this section.
       */
      for (count = 0; count < tc990_WAIT_COUNTER; count++)
      {
        value = readl (ioaddr + tc990_INT_STATUS_REG);
        if (value & tc990_3XP_COMM_INT0)
           break;
        udelay (50);
      }

      if (count == tc990_WAIT_COUNTER)
      {
        pci_free_consistent (np->DownloadPageVirtual);
        return (1);
      }

      udelay (5000);
      writel (tc990_3XP_COMM_INT0, ioaddr + tc990_INT_STATUS_REG);
      value = readl (ioaddr + tc990_3XP2HOST_COMM_0_REG);

      if (value != tc990_WAITING_FOR_SEGMENT)
      {
        pci_free_consistent (np->DownloadPageVirtual);
        return (1);
      }

      /* Calculate the location of the data.
       */
      sectionDataBuffer = (BYTE*) (tc990image + thisSectionHeaderLocation +
                                   thisSectionDataLocation);
      /* 
       * 3cr990 is ready to accept section. 
       * Prepare the section. Download entire segment if it fits
       */
      tempLength = numBytesInThisSection;
      memcpy (np->DownloadPageVirtual, sectionDataBuffer, tempLength);
      checksum = calculate_buffer_checksum ((WORD*) np->DownloadPageVirtual, tempLength);

      writel (tempLength, ioaddr + tc990_HostTo3XP_COMM_1_REG);
      writel (checksum, ioaddr + tc990_HostTo3XP_COMM_2_REG);
      writel (start_address, ioaddr + tc990_HostTo3XP_COMM_3_REG);
      writel (0, ioaddr + tc990_HostTo3XP_COMM_4_REG);
      writel (np->DownloadPagePhysical, ioaddr + tc990_HostTo3XP_COMM_5_REG);

      writel (tc990_BOOTCMD_SEGMENT_AVAILABLE, ioaddr + tc990_HostTo3XP_COMM_0_REG);

      /* Update length and load address for next pass.
       */
      numBytesInThisSection -= tempLength;
      start_address += tempLength;

      /* Update the section data location.
       */
      thisSectionDataLocation += tempLength;
    }

    /* Update the section header location
     */
    numBytesInThisSection = fileSection->num_bytes;
    thisSectionHeaderLocation += (numBytesInThisSection + sizeof(tc990_FILE_SECTION));
  }

  udelay (5000);

  /* We have been able to download all the sections successfully.
   * Tell 3cr990 we are done and wait for a boot message.
   */
  writel (tc990_BOOTCMD_DOWNLOAD_COMPLETE, ioaddr + tc990_HostTo3XP_COMM_0_REG);
  for (count = 0; count < tc990_WAIT_COUNTER; count++)
  {
    value = readl (ioaddr + tc990_3XP2HOST_COMM_0_REG);
    if (value == tc990_WAITING_FOR_BOOT)
       break;
    udelay (50);
  }

  if (count == tc990_WAIT_COUNTER)
  {
    pci_free_consistent (np->DownloadPageVirtual);
    return (1);
  }

  udelay (100);

  /* Program the original values back on IntEnable and IntMask
   */
  writel (oldIntMask, ioaddr + tc990_INT_MASK_REG);
  writel (oldIntEnable, ioaddr + tc990_INT_ENABLE_REG);

  pci_free_consistent (np->DownloadPageVirtual);
  return (0);
}

static WORD calculate_buffer_checksum (WORD * Buffer, DWORD Count)
{
  DWORD checksum = 0;

  while (Count > 1)
  {
    /* This is the inner loop.
     */
    checksum += *Buffer++;
    Count -= 2;

    /* Fold 32-bit checksum to 16 bits.
     */
    while (checksum >> 16)
      checksum = (checksum & 0xffff) + (checksum >> 16);
  }
  if (Count > 0)
     checksum += *Buffer;
  while (checksum >> 16)
     checksum = (checksum & 0xffff) + (checksum >> 16);
  return (WORD) (~checksum);
}

static void update_tx_ring_index (DWORD * writePUW)
{
  *writePUW += sizeof (TX_DESC);
  if (*writePUW >= (TX_ENTRIES * sizeof (TX_DESC)))
     *writePUW = 0;
}

#endif
