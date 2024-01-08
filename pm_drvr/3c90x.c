/*
 * 3Com EtherLink 10/100 PCI (3C90x) Linux Network Driver, Copyright (c) 1999
 * 3Com Corporation. All rights reserved.
 * 3Com Linux Network Driver software is distributed as is, without any warranty
 * of any kind, either express or implied as further specified in the GNU Public
 * License. This software may be used and distributed according to the terms of
 * the GNU Public License, located in the file LICENSE.
 * 
 * 3Com and EtherLink are registered trademarks of 3Com Corporation. Linux is a
 * registered trademarks of Linus Torvalds. 
 * 
 * 
 * Credit
 * ------
 * Special thanks goes to Donald Becker for providing the skeleton driver outline
 * used in this driver, and for the hard work he has put into the 3c59x EtherLink
 * The 3Com 3c90x driver works in cooperation with his 3c59x driver.
 * 
 * skeleton.c: A network driver out line for Linux.
 * Copyright 1993 Uninted States Government as represented by the Director, National
 * Security Agency.
 * 
 * Donald Becker may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O Center of
 * Excellence in Space Data and Information Sciences Code 930.5, Goddard Space Flight
 * Center, Greenbelt MD 20771
 * http://cesdis.gsfc.nasa.gov/linux/
 * 
 * 
 * This is 3Com's EtherLink PCI driver for Linux.  It provides support for the
 * 3c90x and 3c980 network adapters listed here:
 * 
 * EtherLink 10/100 PCI NICs
 *   3C905C Family and 3C920 ASICs EtherLink 10/100 PCI including
 * the -TX and -TX-M
 *   3C905B Family and 3C918 ASICs EtherLink 10/100 PCI including
 * the -TX -TX-M and -TX-NM
 *   3C905B-COMBO  EtherLink 10/100 PCI COMBO
 *   3C905B-T4     EtherLink 10/100 PCI T4
 * 
 * EtherLink Server 10/100 PCI NICs
 *   3C980C-TX    EtherLink Server 10/100 PCI
 *   3C980B-TX    EtherLink Server 10/100 PCI
 *   3C980-TX     EtherLink Server 10/100 PCI
 * 
 * EtherLink 100 PCI NIC
 *   3C905B-FX     EtherLink 100 PCI Fiber
 * 
 * EtherLink 10 PCI NICs
 *   3C900B-TPO    EtherLink 10 PCI TPO
 *   3C900B-TPC    EtherLink 10 PCI TPC
 *   3C900B-COMBO  EtherLink 10 PCI COMBO
 *   3C900B-FL     EtherLink 10 PCI Fiber
 * 
 * E-mail Support:
 * 
 * - USA or Canada: 3COM_US_NIC_FAMILY@3COM.COM
 * - Mexico and Latin America: AMI_HD@3com.com
 * - Brazil: br-nicsupport@3com.com 
 * - Europe, Middle East and Africa: European_Technical_Support@3com.com
 * - Asia Pacific Rim: apr_technical_support@3com.com
 * 
 * URL: http://support.3com.com/infodeli/tools/nic/linux.htm
 */

#include "pmdrvr.h"
#include "module.h"
#include "bios32.h"
#include "pci.h"

#define IMPLEMENT_3C90X
#include "3c90x.h"

#undef  STATIC
#define STATIC                  /* for.map-file */

#define SERR_NEEDED 1

static char *version = "3Com 3c90x Version 1.0.0i 1999 <linux_drivers@3com.com>\n";

static const int MTU = 1500;

static DWORD tc90x_Index = 0;

static BYTE BroadcastAddr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

DWORD tc90xbc_debug = 0;

static BOOL  InWaitTimer;
static BOOL  DCConverterEnabledState_g;
static DWORD TimeOutCount = 0;
static WORD  MediaStatus_g = 0;
static BOOL  PhyResponding_g;
static WORD  PhyStatus_g;
static DWORD DownListPointer_g;
static DWORD UpListPointer_g;
static DWORD portValue_g;
static DWORD dmaControl_g;

static struct device    *RootNICDevice = NULL;
static struct timer_list WaitTimer;

STATIC void  FillDeviceStructure (struct NIC_INFORMATION *Adapter);
STATIC int   ScanDevices (struct device *Device);
STATIC DWORD ReadCommandLineChanges (struct NIC_INFORMATION *Adapter);
STATIC void  FreeAdapterResources (struct NIC_INFORMATION *Adapter);
STATIC DWORD AllocateSharedMemory (struct NIC_INFORMATION *Adapter);
STATIC void  WaitTimerHandler (DWORD Data);
STATIC DWORD RegisterAdapter (struct NIC_INFORMATION *Adapter);
STATIC DWORD GetAdapterProperties (struct NIC_INFORMATION *Adapter);
STATIC DWORD BasicInitializeAdapter (struct NIC_INFORMATION *Adapter);
STATIC DWORD TestAdapter (struct NIC_INFORMATION *Adapter);
STATIC DWORD SetupMedia (struct device *Device);
STATIC DWORD SoftwareWork (struct NIC_INFORMATION *Adapter);
STATIC DWORD StartAdapter (struct NIC_INFORMATION *Adapter);
STATIC void  ReStartAdapter (struct NIC_INFORMATION *Adapter);
STATIC void  CleanupSendLogic (struct device *Device);
STATIC BOOL  TryMII (struct NIC_INFORMATION *Adapter, WORD MediaOptions);
STATIC BOOL  TryLinkBeat (struct NIC_INFORMATION *Adapter, enum CONNECTOR_TYPE NewConnector);
STATIC void  SetupConnector (struct NIC_INFORMATION *, enum CONNECTOR_TYPE, enum CONNECTOR_TYPE *);
STATIC BOOL  TestPacket (struct NIC_INFORMATION *Adapter);
STATIC BOOL  CheckDCConverter (struct NIC_INFORMATION *Adapter, BOOL EnabledState);
STATIC BOOL  CheckMIIConfiguration (struct NIC_INFORMATION *Adapter, WORD MediaOptions);
STATIC void  CheckMIIAutoNegotiationStatus (struct NIC_INFORMATION *Adapter);
STATIC BOOL  FindMIIPhy (struct NIC_INFORMATION *Adapter);
STATIC BOOL  ReadMIIPhy (struct NIC_INFORMATION *Adapter, WORD RegisterAddress, WORD *pInput);
STATIC BOOL  ProgramMII (struct NIC_INFORMATION *Adapter, enum CONNECTOR_TYPE NewConnector);
STATIC BOOL  ConfigureMII (struct NIC_INFORMATION *Adapter, WORD MediaOptions);
STATIC void  HurricaneEarlyRevision (struct NIC_INFORMATION *Adapter);
STATIC BOOL  GetLinkSpeed (struct NIC_INFORMATION *Adapter, BOOL *handles100Mbitptr);
STATIC BOOL  DownloadSelfDirected (struct NIC_INFORMATION *Adapter);
STATIC DWORD ReadEEPROM (struct NIC_INFORMATION *Adapter, WORD EEPROMAddress, WORD *Contents);
STATIC DWORD ResetAndEnableTransmitter (struct NIC_INFORMATION *Adapter);
STATIC DWORD ResetAndEnableReceiver (struct NIC_INFORMATION *Adapter);
STATIC WORD  HashAddress (BYTE *Address);
STATIC void  InitializeHashFilter (struct NIC_INFORMATION *Adapter);
STATIC void  ProcessMediaOverrides (struct NIC_INFORMATION *Adapter, WORD OptionAvailable);
STATIC void  FlowControl (struct NIC_INFORMATION *Adapter);
STATIC void  CheckTPLinkState (struct NIC_INFORMATION *Adapter);
STATIC void  CheckFXLinkState (struct NIC_INFORMATION *Adapter);
STATIC void  IndicateToOSLinkStateChange (struct NIC_INFORMATION *Adapter);
STATIC void  SetupNewSpeed (struct NIC_INFORMATION *Adapter);
STATIC DWORD SetupNewDuplex (struct NIC_INFORMATION *Adapter);
STATIC void  TxCompleteEvent (struct NIC_INFORMATION *Adapter);
STATIC void  UpCompleteEvent (struct NIC_INFORMATION *Adapter);
STATIC void  HostErrorEvent (struct NIC_INFORMATION *Adapter);
STATIC void  UpdateStatisticsEvent (struct NIC_INFORMATION *Adapter);
STATIC void  CountDownTimerEvent (struct NIC_INFORMATION *Adapter);

STATIC int   NICOpen (struct device *Device);
STATIC void  NICClose (struct device *Device);
STATIC int   NICSendPacket (struct device *Device, const void *buf, int len);
STATIC void *NICGetStatistics (struct device *Device);
STATIC void  NICSetReceiveMode (struct device *Device);
STATIC void  NICTimer (DWORD Data);
STATIC void  NICInterrupt (int Irq);
         

/*
 * This routine finds the adapter using ScanDevices()
 */
int tc90xbc_probe (struct device *Device)
{
  static int scanned = 0;

  if (scanned++)
     return (0);

  printk (version);

  if (ScanDevices (Device) == NIC_STATUS_SUCCESS)
     return (1);
  return (0);
}


STATIC int ScanDevices (struct device *Device)
{
  struct NIC_INFORMATION *adapter = NULL;
  static int pciIndex = 0;
  int    ioBaseAddress, interruptVector, noAdapterFound = 0;
  WORD   pciCommand, vendorId, deviceId;
  BYTE   pciBus, pciDeviceFunction;
  BYTE   cacheLineSize, revisionId;
  WORD   powerManagementControl;

  for (; pciIndex < 0xff; pciIndex++)
  {
    if (pcibios_find_class (PCI_CLASS_NETWORK_ETHERNET << 8,
                            pciIndex, &pciBus, &pciDeviceFunction) !=
                           PCIBIOS_SUCCESSFUL)
       break;

    pcibios_read_config_word (pciBus, pciDeviceFunction,
                              PCI_VENDOR_ID, &vendorId);
    if (vendorId != NIC_VENDOR_ID)
       continue;

    pcibios_read_config_word (pciBus, pciDeviceFunction, PCI_DEVICE_ID,
                              &deviceId);
    switch (deviceId)
    {
      case NIC_PCI_DEVICE_ID_9055:
           DBGPRINT_INIT (("10/100 Base-TX NIC found\n"));
           break;

      case NIC_PCI_DEVICE_ID_9058:
           DBGPRINT_INIT (("10/100 COMBO Deluxe board found\n"));
           break;

      case NIC_PCI_DEVICE_ID_9004:
           DBGPRINT_INIT (("10Base-T TPO NIC found\n"));
           break;

      case NIC_PCI_DEVICE_ID_9005:
           DBGPRINT_INIT (("10Base-T/10Base-2/AUI Combo found\n"));

      case NIC_PCI_DEVICE_ID_9006:
           DBGPRINT_INIT (("10Base-T/10Base-2/TPC found\n"));
           break;

      case NIC_PCI_DEVICE_ID_900A:
           DBGPRINT_INIT (("10Base-FL NIC found\n"));
           break;

      case NIC_PCI_DEVICE_ID_905A:
           DBGPRINT_INIT (("100Base-Fx NIC found\n"));
           break;

      case NIC_PCI_DEVICE_ID_9200:
           DBGPRINT_INIT (("Tornado NIC found\n"));
           break;

      case NIC_PCI_DEVICE_ID_9800:
           DBGPRINT_INIT (("10/100 Base-TX NIC(Python-H) found\n"));
           break;

      case NIC_PCI_DEVICE_ID_9805:
           DBGPRINT_INIT (("10/100 Base-TX NIC(Python-T) found\n"));
           break;

      case NIC_PCI_DEVICE_ID_4500:
           DBGPRINT_INIT (("10/100 Base-TX NIC(Home Network) found\n"));
           break;

      case NIC_PCI_DEVICE_ID_7646:
           DBGPRINT_INIT (("10/100 Base-TX NIC(SOHO) found\n"));
           break;

      default:
           DBGPRINT_INIT (("UnSupported NIC found\n"));
           continue;
    }

    /* Initialize the ether device.
     */
    Device = init_etherdev (Device, 0);
    Device->priv = k_calloc (sizeof (struct NIC_INFORMATION), 1);
    adapter = (struct NIC_INFORMATION *) Device->priv;
    adapter->Device = Device;

    /* Save the NIC index.
     */
    adapter->Index = tc90x_Index++;

    pcibios_read_config_word (pciBus, pciDeviceFunction, PCI_COMMAND,
                              &pciCommand);

    if (!(pciCommand & PCI_COMMAND_MASTER))
    {
      DBGPRINT_INIT (("Enabling Bus Matering\n"));
      pciCommand |= PCI_COMMAND_MASTER;
    }
    else
    {
      DBGPRINT_INIT (("Bus Mastering enabled by BIOS\n"));
    }

    {
      struct pci_dev *pdev = pci_find_slot (pciBus,
                                            pciDeviceFunction);

      ioBaseAddress = pdev->base_address[0];
      interruptVector = pdev->irq;
    }
    ioBaseAddress &= ~1;

    DBGPRINT_INIT (("Irq = %d, IoAddress = 0x%x\n",
                   interruptVector, ioBaseAddress));

    pcibios_read_config_byte (pciBus, pciDeviceFunction,
                              PCI_REVISION_ID, &revisionId);

    pcibios_read_config_byte (pciBus, pciDeviceFunction, PCI_CACHE_LINE_SIZE,
                              &cacheLineSize);

    powerManagementControl = PCI_PME_STATUS | PCI_POWER_STATE_D0;

    pcibios_write_config_word (pciBus, pciDeviceFunction, PCI_POWER_CONTROL,
                               powerManagementControl);

    pcibios_write_config_dword (pciBus, pciDeviceFunction, PCI_BASE_ADDRESS_0,
                                ioBaseAddress);

    pcibios_write_config_byte (pciBus, pciDeviceFunction, PCI_INTERRUPT_LINE,
                               interruptVector);

    pcibios_write_config_byte (pciBus, pciDeviceFunction, PCI_CACHE_LINE_SIZE,
                               cacheLineSize);

    pcibios_write_config_word (pciBus, pciDeviceFunction, PCI_COMMAND,
                               pciCommand);

    adapter->Hardware.CacheLineSize = cacheLineSize * 4;

    if ((adapter->Hardware.CacheLineSize % 0x10) ||
        (!adapter->Hardware.CacheLineSize))
    {
      DBGPRINT_ERROR (("tc90xbc_Scan: Cacheline size wrong\n"));
      adapter->Hardware.CacheLineSize = 0x20;
    }

    /* Save the variables in adapter structure
     */
    adapter->IoBaseAddress       = ioBaseAddress;
    adapter->PCI.IoBaseAddress   = ioBaseAddress;
    adapter->PCI.InterruptVector = interruptVector;
    adapter->Hardware.RevisionId = revisionId;
    adapter->Hardware.DeviceId   = deviceId;

    /* Fill the device structure
     */
    FillDeviceStructure (adapter);

    if (ReadCommandLineChanges (adapter) != NIC_STATUS_SUCCESS)
    {
      DBGPRINT_ERROR (("ReadCommandLineChanges failed\n"));
      FreeAdapterResources (adapter);
      continue;
    }

    /* Allocate Shared memory
     */
    if (AllocateSharedMemory (adapter) != NIC_STATUS_SUCCESS)
    { 
      DBGPRINT_ERROR (("AllocateSharedMemory failed\n"));
      FreeAdapterResources (adapter);
      continue;
    }

    /* Set the root device and the next device.
     */
    ((struct NIC_INFORMATION*) (Device->priv))->NextDevice = RootNICDevice;
    ((struct NIC_INFORMATION*) (Device->priv))->Device     = Device;
    RootNICDevice = Device;

    Device = 0;
    noAdapterFound++;
  }

  if (noAdapterFound)
  {
    DBGPRINT_INITIALIZE (("NoAdapter = %x\n", noAdapterFound));
    return (NIC_STATUS_SUCCESS);
  }
  return (0);
}

/*
 * This routine fills the device structure 
 */
STATIC void FillDeviceStructure (struct NIC_INFORMATION *adapter)
{
  struct device *device = adapter->Device;

  device->base_addr = adapter->PCI.IoBaseAddress;
  device->irq       = adapter->PCI.InterruptVector;
  device->mtu       = MTU;

  /* Set the routine addresses
   */
  device->open      = NICOpen;
  device->xmit      = NICSendPacket;
  device->close     = NICClose;
  device->get_stats = NICGetStatistics;
  device->set_multicast_list = NICSetReceiveMode;
}


/*
 * This routine opens the interface. 
 */
STATIC int NICOpen (struct device *Device)
{
  struct NIC_INFORMATION *adapter = (struct NIC_INFORMATION*) Device->priv;

  DBGPRINT_FUNCTION (("New NICOpen: IN\n"));

  /* Initialize general wait timer used during driver init
   */
  init_timer (&WaitTimer);
  WaitTimer.data     = (DWORD) Device;
  WaitTimer.function = WaitTimerHandler;
  adapter->WaitCases = NONE;
  add_timer (&WaitTimer);
  InWaitTimer = TRUE;
  adapter->ResourcesReserved |= WAIT_TIMER_REGISTERED;

  /* Register IOBase and Irq with the OS
   */
  if (RegisterAdapter (adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("NICOpen: RegisterAdapter failed\n"));
    DBGPRINT_FUNCTION (("NICOpen: Out with ERROR\n"));
    FreeAdapterResources (adapter);
    return (0);
  }

  if (GetAdapterProperties (adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("NICOpen: GetAdapterProperties failed\n"));
    DBGPRINT_FUNCTION (("NICOpen: Out with ERROR\n"));
    FreeAdapterResources (adapter);
    return (0);
  }

  if (BasicInitializeAdapter (adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("NICOpen: BasicInitializeAdapter failed\n"));
    DBGPRINT_FUNCTION (("NICOpen: Out with error\n"));
    FreeAdapterResources (adapter);
    return (0);
  }

  if (TestAdapter (adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("NICOpen: TestAdapter failed\n"));
    DBGPRINT_FUNCTION (("NICOpen: Out with error\n"));
    FreeAdapterResources (adapter);
    return (0);
  }

  if (SetupMedia (Device) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("NICOpen: SetupMedia failed\n"));
    DBGPRINT_FUNCTION (("NICOpen: Out with error\n"));
    FreeAdapterResources (adapter);
    return (0);
  }

  if (SoftwareWork (adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("NICOpen: EnableSoftwareWork failed\n"));
    DBGPRINT_FUNCTION (("NICOpen: Out with error\n"));
    FreeAdapterResources (adapter);
    return (0);
  }

  if (StartAdapter (adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("NICOpen: StartAdapter failed\n"));
    DBGPRINT_FUNCTION (("NICOpen: Out with error\n"));
    FreeAdapterResources (adapter);
    return (0);
  }

  Device->tx_busy = 0;
  Device->reentry = 0;
  Device->start = 1;

  /* Initialize the timer.
   */
  adapter->Resources.TimerInterval = 100;
  init_timer (&adapter->Resources.Timer);
  adapter->Resources.Timer.expires  = RUN_AT (HZ / 10);
  adapter->Resources.Timer.data     = (DWORD) Device;
  adapter->Resources.Timer.function = NICTimer;
  add_timer (&adapter->Resources.Timer);

  /* Timer has been registered.
   */
  adapter->ResourcesReserved |= NIC_TIMER_REGISTERED;

  DBGPRINT_FUNCTION (("NICOpen:  OUT with SUCCESS\n"));
  return (1);
}

/*
 * This routine closes the interface. 
 */
STATIC void NICClose (struct device *Device)
{ 
  struct NIC_INFORMATION *adapter = (struct NIC_INFORMATION*) Device->priv;
  struct device          *device = adapter->Device;

  DBGPRINT_FUNCTION (("NICClose: IN\n"));

  Device->start = 0;
  Device->tx_busy = 1;

  /* Disable transmit and receive.
   */
  NIC_COMMAND (adapter, COMMAND_TX_DISABLE);
  NIC_COMMAND (adapter, COMMAND_RX_DISABLE);

  /* Wait for the transmit in progress to go off.
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);
  MediaStatus_g = NIC_READ_PORT_WORD (adapter, MEDIA_STATUS_REGISTER);
  udelay (10);

  if (MediaStatus_g & MEDIA_STATUS_TX_IN_PROGRESS)
  {
    adapter->WaitCases = CHECK_TRANSMIT_IN_PROGRESS;
    TimeOutCount = jiffies + HZ; /* max = 1s */
    if (!InWaitTimer)
    {
      WaitTimer.expires = RUN_AT (HZ / 100);
      add_timer (&WaitTimer);
      InWaitTimer = TRUE;
    }
    while (TimeOutCount > jiffies) ;
    if (!(MediaStatus_g & MEDIA_STATUS_TX_IN_PROGRESS))
       DBGPRINT_ERROR (("NICClose: Adapter is not responding\n"));
  }

  /* Issue Global reset.
   */
  NIC_COMMAND_WAIT (adapter,
                    COMMAND_GLOBAL_RESET |
                    GLOBAL_RESET_MASK_TP_AUI_RESET |
                    GLOBAL_RESET_MASK_ENDEC_RESET |
                    GLOBAL_RESET_MASK_AISM_RESET |
                    GLOBAL_RESET_MASK_SMB_RESET |
                    GLOBAL_RESET_MASK_VCO_RESET);

  NIC_COMMAND_WAIT (adapter, COMMAND_TX_RESET);
  NIC_COMMAND_WAIT (adapter, COMMAND_RX_RESET);

  /* Mask and acknowledge all interrupts.
   */
  NIC_MASK_ALL_INTERRUPT (adapter);
  NIC_ACKNOWLEDGE_ALL_INTERRUPT (adapter);
  CleanupSendLogic (Device);

  /* Unregister the interrupt handler.
   */
  if (adapter->ResourcesReserved & NIC_INTERRUPT_REGISTERED)
  {
    DBGPRINT_INITIALIZE (("Releasing interrupt\n"));
    free_irq (device->irq);
    irq2dev_map[device->irq] = NULL;
    adapter->ResourcesReserved &= ~NIC_INTERRUPT_REGISTERED;
  }

  /* Unregister the timer handler.
   */
  if (adapter->ResourcesReserved & NIC_TIMER_REGISTERED)
  {
    DBGPRINT_INITIALIZE (("Releasing Timer\n"));
    if (del_timer (&adapter->Resources.Timer))
       DBGPRINT_INITIALIZE (("Timer already queued\n"));

    adapter->ResourcesReserved &= ~NIC_TIMER_REGISTERED;
  }
  if (adapter->ResourcesReserved & WAIT_TIMER_REGISTERED)
  {
    DBGPRINT_INITIALIZE (("Releasing WaitTimer\n"));
    if (del_timer (&WaitTimer))
       DBGPRINT_INITIALIZE (("WaitTimer already queued\n"));
    adapter->ResourcesReserved &= ~WAIT_TIMER_REGISTERED;
  }
  DBGPRINT_FUNCTION (("NICClose: OUT\n"));
}


/*
 * If autoselection is set, determine the connector and link speed
 * by trying the various transceiver types.
 */
STATIC void MainAutoSelectionRoutine (struct NIC_INFORMATION *adapter,
                                      WORD Options)
{
  enum CONNECTOR_TYPE NotUsed;
  WORD index;

  DBGPRINT_FUNCTION (("MainAutoSelectionRoutine:\n"));

  adapter->Hardware.Connector = CONNECTOR_UNKNOWN;

  /* Try 100MB Connectors
   */
  if ((Options & MEDIA_OPTIONS_100BASETX_AVAILABLE) ||
      (Options & MEDIA_OPTIONS_10BASET_AVAILABLE) ||
      (Options & MEDIA_OPTIONS_MII_AVAILABLE))
  {
    /* For 10Base-T and 100Base-TX, select autonegotiation
     * instead of autoselect before calling trymii
     */
    if ((Options & MEDIA_OPTIONS_100BASETX_AVAILABLE) ||
        (Options & MEDIA_OPTIONS_10BASET_AVAILABLE))
         adapter->Hardware.Connector = CONNECTOR_AUTONEGOTIATION;
    else adapter->Hardware.Connector = CONNECTOR_MII;

    DBGPRINT_INITIALIZE (("Trying MII\n"));

    if (!TryMII (adapter, Options))
       adapter->Hardware.Connector = CONNECTOR_UNKNOWN;
  }

  /* Transceiver available is 100Base-FX
   */
  if ((Options & MEDIA_OPTIONS_100BASEFX_AVAILABLE) &&
      (adapter->Hardware.Connector == CONNECTOR_UNKNOWN))
  {
    DBGPRINT_INITIALIZE (("Trying 100BFX\n"));
    if (TryLinkBeat (adapter, CONNECTOR_100BASEFX))
    {
      adapter->Hardware.Connector = CONNECTOR_100BASEFX;
      adapter->Hardware.LinkSpeed = LINK_SPEED_100;
    }
  }

  /* Transceiver available is 10AUI
   */
  if ((Options & MEDIA_OPTIONS_10AUI_AVAILABLE) &&
      (adapter->Hardware.Connector == CONNECTOR_UNKNOWN))
  {
    DBGPRINT_INITIALIZE (("Trying 10AUI\n"));
    SetupConnector (adapter, CONNECTOR_10AUI, &NotUsed);

    /* Try to loopback packet
     */
    for (index = 0; index < 3; index++)
    {
      if (TestPacket (adapter))
      {
        adapter->Hardware.Connector = CONNECTOR_10AUI;
        adapter->Hardware.LinkSpeed = LINK_SPEED_10;
        DBGPRINT_INITIALIZE (("Found AUI\n"));
        break;
      }
    }
    if (index == 3)
       DBGPRINT_INITIALIZE (("Unable to find AUI\n"));
  }

  /* Transceiver available is 10Base-2
   */
  if ((Options & MEDIA_OPTIONS_10BASE2_AVAILABLE) &&
      (adapter->Hardware.Connector == CONNECTOR_UNKNOWN))
  {
    DBGPRINT_INITIALIZE (("Trying 10BASEB2\n"));

    /* Set up the connector
     */
    SetupConnector (adapter, CONNECTOR_10BASE2, &NotUsed);

    /* Try to loopback packet
     */
    for (index = 0; index < 3; index++)
    {
      if (TestPacket (adapter))
      {
        adapter->Hardware.Connector = CONNECTOR_10BASE2;
        adapter->Hardware.LinkSpeed = LINK_SPEED_10;

        DBGPRINT_INITIALIZE (("Found 10Base2\n"));
        break;
      }
    }
    if (index == 3)
       DBGPRINT_INITIALIZE (("Unable to find 10Base2\n"));

    /* Disable DC converter
     */
    NIC_COMMAND (adapter, COMMAND_DISABLE_DC_CONVERTER);

    /* Check if DC convertor has been disabled
     */
    CheckDCConverter (adapter, FALSE);
  }

  /* Nothing left to try!
   */
  if (adapter->Hardware.Connector == CONNECTOR_UNKNOWN)
  {
    adapter->Hardware.Connector = adapter->Hardware.ConfigConnector;
    adapter->Hardware.LinkSpeed = LINK_SPEED_10;
    DBGPRINT_INITIALIZE (("AutoSelection failed. Using default.\n"));
    DBGPRINT_INITIALIZE (("Connector: %x\n", adapter->Hardware.Connector));
    adapter->Hardware.LinkState = LINK_DOWN_AT_INIT;
  }

  SetupConnector (adapter, adapter->Hardware.Connector, &NotUsed);

  DBGPRINT_FUNCTION (("MainAutoSelectionRoutine: OUT\n"));
}


/*
 * 
 */
STATIC BOOL CheckDCConverter (struct NIC_INFORMATION *adapter, BOOL EnabledState)
{
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);
  MediaStatus_g = NIC_READ_PORT_WORD (adapter, MEDIA_STATUS_REGISTER);
  udelay (1000);

  if (((EnabledState) && !(MediaStatus_g & MEDIA_STATUS_DC_CONVERTER_ENABLED)) ||
      ((!EnabledState) && (MediaStatus_g & MEDIA_STATUS_DC_CONVERTER_ENABLED)))
  {
    DCConverterEnabledState_g = EnabledState; /* for waittimer */
    adapter->WaitCases = CHECK_DC_CONVERTER;
    TimeOutCount = jiffies + 3; /* 30ms */
    if (!InWaitTimer)
    {
      WaitTimer.expires = RUN_AT (HZ / 100);
      add_timer (&WaitTimer);
      InWaitTimer = TRUE;
    }
    while (TimeOutCount > jiffies) ;
    if ((EnabledState && !(MediaStatus_g & MEDIA_STATUS_DC_CONVERTER_ENABLED)) ||
        (!EnabledState && (MediaStatus_g & MEDIA_STATUS_DC_CONVERTER_ENABLED)))
    {
      adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
      DBGPRINT_ERROR (("ConfigureDCConverter: Timeout setting DC Converter\n"));
      return (FALSE);
    }
  }
  return (TRUE);
}


/*
 * Setup new transceiver type in InternalConfig. Determine whether to 
 * set JabberGuardEnable, enableSQEStats and linkBeatEnable in MediaStatus.
 * Determine if the coax transceiver also needs to be enabled/disabled.
 */
STATIC void SetupConnector (struct NIC_INFORMATION *adapter,
                            enum CONNECTOR_TYPE     NewConnector,
                            enum CONNECTOR_TYPE    *OldConnector)
{
  DWORD InternalConfig = 0;
  DWORD OldInternalConfig = 0;
  WORD  MediaStatus = 0;

  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_3);

  InternalConfig = NIC_READ_PORT_DWORD (adapter, INTERNAL_CONFIG_REGISTER);
  OldInternalConfig = InternalConfig;

  /* Save old choice
   */
  *OldConnector = (InternalConfig & INTERNAL_CONFIG_TRANSCEIVER_MASK) >> 20;

  /* Program the MII registers if forcing the configuration to 10/100BaseT.
   */
  if (NewConnector == CONNECTOR_10BASET ||
      NewConnector == CONNECTOR_100BASETX)
  {
    /* Clear transceiver type and change to new transceiver type.
     */
    InternalConfig &= ~(INTERNAL_CONFIG_TRANSCEIVER_MASK);
    InternalConfig |= (CONNECTOR_AUTONEGOTIATION << 20);

    /* Update the internal config register. Only do this if the value has
     * changed to avoid dropping link.
     */
    if (OldInternalConfig != InternalConfig)
       NIC_WRITE_PORT_DWORD (adapter, INTERNAL_CONFIG_REGISTER, InternalConfig);

    /* Force the MII registers to the correct settings.
     */
    if (!CheckMIIConfiguration (adapter,
                                (WORD)(NewConnector == CONNECTOR_100BASETX
                                ? MEDIA_OPTIONS_100BASETX_AVAILABLE : MEDIA_OPTIONS_10BASET_AVAILABLE)))
    {
      /* If the forced configuration didn't work, check the results and see why.
       */
      CheckMIIAutoNegotiationStatus (adapter);
      return;
    }
  }
  else
  {
    /* Clear transceiver type and change to new transceiver type
     */
    InternalConfig = InternalConfig & (~INTERNAL_CONFIG_TRANSCEIVER_MASK);
    InternalConfig |= (NewConnector << 20);

    /* Update the internal config register. Only do this if the value has
     * changed to avoid dropping link.
     */
    if (OldInternalConfig != InternalConfig)
       NIC_WRITE_PORT_DWORD (adapter, INTERNAL_CONFIG_REGISTER, InternalConfig);
  }

  /* Determine whether to set enableSQEStats and linkBeatEnable
   * Automatically set JabberGuardEnable in MediaStatus register.
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);

  MediaStatus = NIC_READ_PORT_WORD (adapter, MEDIA_STATUS_REGISTER);

  MediaStatus &= ~(MEDIA_STATUS_SQE_STATISTICS_ENABLE |
                   MEDIA_STATUS_LINK_BEAT_ENABLE |
                   MEDIA_STATUS_JABBER_GUARD_ENABLE);

  MediaStatus |= MEDIA_STATUS_JABBER_GUARD_ENABLE;

  if (NewConnector == CONNECTOR_10AUI)
     MediaStatus |= MEDIA_STATUS_SQE_STATISTICS_ENABLE;

  if (NewConnector == CONNECTOR_AUTONEGOTIATION)
     MediaStatus |= MEDIA_STATUS_LINK_BEAT_ENABLE;
  else
  {
    if ((NewConnector == CONNECTOR_10BASET) ||
        (NewConnector == CONNECTOR_100BASETX) ||
        (NewConnector == CONNECTOR_100BASEFX))
    {
      if (!adapter->Hardware.LinkBeatDisable)
        MediaStatus |= MEDIA_STATUS_LINK_BEAT_ENABLE;
    }
  }
  NIC_WRITE_PORT_WORD (adapter, MEDIA_STATUS_REGISTER, MediaStatus);

  DBGPRINT_INITIALIZE (("SetupConnector: MediaStatus = %x\n", MediaStatus));

  /* If configured for coax we must start the internal transceiver.
   * If not, we stop it (in case the configuration changed across a
   * warm boot).
   */
  if (NewConnector == CONNECTOR_10BASE2)
  {
    NIC_COMMAND (adapter, COMMAND_ENABLE_DC_CONVERTER);

    /* Check if DC convertor has been enabled
     */
    CheckDCConverter (adapter, TRUE);
  }
  else
  {
    NIC_COMMAND (adapter, COMMAND_DISABLE_DC_CONVERTER);

    /* Check if DC convertor has been disabled
     */
    CheckDCConverter (adapter, FALSE);
  }
}


/*
 * Used to detect if 10Base-T, 100Base-TX, or external MII is available. 
 */
STATIC BOOL TryMII (struct NIC_INFORMATION *adapter, WORD MediaOptions)
{
  BOOL Handles100Mbit = FALSE;

#if 0
  enum CONNECTOR_TYPE NotUsed;
  WORD PhyControl, PhyStatus;
  BOOL PhyResponding;
#endif

  DBGPRINT_FUNCTION (("TryMII:\n"));

  /* First see if there's anything connected to the MII
   */
  if (!FindMIIPhy (adapter))
  {
    DBGPRINT_ERROR (("TryMII: FindMIIPhy failed\n"));
    adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
    return (FALSE);
  }

  /* Nowhere is it written that the register must be latched, and since
   * reset is the last bit out, the contents might not be valid.  read
   * it one more time.
   */
#if 0
  PhyResponding_g = ReadMIIPhy (adapter, MII_PHY_CONTROL, &PhyControl);
  if (!PhyResponding_g)
  {
    DBGPRINT_ERROR (("TryMII: Phy not responding (1)\n"));
    adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
    return (FALSE);
  }
#endif

  /* Now we can read the status and try to figure out what's out there.
   */
  PhyResponding_g = ReadMIIPhy (adapter, MII_PHY_STATUS, &PhyStatus_g);
  if (!PhyResponding_g)
  {
    DBGPRINT_ERROR (("TryMII: Phy not responding (2)\n"));
    adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
    return (FALSE);
  }

  if ((PhyStatus_g & MII_STATUS_AUTO) && (PhyStatus_g & MII_STATUS_EXTENDED))
  {
    /* If it is capable of auto negotiation, see if it has
     * been done already.
     */
    DBGPRINT_INITIALIZE (("MII Capable of Autonegotiation\n"));

    /* Check the current MII auto-negotiation state and see if we need to
     * start auto-neg over.
    */
    if (!CheckMIIConfiguration (adapter, MediaOptions))
       return (FALSE);

    /* See if link is up...
     */
    PhyResponding_g = ReadMIIPhy (adapter, MII_PHY_STATUS, &PhyStatus_g);
    if (!PhyResponding_g)
    {
      DBGPRINT_ERROR (("TryMII: Phy not responding (3)\n"));
      adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
      return (FALSE);
    }
    if (PhyStatus_g & MII_STATUS_LINK_UP)
    {
      if (!GetLinkSpeed (adapter, &Handles100Mbit))
      {
        DBGPRINT_ERROR (("TryMII: Unknown link speed\n"));
        adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
        return (FALSE);
      }
      if (Handles100Mbit)
      {
        DBGPRINT_INITIALIZE (("TryMII: Link speed set to 100\n"));
        adapter->Hardware.LinkSpeed = LINK_SPEED_100;
      }
      else
      {
        DBGPRINT_INITIALIZE (("TryMII: Link speed set to 10\n"));
        adapter->Hardware.LinkSpeed = LINK_SPEED_10;
      }
      return (TRUE);
    }

    /* Assume 10Mbit if no link
     */
    if (PhyStatus_g & MII_STATUS_100MB_MASK)
    {
      DBGPRINT_INITIALIZE (("TryMII: Link speed defaulted to 100\n"));
      adapter->Hardware.LinkSpeed = LINK_SPEED_100;
    }
    else
    {
      DBGPRINT_INITIALIZE (("TryMII: Link speed defaulted to 10\n"));
      adapter->Hardware.LinkSpeed = LINK_SPEED_10;
    }
    return (TRUE);
  }
  return (FALSE);
}


/*
 * Download a self-directed packet. If successful, 100BASE-FX is available.
 */
STATIC BOOL TryLinkBeat (struct NIC_INFORMATION *adapter,
                         enum CONNECTOR_TYPE     NewConnector)
{
  WORD MediaStatus = 0;
  BOOL retval      = FALSE;
  enum CONNECTOR_TYPE NotUsed;

  DBGPRINT_FUNCTION (("TryLinkBeat: IN\n"));

  /* Go quiet for 1.5 seconds to get any N-Way hub into a receptive
   * state to sense the new link speed.  We go quiet by switching over
   * to 10BT and disabling Linkbeat.
   */
  adapter->Hardware.LinkBeatDisable = TRUE;
  SetupConnector (adapter, CONNECTOR_10BASET, &NotUsed);

  /* Delay 1.5 seconds
   */
  TimeOutCount = jiffies + 15 * HZ / 10;
  while (TimeOutCount > jiffies)
         ;

  NIC_COMMAND_WAIT (adapter, COMMAND_TX_RESET | TX_RESET_MASK_NETWORK_RESET);
  NIC_COMMAND_WAIT (adapter, COMMAND_RX_RESET | RX_RESET_MASK_NETWORK_RESET);

  /* Set up for TP transceiver
  */
  SetupConnector (adapter, NewConnector, &NotUsed);

  /* delay 10 milliseconds
   */
  udelay (10000);

  /* We need to send a test packet to clear the the Partition if for
   * some reason it's partitioned (i.e., we were testing 100mb before.)
   * Download a 20 byte packet into the TxFIFO, from us, to us
   */
  if (!DownloadSelfDirected (adapter))
     return (FALSE);

  /* Acknowledge the down complete
   */
  NIC_COMMAND (adapter, COMMAND_ACKNOWLEDGE_INTERRUPT + INTSTATUS_DOWN_COMPLETE);

  /* Check MediaStatus for linkbeat indication
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);

  MediaStatus = NIC_READ_PORT_WORD (adapter, MEDIA_STATUS_REGISTER);

  if (MediaStatus & MEDIA_STATUS_LINK_DETECT)
     retval = TRUE;

  NIC_COMMAND_WAIT (adapter, COMMAND_TX_RESET | TX_RESET_MASK_NETWORK_RESET);
  NIC_COMMAND_WAIT (adapter, COMMAND_RX_RESET | RX_RESET_MASK_NETWORK_RESET);

  DBGPRINT_INITIALIZE (("TryLinkBeat: OUT with success\n"));
  return (retval);
}

/*
 * Download a 20 byte packet into the TxFIFO, from us, to us.
 */
STATIC BOOL DownloadSelfDirected (struct NIC_INFORMATION *adapter)
{
  struct DPD_LIST_ENTRY *dpdVirtual;
  DWORD  PacketBufPhysAddr;
  BYTE  *PacketBuffer;
  BYTE  *pbuffer;

  /* Set allocated buffer for transmit
   */
  PacketBuffer = (BYTE*) adapter->TestBufferVirtual[0];
  PacketBufPhysAddr = adapter->TestBufferPhysical[0];

  /* Fill data in the buffer
   */
  pbuffer = adapter->StationAddress;
  memcpy (PacketBuffer, pbuffer, 6);   /* destination is me */
  memcpy (PacketBuffer+6, pbuffer, 6); /* source is me */
  *(DWORD*) (PacketBuffer+12) = 0;     /* ether-type 0 */
  *(DWORD*) (PacketBuffer+16) = 0;

  /* Create a single DPD
   */
  dpdVirtual = (struct DPD_LIST_ENTRY*) adapter->TestDPDVirtual[0];

  dpdVirtual->DownNextPointer   = 0;
  dpdVirtual->FrameStartHeader  = 20 | FSH_ROUND_UP_DEFEAT;
  dpdVirtual->SGList[0].Address = PacketBufPhysAddr;
  dpdVirtual->SGList[0].Count   = 20 | 0x80000000;

  /* Download DPD
   */
  NIC_COMMAND_WAIT (adapter, COMMAND_DOWN_STALL);

  NIC_WRITE_PORT_DWORD (adapter, DOWN_LIST_POINTER_REGISTER,
                        dpdVirtual->DPDPhysicalAddress);
  NIC_COMMAND (adapter, COMMAND_DOWN_UNSTALL);

  /* Check if the DPD is done with
   */
  DownListPointer_g = NIC_READ_PORT_DWORD (adapter, DOWN_LIST_POINTER_REGISTER);

  udelay (100);

  if (DownListPointer_g == dpdVirtual->DPDPhysicalAddress)
  {
    adapter->WaitCases = CHECK_DOWNLOAD_SELFDIRECTED;
    TimeOutCount = jiffies + 3 * HZ; /* max = 3s */

#if 0
    WaitTimer.expires = RUN_AT (HZ/100);
    add_timer (&WaitTimer);
    InWaitTimer = TRUE;
#endif
    while (TimeOutCount > jiffies)
           ;
    if (DownListPointer_g != dpdVirtual->DPDPhysicalAddress)
    {
      adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
      DBGPRINT_ERROR (("DownloadSelfDirected: DPD not finished\n"));
      return (FALSE);
    }
  }
  return (TRUE);
}

STATIC BOOL CheckTransmitInProgress (struct NIC_INFORMATION *adapter)
{
  MediaStatus_g = 0;

  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);

  MediaStatus_g = NIC_READ_PORT_WORD (adapter, MEDIA_STATUS_REGISTER);

  udelay (10);

  if (MediaStatus_g & MEDIA_STATUS_TX_IN_PROGRESS)
  {
    adapter->WaitCases = CHECK_TRANSMIT_IN_PROGRESS;
    TimeOutCount = jiffies + HZ; /* max = 1s */
    if (!InWaitTimer)
    {
      WaitTimer.expires = RUN_AT (HZ / 100);
      add_timer (&WaitTimer);
      InWaitTimer = TRUE;
    }
    while (TimeOutCount > jiffies) ;
    if (!(MediaStatus_g & MEDIA_STATUS_TX_IN_PROGRESS))
    {
      adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
      DBGPRINT_ERROR (("CheckTransmitInProgress: Transmit still in progress\n"));
      return (FALSE);
    }
  }
  return (TRUE);
}


/*
 * This function is called by TryLoopback to determine if a packet can 
 * successfully be loopbacked for 10Base-2 and AUI.
 */
STATIC BOOL TestPacket (struct NIC_INFORMATION *adapter)
{
  BOOL  ReturnValue  = FALSE;
  WORD  MacControl   = 0;
  DWORD PacketStatus = 0;

  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_3);

  MacControl = NIC_READ_PORT_WORD (adapter, MAC_CONTROL_REGISTER);

  /* Enable full duplex
   */
  NIC_WRITE_PORT_WORD (adapter, MAC_CONTROL_REGISTER,
                       (WORD)(MacControl | MAC_CONTROL_FULL_DUPLEX_ENABLE));

  /* Write UpListPointer to UpListPointer register and unstall
   */
  NIC_COMMAND_WAIT (adapter, COMMAND_UP_STALL);

  NIC_WRITE_PORT_DWORD (adapter, UP_LIST_POINTER_REGISTER, adapter->HeadUPDVirtual->UPDPhysicalAddress);

  NIC_COMMAND (adapter, COMMAND_UP_UNSTALL);

  /* Enable receive and transmit and setup our packet filter
   */
  NIC_COMMAND_WAIT (adapter, COMMAND_TX_ENABLE);
  NIC_COMMAND_WAIT (adapter, COMMAND_RX_ENABLE);
  NIC_COMMAND_WAIT (adapter, COMMAND_SET_RX_FILTER + RX_FILTER_INDIVIDUAL);

  /* Create single DPD and download
   */
  if (!DownloadSelfDirected (adapter))
     return (FALSE);

  /* Check if transmit is still in progress
  */
  if (!CheckTransmitInProgress (adapter))
    return (FALSE);

  /* Reset the transmitter to get rid of any TxStatus we haven't seen yet
   */
  NIC_COMMAND_WAIT (adapter, COMMAND_TX_RESET | TX_RESET_MASK_NETWORK_RESET);

  /* Check UpListPtr to see if it has changed to see if upload complete
   */
  UpListPointer_g = NIC_READ_PORT_DWORD (adapter, UP_LIST_POINTER_REGISTER);

  udelay (100);

  if (UpListPointer_g == adapter->HeadUPDVirtual->UPDPhysicalAddress)
  {
    adapter->WaitCases = AUTONEG_TEST_PACKET;
    TimeOutCount = jiffies + HZ; /* max=1s */
    while (TimeOutCount > jiffies) ;
    if (UpListPointer_g != adapter->HeadUPDVirtual->UPDPhysicalAddress)
    {
      DBGPRINT_ERROR (("TestPacket: UPD not finished\n"));
      return (FALSE);
    }
  }

  /* Check RxStatus. If we've got a packet without any errors, this
   * connector is okay.
   */
  PacketStatus = adapter->HeadUPDVirtual->UpPacketStatus;

  if (!(PacketStatus & UP_PACKET_STATUS_ERROR) &&
       (PacketStatus & UP_PACKET_STATUS_COMPLETE))
     ReturnValue = TRUE;         /* Received a good packet */

  /* The following cleans up after the test we just ran
   */
  NIC_WRITE_PORT_DWORD (adapter, UP_LIST_POINTER_REGISTER, 0);
  NIC_WRITE_PORT_DWORD (adapter, DOWN_LIST_POINTER_REGISTER, 0);
  adapter->HeadUPDVirtual->UpPacketStatus = 0;

  /* Reset the receiver to wipe anything we haven't seen yet
   */
  NIC_COMMAND_WAIT (adapter, COMMAND_RX_RESET | RX_RESET_MASK_NETWORK_RESET);
  NIC_COMMAND (adapter, COMMAND_ACKNOWLEDGE_INTERRUPT + INTSTATUS_ACKNOWLEDGE_ALL);

  /* Get out of loopback mode
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_3);

  MacControl = NIC_READ_PORT_WORD (adapter, MAC_CONTROL_REGISTER);

  NIC_WRITE_PORT_WORD (adapter, MAC_CONTROL_REGISTER,
                       (WORD)(MacControl & ~MAC_CONTROL_FULL_DUPLEX_ENABLE));
  return (ReturnValue);
}


/*
 * Determine from the MII AutoNegotiationAdvertisement and
 * AutoNegotiationPartnerAbility registers whether the 
 * current linkspeed is 10Mbits or 100Mbits.
 */
STATIC BOOL GetLinkSpeed (struct NIC_INFORMATION *adapter,
                          BOOL *handles100Mbitptr)
{
  BOOL PhyResponding;
  WORD PhyAnlpar, PhyAner, PhyAnar, PhyStatus;

  PhyResponding = ReadMIIPhy (adapter, MII_PHY_ANER, &PhyAner);
  if (!PhyResponding)
     return (FALSE);

  PhyResponding = ReadMIIPhy (adapter, MII_PHY_ANLPAR, &PhyAnlpar);
  if (!PhyResponding)
     return (FALSE);

  PhyResponding = ReadMIIPhy (adapter, MII_PHY_ANAR, &PhyAnar);
  if (!PhyResponding)
     return (FALSE);

  PhyResponding = ReadMIIPhy (adapter, MII_PHY_STATUS, &PhyStatus);
  if (!PhyResponding)
     return (FALSE);

  /* Check to see if we've completed auto-negotiation.
   */
  if (!(PhyStatus & MII_STATUS_AUTO_DONE))
     return (FALSE);

  if ((PhyAnar & MII_ANAR_100TXFD) && (PhyAnlpar & MII_ANLPAR_100TXFD))
  {
    adapter->Hardware.MIIPhyUsed = MII_100TXFD;
    *handles100Mbitptr = TRUE;
    adapter->Hardware.FullDuplexEnable = TRUE;
  }
  else if ((PhyAnar & MII_ANAR_100TX) && (PhyAnlpar & MII_ANLPAR_100TX))
  {
    adapter->Hardware.MIIPhyUsed = MII_100TX;
    *handles100Mbitptr = TRUE;
    adapter->Hardware.FullDuplexEnable = FALSE;
  }
  else if ((PhyAnar & MII_ANAR_10TFD) && (PhyAnlpar & MII_ANLPAR_10TFD))
  {
    adapter->Hardware.MIIPhyUsed = MII_10TFD;
    adapter->Hardware.FullDuplexEnable = TRUE;
    *handles100Mbitptr = FALSE;
  }
  else if ((PhyAnar & MII_ANAR_10T) && (PhyAnlpar & MII_ANLPAR_10T))
  {
    adapter->Hardware.MIIPhyUsed = MII_10T;
    adapter->Hardware.FullDuplexEnable = FALSE;
    *handles100Mbitptr = FALSE;
  }
  else if (!(PhyAner & MII_ANER_LPANABLE))
  {
    /* Link partner is not capable of auto-negotiation. Fall back to 10HD.
     */
    adapter->Hardware.MIIPhyUsed = MII_10T;
    adapter->Hardware.FullDuplexEnable = FALSE;
    *handles100Mbitptr = FALSE;
  }
  else
    return (FALSE);

  return (TRUE);
}


/*
 * This routine frees up the adapter resources.
 */
STATIC void FreeAdapterResources (struct NIC_INFORMATION *adapter)
{
  struct device         *device            = adapter->Device;
  struct UPD_LIST_ENTRY *currentUPDVirtual = adapter->HeadUPDVirtual;

  DBGPRINT_FUNCTION (("FreeAdapterResources: IN\n"));

  if (adapter->ResourcesReserved & NIC_INTERRUPT_REGISTERED)
  {
    DBGPRINT_INITIALIZE (("Releasing interrupt\n"));
    free_irq (device->irq);
    irq2dev_map[device->irq] = NULL;
    adapter->ResourcesReserved &= ~NIC_INTERRUPT_REGISTERED;
  }

  if (adapter->ResourcesReserved & WAIT_TIMER_REGISTERED)
  {
    DBGPRINT_INITIALIZE (("Releasing WaitTimer\n"));
    if (del_timer (&WaitTimer))
       DBGPRINT_ERROR (("WaitTimer already queued\n"));
    adapter->ResourcesReserved &= ~WAIT_TIMER_REGISTERED;
  }

  if (adapter->ResourcesReserved & NIC_TIMER_REGISTERED)
  {
    DBGPRINT_INITIALIZE (("Releasing Timers\n"));
    if (del_timer (&adapter->Resources.Timer))
       DBGPRINT_ERROR (("Timer already queued\n"));

    adapter->ResourcesReserved &= ~NIC_TIMER_REGISTERED;
    if (del_timer (&WaitTimer))
       DBGPRINT_ERROR (("WaitTimer already queued\n"));
  }

  if (adapter->ResourcesReserved & NIC_SHARED_MEMORY_ALLOCATED)
  {
    DBGPRINT_INITIALIZE (("Releasing memory\n"));
    currentUPDVirtual = adapter->HeadUPDVirtual;

    /* Release the SKBs allocated
     */
    while (1)
    {
      if (currentUPDVirtual->SocketBuffer)
         k_free (currentUPDVirtual->SocketBuffer);
      currentUPDVirtual = currentUPDVirtual->Next;
      if (currentUPDVirtual == adapter->HeadUPDVirtual)
         break;
    }

    k_free (adapter->Resources.SharedMemoryVirtual);
    adapter->ResourcesReserved &= ~NIC_SHARED_MEMORY_ALLOCATED;
  }
  DBGPRINT_FUNCTION (("FreeAdapterResources: OUT\n"));
}


/*
 * This routine registers the adapter resources with Linux.
 */
STATIC DWORD RegisterAdapter (struct NIC_INFORMATION *adapter)
{
  struct device *device = adapter->Device;

  DBGPRINT_FUNCTION (("RegisterAdapter: IN\n"));

  /* Use the non-standard shared IRQ implementation.
   */
  DBGPRINT_INITIALIZE (("registering IRQ %d\n", device->irq));

  /* MUST set irq2dev_map first, because IRQ may come
   * before request_irq() returns.
   */
  irq2dev_map [device->irq] = device;
  if (!request_irq (device->irq, NICInterrupt))
  {
    irq2dev_map [device->irq] = NULL;
    DBGPRINT_ERROR (("RegisterAdapter: IRQ registration failed\n"));
    return (NIC_STATUS_FAILURE);
  }

  adapter->ResourcesReserved |= NIC_INTERRUPT_REGISTERED;

  DBGPRINT_FUNCTION (("RegisterAdapter: OUT\n"));
  return (NIC_STATUS_SUCCESS);
}

/*
 * This routine sets up the adapter.
 */
STATIC DWORD GetAdapterProperties (struct NIC_INFORMATION *adapter)
{
  struct device                *device = adapter->Device;
  struct COMPATABILITY_WORD     compatability;
  struct SOFTWARE_INFORMATION_1 information1;
  struct SOFTWARE_INFORMATION_2 information2;
  struct CAPABILITIES_WORD      capabilities;
  DWORD  nicStatus;
  WORD   eepromValue, intStatus;
  BYTE   value, index;

  DBGPRINT_FUNCTION (("GetAdapterProperties: IN\n"));

  /* Check the ASIC type. Udp checksum should not be used in
   * pre-Tornado cards-  JRR
   */
  if (adapter->Hardware.DeviceId == NIC_PCI_DEVICE_ID_9055 ||
      adapter->Hardware.DeviceId == NIC_PCI_DEVICE_ID_9004 ||
      adapter->Hardware.DeviceId == NIC_PCI_DEVICE_ID_9005 ||
      adapter->Hardware.DeviceId == NIC_PCI_DEVICE_ID_9006 ||
      adapter->Hardware.DeviceId == NIC_PCI_DEVICE_ID_9058 ||
      adapter->Hardware.DeviceId == NIC_PCI_DEVICE_ID_900A ||
      adapter->Hardware.DeviceId == NIC_PCI_DEVICE_ID_905A ||
      adapter->Hardware.DeviceId == NIC_PCI_DEVICE_ID_4500 ||
      adapter->Hardware.DeviceId == NIC_PCI_DEVICE_ID_7646 ||
      adapter->Hardware.DeviceId == NIC_PCI_DEVICE_ID_9800)
  {
    adapter->Hardware.BitsInHashFilter = 0x40;
  }
  else
  {
    adapter->Hardware.BitsInHashFilter = 0x100;
    adapter->Hardware.UDPChecksumErrDone = TRUE;
  }

  adapter->Hardware.Status = HARDWARE_STATUS_WORKING;
  adapter->Hardware.LinkSpeed = LINK_SPEED_100;

  /* Make sure we can see the adapter.  Set up for window 7, and make
   * sure the window number gets reflected in status.
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_7);

  intStatus = NIC_READ_PORT_WORD (adapter, INTSTATUS_COMMAND_REGISTER);

  DBGPRINT_INITIALIZE (("intStatus = %04X\n", intStatus));
  DBGPRINT_INITIALIZE (("ioBase = %X\n", adapter->IoBaseAddress));

  if ((intStatus & REGISTER_WINDOW_MASK) != (REGISTER_WINDOW_7 << 13))
  {
    DBGPRINT_ERROR (("Setuadapter: Window selection failure\n"));
    DBGPRINT_INITIALIZE (("Setuadapter: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

  /* Read the compatability level
   */
  nicStatus = ReadEEPROM (adapter, EEPROM_COMPATABILITY_WORD,
                          (WORD*)&compatability);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_INITIALIZE (("GetAdapterProperties: compatability read failed\n"));
    return (NIC_STATUS_FAILURE);
  }

  /* Check the Failure level.
   */
  if (compatability.FailureLevel > EEPROM_COMPATABILITY_LEVEL)
  {
    DBGPRINT_ERROR (("GetAdapterProperties: Incompatible level\n"));
    DBGPRINT_INITIALIZE (("GetAdapterProperties: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

  /* Check the warning level.
   */
  if (compatability.WarningLevel > EEPROM_COMPATABILITY_LEVEL)
     DBGPRINT_ERROR (("GetAdapterProperties: Wrong down compatability level\n"));

  /* Read the software information 1
   */
  nicStatus = ReadEEPROM (adapter, EEPROM_SOFTWARE_INFORMATION_1,
                          (WORD*)&information1);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_INITIALIZE (("GetAdapterProperties: EEPROM s/w info1 read failed\n"));
    return (NIC_STATUS_FAILURE);
  }

  if (information1.LinkBeatDisable)
  {
    DBGPRINT_INITIALIZE (("s/w information1 - Link beat disable\n"));
    adapter->Hardware.LinkBeatDisable = TRUE;
  }
  if (adapter->Hardware.DuplexCommandOverride == FALSE)
  {
    if (information1.FullDuplexMode)
    {
      DBGPRINT_INITIALIZE (("s/w information1 - Full duplex enable\n"));
      adapter->Hardware.FullDuplexEnable = TRUE;
    }
    else
    {
      DBGPRINT_INITIALIZE (("s/w information 1 - Full duplex disabled\n"));
      adapter->Hardware.FullDuplexEnable = FALSE;
    }
  }

  switch (information1.OptimizeFor)
  {
    case EEPROM_OPTIMIZE_FOR_THROUGHPUT:
         DBGPRINT_INITIALIZE (("sw info1 - optimize throughput\n"));
         adapter->Hardware.OptimizeForThroughput = TRUE;
         break;

    case EEPROM_OPTIMIZE_FOR_CPU:
         DBGPRINT_INITIALIZE (("s/w info1 - optimize CPU\n"));
         adapter->Hardware.OptimizeForCPU = TRUE;
         break;

    case EEPROM_OPTIMIZE_NORMAL:
         DBGPRINT_INITIALIZE (("s/w info1 - optimize Normal\n"));
         adapter->Hardware.OptimizeNormal = TRUE;
         break;

    default:
         DBGPRINT_ERROR (("GetAdapterProperties: Wrong optimization level\n"));
         return (NIC_STATUS_FAILURE);
         break;
  }

  /* Read the capabilities information
   */
  nicStatus = ReadEEPROM (adapter, EEPROM_CAPABILITIES_WORD,
                          (WORD*)&capabilities);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_INITIALIZE (("GetAdapterProprties: EEPROM s/w capabilities read failed\n"));
    return (NIC_STATUS_FAILURE);
  }

  if (capabilities.SupportsPowerManagement)
  {
    DBGPRINT_INITIALIZE (("Adapter supports power management\n"));
    adapter->Hardware.SupportsPowerManagement = TRUE;
  }

  /* Read the software information 2
   */
  nicStatus = ReadEEPROM (adapter, EEPROM_SOFTWARE_INFORMATION_2,
                          (WORD*)&information2);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("GetAdapterProperties: ReadEEPROM , SWINFO2 failed\n"));
    DBGPRINT_INITIALIZE (("GetAdapterProperties: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

  if (information2.BroadcastRxErrDone)
  {
    DBGPRINT_INITIALIZE (("Adapter has BroadcastRxErrDone\n"));
    adapter->Hardware.BroadcastErrDone = TRUE;
  }

  if (information2.MWIErrDone)
  {
    DBGPRINT_INITIALIZE (("Adapter has MWIErrDone\n"));
    adapter->Hardware.MWIErrDone = TRUE;
  }

  if (information2.WOLConnectorPresent)
  {
    DBGPRINT_INITIALIZE (("WOL is connected\n"));
    adapter->Hardware.WOLConnectorPresent = TRUE;
  }

  if (information2.AutoResetToD0)
  {
    DBGPRINT_INITIALIZE (("Auto reset to D0 bit on\n"));
    adapter->Hardware.AutoResetToD0 = TRUE;
  }

  /* Read the OEM station address
   */
  nicStatus = ReadEEPROM (adapter, EEPROM_OEM_NODE_ADDRESS_WORD_0, &eepromValue);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("GetAdapterProperties: EEPROM read word 0 failed\n"));
    DBGPRINT_INITIALIZE (("GetAdapterProperties: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

  adapter->PermanentAddress[0] = hiBYTE (eepromValue);
  adapter->PermanentAddress[1] = loBYTE (eepromValue);

  nicStatus = ReadEEPROM (adapter, EEPROM_OEM_NODE_ADDRESS_WORD_1, &eepromValue);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("GetAdapterProperties: EEPROM read word 1 failed\n"));
    DBGPRINT_INITIALIZE (("GetAdapterProperties: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

  adapter->PermanentAddress[2] = hiBYTE (eepromValue);
  adapter->PermanentAddress[3] = loBYTE (eepromValue);

  nicStatus = ReadEEPROM (adapter, EEPROM_OEM_NODE_ADDRESS_WORD_2, &eepromValue);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("GetAdapterProperties: EEPROM read word 2 failed\n"));
    DBGPRINT_INITIALIZE (("GetAdapterProperties: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

  adapter->PermanentAddress[4] = hiBYTE (eepromValue);
  adapter->PermanentAddress[5] = loBYTE (eepromValue);

  /* If the station address has not been overriden, fill the permanent
   * address into it.
   */
  value = adapter->StationAddress[0] |
          adapter->StationAddress[1] |
          adapter->StationAddress[2] |
          adapter->StationAddress[3] |
          adapter->StationAddress[4] |
          adapter->StationAddress[5];

  /* If the station address has not been overriden, set this value
   * in the station address.
   */
  if (!value)
     for (index = 0; index < 6; index++)
         adapter->StationAddress[index] = adapter->PermanentAddress[index];

  for (index = 0; index < 6; index++)
    device->dev_addr[index] = adapter->StationAddress[index];

  DBGPRINT_FUNCTION (("GetAdapterProperties: OUT\n"));
  return (NIC_STATUS_SUCCESS);
}


/*
 * This routine does the basic initialize of the adapter. It does
 * not set the media specific stuff.
 */
STATIC DWORD BasicInitializeAdapter (struct NIC_INFORMATION *adapter)
{
  WORD  stationTemp, macControl;
  DWORD nicStatus;

  DBGPRINT_FUNCTION (("BasicInitializeAdapter: In\n"));

  /* Tx Engine handling
   */
  nicStatus = NIC_COMMAND_WAIT (adapter, COMMAND_TX_DISABLE);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("BasicInitializeAdapter: COMMAND_TX_DISABLE failed\n"));
    DBGPRINT_FUNCTION (("BasicInitializeAdapter: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

#ifdef SERR_NEEDED
  /* Down stall the adapter and wait for 100 milliseconds for
   * any pending PCI retry to be over.
   */
  NIC_COMMAND_WAIT (adapter, COMMAND_DOWN_STALL);
#endif

  nicStatus = NIC_COMMAND_WAIT (adapter, COMMAND_TX_RESET | TX_RESET_MASK_NETWORK_RESET);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("BasicInitializeAdapter: COMMAND_TX_RESET failed\n"));
    DBGPRINT_FUNCTION (("GetAdapterProperties: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

  /* Rx engine handling
   */
  nicStatus = NIC_COMMAND_WAIT (adapter, COMMAND_RX_DISABLE);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("BasicInitializeAdapter: Rx disable failed\n"));
    DBGPRINT_FUNCTION (("BasicInitializeAdapter: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

#ifdef SERR_NEEDED
  /* Up stall the adapter and wait for 100 milliseconds for
   * any pending PCI retry to be over.
   */
  NIC_COMMAND_WAIT (adapter, COMMAND_UP_STALL);
#endif

  nicStatus = NIC_COMMAND_WAIT (adapter, COMMAND_RX_RESET | RX_RESET_MASK_NETWORK_RESET);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("BasicInitializeAdapter: Rx reset failed\n"));
    DBGPRINT_FUNCTION (("BasicInitializeAdapter: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

  /* Take care of the interrupts.
   */
  NIC_ACKNOWLEDGE_ALL_INTERRUPT (adapter);
  NIC_COMMAND (adapter, COMMAND_STATISTICS_DISABLE);

  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_6);

  /* Clear the statistics from the hardware.
   */
  NIC_READ_PORT_BYTE (adapter, CARRIER_LOST_REGISTER);
  NIC_READ_PORT_BYTE (adapter, SQE_ERRORS_REGISTER);
  NIC_READ_PORT_BYTE (adapter, MULTIPLE_COLLISIONS_REGISTER);
  NIC_READ_PORT_BYTE (adapter, SINGLE_COLLISIONS_REGISTER);
  NIC_READ_PORT_BYTE (adapter, LATE_COLLISIONS_REGISTER);
  NIC_READ_PORT_BYTE (adapter, RX_OVERRUNS_REGISTER);
  NIC_READ_PORT_BYTE (adapter, FRAMES_TRANSMITTED_OK_REGISTER);
  NIC_READ_PORT_BYTE (adapter, FRAMES_RECEIVED_OK_REGISTER);
  NIC_READ_PORT_BYTE (adapter, FRAMES_DEFERRED_REGISTER);
  NIC_READ_PORT_BYTE (adapter, UPPER_FRAMES_OK_REGISTER);
  NIC_READ_PORT_WORD (adapter, BYTES_RECEIVED_OK_REGISTER);
  NIC_READ_PORT_WORD (adapter, BYTES_TRANSMITTED_OK_REGISTER);

  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);

  NIC_READ_PORT_BYTE (adapter, BAD_SSD_REGISTER);
  NIC_READ_PORT_BYTE (adapter, UPPER_BYTES_OK_REGISTER);

  /* Program the station address.
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_2);

  stationTemp = adapter->StationAddress[1] << 8;
  stationTemp |= adapter->StationAddress[0];

  NIC_WRITE_PORT_WORD (adapter, STATION_ADDRESS_LOW_REGISTER, stationTemp);

  stationTemp = adapter->StationAddress[3] << 8;
  stationTemp |= adapter->StationAddress[2];

  NIC_WRITE_PORT_WORD (adapter, STATION_ADDRESS_MID_REGISTER, stationTemp);

  stationTemp = adapter->StationAddress[5] << 8;
  stationTemp |= adapter->StationAddress[4];

  NIC_WRITE_PORT_WORD (adapter, STATION_ADDRESS_HIGH_REGISTER, stationTemp);

  NIC_WRITE_PORT_WORD (adapter, 0x6, 0);
  NIC_WRITE_PORT_WORD (adapter, 0x8, 0);
  NIC_WRITE_PORT_WORD (adapter, 0xA, 0);
  NIC_COMMAND (adapter, COMMAND_STATISTICS_ENABLE);

  /* Clear the MAC control register.
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_3);

  macControl = NIC_READ_PORT_WORD (adapter, MAC_CONTROL_REGISTER);
  macControl &= 0x1;
  NIC_WRITE_PORT_WORD (adapter, MAC_CONTROL_REGISTER, macControl);

  DBGPRINT_FUNCTION (("BasicInitializeAdapter: Out with success\n"));

  return (NIC_STATUS_SUCCESS);
}

/*
 * This routine allocates the shared memory
 */
STATIC DWORD AllocateSharedMemory (struct NIC_INFORMATION *adapter)
{
  struct UPD_LIST_ENTRY *currentUPDVirtual  = NULL;
  struct UPD_LIST_ENTRY *firstUPDVirtual    = NULL;
  struct UPD_LIST_ENTRY *previousUPDVirtual = NULL;
  struct DPD_LIST_ENTRY *currentDPDVirtual  = NULL;
  struct DPD_LIST_ENTRY *headDPDVirtual     = NULL;
  struct DPD_LIST_ENTRY *previousDPDVirtual = NULL;

  DWORD  updMemoryForOne, totalUPDMemory;
  DWORD  dpdMemoryForOne, totalDPDMemory, rxMemoryForOne;
  DWORD  cacheLineSize, count, alignment;
  DWORD  memoryBaseVirtual, memoryBasePhysical;
  DWORD  updMemoryVirtualStart, updMemoryPhysicalStart;
  DWORD  dpdMemoryVirtualStart, dpdMemoryPhysicalStart;
  DWORD  currentUPDPhysical, previousUPDPhysical;
  DWORD  firstUPDPhysical = 0;
  DWORD  currentDPDPhysical = 0;
  DWORD  testMemoryVirtualStart, testMemoryPhysicalStart;
  DWORD  totalTestMemory, total;

  DBGPRINT_FUNCTION (("AllocateSharedMemory: In\n"));

  cacheLineSize = adapter->Hardware.CacheLineSize;

  /* UPD structure memory requirement.
   */
  updMemoryForOne = sizeof (struct UPD_LIST_ENTRY) + cacheLineSize;
  totalUPDMemory = adapter->Resources.ReceiveCount * updMemoryForOne;

  /* Receive buffer requirement.
   */
  rxMemoryForOne = ETHERNET_MAXIMUM_FRAME_SIZE + cacheLineSize;

  /* DPD structure memory requirement.
   */
  dpdMemoryForOne = sizeof (struct DPD_LIST_ENTRY) + cacheLineSize;
  totalDPDMemory = adapter->Resources.SendCount * dpdMemoryForOne;

  /* Calculate the test memory required.
   */
  totalTestMemory = MAXIMUM_TEST_BUFFERS * (dpdMemoryForOne + rxMemoryForOne);
  total = adapter->Resources.SharedMemorySize = totalUPDMemory +
                                                totalDPDMemory +
                                                totalTestMemory;
  /* Allocate the memory
   */
  adapter->Resources.SharedMemoryVirtual = k_calloc (total, 1);
  if (!adapter->Resources.SharedMemoryVirtual)
     return (NIC_STATUS_FAILURE);

  adapter->ResourcesReserved |= NIC_SHARED_MEMORY_ALLOCATED;

  /* Carve out the regions
   */
  memoryBaseVirtual  = (DWORD) adapter->Resources.SharedMemoryVirtual;
  memoryBasePhysical = VIRT_TO_PHYS (adapter->Resources.SharedMemoryVirtual);

  DBGPRINT_INITIALIZE (("memoryBaseVirtual = %08x, memoryBasePhysical = %08x\n",
                        memoryBaseVirtual, memoryBasePhysical));
  /* Virtual addresses of the regions.
   */
  updMemoryVirtualStart  = memoryBaseVirtual;
  dpdMemoryVirtualStart  = updMemoryVirtualStart + totalUPDMemory;
  testMemoryVirtualStart = dpdMemoryVirtualStart + totalDPDMemory;

  /* Physical addresses of the regions.
   */
  updMemoryPhysicalStart  = memoryBasePhysical;
  dpdMemoryPhysicalStart  = updMemoryPhysicalStart + totalUPDMemory;
  testMemoryPhysicalStart = dpdMemoryPhysicalStart + totalDPDMemory;

  /* Make the receive structures
   */
  for (count = 0; count < adapter->Resources.ReceiveCount; count++)
  {
    currentUPDPhysical = updMemoryPhysicalStart + count * updMemoryForOne;

    alignment = cacheLineSize - (currentUPDPhysical % cacheLineSize);
    currentUPDPhysical += alignment;

    currentUPDVirtual = (struct UPD_LIST_ENTRY*) (updMemoryVirtualStart +
                          alignment + count * updMemoryForOne);

    /* Store the physical address of this UPD in the UPD itself.
     */
    currentUPDVirtual->UPDPhysicalAddress = currentUPDPhysical;

    if (0 == count)
    {
      /* Store the virtual and physical address of the first UPD.
       */
      firstUPDVirtual  = currentUPDVirtual;
      firstUPDPhysical = currentUPDPhysical;
    }
    else
    {
      /* Put the links in the UPDs.
       */
      previousUPDVirtual->Next          = currentUPDVirtual;
      previousUPDVirtual->UpNextPointer = currentUPDPhysical;
      currentUPDVirtual->Previous       = previousUPDVirtual;
    }

    /* Allocate a receive buffer per UPD
     */
    currentUPDVirtual->SocketBuffer = k_calloc (ETHERNET_MAXIMUM_FRAME_SIZE + 2, 1);
    if (currentUPDVirtual->SocketBuffer == NULL)
    {
      DBGPRINT_ERROR (("Socket buffer allocation failed\n"));
      return (NIC_STATUS_FAILURE);
    }

    currentUPDVirtual->RxBufferVirtual   = currentUPDVirtual->SocketBuffer;
    currentUPDVirtual->SGList[0].Address = VIRT_TO_PHYS (currentUPDVirtual->SocketBuffer);
    currentUPDVirtual->SGList[0].Count   = ETHERNET_MAXIMUM_FRAME_SIZE | 0x80000000;

    previousUPDVirtual  = currentUPDVirtual;
    previousUPDPhysical = currentUPDPhysical;
  }

  /* Link the first and last UPDs.
   */
  currentUPDVirtual->Next          = firstUPDVirtual;
  currentUPDVirtual->UpNextPointer = firstUPDPhysical;
  firstUPDVirtual->Previous        = currentUPDVirtual;

  /* Save the address of the first UPD in the adapter structure.
   */
  adapter->HeadUPDVirtual = firstUPDVirtual;

  /* Carve out DPD structures
   */
  for (count = 0; count < adapter->Resources.SendCount; count++)
  { 
    currentDPDPhysical = dpdMemoryPhysicalStart + count * dpdMemoryForOne;

    alignment = cacheLineSize - (currentDPDPhysical % cacheLineSize);
    currentDPDPhysical += alignment;

    currentDPDVirtual = (struct DPD_LIST_ENTRY*) (dpdMemoryVirtualStart +
                        count * dpdMemoryForOne + alignment);

    /* Save the physical address of this DPD in the DPD itself.
     */
    currentDPDVirtual->DPDPhysicalAddress = currentDPDPhysical;

    if (0 == count)
       headDPDVirtual = currentDPDVirtual;
    else
    {
      previousDPDVirtual->Next = currentDPDVirtual;
      currentDPDVirtual->Previous = previousDPDVirtual;
    }
    previousDPDVirtual = currentDPDVirtual;
  }

  /* Link head and tail.
   */
  headDPDVirtual->Previous = currentDPDVirtual;
  currentDPDVirtual->Next  = headDPDVirtual;

  /* Point head and tail to this DPD.
   */
  adapter->HeadDPDVirtual = headDPDVirtual;
  adapter->TailDPDVirtual = headDPDVirtual;

  /* Test DPD and test buffer
   */
  for (count = 0; count < MAXIMUM_TEST_BUFFERS; count++)
  {  
    struct DPD_LIST_ENTRY *dpd;

    alignment = cacheLineSize - (testMemoryPhysicalStart % cacheLineSize);
    adapter->TestDPDVirtual[count]  = testMemoryVirtualStart + alignment;
    adapter->TestDPDPhysical[count] = testMemoryPhysicalStart + alignment;

    testMemoryVirtualStart  += dpdMemoryForOne;
    testMemoryPhysicalStart += dpdMemoryForOne;

    alignment = cacheLineSize - (testMemoryPhysicalStart % cacheLineSize);

    adapter->TestBufferVirtual[count]  = testMemoryVirtualStart + alignment;
    adapter->TestBufferPhysical[count] = testMemoryPhysicalStart + alignment;

    /* Save the physical address of the DPD, in the DPD.
     */
    dpd = (struct DPD_LIST_ENTRY*)adapter->TestDPDVirtual[count];
    dpd->DPDPhysicalAddress = adapter->TestDPDPhysical[count];

    testMemoryVirtualStart  += rxMemoryForOne;
    testMemoryPhysicalStart += rxMemoryForOne;
  }
  DBGPRINT_FUNCTION (("AllocateSharedMemory: Out\n"));
  return (NIC_STATUS_SUCCESS);
}

/*
 * This routine tests the functionality of the adapter.
 */
STATIC DWORD TestAdapter (struct NIC_INFORMATION *adapter)
{
  struct DPD_LIST_ENTRY *dpdVirtual;
  struct UPD_LIST_ENTRY *updVirtual;
  BYTE  *sourceBufferVirtual, *destinationBufferVirtual;
  DWORD  sourceBufferPhysical, count, nicStatus;
  WORD   networkDiagnosticsValue;

  DBGPRINT_FUNCTION (("TestAdapter: IN\n"));

  /* Select the network diagnostics window.
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);

  /* Read the network diagnostics register.
   */
  networkDiagnosticsValue = NIC_READ_PORT_WORD (adapter, NETWORK_DIAGNOSTICS_REGISTER);

  /* Enable loop back on the adapter.
   */
  networkDiagnosticsValue |= BIT_12;
  NIC_WRITE_PORT_WORD (adapter, NETWORK_DIAGNOSTICS_REGISTER,
                       networkDiagnosticsValue);

  NIC_COMMAND (adapter, COMMAND_TX_ENABLE);
  NIC_COMMAND (adapter, COMMAND_RX_ENABLE);

  /* Write the address to the UpListPointer register.
   */
  NIC_WRITE_PORT_DWORD (adapter, UP_LIST_POINTER_REGISTER,
                        adapter->HeadUPDVirtual->UPDPhysicalAddress);

  NIC_COMMAND (adapter, COMMAND_UP_UNSTALL);

  /* Use the buffer in the second UPD to make the packet.
   */
  sourceBufferVirtual  = (BYTE*) adapter->TestBufferVirtual[0];
  sourceBufferPhysical = adapter->TestBufferPhysical[0];

  for (count = 0; count < ETHERNET_MAXIMUM_FRAME_SIZE; count++)
      *sourceBufferVirtual++ = (BYTE) count;
  
  dpdVirtual = (struct DPD_LIST_ENTRY*) adapter->TestDPDVirtual[0];
  dpdVirtual->FrameStartHeader  = ETHERNET_MAXIMUM_FRAME_SIZE;
  dpdVirtual->DownNextPointer   = 0;
  dpdVirtual->SGList[0].Address = sourceBufferPhysical;
  dpdVirtual->SGList[0].Count   = ETHERNET_MAXIMUM_FRAME_SIZE | 0x80000000;

  NIC_COMMAND_WAIT (adapter, COMMAND_DOWN_STALL);

  /* Write the down list pointer register.
   */
  NIC_WRITE_PORT_DWORD (adapter, DOWN_LIST_POINTER_REGISTER,
                        dpdVirtual->DPDPhysicalAddress);

  NIC_COMMAND (adapter, COMMAND_DOWN_UNSTALL);

  /* Check that packet has been picked up by the hardware.
   */
  portValue_g = NIC_READ_PORT_DWORD (adapter, DOWN_LIST_POINTER_REGISTER);
  udelay (10);

  if (portValue_g == dpdVirtual->DPDPhysicalAddress)
  {
    adapter->WaitCases = CHECK_DOWNLOAD_STATUS;
    TimeOutCount = jiffies + HZ;
    if (!InWaitTimer)
    {
      WaitTimer.expires = RUN_AT (HZ / 100); /* max=1s */
      add_timer (&WaitTimer);
      InWaitTimer = TRUE;
    }
    while (TimeOutCount > jiffies)
           ;
    if (portValue_g == dpdVirtual->DPDPhysicalAddress)
    {
      DBGPRINT_ERROR (("Packet not picked up by the hardware\n"));
      DBGPRINT_INITIALIZE (("TestAdapter: Out with error\n"));
      return (NIC_STATUS_FAILURE);
    }
  }

  /* Check the upload information.
   */
  portValue_g = NIC_READ_PORT_DWORD (adapter, UP_LIST_POINTER_REGISTER);

  udelay (10);

  if (portValue_g == adapter->HeadUPDVirtual->UPDPhysicalAddress)
  {
    adapter->WaitCases = CHECK_UPLOAD_STATUS;
    TimeOutCount = jiffies + HZ;
    if (!InWaitTimer)
    {
      WaitTimer.expires = RUN_AT (HZ / 100); /* max=1s */
      add_timer (&WaitTimer);
      InWaitTimer = TRUE;
    }
    while (TimeOutCount > jiffies) ;
    if (portValue_g == adapter->HeadUPDVirtual->UPDPhysicalAddress)
    {
      DBGPRINT_ERROR (("Packet not uploaded by adapter\n"));
      DBGPRINT_INITIALIZE (("TestAdapter: Out with error\n"));
      return (NIC_STATUS_FAILURE);
    }
  }

  /* Check the contents of the packet.
   */
  updVirtual = adapter->HeadUPDVirtual;
  destinationBufferVirtual = (BYTE*) updVirtual->RxBufferVirtual;

  for (count = 0; count < ETHERNET_MAXIMUM_FRAME_SIZE; count++)
      if (destinationBufferVirtual[count] != (BYTE)count)
         break;

  if (ETHERNET_MAXIMUM_FRAME_SIZE != count)
  {
    DBGPRINT_ERROR (("TestAdapter: Receive buffer contents not ok\n"));
    DBGPRINT_INITIALIZE (("TestAdapter: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

  adapter->HeadUPDVirtual->UpPacketStatus = 0;

  NIC_WRITE_PORT_DWORD (adapter, UP_LIST_POINTER_REGISTER, 0);
  NIC_ACKNOWLEDGE_ALL_INTERRUPT (adapter);

  /* Issue transmit and receive reset here.
   */
  nicStatus = NIC_COMMAND_WAIT (adapter, COMMAND_TX_RESET |
                                TX_RESET_MASK_NETWORK_RESET);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("TestAdapter: Transmit reset failed\n"));
    DBGPRINT_INITIALIZE (("TestAdapter: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

  nicStatus = NIC_COMMAND_WAIT (adapter, COMMAND_RX_RESET |
                                RX_RESET_MASK_NETWORK_RESET);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("TestAdapter: Receiver reset failed\n"));
    DBGPRINT_INITIALIZE (("TestAdapter: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

  /* Clear the loop back bit in network diagnostics.
   */
  networkDiagnosticsValue &= ~BIT_12;

  NIC_WRITE_PORT_WORD (adapter, NETWORK_DIAGNOSTICS_REGISTER,
                       networkDiagnosticsValue);

  DBGPRINT_FUNCTION (("TestAdapter: OUT\n"));
  return (NIC_STATUS_SUCCESS);
}


/*
 * This routine starts the adapter.
 */
STATIC DWORD StartAdapter (struct NIC_INFORMATION *adapter)
{
  struct DPD_LIST_ENTRY *headDPDVirtual;
  DWORD  nicStatus, dmaControl;
  WORD   diagnostics;

  DBGPRINT_FUNCTION (("StartAdapter: In\n"));

  /* Enable upper bytes counting in diagnostics register.
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);

  diagnostics = NIC_READ_PORT_WORD (adapter, NETWORK_DIAGNOSTICS_REGISTER);

  diagnostics |= NETWORK_DIAGNOSTICS_UPPER_BYTES_ENABLE;

  NIC_WRITE_PORT_WORD (adapter, NETWORK_DIAGNOSTICS_REGISTER, diagnostics);

  /* Enable counter speed in DMA control.
   */
  dmaControl = NIC_READ_PORT_DWORD (adapter, DMA_CONTROL_REGISTER);

  if (adapter->Hardware.LinkSpeed == 100000000)
     dmaControl |= DMA_CONTROL_COUNTER_SPEED;

  NIC_WRITE_PORT_DWORD (adapter, DMA_CONTROL_REGISTER, dmaControl);

  /* Give download structures to the adapter:
   *
   * Stall the download engine.
   */
  nicStatus = NIC_COMMAND_WAIT (adapter, COMMAND_DOWN_STALL);

  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("StartAdapter: down stall failed\n"));
    DBGPRINT_INITIALIZE (("StartAdapter: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

  /* Use the head DPD to mark it as dummy.
   */
  headDPDVirtual = adapter->HeadDPDVirtual;
  headDPDVirtual->FrameStartHeader = FSH_DPD_EMPTY;

  /* Now move head and tail to next one.
   */
  adapter->HeadDPDVirtual = headDPDVirtual->Next;
  adapter->TailDPDVirtual = headDPDVirtual->Next;

  /* Write the first DPD address to the hardware.
   */
  NIC_WRITE_PORT_DWORD (adapter, DOWN_LIST_POINTER_REGISTER,
                        headDPDVirtual->DPDPhysicalAddress);

  /* Enable down polling on the hardware.
   */
  NIC_WRITE_PORT_BYTE (adapter, DOWN_POLL_REGISTER,
                       (BYTE)adapter->Resources.DownPollRate);

  /* Unstall the download engine.
   */
  NIC_COMMAND (adapter, COMMAND_DOWN_UNSTALL);

  /* Give upload structures to the adapter:
   *
   * Stall the upload engine.
   */
  nicStatus = NIC_COMMAND_WAIT (adapter, COMMAND_UP_STALL);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("StartAdapter: up stall failed\n"));
    DBGPRINT_INITIALIZE (("StartAdapter: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

  /* Give the address of the first UPD to the adapter.
   */
  NIC_WRITE_PORT_DWORD (adapter, UP_LIST_POINTER_REGISTER,
                        adapter->HeadUPDVirtual->UPDPhysicalAddress);

  /* Write the up poll register.
   */
  NIC_WRITE_PORT_BYTE (adapter, UP_POLL_REGISTER, 8);

  /* Unstall the download engine.
   */
  NIC_COMMAND (adapter, COMMAND_UP_UNSTALL);

  /* Enable the statistics back.
   */
  NIC_COMMAND (adapter, COMMAND_STATISTICS_ENABLE);

  /* Acknowledge any pending interrupts.
   */
  NIC_ACKNOWLEDGE_ALL_INTERRUPT (adapter);

  /* Enable indication for all interrupts.
   */
  NIC_ENABLE_ALL_INTERRUPT_INDICATION (adapter);

  /* Enable all interrupts to the host.
   */
  NIC_UNMASK_ALL_INTERRUPT (adapter);

  /* Enable the transmit and receive engines.
   */
  NIC_COMMAND (adapter, COMMAND_RX_ENABLE);
  NIC_COMMAND (adapter, COMMAND_TX_ENABLE);

  /* Delay three seconds, only some switches need this,
   * default is no delay, user can enable this delay in command line
   */
  if (adapter->DelayStart == TRUE)
  {
    TimeOutCount = jiffies + 3 * HZ;
    while (TimeOutCount > jiffies) ;
  }
  DBGPRINT_FUNCTION (("StartAdapter: Out\n"));
  return (NIC_STATUS_SUCCESS);
}

/*
 * This routine restarts the adapter.
 */
STATIC void ReStartAdapter (struct NIC_INFORMATION *adapter)
{
  DWORD internalConfig = adapter->keepForGlobalReset;

  DBGPRINT_INTERRUPT (("ReStartAdapter after global reset: IN\n"));

  /* Delay for 1 second, might not be that long since some has
   * been comsumed with task queue
   */
  TimeOutCount = jiffies + HZ;
  while (TimeOutCount > jiffies)
         ;

  /* Mask all the interrupts.
   */
  NIC_MASK_ALL_INTERRUPT (adapter);

  /* Enable indication for all interrupts.
   */
  NIC_ENABLE_ALL_INTERRUPT_INDICATION (adapter);

  /* Write the internal config back.
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_3);
  NIC_WRITE_PORT_DWORD (adapter, INTERNAL_CONFIG_REGISTER, internalConfig);

  /* Set the adapter for operation
   */
  if (GetAdapterProperties (adapter) != NIC_STATUS_SUCCESS)
     DBGPRINT_INTERRUPT (("GetAdapterProperties failed\n"));

  if (BasicInitializeAdapter (adapter) != NIC_STATUS_SUCCESS)
     DBGPRINT_INTERRUPT (("BasicInitialize failed\n"));

  if (SetupMedia (adapter->Device) != NIC_STATUS_SUCCESS)
     DBGPRINT_INTERRUPT (("SetupMedia failed\n"));

  if (SoftwareWork (adapter) != NIC_STATUS_SUCCESS)
     DBGPRINT_INTERRUPT (("SoftwareWork failed\n"));

  /* Countdown timer is cleared by reset. So write it back.
   */
  SetCountDownTimer (adapter);

  adapter->DelayStart = FALSE; /* no need to delay for switch here */

  if (StartAdapter (adapter) != NIC_STATUS_SUCCESS)
     DBGPRINT_INTERRUPT (("ReStartAdapter: StartAdapter failed\n"));

  DBGPRINT_INTERRUPT (("ReStartAdapter after global reset: OUT\n"));
}


/*
 * This routine sends the packet 
 */
STATIC int NICSendPacket (struct device *Device, const void *buf, int len)
{
  struct NIC_INFORMATION *adapter = (struct NIC_INFORMATION*) Device->priv;
  struct DPD_LIST_ENTRY  *dpdVirtual;

  if (Device->tx_busy)
  {
    DBGPRINT_SEND (("SendPacket: %s Device busy\n", Device->name));
    return (0);
  }
  Device->tx_busy = 1;

  if (adapter->TailDPDVirtual->Next == adapter->HeadDPDVirtual)
  {
    DBGPRINT_SEND (("SendPacket: %s DPDring full\n", Device->name));
    adapter->DPDRingFull = TRUE;
    return (0);
  }

  /* Get the free DPD from the DPD ring
   */
  dpdVirtual = adapter->TailDPDVirtual;
  dpdVirtual->FrameStartHeader  = 0;
  dpdVirtual->SGList[0].Address = VIRT_TO_PHYS (buf);
  dpdVirtual->SGList[0].Count   = len | 0x80000000;
  dpdVirtual->FrameStartHeader |= (DWORD) FSH_ROUND_UP_DEFEAT;
  dpdVirtual->SocketBuffer      = (BYTE*) buf;
  dpdVirtual->PacketLength      = len;
  dpdVirtual->DownNextPointer   = 0;

  DISABLE();
  NIC_COMMAND_WAIT (adapter, COMMAND_DOWN_STALL);
  adapter->BytesInDPDQueue += len;
  adapter->TailDPDVirtual->Previous->DownNextPointer = dpdVirtual->DPDPhysicalAddress;
  adapter->TailDPDVirtual = dpdVirtual->Next;
  NIC_COMMAND (adapter, COMMAND_DOWN_UNSTALL);
  SetCountDownTimer (adapter);
  ENABLE();

  Device->tx_busy  = 0;
  Device->tx_start = jiffies;
  return (1);
}


/*
 * This routine handles the Tx complete event.
 */
STATIC void TxCompleteEvent (struct NIC_INFORMATION *adapter)
{
  BYTE txStatus;

  DBGPRINT_ERROR (("TxCompleteEvent: IN\n"));

  txStatus = NIC_READ_PORT_BYTE (adapter, TX_STATUS_REGISTER);
  NIC_WRITE_PORT_BYTE (adapter, TX_STATUS_REGISTER, txStatus);

  if (txStatus & TX_STATUS_HWERROR)
  {
    /* Transmit HWError recovery.
     */
    DBGPRINT_SEND (("TxCompleteEvent: TxHWError\n"));
    adapter->Statistics.TxHWErrors++;
    if (ResetAndEnableTransmitter (adapter) != NIC_STATUS_SUCCESS)
    {
      DBGPRINT_ERROR (("TxCompleteEvent: TxReset failed\n"));
      adapter->Hardware.Status = HARDWARE_STATUS_HUNG;
      return;
    }
  }
  else if (txStatus & TX_STATUS_JABBER)
  {
    DBGPRINT_ERROR (("TxCompleteEvent: Jabber\n"));
    adapter->Statistics.TxJabberError++;

    if (ResetAndEnableTransmitter (adapter) != NIC_STATUS_SUCCESS)
    {
      DBGPRINT_ERROR (("TxCompleteEvent: TxReset failed\n"));
      adapter->Hardware.Status = HARDWARE_STATUS_HUNG;
      return;
    }
  }
  else if (txStatus & TX_STATUS_MAXIMUM_COLLISION)
  {
    DBGPRINT_ERROR (("TxCompleteEvent: Maximum collision\n"));
    adapter->Statistics.TxMaximumCollisions++;
    NIC_COMMAND (adapter, COMMAND_TX_ENABLE);
  }
  else
  {
    if (txStatus != 0)
    {
      DBGPRINT_ERROR (("TxCompleteEvent: Unknown error\n"));
      adapter->Statistics.TxUnknownError++;
      NIC_COMMAND (adapter, COMMAND_TX_ENABLE);
    }
  }
  DBGPRINT_ERROR (("TxCompleteEvent: OUT\n"));
}


/*
 * This routine resets the transmitter.
 */
STATIC DWORD ResetAndEnableTransmitter (struct NIC_INFORMATION *adapter)
{
  DWORD nicStatus;

  NIC_COMMAND (adapter, COMMAND_TX_DISABLE);

  /* Wait for the transmit to go quiet.
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);

  MediaStatus_g = NIC_READ_PORT_WORD (adapter, MEDIA_STATUS_REGISTER);

  udelay (10);

  if (MediaStatus_g & MEDIA_STATUS_TX_IN_PROGRESS)
  {
    adapter->WaitCases = CHECK_TRANSMIT_IN_PROGRESS;
    TimeOutCount = jiffies + HZ;
    if (!InWaitTimer)
    {
      WaitTimer.expires = RUN_AT (HZ / 100); /* max = 1s */
      add_timer (&WaitTimer);
      InWaitTimer = TRUE;
    }
    while (TimeOutCount > jiffies) ;
    if (!(MediaStatus_g & MEDIA_STATUS_TX_IN_PROGRESS))
    {
      DBGPRINT_ERROR (("ResetAndEnableTransmitter: media status is hung\n"));
      return (NIC_STATUS_FAILURE);
    }
  }

#ifdef SERR_NEEDED
  /* Issue down stall and delay 100 miliseconds for PCI retries to be over
   */
  NIC_COMMAND_WAIT (adapter, COMMAND_DOWN_STALL);
  udelay (100000);
#endif

  /* Wait for download engine to stop
   */
  dmaControl_g = NIC_READ_PORT_DWORD (adapter, DMA_CONTROL_REGISTER);
  udelay (10);

  if (dmaControl_g & DMA_CONTROL_DOWN_IN_PROGRESS)
  {
    adapter->WaitCases = CHECK_DMA_CONTROL;
    TimeOutCount = jiffies + HZ;
    if (!InWaitTimer)
    {
      WaitTimer.expires = RUN_AT (HZ / 100); /* max = 1s */
      add_timer (&WaitTimer);
      InWaitTimer = TRUE;
    }
    while (TimeOutCount > jiffies) ;
    if (!(dmaControl_g & DMA_CONTROL_DOWN_IN_PROGRESS))
    {
      DBGPRINT_ERROR (("ResetAndEnableTransmitter: DMAControl hung\n"));
      return (NIC_STATUS_FAILURE);
    }
  }

  nicStatus = NIC_COMMAND_WAIT (adapter, COMMAND_TX_RESET | TX_RESET_MASK_DOWN_RESET);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("ResetAndEnableTransmitter: Tx reset failed\n"));
    return (NIC_STATUS_FAILURE);
  }

  NIC_COMMAND (adapter, COMMAND_TX_ENABLE);
  return (NIC_STATUS_SUCCESS);
}


/*
 * This routines cleans the send logic.
 */
STATIC void CleanupSendLogic (struct device *Device)
{
  struct NIC_INFORMATION *adapter = (struct NIC_INFORMATION*) Device->priv;
  struct DPD_LIST_ENTRY  *headDPDVirtual;

  DBGPRINT_SEND (("CleanupSendLogic: IN\n"));

  /* Now clean up the DPD ring.
   */
  headDPDVirtual = adapter->HeadDPDVirtual;

  /* This is to take care of hardware raise condition.
   */
  adapter->TailDPDVirtual->FrameStartHeader = 0;

  while (1)
  {
    if (headDPDVirtual == adapter->TailDPDVirtual)
       break;

    /* Complete all the packets.
     */
    adapter->BytesInDPDQueue -= headDPDVirtual->PacketLength;

    k_free (headDPDVirtual->SocketBuffer);

    headDPDVirtual->SocketBuffer = NULL;
    headDPDVirtual->FrameStartHeader = 0;

    headDPDVirtual = headDPDVirtual->Next;
  }

  /* Update the head to point to this DPD now.
   */
  adapter->HeadDPDVirtual = headDPDVirtual;

  /* Initialize all DPDs.
   */
  headDPDVirtual = adapter->HeadDPDVirtual;

  while (1)
  {
    headDPDVirtual->DownNextPointer = 0;
    headDPDVirtual->SocketBuffer = NULL;
    headDPDVirtual->FrameStartHeader = 0;
    headDPDVirtual = headDPDVirtual->Next;
    if (headDPDVirtual == adapter->HeadDPDVirtual)
       break;
  }
  DBGPRINT_SEND (("CleanupSendLogic: OUT\n"));
}


/*
 * This routine handles the receive event.
 */
STATIC void UpCompleteEvent (struct NIC_INFORMATION *adapter)
{
  struct device         *device = adapter->Device;
  struct UPD_LIST_ENTRY *currentUPDVirtual = adapter->HeadUPDVirtual;
  DWORD  upPacketStatus;
  DWORD  frameLength;
  BYTE  *socketBuffer;

  DBGPRINT_RECEIVE (("UpCompleteEvent: IN\n"));

  while (1)
  {
    /* If done with all UPDs break.
     */
    upPacketStatus = currentUPDVirtual->UpPacketStatus;

    if (!(upPacketStatus & UP_PACKET_STATUS_COMPLETE))
       break;

    /* Get the frame length from the UPD.
     */
    frameLength = currentUPDVirtual->UpPacketStatus & 0x1FFF;

    /* Check if there is any error bit set.
     */
    if (upPacketStatus & UP_PACKET_STATUS_ERROR)
    {
      if ((frameLength < ETHERNET_MINIMUM_FRAME_SIZE) ||
          (upPacketStatus & UP_PACKET_STATUS_OVERRUN) ||
          (upPacketStatus & UP_PACKET_STATUS_ALIGNMENT_ERROR) ||
          (upPacketStatus & UP_PACKET_STATUS_CRC_ERROR) ||
          (upPacketStatus & UP_PACKET_STATUS_OVERSIZE_FRAME))
      {
        if (upPacketStatus & UP_PACKET_STATUS_RUNT_FRAME)
        {
          DBGPRINT_ERROR (("UpCompleteEvent: Runt\n"));
        }
        if (upPacketStatus & UP_PACKET_STATUS_ALIGNMENT_ERROR)
        {
          DBGPRINT_ERROR (("UpCompleteEvent: Alignment\n"));
          adapter->Statistics.RxAlignmentError++;
        }
        if (upPacketStatus & UP_PACKET_STATUS_CRC_ERROR)
        {
          DBGPRINT_ERROR (("UpCompleteEvent: Crc\n"));
          adapter->Statistics.RxBadCRCError++;
        }
        if (upPacketStatus & UP_PACKET_STATUS_OVERSIZE_FRAME)
        {
          DBGPRINT_ERROR (("UpCompleteEvent: Oversize\n"));
          adapter->Statistics.RxOversizeError++;
        }
        /* Discard this packet and move on.
         */
        currentUPDVirtual->UpPacketStatus = 0;
        currentUPDVirtual = currentUPDVirtual->Next;
        continue;
      }
      else
      {
        adapter->Statistics.RxFramesOk++;
        adapter->Statistics.RxBytesOk += frameLength;
      }
    }

    /* Try to allocate socket buffer
     */
    if (device->get_rx_buf &&
        (socketBuffer = (*device->get_rx_buf) (frameLength)) != NULL)
    {
      struct ETH_ADDR *EthAddr;

      currentUPDVirtual->SGList[0].Address = VIRT_TO_PHYS (socketBuffer);
      currentUPDVirtual->SGList[0].Count   = ETHERNET_MAXIMUM_FRAME_SIZE | 0x80000000;
      currentUPDVirtual->RxBufferVirtual   = socketBuffer;

      device->last_rx = jiffies;
      currentUPDVirtual->SocketBuffer = socketBuffer;

      /* Check for Multicast
       */
      EthAddr = (struct ETH_ADDR*) currentUPDVirtual->SocketBuffer;
      if ((EthAddr->Addr[0] & ETH_MULTICAST_BIT) &&
          !(COMPARE_MACS (EthAddr, BroadcastAddr)))
         adapter->Statistics.Rx_MulticastPkts++;
    }
    else
    {
      printk ("SKB allocation failed\n");
      DBGPRINT_ERROR (("UpCompleteEvent: SKB allocation failed\n"));
    }
    currentUPDVirtual->UpPacketStatus = 0;
    currentUPDVirtual = currentUPDVirtual->Next;
  }
  adapter->HeadUPDVirtual = currentUPDVirtual;

  DBGPRINT_RECEIVE (("UpCompleteEvent: OUT\n"));
}


/*
 * This routine resets the receiver.
 */
STATIC DWORD ResetAndEnableReceiver (struct NIC_INFORMATION *adapter)
{
  NIC_COMMAND_WAIT (adapter, COMMAND_RX_DISABLE);
  NIC_COMMAND_WAIT (adapter, COMMAND_RX_RESET | RX_RESET_MASK_NETWORK_RESET);
  NIC_COMMAND_WAIT (adapter, COMMAND_RX_ENABLE);
  return (NIC_STATUS_SUCCESS);
}

/*
 * This routine sets receive mode 
 */
STATIC void NICSetReceiveMode (struct device *Device)
{
  struct NIC_INFORMATION *adapter = (struct NIC_INFORMATION*) Device->priv;
  DWORD  count, hardwareReceiveFilter = 0;
  BYTE   flowControlAddress[ETHERNET_ADDRESS_SIZE];
  BYTE   broadcastAddress[ETHERNET_ADDRESS_SIZE];
  DWORD  bitsInHashFilter;

  DBGPRINT_FUNCTION (("NICSetReceiveMode: In\n"));

  bitsInHashFilter = adapter->Hardware.BitsInHashFilter;

  /* Check if Promiscuous mode to be enabled.
   */
  if (Device->flags & IFF_PROMISC)
  {
    DBGPRINT_INITIALIZE (("IFF_PROMISC mode\n"));
    hardwareReceiveFilter |= RX_FILTER_PROMISCUOUS;
  }
  else if (Device->flags & IFF_ALLMULTI)
  {
    /* Check if ALL_MULTI mode to be enabled.
     */
    DBGPRINT_INITIALIZE (("IFF_ALLMULTI\n"));
    hardwareReceiveFilter |= RX_FILTER_INDIVIDUAL;
    hardwareReceiveFilter |= RX_FILTER_BROADCAST;
    hardwareReceiveFilter |= RX_FILTER_ALL_MULTICAST;
  }
  else if (Device->flags & IFF_MULTICAST)
  {
    /* Check if hash multicast to be enabled.
     */
    DBGPRINT_INITIALIZE (("IFF_MULTICAST\n"));
    hardwareReceiveFilter |= RX_FILTER_INDIVIDUAL;
    hardwareReceiveFilter |= RX_FILTER_BROADCAST;
    hardwareReceiveFilter |= RX_FILTER_MULTICAST_HASH;
  }
  else
  {
    /* OS does not want to enable multicast.
     */
    DBGPRINT_INITIALIZE (("Setting filter individual and broadcast\n"));
    hardwareReceiveFilter |= RX_FILTER_INDIVIDUAL;
    hardwareReceiveFilter |= RX_FILTER_BROADCAST;
  }

  /* Write the Rx filter
   */
  NIC_COMMAND (adapter, (WORD) (COMMAND_SET_RX_FILTER | hardwareReceiveFilter));

  /* Clear the hash filter.
   */
  for (count = 0; count < bitsInHashFilter; count++)
      NIC_COMMAND (adapter, (WORD) (COMMAND_SET_HASH_FILTER_BIT | count));

  /* Set the hash filter.
   */
  for (count = 0; count < Device->mc_count; count++)
      NIC_COMMAND (adapter, (WORD) (COMMAND_SET_HASH_FILTER_BIT | 0x400 |
                   HashAddress (&Device->mc_list[count][0])));

  /* If receive filter is not promiscuos or multicast, enable
   * hash multicast for receiving the flow control packets and
   * for the broadcast.
   */
  if (!((hardwareReceiveFilter & RX_FILTER_PROMISCUOUS) ||
        (hardwareReceiveFilter & RX_FILTER_ALL_MULTICAST)))
  {
    hardwareReceiveFilter |= RX_FILTER_MULTICAST_HASH;

    /* Set the flow control enable
     */
    if (adapter->Hardware.FlowControlEnable &&
        adapter->Hardware.FlowControlSupported &&
        adapter->Hardware.FullDuplexEnable)
    {
      DBGPRINT_INITIALIZE (("Setting flow control bit\n"));

      /* Set the flow control address bit
       */
      flowControlAddress[0] = NIC_FLOW_CONTROL_ADDRESS_0;
      flowControlAddress[1] = NIC_FLOW_CONTROL_ADDRESS_1;
      flowControlAddress[2] = NIC_FLOW_CONTROL_ADDRESS_2;
      flowControlAddress[3] = NIC_FLOW_CONTROL_ADDRESS_3;
      flowControlAddress[4] = NIC_FLOW_CONTROL_ADDRESS_4;
      flowControlAddress[5] = NIC_FLOW_CONTROL_ADDRESS_5;

      NIC_COMMAND (adapter, (WORD)(COMMAND_SET_HASH_FILTER_BIT | 0x0400 |
                              HashAddress (flowControlAddress)));
    }

    /* If there is a broadcast error, write value for broadcast.
     */
    if (FALSE == adapter->Hardware.BroadcastErrDone)
    {
      DBGPRINT_INITIALIZE (("Broadcast Err Done\n"));
      for (count = 0; count < ETHERNET_ADDRESS_SIZE; count++)
          broadcastAddress[count] = 0xff;

      NIC_COMMAND (adapter, (WORD) (COMMAND_SET_HASH_FILTER_BIT | 0x400 |
                              HashAddress (broadcastAddress)));
    }
  }

  DBGPRINT_FUNCTION (("NICSetReceiveMode: Out\n"));
}


/*
 * This routine gets the statistics. 
 */
STATIC void *NICGetStatistics (struct device *Device)
{
  struct NIC_INFORMATION  *adapter    = (struct NIC_INFORMATION*) Device->priv;
  struct NIC_STATISTICS   *statistics = &adapter->Statistics;
  struct net_device_stats *stats      = &adapter->EnetStatistics;

  if (Device->start)
  {
    DISABLE();

    stats->rx_packets = statistics->RxFramesOk;
    stats->tx_packets = statistics->TxFramesOk;

    stats->rx_bytes = statistics->RxBytesOk;
    stats->tx_bytes = statistics->TxBytesOk;

    stats->rx_errors = statistics->RxBadCRCError +
                       statistics->RxOverruns +
                       statistics->RxAlignmentError +
                       statistics->RxOversizeError;

    stats->tx_errors = statistics->TxHWErrors +
                       statistics->TxMaximumCollisions +
                       statistics->TxJabberError +
                       statistics->TxUnknownError;

    stats->rx_dropped          = statistics->RxOverruns;
    stats->multicast           = statistics->Rx_MulticastPkts;
    stats->tx_collisions       = statistics->TxMaximumCollisions;
    stats->rx_over_errors      = statistics->RxOversizeError;
    stats->rx_crc_errors       = statistics->RxBadCRCError;
    stats->rx_frame_errors     = statistics->RxAlignmentError;
    stats->rx_fifo_errors      = statistics->RxOverruns;
    stats->tx_aborted_errors   = statistics->TxUnknownError;
    stats->tx_carrier_errors   = statistics->TxCarrierLost;
    stats->tx_heartbeat_errors = statistics->TxSQEErrors;

    stats->rx_missed_errors = 0;
    stats->tx_dropped       = 0;
    stats->rx_length_errors = 0;
    stats->tx_fifo_errors   = 0;
    stats->tx_window_errors = 0;

    ENABLE();
  }
  return (void*)stats;
}


/*
 * This routine returns a bit corresponding to the hash address.
 */
STATIC WORD HashAddress (BYTE *Address)
{
  DWORD crc, carry, count, bit;
  BYTE  thisByte;

  crc = 0xffffffff;

  for (count = 0; count < 6; count++)
  {
    thisByte = Address[count];
    for (bit = 0; bit < 8; bit++)
    {
      carry = ((crc & 0x80000000) ? 1 : 0) ^ (thisByte & 0x01);
      crc <<= 1;
      thisByte >>= 1;
      if (carry)
        crc = (crc ^ 0x04c11db6) | carry;
    }
  }
  return (WORD) (crc & 0x000003FF);
}


/*
 * This routine sets up the multicast list on the adapter.
 */
STATIC DWORD SetMulticastAddresses (struct NIC_INFORMATION *adapter)
{
  BYTE FlowControlAddress[ETHERNET_ADDRESS_SIZE];

  DBGPRINT_FUNCTION (("SetMulticastAddresses: IN\n"));

  /* Clear all bits in the hash filter, then write all multicast bits back
   */
  InitializeHashFilter (adapter);

  if (adapter->Hardware.FlowControlEnable &&
      adapter->Hardware.FlowControlSupported &&
      adapter->Hardware.FullDuplexEnable)
  {
    /* Set the flow control address bit
     */
    FlowControlAddress[0] = NIC_FLOW_CONTROL_ADDRESS_0;
    FlowControlAddress[1] = NIC_FLOW_CONTROL_ADDRESS_1;
    FlowControlAddress[2] = NIC_FLOW_CONTROL_ADDRESS_2;
    FlowControlAddress[3] = NIC_FLOW_CONTROL_ADDRESS_3;
    FlowControlAddress[4] = NIC_FLOW_CONTROL_ADDRESS_4;
    FlowControlAddress[5] = NIC_FLOW_CONTROL_ADDRESS_5;

    NIC_COMMAND (adapter, (WORD) (COMMAND_SET_HASH_FILTER_BIT | 0x0400 |
                                  HashAddress (FlowControlAddress)));
  }
  DBGPRINT_FUNCTION (("SetMulticastAddresses: OUT\n"));
  return (NIC_STATUS_SUCCESS);
}


/*
 * Clear all bits in hash filter and setup the multicast address bit.
 */
STATIC void InitializeHashFilter (struct NIC_INFORMATION *adapter)
{
  struct device *device   = adapter->Device;
  DWORD  count, bitsInHashFilter;

  /* Clear all bits in the hash filter, then write all multicast bits back
   */
  bitsInHashFilter = adapter->Hardware.BitsInHashFilter;

  for (count = 0; count < bitsInHashFilter; count++)
      NIC_COMMAND (adapter, (WORD) (COMMAND_SET_HASH_FILTER_BIT | count));

  for (count = 0; count < device->mc_count; count++)
      NIC_COMMAND (adapter, (WORD)(COMMAND_SET_HASH_FILTER_BIT | 0x400 |
                   HashAddress (&device->mc_list[count][0])));

}


/*
 * This routine checks whether autoselection is specified.  If it
 * does, it calls MainAutoSelectionRoutine else for non-autoselect
 * case, calls ProgramMII.
 */
STATIC DWORD SetupMedia (struct device *Device)
{
  struct NIC_INFORMATION *adapter = (struct NIC_INFORMATION*) Device->priv;
  enum   CONNECTOR_TYPE NotUsed;
  DWORD  nicStatus, InternalConfig = 0;
  WORD   InternalConfig0 = 0;
  WORD   InternalConfig1 = 0;
  WORD   OptionAvailable = 0;
  WORD   MacControl = 0;
  BOOL   Handles100Mbit = FALSE;

  DBGPRINT_FUNCTION (("SetupMedia: In\n"));

  adapter->Hardware.AutoSelect = FALSE;
  adapter->Hardware.ConfigConnector = CONNECTOR_UNKNOWN;
  adapter->Hardware.Connector = CONNECTOR_UNKNOWN;

  /* Assumption made here for Cyclone, Hurricane, and Tornado
   * adapters have the same fixed PHY address.  For other PHY
   * address values, this needs to be changed.
   */
  adapter->Hardware.phys = MII_PHY_ADDRESS;
  adapter->Hardware.MIIReadCommand = MII_PHY_ADDRESS | 0x6000;
  adapter->Hardware.MIIWriteCommand = MII_PHY_ADDRESS | 0x5002;

  /* If this is a 10mb Lightning card, assume that the 10FL bit is
   * set in the media options register
   */
  if (adapter->Hardware.DeviceId == NIC_PCI_DEVICE_ID_900A)
  {
    DBGPRINT_INITIALIZE (("SetupMedia: 10BaseFL force Media Option\n"));
    OptionAvailable = MEDIA_OPTIONS_10BASEFL_AVAILABLE;
  }
  else
  {
    /* Read the MEDIA OPTIONS to see what connectors are available
     */
    NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_3);
    OptionAvailable = NIC_READ_PORT_WORD (adapter, MEDIA_OPTIONS_REGISTER);
  }

  /* Read the internal config through EEPROM since reset
   * invalidates the normal register value.
   */
  nicStatus = ReadEEPROM (adapter, EEPROM_INTERNAL_CONFIG_WORD_0,
                          (WORD*)&InternalConfig0);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("SetupMedia: InternalConfig 0 read failed\n"));
    return (NIC_STATUS_FAILURE);
  }

  nicStatus = ReadEEPROM (adapter, EEPROM_INTERNAL_CONFIG_WORD_1,
                          (WORD*)&InternalConfig1);
  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("SetupMedia: InternalConfig 1 read failed\n"));
    return (NIC_STATUS_FAILURE);
  }

  InternalConfig = InternalConfig0 | (InternalConfig1 << 16);

  DBGPRINT_INITIALIZE (("SetupMedia: InternalConfig %x\n", InternalConfig));

  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_3);

  /* Read the current value of the InternalConfig register. If it's different
   * from the EEPROM values, than write it out using the EEPROM values.
   * This is done since a global reset may invalidate the register value on
   * some ASICs. Also, writing to InternalConfig may reset the PHY on some ASICs.
   */
  if (InternalConfig != NIC_READ_PORT_DWORD (adapter, INTERNAL_CONFIG_REGISTER))
     NIC_WRITE_PORT_DWORD (adapter, INTERNAL_CONFIG_REGISTER, InternalConfig);

  /* Get the connector to use.
   */
  if (adapter->Hardware.ConfigConnector == CONNECTOR_UNKNOWN)
  {
    adapter->Hardware.ConfigConnector = (InternalConfig & INTERNAL_CONFIG_TRANSCEIVER_MASK) >> 20;
    adapter->Hardware.Connector = adapter->Hardware.ConfigConnector;
    if (InternalConfig & INTERNAL_CONFIG_AUTO_SELECT)
       adapter->Hardware.AutoSelect = TRUE;
    ProcessMediaOverrides (adapter, OptionAvailable);
  }

  /* If auto selection of connector was specified, do it now...
   */
  if (adapter->Hardware.AutoSelect)
  {
    DBGPRINT_INITIALIZE (("SetupMedia: Autoselect set\n"));
    NIC_COMMAND (adapter, COMMAND_STATISTICS_DISABLE);
    MainAutoSelectionRoutine (adapter, OptionAvailable);
  }
  else
  {
    /* MII connector needs to be initialized and the data rates
     * set up even in the non-autoselect case
     */
    DBGPRINT_INITIALIZE (("SetupMedia: Adapter in forced-mode\n"));

    if ((adapter->Hardware.Connector == CONNECTOR_MII) ||
        (adapter->Hardware.Connector == CONNECTOR_AUTONEGOTIATION))
       ProgramMII (adapter, CONNECTOR_MII);
    else
    {
      if ((adapter->Hardware.Connector == CONNECTOR_100BASEFX) ||
          (adapter->Hardware.Connector == CONNECTOR_100BASETX))
           adapter->Hardware.LinkSpeed = LINK_SPEED_100;
      else adapter->Hardware.LinkSpeed = LINK_SPEED_10;
    }
    SetupConnector (adapter, adapter->Hardware.Connector, &NotUsed);
  }

  /* Check link speed and duplex settings before doing anything else.
   * If the call succeeds, we know the link is up, so we'll update the
   * link state.
   */
  if (GetLinkSpeed (adapter, &Handles100Mbit))
  {
    adapter->Hardware.LinkSpeed = Handles100Mbit ? LINK_SPEED_100 : LINK_SPEED_10;
    adapter->Hardware.LinkState = LINK_UP;
  }
  else
    adapter->Hardware.LinkState = LINK_DOWN_AT_INIT;

  /* Set up duplex mode
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_3);

  MacControl = NIC_READ_PORT_WORD (adapter, MAC_CONTROL_REGISTER);

  if (adapter->Hardware.FullDuplexEnable)
  {
    /* Set Full duplex in MacControl register
     */
    MacControl |= MAC_CONTROL_FULL_DUPLEX_ENABLE;
    DBGPRINT_INITIALIZE (("Changed link to full duplex\n"));

    /* Since we're switching to full duplex, enable flow control.
     */
    if (adapter->Hardware.FlowControlSupported)
    {
      DBGPRINT_INITIALIZE (("SetupMedia: flow Control support is on!\n"));
      MacControl |= MAC_CONTROL_FLOW_CONTROL_ENABLE;
      adapter->Hardware.FlowControlEnable = TRUE;
      SetMulticastAddresses (adapter);
    }
  }
  else
  {
    /* Set Half duplex in MacControl register
     */
    MacControl &= ~MAC_CONTROL_FULL_DUPLEX_ENABLE;
    DBGPRINT_INITIALIZE (("Changed link to half duplex\n"));

    /* Since we're switching to half duplex, disable flow control
     */
    if (adapter->Hardware.FlowControlSupported)
    {
      MacControl &= ~MAC_CONTROL_FLOW_CONTROL_ENABLE;
      adapter->Hardware.FlowControlEnable = FALSE;
      SetMulticastAddresses (adapter);
    }
  }

  NIC_WRITE_PORT_WORD (adapter, MAC_CONTROL_REGISTER, MacControl);

  if (ResetAndEnableTransmitter (adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_INITIALIZE (("SetupMedia: Reset transmitter failed\n"));
    DBGPRINT_FUNCTION (("SetupMedia: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

  if (ResetAndEnableReceiver (adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_INITIALIZE (("SetupMedia: Reset receiver failed\n"));
    DBGPRINT_FUNCTION (("SetupMedia: Out with error\n"));
    return (NIC_STATUS_FAILURE);
  }

  /* This is for advertisement of flow control.  We only need to
   * call this if the adapter is using flow control, in Autoselect
   * mode and not a Tornado board.
   */
  if ((adapter->Hardware.AutoSelect &&
       adapter->Hardware.FlowControlEnable) &&
      (adapter->Hardware.DeviceId != NIC_PCI_DEVICE_ID_9200) &&
      (adapter->Hardware.DeviceId != NIC_PCI_DEVICE_ID_9805))
    FlowControl (adapter);

  DBGPRINT_FUNCTION (("SetupMedia: Out\n"));
  return (NIC_STATUS_SUCCESS);
}

/*
 * Change the connector and duplex if values are present in command line.
 */
STATIC void ProcessMediaOverrides (struct NIC_INFORMATION *adapter,
                                   WORD OptionAvailable)
{
  DWORD InternalConfig = 0;
  DWORD OldInternalConfig = 0;

  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_3);

  InternalConfig = NIC_READ_PORT_DWORD (adapter, INTERNAL_CONFIG_REGISTER);

  OldInternalConfig = InternalConfig;

  if (adapter->Hardware.MediaOverride == MEDIA_AUTO_SELECT)
       InternalConfig |= INTERNAL_CONFIG_AUTO_SELECT;
  else InternalConfig &= ~INTERNAL_CONFIG_AUTO_SELECT;

  /* Write to the InternalConfig register only if it's changed.
   */
  if (OldInternalConfig != InternalConfig)
     NIC_WRITE_PORT_DWORD (adapter, INTERNAL_CONFIG_REGISTER, InternalConfig);

  switch (adapter->Hardware.MediaOverride)
  {
    case MEDIA_AUTO_SELECT:
         adapter->Hardware.AutoSelect = TRUE;
         break;

    case MEDIA_10BASE_T:
         if (OptionAvailable & MEDIA_OPTIONS_10BASET_AVAILABLE)
         {
           adapter->Hardware.Connector = CONNECTOR_10BASET;
           adapter->Hardware.AutoSelect = FALSE;
         }
         break;

    case MEDIA_10AUI:
         if (OptionAvailable & MEDIA_OPTIONS_10AUI_AVAILABLE)
         {
           adapter->Hardware.Connector = CONNECTOR_10AUI;
           adapter->Hardware.AutoSelect = FALSE;
         }
         break;

    case MEDIA_10BASE_2:
         if (OptionAvailable & MEDIA_OPTIONS_10BASE2_AVAILABLE)
         {
           adapter->Hardware.Connector = CONNECTOR_10BASE2;
           adapter->Hardware.AutoSelect = FALSE;
         }
         break;

    case MEDIA_100BASE_TX:
         if (OptionAvailable & MEDIA_OPTIONS_100BASETX_AVAILABLE)
         {
           adapter->Hardware.Connector = CONNECTOR_100BASETX;
           adapter->Hardware.AutoSelect = FALSE;
         }
         break;

    case MEDIA_100BASE_FX:
         if (OptionAvailable & MEDIA_OPTIONS_100BASEFX_AVAILABLE)
         {
           adapter->Hardware.Connector = CONNECTOR_100BASEFX;
           adapter->Hardware.AutoSelect = FALSE;
         }
         break;

    case MEDIA_10BASE_FL:
         if (OptionAvailable & MEDIA_OPTIONS_10BASEFL_AVAILABLE)
         {
           adapter->Hardware.Connector = CONNECTOR_10AUI;
           adapter->Hardware.AutoSelect = FALSE;

         }
         break;

    case MEDIA_NONE:
         break;
  }
}


/*
 * Adjust linkspeed and duplex in tick handler 
 */
STATIC void TickMediaHandler (struct NIC_INFORMATION *adapter)
{
  if ((adapter->Hardware.Connector == CONNECTOR_AUTONEGOTIATION) ||
      (adapter->Hardware.Connector == CONNECTOR_10BASET) ||
      (adapter->Hardware.Connector == CONNECTOR_100BASETX))
  {
    /* TP case
     */
    CheckTPLinkState (adapter);
  }
  else if (adapter->Hardware.Connector == CONNECTOR_100BASEFX)
  {
    /* FX case
     */
    CheckFXLinkState (adapter);
  }
}


/*
 * Determine whether to notify the operating system if link is lost 
 * or restored. For autonegotiation case, made adjustments to duplex
 * and speed if necessary.
 */
STATIC void CheckTPLinkState (struct NIC_INFORMATION *adapter)
{
  BOOL  handles100Mbit = FALSE;
  WORD  PhyStatus = 0;
  BOOL  OldFullDuplex;
  DWORD OldLinkSpeed;

  if (ReadMIIPhy (adapter, MII_PHY_STATUS, &PhyStatus))
  {
    if (!(PhyStatus & MII_STATUS_LINK_UP))
    {
      /* If OS doesn't know link was lost, go ahead and notify
       */
      if (adapter->Hardware.LinkState == LINK_UP)
         IndicateToOSLinkStateChange (adapter);
    }
    else
    {
      /* If OS doesn't know link was restored, go ahead and notify
       */
      if (adapter->Hardware.LinkState != LINK_UP)
         IndicateToOSLinkStateChange (adapter);

      if (((PhyStatus & MII_STATUS_AUTO) &&
           (PhyStatus & MII_STATUS_EXTENDED)) &&
           (adapter->Hardware.Connector == CONNECTOR_AUTONEGOTIATION))
      {
        OldLinkSpeed  = adapter->Hardware.LinkSpeed;
        OldFullDuplex = adapter->Hardware.FullDuplexEnable;

        /* Capable of autonegotiation
         */
        if (GetLinkSpeed (adapter, &handles100Mbit))
        {
          if (handles100Mbit)
               adapter->Hardware.LinkSpeed = LINK_SPEED_100;
          else adapter->Hardware.LinkSpeed = LINK_SPEED_10;

          /* Set up for new speed if speed changed. Set
           * counterSpeed bit in DmaCtrl register.
           */
          if (adapter->Hardware.LinkSpeed != OldLinkSpeed)
             SetupNewSpeed (adapter);

          /* Set up for new duplex mode if duplex changed.
           */
          if (adapter->Hardware.FullDuplexEnable != OldFullDuplex)
             SetupNewDuplex (adapter);
        }
        else
          DBGPRINT_INITIALIZE (("Failed to get link speed\n"));
      }
    }
  }
}

/*
 * 
 */
STATIC void CheckFXLinkState (struct NIC_INFORMATION *adapter)
{
  WORD MediaStatus = 0;

  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);

  MediaStatus = NIC_READ_PORT_WORD (adapter, MEDIA_STATUS_REGISTER);

  if (!(MediaStatus & MEDIA_STATUS_LINK_DETECT))
  {
    if (adapter->Hardware.LinkState == LINK_UP)
       IndicateToOSLinkStateChange (adapter);
  }
  else
  {
    if (adapter->Hardware.LinkState != LINK_UP)
       IndicateToOSLinkStateChange (adapter);
  }
}

/*
 * Setup new duplex in MacControl register.
 */
STATIC DWORD SetupNewDuplex (struct NIC_INFORMATION *adapter)
{
  WORD MacControl = 0;

  DBGPRINT_INITIALIZE (("SetupNewDuplex: IN\n"));

  NIC_COMMAND (adapter, COMMAND_RX_DISABLE);
  NIC_COMMAND (adapter, COMMAND_TX_DISABLE);

  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);

  /* Wait for transmit to go quiet
   */
  MediaStatus_g = 0;
  MediaStatus_g = NIC_READ_PORT_WORD (adapter, MEDIA_STATUS_REGISTER);
  udelay (10);

  if (MediaStatus_g & MEDIA_STATUS_TX_IN_PROGRESS)
  {
    adapter->WaitCases = CHECK_TRANSMIT_IN_PROGRESS;
    TimeOutCount = jiffies + HZ;
    if (!InWaitTimer)
    {
      WaitTimer.expires = RUN_AT (HZ / 100); /* max = 1s */
      add_timer (&WaitTimer);
      InWaitTimer = TRUE;
    }
    while (TimeOutCount > jiffies) ;
    if (!(MediaStatus_g & MEDIA_STATUS_TX_IN_PROGRESS))
    {
      DBGPRINT_ERROR (("SetupNewDuplex: Packet not picked up by hardware"));
      return (NIC_STATUS_FAILURE);
    }
  }

  /* Wait for receive to go quiet
   */
  MediaStatus_g = NIC_READ_PORT_WORD (adapter, MEDIA_STATUS_REGISTER);

  udelay (10);

  if (MediaStatus_g & MEDIA_STATUS_CARRIER_SENSE)
  {
    adapter->WaitCases = CHECK_CARRIER_SENSE;
    TimeOutCount = jiffies + HZ;
    if (!InWaitTimer)
    {
      WaitTimer.expires = RUN_AT (HZ / 100); /* max = 1s */
      add_timer (&WaitTimer);
      InWaitTimer = TRUE;
    }
    while (TimeOutCount > jiffies) ;
    if (!(MediaStatus_g & MEDIA_STATUS_CARRIER_SENSE))
    {
      adapter->Hardware.Status = HARDWARE_STATUS_HUNG;
      DBGPRINT_ERROR (("SetupNewDuplex: Packet not uploaded by hardware"));
      return (NIC_STATUS_FAILURE);
    }
  }

  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_3);

  MacControl = NIC_READ_PORT_WORD (adapter, MAC_CONTROL_REGISTER);

  if (adapter->Hardware.FullDuplexEnable)
  {
    /* Set Full duplex in MacControl register
     */
    MacControl |= MAC_CONTROL_FULL_DUPLEX_ENABLE;
    DBGPRINT_INITIALIZE (("Changed link to full duplex\n"));

    /* Since we're switching to full duplex, enable flow control.
     */
    if (adapter->Hardware.FlowControlSupported)
    {
      MacControl |= MAC_CONTROL_FLOW_CONTROL_ENABLE;
      adapter->Hardware.FlowControlEnable = TRUE;
      SetMulticastAddresses (adapter);
    }
  }
  else
  {
    /* Set Half duplex in MacControl register
     */
    MacControl &= ~MAC_CONTROL_FULL_DUPLEX_ENABLE;
    DBGPRINT_INITIALIZE (("Changed link to half duplex\n"));

    /* Since we're switching to half duplex, disable flow control
     */
    if (adapter->Hardware.FlowControlEnable && adapter->Hardware.FlowControlSupported)
    {
      MacControl &= ~MAC_CONTROL_FLOW_CONTROL_ENABLE;
      adapter->Hardware.FlowControlEnable = FALSE;
      SetMulticastAddresses (adapter);
    }
  }
  NIC_WRITE_PORT_WORD (adapter, MAC_CONTROL_REGISTER, MacControl);
  udelay (20);

#if 0
  /* must do Tx/Rx reset and wait at least 3s
   */
  NIC_COMMAND(adapter,COMMAND_TX_RESET);
  NIC_COMMAND(adapter, COMMAND_RX_RESET);

  TimeOutCount = jiffies + 3*HZ;
  while (TimeOutCount > jiffies)
       ;
#endif

  NIC_COMMAND (adapter, COMMAND_RX_ENABLE);
  NIC_COMMAND (adapter, COMMAND_TX_ENABLE);

  DBGPRINT_INITIALIZE (("SetupNewDuplex: OUT\n"));

  return (NIC_STATUS_SUCCESS);
}


/*
 * Sets the counter speed in the DMA control register. Clear bit
 * for 10Mbps or set the bit for 100Mbps.
 */
STATIC void SetupNewSpeed (struct NIC_INFORMATION *adapter)
{
  DWORD DmaControl = 0;

  DBGPRINT_INITIALIZE (("SetupNewSpeed: IN\n"));

  DmaControl = NIC_READ_PORT_DWORD (adapter, DMA_CONTROL_REGISTER);

  if (adapter->Hardware.LinkSpeed == LINK_SPEED_100)
       DmaControl |= DMA_CONTROL_COUNTER_SPEED;
  else DmaControl &= ~DMA_CONTROL_COUNTER_SPEED;

  NIC_WRITE_PORT_DWORD (adapter, DMA_CONTROL_REGISTER, DmaControl);

  DBGPRINT_INITIALIZE (("Changed link speed to %ld bps\n",
                       adapter->Hardware.LinkSpeed));

  DBGPRINT_INITIALIZE (("SetupNewSpeed: OUT\n"));
}


/*
 * Notify to NDI wrapper the change in link state.
 */
STATIC void IndicateToOSLinkStateChange (struct NIC_INFORMATION *adapter)
{
  if (adapter->Hardware.LinkState != LINK_DOWN)
  {
    DBGPRINT_INITIALIZE (("Link Lost...\n"));
    adapter->Hardware.LinkState = LINK_DOWN;
    (adapter->Device)->flags &= ~(IFF_UP|IFF_RUNNING);
  }
  else
  {
    DBGPRINT_INITIALIZE (("Link Regained...\n"));
    adapter->Hardware.LinkState = LINK_UP;
    (adapter->Device)->flags |= (IFF_UP|IFF_RUNNING);
  }
}
 

/*
 * This routine handles the interrupt.
 */
STATIC void NICInterrupt (int Irq)
{
  struct device          *Device  = irq2dev_map[Irq];
  struct NIC_INFORMATION *adapter = (struct NIC_INFORMATION*) Device->priv;
  WORD   intStatus = 0;
  BYTE   loopCount = 2;
  BOOL   countDownTimerEventCalled = FALSE;

  if (Device->reentry)
  {
    printk ("%s: Re-enter the interrupt handler.\n", Device->name);
    return;
  }

  Device->reentry = 1;

  intStatus = NIC_READ_PORT_BYTE (adapter, INTSTATUS_COMMAND_REGISTER);

  if (!(intStatus & INTSTATUS_INTERRUPT_LATCH))
  {
    Device->reentry = 0;
    return;
  }

  /* Mask all the interrupts
   */
  NIC_MASK_ALL_INTERRUPT (adapter);
  NIC_COMMAND (adapter, COMMAND_ACKNOWLEDGE_INTERRUPT |
                         ACKNOWLEDGE_INTERRUPT_LATCH);

  while (loopCount--)
  {
    /* Read the interrupt status register.
     */
    intStatus = NIC_READ_PORT_WORD (adapter, INTSTATUS_COMMAND_REGISTER);

    intStatus &= INTSTATUS_INTERRUPT_MASK;
    if (!intStatus)
       break;

    if (intStatus & INTSTATUS_HOST_ERROR)
    {
      printk ("HostError ");
      DBGPRINT_ERROR (("NICInterrupt: HostError event happened.\n"));
      HostErrorEvent (adapter);
    }

    if (intStatus & INTSTATUS_UPDATE_STATISTICS)
    {
      /* interrupt is cleared by reading statistics.
       */
      UpdateStatisticsEvent (adapter);
    }

    if (intStatus & INTSTATUS_UP_COMPLETE)
    {
      NIC_COMMAND (adapter, COMMAND_ACKNOWLEDGE_INTERRUPT |
                             ACKNOWLEDGE_UP_COMPLETE);
      UpCompleteEvent (adapter);
    }

    if (intStatus & INTSTATUS_INTERRUPT_REQUESTED)
    {
      NIC_COMMAND (adapter, COMMAND_ACKNOWLEDGE_INTERRUPT |
                             ACKNOWLEDGE_INTERRUPT_REQUESTED);
      CountDownTimerEvent (adapter);
      countDownTimerEventCalled = TRUE;
    }

    if (intStatus & INTSTATUS_TX_COMPLETE)
       TxCompleteEvent (adapter);

    if ((intStatus & INTSTATUS_RX_COMPLETE) ||
        (intStatus & INTSTATUS_LINK_EVENT) ||
        (intStatus & INTSTATUS_DOWN_COMPLETE))
      printk ("NICInterrupt: Unknown interrupt, IntStatus =%x\n", intStatus);

    if (FALSE == countDownTimerEventCalled)
    {
      CountDownTimerEvent (adapter);
      countDownTimerEventCalled = TRUE;
    }
  }
  NIC_UNMASK_ALL_INTERRUPT (adapter);
  Device->reentry = 0;
}


/*
 * This routine handles the host error.
 */
STATIC void HostErrorEvent (struct NIC_INFORMATION *adapter)
{
  DBGPRINT_INTERRUPT (("HostErrorEvent: IN\n"));

  /* Read the internal config.
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_3);

  adapter->keepForGlobalReset = NIC_READ_PORT_DWORD (adapter,
                                   INTERNAL_CONFIG_REGISTER);

  DBGPRINT_INTERRUPT (("Adapter does global reset and restart\n"));

  /* Issue Global reset. I will mask the updown reset so that
   * I don't have to set the UpPoll, DownPoll, UpListPointer
   * and DownListPointer.
   */
  NIC_COMMAND_WAIT (adapter,
                    COMMAND_GLOBAL_RESET |
                    GLOBAL_RESET_MASK_NETWORK_RESET |
                    GLOBAL_RESET_MASK_TP_AUI_RESET |
                    GLOBAL_RESET_MASK_ENDEC_RESET |
                    GLOBAL_RESET_MASK_AISM_RESET |
                    GLOBAL_RESET_MASK_SMB_RESET |
                    GLOBAL_RESET_MASK_VCO_RESET |
                    GLOBAL_RESET_MASK_UP_DOWN_RESET);

  ReStartAdapter (adapter);
  DBGPRINT_INTERRUPT (("HostErrorEvent: OUT\n"));
}


/*
 * This routine handles the update statistics interrupt.
 */
STATIC void UpdateStatisticsEvent (struct NIC_INFORMATION *adapter)
{
  struct NIC_STATISTICS *statistics = &adapter->Statistics;
  WORD   rxPackets, txPackets, highPackets;
  WORD   rxBytes, txBytes, highBytes;

  DBGPRINT_INTERRUPT (("UpdateStatisticsEvent: IN\n"));

  /* Change the window.
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_6);

  statistics->TxSQEErrors += NIC_READ_PORT_BYTE (adapter, SQE_ERRORS_REGISTER);

  statistics->TxMultipleCollisions += NIC_READ_PORT_BYTE (adapter,
                                        MULTIPLE_COLLISIONS_REGISTER);

  statistics->TxSingleCollisions += NIC_READ_PORT_BYTE (adapter,
                                      SINGLE_COLLISIONS_REGISTER);

  statistics->RxOverruns += NIC_READ_PORT_BYTE (adapter, RX_OVERRUNS_REGISTER);

  statistics->TxCarrierLost += NIC_READ_PORT_BYTE (adapter,
                                 CARRIER_LOST_REGISTER);

  statistics->TxLateCollisions += NIC_READ_PORT_BYTE (adapter,
                                    LATE_COLLISIONS_REGISTER);

  statistics->TxFramesDeferred += NIC_READ_PORT_BYTE (adapter,
                                    FRAMES_DEFERRED_REGISTER);
  rxPackets = NIC_READ_PORT_BYTE (adapter, FRAMES_RECEIVED_OK_REGISTER);

  txPackets = NIC_READ_PORT_BYTE (adapter, FRAMES_TRANSMITTED_OK_REGISTER);

  highPackets = NIC_READ_PORT_BYTE (adapter, UPPER_FRAMES_OK_REGISTER);

  rxPackets += ((highPackets & 0x03) << 8);
  txPackets += ((highPackets & 0x30) << 4);

  if (adapter->Hardware.SQEDisable)
     statistics->TxSQEErrors += txPackets;

  statistics->RxFramesOk += rxPackets;
  statistics->TxFramesOk += txPackets;

  rxBytes = NIC_READ_PORT_WORD (adapter, BYTES_RECEIVED_OK_REGISTER);

  txBytes = NIC_READ_PORT_WORD (adapter, BYTES_TRANSMITTED_OK_REGISTER);

  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);

  highBytes = NIC_READ_PORT_BYTE (adapter, UPPER_BYTES_OK_REGISTER);

  rxBytes += ((highBytes & 0x0F) << 8);
  txBytes += ((highBytes & 0xF0) << 4);

  statistics->RxBytesOk += rxBytes;
  statistics->TxBytesOk += txBytes;

  statistics->RxBadSSD += NIC_READ_PORT_BYTE (adapter, BAD_SSD_REGISTER);

  DBGPRINT_INTERRUPT (("UpdateStatisticsEvent: OUT\n"));
}

/*
 * This routine handles the interrupt requested event.
 */
STATIC void CountDownTimerEvent (struct NIC_INFORMATION *adapter)
{
  struct DPD_LIST_ENTRY *headDPDVirtual;
  struct device         *device = adapter->Device;

  DBGPRINT_INTERRUPT (("CountDownTimerEvent: IN\n"));
  headDPDVirtual = adapter->HeadDPDVirtual;

  /* This clears the FSH_DPD_EMPTY and a raise condition of hardware.
   */
  adapter->TailDPDVirtual->FrameStartHeader = 0;
  while (1)
  {
    if (!(headDPDVirtual->FrameStartHeader & FSH_DOWN_COMPLETE))
       break;

    ASSERT (headDPDVirtual->SocketBuffer != NULL);
    k_free (headDPDVirtual->SocketBuffer);
    headDPDVirtual->SocketBuffer = NULL;

    /* Clear the down complete bit in the frame start header.
     */
    headDPDVirtual->FrameStartHeader = 0;
    adapter->BytesInDPDQueue -= headDPDVirtual->PacketLength;
    headDPDVirtual = headDPDVirtual->Next;
    ASSERT (adapter->HeadDPDVirtual != NULL);
  }
  adapter->HeadDPDVirtual = headDPDVirtual;
  if (adapter->BytesInDPDQueue)
     SetCountDownTimer (adapter);

  /* If DPD ring is full, run the bottom half
   */
  if ((device->tx_busy) && (adapter->DPDRingFull == TRUE))
  {
    DBGPRINT_SEND (("CountdownTimer: set RingFull false,mark bh\n"));
    adapter->DPDRingFull = FALSE;
    device->tx_busy = 0;
  }
  DBGPRINT_INTERRUPT (("CountDownTimerEvent: OUT\n"));
}

    
/*
 * This is the tick handler.
 */
STATIC void NICTimer (DWORD Data)
{
  struct device          *device   = (struct device*) Data;
  struct NIC_INFORMATION *adapter = (struct NIC_INFORMATION*) device->priv;

  adapter->InTimer = TRUE;
  adapter->Statistics.UpdateInterval += adapter->Resources.TimerInterval;
  if (adapter->Statistics.UpdateInterval > 1000)
  {
    adapter->Statistics.UpdateInterval = 0;
    UpdateStatisticsEvent (adapter);
  }
  /* Check every five seconds for media changed speed or duplex
   */
  adapter->Hardware.UpdateInterval += adapter->Resources.TimerInterval;
  if (adapter->Hardware.UpdateInterval > 5000)
  {
    adapter->Hardware.UpdateInterval = 0;
    TickMediaHandler (adapter);
  }

  adapter->Resources.Timer.expires = RUN_AT (HZ / 10);
  add_timer (&adapter->Resources.Timer);
  adapter->InTimer = FALSE;
}


STATIC void WaitTimerHandler (DWORD Data)
{
  struct device          *device   = (struct device*) Data;
  struct NIC_INFORMATION *adapter = (struct NIC_INFORMATION*) device->priv;
  struct DPD_LIST_ENTRY  *dpdVirtual;
  int    NextTick = 0;

  DBGPRINT_FUNCTION (("In WaitTimer-Time=%x\n", (int) jiffies));

  switch (adapter->WaitCases)
  {
    case CHECK_DOWNLOAD_STATUS:
         DBGPRINT_INITIALIZE (("CHECK_DOWNLOAD_STATUS\n"));
         dpdVirtual = (struct DPD_LIST_ENTRY*) adapter->TestDPDVirtual[0];
         if (portValue_g == dpdVirtual->DPDPhysicalAddress)
         {
           portValue_g = NIC_READ_PORT_DWORD (adapter, DOWN_LIST_POINTER_REGISTER);
           NextTick = HZ / 10;
         }
         break;

    case CHECK_UPLOAD_STATUS:
         DBGPRINT_INITIALIZE (("CHECK_UPLOAD_STATUS\n"));
         if (portValue_g == adapter->HeadUPDVirtual->UPDPhysicalAddress)
         {
           portValue_g = NIC_READ_PORT_DWORD (adapter, UP_LIST_POINTER_REGISTER);
           NextTick = HZ / 10;
         }
         break;

    case CHECK_DC_CONVERTER:
         DBGPRINT_INITIALIZE (("CHECK_DC_CONVERTER\n"));
         if (((DCConverterEnabledState_g) && !(MediaStatus_g & MEDIA_STATUS_DC_CONVERTER_ENABLED)) ||
             ((!DCConverterEnabledState_g) && (MediaStatus_g & MEDIA_STATUS_DC_CONVERTER_ENABLED)))
         {
           MediaStatus_g = NIC_READ_PORT_WORD (adapter, MEDIA_STATUS_REGISTER);
           NextTick = HZ / 10;
         }
         break;

    case CHECK_PHY_STATUS:
         DBGPRINT_INITIALIZE (("CHECK_PHY_STATUS\n"));
         if (!(PhyStatus_g & MII_STATUS_AUTO_DONE))
         {
           PhyResponding_g = ReadMIIPhy (adapter, MII_PHY_STATUS, &PhyStatus_g);
           NextTick = HZ / 10;
         }
         break;

    case CHECK_TRANSMIT_IN_PROGRESS:
         DBGPRINT_INITIALIZE (("CHECK_TRANSMIT_IN_PROGRESS\n"));
         if (MediaStatus_g & MEDIA_STATUS_TX_IN_PROGRESS)
         {
           MediaStatus_g = NIC_READ_PORT_WORD (adapter, MEDIA_STATUS_REGISTER);
           NextTick = HZ / 10;
         }
         break;

    case CHECK_DOWNLOAD_SELFDIRECTED:
         DBGPRINT_INITIALIZE (("CHECK_DOWNLOAD_SELFDIRECTED\n"));
         dpdVirtual = (struct DPD_LIST_ENTRY*) adapter->TestDPDVirtual[0];
         if (DownListPointer_g == dpdVirtual->DPDPhysicalAddress)
         {
           DownListPointer_g = NIC_READ_PORT_DWORD (adapter, DOWN_LIST_POINTER_REGISTER);
           NextTick = HZ / 10;
         }
         break;

    case AUTONEG_TEST_PACKET:
         DBGPRINT_INITIALIZE (("AUTONEG_TEST_PACKET\n"));
         if (UpListPointer_g == adapter->HeadUPDVirtual->UPDPhysicalAddress)
         {
           UpListPointer_g = NIC_READ_PORT_DWORD (adapter, UP_LIST_POINTER_REGISTER);
           NextTick = HZ / 10;
         }
         break;

    case CHECK_DMA_CONTROL:
         DBGPRINT_INITIALIZE (("CHECK_DMA_CONTROL\n"));
         if (dmaControl_g & DMA_CONTROL_DOWN_IN_PROGRESS)
         {
           dmaControl_g = NIC_READ_PORT_DWORD (adapter, DMA_CONTROL_REGISTER);
           NextTick = HZ / 10;
         }
         break;

    case CHECK_CARRIER_SENSE:
         DBGPRINT_INITIALIZE (("CHECK_CARRIER_SENSE\n"));
         if (MediaStatus_g & MEDIA_STATUS_CARRIER_SENSE)
         {
           MediaStatus_g = NIC_READ_PORT_WORD (adapter, MEDIA_STATUS_REGISTER);
           NextTick = HZ / 10;
         }
         break;

    default:
         break;
  }
  if (NextTick)
  {
    WaitTimer.expires = RUN_AT (NextTick);
    add_timer (&WaitTimer);
  }
  else
    InWaitTimer = FALSE;

  DBGPRINT_FUNCTION (("Out WaitTimer\n"));
}


/*
 * Search for any PHY that is not known.
 */
STATIC BOOL FindMIIPhy (struct NIC_INFORMATION *adapter)
{
  WORD MediaOptions = 0;
  WORD PhyManagement = 0;
  BYTE index;

  /* Read the MEDIA OPTIONS to see what connectors are available
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_3);

  MediaOptions = NIC_READ_PORT_WORD (adapter, MEDIA_OPTIONS_REGISTER);

  if ((MediaOptions & MEDIA_OPTIONS_MII_AVAILABLE) ||
      (MediaOptions & MEDIA_OPTIONS_100BASET4_AVAILABLE))
  {
    /* Drop everything, so we are not driving the data, and run the
     * clock through 32 cycles in case the PHY is trying to tell us
     * something. Then read the data line, since the PHY's pull-up
     * will read as a 1 if it's present.
     */
    NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);

    NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, 0);

    for (index = 0; index < 32; index++)
    {
      udelay (1);
      NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_CLOCK);

      udelay (1);
      NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, 0);
    }

    PhyManagement = NIC_READ_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER);

    if (PhyManagement & PHY_DATA1)
       return (TRUE);
    return (FALSE);
  }
  return (TRUE);
}

  
/*
 * Establishes the synchronization for each MII transaction. This
 * is done by sending thirty-two "1" bits.
 */
STATIC void SendMIIPhyPreamble (struct NIC_INFORMATION *adapter)
{
  BYTE index;

  /* Set up and send the preamble, a sequence of 32 "1" bits
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);

  NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_WRITE);

  for (index = 0; index < 32; index++)
  {
    NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_WRITE | PHY_DATA1);
    NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_WRITE | PHY_DATA1 | PHY_CLOCK);

    udelay (1);
    NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_WRITE);
    udelay (1);
  }
}


/*
 * Writes to a particular MII PHY register given the proper offset.
 */
STATIC void WriteMIIPhy (struct NIC_INFORMATION *adapter, WORD RegAddr, WORD Output)
{
  DWORD index, index2;
  WORD  writecmd[2];

  writecmd[0] = adapter->Hardware.MIIWriteCommand;
  writecmd[1] = 0;

  SendMIIPhyPreamble (adapter);

  /* Bits 2..6 of the command word specify the register.
   */
  writecmd[0] |= (RegAddr & 0x1F) << 2;
  writecmd[1] = Output;

  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);

  for (index2 = 0; index2 < 2; index2++)
  {
    for (index = 0x8000; index; index >>= 1)
    {
      if (writecmd[index2] & index)
      {
        NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_WRITE | PHY_DATA1);
        NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_WRITE | PHY_DATA1 | PHY_CLOCK);
      }
      else
      {
        NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_WRITE);
        NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_WRITE | PHY_CLOCK);
      }
      udelay (1);
      NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_WRITE);
      udelay (1);
    }
  }

  /* OK now give it a couple of clocks with nobody driving.
   */
  NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, 0);

  for (index = 0; index < 2; index++)
  {
    NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_CLOCK);
    udelay (1);

    NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, 0);
    udelay (1);
  }
}


/*
 * Reads a particular MII PHY register given the proper offset.
 */
STATIC BOOL ReadMIIPhy (struct NIC_INFORMATION *adapter, WORD RegisterAddress, WORD *pInput)
{
  WORD  PhyManagement = 0;
  WORD  ReadCommand;
  DWORD index;

  ReadCommand = adapter->Hardware.MIIReadCommand;

  SendMIIPhyPreamble (adapter);

  /* Bits 2..6 of the command word specify the register.
   */
  ReadCommand |= (RegisterAddress & 0x1F) << 2;

  for (index = 0x8000; index > 2; index >>= 1)
  {
    NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);

    if (ReadCommand & index)
    {
      NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_WRITE | PHY_DATA1);
      NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_WRITE | PHY_DATA1 | PHY_CLOCK);
    }
    else
    {
      NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_WRITE);
      NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_WRITE | PHY_CLOCK);
    }
    udelay (1);
    NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_WRITE);
    udelay (1);
  }

  /* Now run one clock with nobody driving.
   */
  NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, 0);
  NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_CLOCK);

  udelay (1);
  NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, 0);
  udelay (1);

  /* Now run one clock, expecting the PHY to be driving a 0 on the data
   * line.  If we read a 1, it has to be just his pull-up, and he's not
   * responding.
   */
  PhyManagement = NIC_READ_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER);

  if (PhyManagement & PHY_DATA1)
     return (FALSE);

  /* We think we are in sync.  Now we read 16 bits of data from the PHY.
   */
  for (index = 0x8000; index; index >>= 1)
  {
    /* Shift input up one to make room
     */
    NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);
    NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_CLOCK);

    udelay (1);
    NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, 0);
    udelay (1);

    PhyManagement = NIC_READ_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER);

    if (PhyManagement & PHY_DATA1)
         *pInput |= index;
    else *pInput &= ~index;
  }

  /* OK now give it a couple of clocks with nobody driving.
   */
  NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, 0);

  for (index = 0; index < 2; index++)
  {
    NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, PHY_CLOCK);
    udelay (1);

    NIC_WRITE_PORT_WORD (adapter, PHYSICAL_MANAGEMENT_REGISTER, 0);
    udelay (1);
  }
  return (TRUE);
}


/*
 * MII values need to be updated based on what was set in the registry.
 */
STATIC BOOL MIIMediaOverride (struct NIC_INFORMATION *adapter, WORD PhyModes, WORD *MiiType)
{
  switch (adapter->Hardware.MediaOverride)
  {
    case MEDIA_10BASE_T:
         if ((PhyModes & MII_STATUS_10TFD) && (adapter->Hardware.FullDuplexEnable == TRUE))
           *MiiType = MIISELECT_10BT;

         else if (PhyModes & MII_STATUS_10T)
           *MiiType = MIISELECT_10BT;

         else
           return (FALSE);
         break;

    case MEDIA_100BASE_TX:
         if ((PhyModes & MII_STATUS_100TXFD) && (adapter->Hardware.FullDuplexEnable == TRUE))
           *MiiType = MIISELECT_100BTX;

         else if (PhyModes & MII_STATUS_100TX)
           *MiiType = MIISELECT_100BTX;

         else
           return (FALSE);
         break;
  }
  return (TRUE);
}


/*
 * Setup the necessary MII registers with values either 
 * read from the EEPROM or from command line
 */
STATIC BOOL ProgramMII (struct NIC_INFORMATION *adapter, enum CONNECTOR_TYPE NewConnector)
{
  BOOL  PhyResponding;
  WORD  PhyControl, PhyStatus, MiiType = 0, PhyModes;
  DWORD status;

  /* First see if there's anything connected to the MII
   */
  if (!FindMIIPhy (adapter))
     return (FALSE);

  /* Nowhere is it written that the register must be latched, and since
   * reset is the last bit out, the contents might not be valid.  read
   * it one more time.
   */
  PhyResponding = ReadMIIPhy (adapter, MII_PHY_CONTROL, &PhyControl);

  /* Now we can read the status and try to figure out what's out there.
   */
  PhyResponding = ReadMIIPhy (adapter, MII_PHY_STATUS, &PhyStatus);
  if (!PhyResponding)
     return (FALSE);

  /* Reads the miiSelect field in EEPROM. Program MII as the default.
   */
  status = ReadEEPROM (adapter, EEPROM_SOFTWARE_INFORMATION_3, &MiiType);

  /* If an override is present AND the transceiver type is available
   * on the card, that type will be used.
   */
  PhyResponding = ReadMIIPhy (adapter, MII_PHY_STATUS, &PhyModes);
  if (!PhyResponding)
     return (FALSE);

  PhyResponding = ReadMIIPhy (adapter, MII_PHY_CONTROL, &PhyControl);
  if (!PhyResponding)
     return (FALSE);

  if (!MIIMediaOverride (adapter, PhyModes, &MiiType))
     return (FALSE);

  /* If full duplex selected, set it in PhyControl.
   */
  if (adapter->Hardware.FullDuplexEnable)
       PhyControl |= MII_CONTROL_FULL_DUPLEX;
  else PhyControl &= ~MII_CONTROL_FULL_DUPLEX;

  PhyControl &= ~MII_CONTROL_ENABLE_AUTO;

  if (((MiiType & MIITXTYPE_MASK) == MIISELECT_100BTX) ||
      ((MiiType & MIITXTYPE_MASK) == MIISELECT_100BTX_ANE))
  {
    PhyControl |= MII_CONTROL_100MB;
    WriteMIIPhy (adapter, MII_PHY_CONTROL, PhyControl);

    /* delay 600 milliseconds
     */
    udelay (600000);
    adapter->Hardware.LinkSpeed = 100000000L;
    DBGPRINT_INITIALIZE (("ProgramMII() Set to 100M\n"));
    return (TRUE);
  }
  if (((MiiType & MIITXTYPE_MASK) == MIISELECT_10BT) ||
      ((MiiType & MIITXTYPE_MASK) == MIISELECT_10BT_ANE))
  {
    PhyControl &= ~MII_CONTROL_100MB;
    WriteMIIPhy (adapter, MII_PHY_CONTROL, PhyControl);

    /* delay 600 milliseconds
     */
    udelay (600000);
    adapter->Hardware.LinkSpeed = 10000000L;
    DBGPRINT_INITIALIZE (("ProgramMII() Set to 10M\n"));
    return (TRUE);
  }

  PhyControl &= ~MII_CONTROL_100MB;
  WriteMIIPhy (adapter, MII_PHY_CONTROL, PhyControl);

  /* delay 600 milliseconds
   */
  udelay (600000);
  adapter->Hardware.LinkSpeed = 10000000L;
  DBGPRINT_INITIALIZE (("ProgramMII() Defaults to 10M\n"));
  return (FALSE);
}

/*
 * Sets up the tranceiver through MII registers. This will first check on
 * the current connection state as shown by the MII registers. If the
 * current state matches what the media options support, then the link
 * is kept. If not, the registers will be configured in the proper manner
 * and auto-negotiation will be restarted.
 * 
 * Since this function is called for forced xcvr configurations, it assumes
 * that the xcvr type has been verified as supported by the NIC.
 */
STATIC BOOL CheckMIIConfiguration (struct NIC_INFORMATION *adapter, WORD MediaOptions)
{
  WORD PhyControl, PhyStatus, PhyAnar, tempAnar;
  BOOL PhyResponding;

  /* Check to see if auto-negotiation has completed. Check the results
   * in the control and status registers.
   */
  PhyResponding = ReadMIIPhy (adapter, MII_PHY_CONTROL, &PhyControl);
  if (!PhyResponding)
  {
    DBGPRINT_ERROR (("CheckMIIConfiguration: Phy not responding (1)\n"));
    adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
    return (FALSE);
  }

  PhyResponding = ReadMIIPhy (adapter, MII_PHY_STATUS, &PhyStatus);
  if (!PhyResponding)
  {
    DBGPRINT_ERROR (("CheckMIIConfiguration: Phy not responding (2)\n"));
    adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
    return (FALSE);
  }

  if (!((PhyControl & MII_CONTROL_ENABLE_AUTO) &&
        (PhyStatus & MII_STATUS_AUTO_DONE)))
  {
    /* Auto-negotiation did not complete, so start it over using the new settings.
     */
    if (!ConfigureMII (adapter, MediaOptions))
      return (FALSE);
  }

  /* Auto-negotiation has completed. Check the results against the ANAR and
   * ANLPAR registers to see if we need to restart auto-neg.
   */
  PhyResponding = ReadMIIPhy (adapter, MII_PHY_ANAR, &PhyAnar);
  if (!PhyResponding)
  {
    DBGPRINT_ERROR (("CheckMIIConfiguration: Phy not responding (3)\n"));
    adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
    return (FALSE);
  }

  /* Check to see what we negotiated with the link partner. First, let's make
   * sure that the ANAR is set properly based on the media options defined.
   */
  tempAnar = 0;
  if (MediaOptions & MEDIA_OPTIONS_100BASETX_AVAILABLE)
  {
    if (adapter->Hardware.AutoSelect)
       tempAnar |= MII_ANAR_100TXFD | MII_ANAR_100TX;
    else
    {
      if (adapter->Hardware.FullDuplexEnable)
           tempAnar |= MII_ANAR_100TXFD;
      else tempAnar |= MII_ANAR_100TX;
    }
  }
  if (MediaOptions & MEDIA_OPTIONS_10BASET_AVAILABLE)
  {
    if (adapter->Hardware.AutoSelect)
       tempAnar |= MII_ANAR_10TFD | MII_ANAR_10T;
    else
    {
      if (adapter->Hardware.FullDuplexEnable)
           tempAnar |= MII_ANAR_10TFD;
      else tempAnar |= MII_ANAR_10T;
    }
  }
  if (adapter->Hardware.FullDuplexEnable &&
      adapter->Hardware.FlowControlSupported)
     tempAnar |= MII_ANAR_FLOWCONTROL;

  if ((PhyAnar & MII_ANAR_MEDIA_MASK) == tempAnar)
  {
    /* The negotiated configuration hasn't changed.
     * So, return and don't restart auto-negotiation.
     */
    return (TRUE);
  }

  /* Check the media settings.
   */
  if (MediaOptions & MEDIA_OPTIONS_100BASETX_AVAILABLE)
  {
    /* Check 100BaseTX settings.
     */
    if ((PhyAnar & MII_ANAR_MEDIA_100_MASK) != (tempAnar & MII_ANAR_MEDIA_100_MASK))
    {
      DBGPRINT_INITIALIZE (("CheckMIIConfiguration: Re-Initiating autonegotiation...\n"));
      return ConfigureMII (adapter, MediaOptions);
    }
  }
  if (MediaOptions & MEDIA_OPTIONS_10BASET_AVAILABLE)
  {
    /* Check 10BaseT settings.
     */
    if ((PhyAnar & MII_ANAR_MEDIA_10_MASK) != (tempAnar & MII_ANAR_MEDIA_10_MASK))
    {
      DBGPRINT_INITIALIZE (("CheckMIIConfiguration: Re-Initiating autonegotiation...\n"));
      return ConfigureMII (adapter, MediaOptions);
    }
  }
  return (TRUE);
}

/*
 * Used to setup the configuration for 10Base-T and 100Base-TX.
 */
STATIC BOOL ConfigureMII (struct NIC_INFORMATION *adapter, WORD MediaOptions)
{
  BOOL  PhyResponding;
  WORD  PhyControl, PhyAnar;
  DWORD TimeOutCount;

  DBGPRINT_INITIALIZE (("ConfigureMII: IN\n"));

  /* Nowhere is it written that the register must be latched, and since
   * reset is the last bit out, the contents might not be valid.  read
   * it one more time.
   */
  PhyResponding = ReadMIIPhy (adapter, MII_PHY_CONTROL, &PhyControl);
  if (!PhyResponding)
  {
    DBGPRINT_ERROR (("ConfigureMII: Phy not responding (1)\n"));
    adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
    return (FALSE);
  }

  /* Also, read the ANAR register and clear out it's current settings.
   */
  PhyResponding = ReadMIIPhy (adapter, MII_PHY_ANAR, &PhyAnar);
  if (!PhyResponding)
  {
    DBGPRINT_ERROR (("ConfigureMII: Phy not responding (2)\n"));
    adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
    return (FALSE);
  }

  /* Set up speed and duplex settings in MII Control and ANAR register.
   */
  PhyAnar &= ~(MII_ANAR_100TXFD | MII_ANAR_100TX | MII_ANAR_10TFD | MII_ANAR_10T);

  /* Set up duplex.
   */
  if (adapter->Hardware.FullDuplexEnable)
       PhyControl |=  MII_CONTROL_FULL_DUPLEX;
  else PhyControl &= ~MII_CONTROL_FULL_DUPLEX;

  /* Set up flow control.
   * NOTE: On some NICs, such as Tornado, this will be hardwired to be set.
   * Clearing it will have no effect.
   */
  if (adapter->Hardware.FlowControlSupported)
       PhyAnar |= MII_ANAR_FLOWCONTROL;
  else PhyAnar &= ~(MII_ANAR_FLOWCONTROL);

  /* Set up the media options. For duplex settings, if we're set to auto-select
   * then enable both half and full-duplex settings. Otherwise, go by what's
   * been enabled for duplex mode.
   */
  if (MediaOptions & MEDIA_OPTIONS_100BASETX_AVAILABLE)
  {
    if (adapter->Hardware.AutoSelect)
       PhyAnar |= (MII_ANAR_100TXFD | MII_ANAR_100TX);
    else
    {
      if (adapter->Hardware.FullDuplexEnable)
           PhyAnar |= MII_ANAR_100TXFD;
      else PhyAnar |= MII_ANAR_100TX;
    }
  }
  if (MediaOptions & MEDIA_OPTIONS_10BASET_AVAILABLE)
  {
    if (adapter->Hardware.AutoSelect)
       PhyAnar |= (MII_ANAR_10TFD | MII_ANAR_10T);
    else
    {
      if (adapter->Hardware.FullDuplexEnable)
           PhyAnar |= MII_ANAR_10TFD;
      else PhyAnar |= MII_ANAR_10T;
    }
  }

  /* Enable and start auto-negotiation
   */
  PhyControl |= (MII_CONTROL_ENABLE_AUTO | MII_CONTROL_START_AUTO);

  /* Write the MII registers back.
   */
  WriteMIIPhy (adapter, MII_PHY_ANAR, PhyAnar);
  WriteMIIPhy (adapter, MII_PHY_CONTROL, PhyControl);

  /* Wait for auto-negotiation to finish.
   */
  PhyStatus_g = 0;
  PhyResponding_g = ReadMIIPhy (adapter, MII_PHY_STATUS, &PhyStatus_g);
  udelay (1000);

  if (!PhyResponding_g)
  {
    DBGPRINT_ERROR (("ConfigureMII: Phy not responding (3)\n"));
    adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
    return (FALSE);
  }
  if (!(PhyStatus_g & MII_STATUS_AUTO_DONE))
  {
    TimeOutCount = jiffies + (3 * HZ); /* run max = 3s */
    adapter->WaitCases = CHECK_PHY_STATUS;
    if (!InWaitTimer)
    {
      WaitTimer.expires = RUN_AT (HZ / 10);
      add_timer (&WaitTimer);
      InWaitTimer = TRUE;
    }
    while (TimeOutCount > jiffies)
          ;
    adapter->WaitCases = NONE;
    if (!(PhyStatus_g & MII_STATUS_AUTO_DONE))
    {
      DBGPRINT_ERROR (("ConfigureMII: Autonegotiation not done\n"));
      adapter->Hardware.LinkState = LINK_DOWN_AT_INIT;
      return (FALSE);
    }
  }
  adapter->Hardware.LinkState = LINK_UP;
  return (TRUE);
}

/*
 * Since this function is called for forced xcvr configurations, it
 * assumes that the xcvr type has been verified as supported by the NIC.
 */
STATIC void CheckMIIAutoNegotiationStatus (struct NIC_INFORMATION *adapter)
{
  WORD PhyStatus, PhyAnar, PhyAnlpar;
  BOOL PhyResponding;

  /* Check to see if auto-negotiation has completed. Check the results in
   * the status registers.
   */
  PhyResponding = ReadMIIPhy (adapter, MII_PHY_STATUS, &PhyStatus);
  if (!PhyResponding)
  {
    DBGPRINT_ERROR (("CheckMIIAutoNegotiationStatus: Phy not responding (1)\n"));
    adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
    return;
  }

  if (PhyStatus & MII_STATUS_LINK_UP)
     return;   /* We have a valid link, so get out! */

  /* Check to see why auto-negotiation or parallel detection has failed.
   * We'll do this by comparing the advertisement registers between the
   * NIC and the link partner.
   */
  PhyResponding = ReadMIIPhy (adapter, MII_PHY_ANAR, &PhyAnar);
  if (!PhyResponding)
  {
    DBGPRINT_ERROR (("CheckMIIAutoNegotiationStatus: Phy not responding (2)\n"));
    adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
    return;
  }

  PhyResponding = ReadMIIPhy (adapter, MII_PHY_ANLPAR, &PhyAnlpar);
  if (!PhyResponding)
  {
    DBGPRINT_ERROR (("CheckMIIAutoNegotiationStatus: Phy not responding (3)\n"));
    adapter->Hardware.Status = HARDWARE_STATUS_FAILURE;
    return;
  }

  /* Now, compare what was advertised between the NIC and it's link partner.
   * If the media bits don't match, then write an error log entry.
   */
  if ((PhyAnar & MII_ANAR_MEDIA_MASK) != (PhyAnlpar & MII_ANAR_MEDIA_MASK))
     DBGPRINT_ERROR (("CheckMIIAutoNegotiationStatus: Incompatible configuration\n"));
}


/*
 * This routine checks if the EEPROM is busy
 */
STATIC DWORD CheckIfEEPROMBusy (struct NIC_INFORMATION *adapter)
{
  WORD  command = 0;
  DWORD count   = jiffies + HZ;
  do
  {
    command = NIC_READ_PORT_WORD (adapter, EEPROM_COMMAND_REGISTER);
    udelay (10);
  }
  while ((command & EEPROM_BUSY_BIT) && (count > jiffies));

  if (count < jiffies)
  {
    DBGPRINT_ERROR (("CheckIfEEPROMBusy: command timeout"));
    return (NIC_STATUS_FAILURE);
  }
  return (NIC_STATUS_SUCCESS);
}

/*
 * This routine reads from the EEPROM
 */
STATIC DWORD ReadEEPROM (struct NIC_INFORMATION *adapter,
                         WORD   EEPROMAddress,
                         WORD  *Contents)
{
  WORD lowerOffset = 0;
  WORD upperOffset = 0;

  if (EEPROMAddress > 0x003F)
  {
    lowerOffset = EEPROMAddress & 0x003F;
    upperOffset = (EEPROMAddress & 0x03C0) << 2;
    EEPROMAddress = upperOffset | lowerOffset;
  }

  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_0);

  if (CheckIfEEPROMBusy (adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("ReadEEPROM: EEPROM is busy\n"));
    return (NIC_STATUS_FAILURE);
  }

  /* Issue the read eeprom data command
   */
  NIC_WRITE_PORT_WORD (adapter, EEPROM_COMMAND_REGISTER, (WORD) (EEPROM_COMMAND_READ + (WORD) EEPROMAddress));

  if (CheckIfEEPROMBusy (adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("ReadEEPROM: EEPROM is busy after command.\n"));
    return (NIC_STATUS_FAILURE);
  }

  /* Save value read from eeprom
   */
  *Contents = NIC_READ_PORT_WORD (adapter, EEPROM_DATA_REGISTER);
  return (NIC_STATUS_SUCCESS);
}


/*
 * This routine writes to the EEPROM
 */
STATIC DWORD WriteEEPROM (struct NIC_INFORMATION *adapter,
                          WORD EEPROMAddress, WORD Data)
{
  WORD lowerOffset = 0;
  WORD upperOffset = 0;
  WORD saveAddress;

  saveAddress = EEPROMAddress;

  if (EEPROMAddress > 0x003F)
  {
    lowerOffset = EEPROMAddress & 0x003F;
    upperOffset = (EEPROMAddress & 0x03C0) << 2;
    EEPROMAddress = upperOffset | lowerOffset;
  }

  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_0);

  /* Issue erase register command prior to writing
   */
  NIC_WRITE_PORT_WORD (adapter, EEPROM_COMMAND_REGISTER, (WORD)EEPROM_WRITE_ENABLE);

  if (CheckIfEEPROMBusy (adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("WriteEEPROM: Write enable, EEPROM is busy\n"));
    return (NIC_STATUS_FAILURE);
  }

  NIC_WRITE_PORT_WORD (adapter, EEPROM_COMMAND_REGISTER,
                       (WORD)(EEPROM_ERASE_REGISTER + (WORD)EEPROMAddress));

  if (CheckIfEEPROMBusy (adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("WriteEEPROM: Erase Register, EEPROM is busy\n"));
    return (NIC_STATUS_FAILURE);
  }

  /* Load data to be written to the eeprom
   */
  NIC_WRITE_PORT_WORD (adapter, EEPROM_DATA_REGISTER, Data);

  DBGPRINT_INITIALIZE (("WriteEeprom: Writing value %x at %x\n",
                        Data, saveAddress));

  if (CheckIfEEPROMBusy (adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("WriteEEPROM: Write data, EEPROM is busy\n"));
    return (NIC_STATUS_FAILURE);
  }
   
  /* Issue the write eeprom data command
   */
  NIC_WRITE_PORT_WORD (adapter, EEPROM_COMMAND_REGISTER, (WORD)EEPROM_WRITE_ENABLE);

  if (CheckIfEEPROMBusy (adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("WriteEEPROM: EEPROM is busy\n"));
    return (NIC_STATUS_FAILURE);
  }

  NIC_WRITE_PORT_WORD (adapter, EEPROM_COMMAND_REGISTER,
                       (WORD)(EEPROM_WRITE_REGISTER + (WORD)EEPROMAddress));

  if (CheckIfEEPROMBusy (adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR (("WriteEEPROM: Write register, EEPROM is busy\n"));
    return (NIC_STATUS_FAILURE);
  }
  return (NIC_STATUS_SUCCESS);
}

 
/*
 * Calculates the EEPROM checksum #1 from offset 0 to 0x1F.
 */
STATIC WORD CalculateEEPROMChecksum1 (struct NIC_INFORMATION *adapter)
{
  DWORD status;
  WORD  checksum = 0;
  WORD  value = 0;
  BYTE  index;

  DBGPRINT_FUNCTION (("CalculateChecksum1: IN\n"));

  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_0);

  for (index = EEPROM_NODE_ADDRESS_WORD_0; index < EEPROM_CHECKSUM_1; index++)
  {
    status = ReadEEPROM (adapter, index, &value);
    if (status == NIC_STATUS_FAILURE)
    {                                
      DBGPRINT_ERROR (("CalculateEEPROMChecksum1: Read failure\n"));
      return (NIC_STATUS_FAILURE);
    }
    checksum ^= (WORD) loBYTE (value);
    checksum ^= (WORD) hiBYTE (value);
  }

  DBGPRINT_FUNCTION (("CalculateChecksum1: OUT\n"));
  return ((WORD)checksum);
}


STATIC DWORD SoftwareWork (struct NIC_INFORMATION *adapter)
{
  WORD  SoftwareInformation2 = 0;
  DWORD DmaControl = 0;
  WORD  NetDiag = 0;
  WORD  Contents = 0;

  DBGPRINT_FUNCTION (("SoftwareWork: IN\n"));

  /* Additional work#1
   */
  ReadEEPROM (adapter, EEPROM_SOFTWARE_INFORMATION_2, &Contents);

  if (!(Contents & ENABLE_MWI_WORK))
  {
    DmaControl = NIC_READ_PORT_DWORD (adapter, DMA_CONTROL_REGISTER);
    NIC_WRITE_PORT_DWORD (adapter, DMA_CONTROL_REGISTER,
                          (DWORD)(DMA_CONTROL_DEFEAT_MWI | DmaControl));
  }

  /* Additional work#2
   */
  NIC_COMMAND (adapter, COMMAND_SELECT_REGISTER_WINDOW | REGISTER_WINDOW_4);

  NetDiag = NIC_READ_PORT_WORD (adapter, NETWORK_DIAGNOSTICS_REGISTER);

  if ((((NetDiag & NETWORK_DIAGNOSTICS_ASIC_REVISION) >> 4) == 1) &&
      (((NetDiag & NETWORK_DIAGNOSTICS_ASIC_REVISION_LOW) >> 1) < 4))
  {
    adapter->Hardware.HurricaneEarlyRevision = TRUE;
    DBGPRINT_INITIALIZE (("Hurricane Early board\n"));
    HurricaneEarlyRevision (adapter);
  }

  SoftwareInformation2 = 0;
  ((struct SOFTWARE_INFORMATION_2*) &SoftwareInformation2)->D3Work = 1;

  if (Contents & SoftwareInformation2)
  {
    DBGPRINT_INITIALIZE (("Enable D3 work\n"));
    adapter->Hardware.D3Work = TRUE;
  }

  /* Additional work#3
   */
  adapter->Hardware.DontSleep = FALSE;

  SoftwareInformation2 = 0;
  ((struct SOFTWARE_INFORMATION_2*) &SoftwareInformation2)->WOLConnectorPresent = 1;
  ((struct SOFTWARE_INFORMATION_2*) &SoftwareInformation2)->AutoResetToD0 = 1;

  DBGPRINT_INITIALIZE (("SoftwareInfo2 : %x\n", SoftwareInformation2));

  if (!(Contents & SoftwareInformation2))
  {
    DBGPRINT_INITIALIZE (("Don't sleep is TRUE\n"));
    adapter->Hardware.DontSleep = TRUE;
  }

  DBGPRINT_FUNCTION (("SoftwareWork: OUT\n"));
  return (NIC_STATUS_SUCCESS);
}

STATIC void FlowControl (struct NIC_INFORMATION *adapter)
{
  WORD PhyAnar, PhyControl;
  BOOL PhyResponding = ReadMIIPhy (adapter, MII_PHY_ANAR, &PhyAnar);

  PhyAnar |= MII_ANAR_FLOWCONTROL;
  WriteMIIPhy (adapter, MII_PHY_ANAR, PhyAnar);

  PhyResponding = ReadMIIPhy (adapter, MII_PHY_CONTROL, &PhyControl);
  PhyControl |= MII_CONTROL_START_AUTO;
  WriteMIIPhy (adapter, MII_PHY_CONTROL, PhyControl);
}

STATIC void HurricaneEarlyRevision (struct NIC_INFORMATION *adapter)
{
  WORD PhyRegisterValue;
  BOOL PhyResponding = ReadMIIPhy (adapter, MII_PHY_REGISTER_24, &PhyRegisterValue);

  if (!PhyResponding)
     DBGPRINT_ERROR (("CaneRev-ReadMIIPhy: Phy not responding\n"));

  PhyRegisterValue |= MII_PHY_REGISTER_24_PVCIRC;

  WriteMIIPhy (adapter, MII_PHY_REGISTER_24, PhyRegisterValue);
}

#ifdef NOT_USED
STATIC DWORD RxResetAndWork (struct NIC_INFORMATION *adapter)
{
  DWORD nicStatus = NIC_COMMAND_WAIT (adapter, COMMAND_RX_RESET);

  if (nicStatus != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR(("RxResetAndWork: Rx reset failed\n"));
    return (NIC_STATUS_FAILURE);
  }
  /* not all work needed after Rx reset. Refine later
   */
  if (SoftwareWork(adapter) != NIC_STATUS_SUCCESS)
  {
    DBGPRINT_ERROR(("RxResetAndWork: SoftwareWork failed\n"));
    return (NIC_STATUS_FAILURE);
  }
  return (NIC_STATUS_SUCCESS);
}
#endif


#define MAX_UNITS 8


static int switchdelay[]           = { 0, 0, 0, 0, 0, 0, 0, 0 };
static int media_select[MAX_UNITS] = { MEDIA_NONE, };
static int flowcontrol[MAX_UNITS]  = { 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1 };
static int downpoll[MAX_UNITS]     = { 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8 };
static int full_duplex[MAX_UNITS]  = { -1, -1, -1, -1, -1, -1, -1, -1 };

static DWORD tc90x_SendCount[]    = { 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40 };
static DWORD tc90x_ReceiveCount[] = { 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40 };

STATIC DWORD ReadCommandLineChanges (struct NIC_INFORMATION *adapter)
{
  int  index = adapter->Index;
  WORD mediatype;

  DBGPRINT_INIT (("ReadCommandLineChanges: IN index=%x\n", index));

  if (tc90x_SendCount[index] < NIC_MINIMUM_SEND_COUNT ||
      tc90x_SendCount[index] > NIC_MAXIMUM_SEND_COUNT)
  {
    DBGPRINT_ERROR (("ProcessOverride: Using default value\n"));
    adapter->Resources.SendCount = NIC_DEFAULT_SEND_COUNT;
  }
  else
  {
    DBGPRINT_INITIALIZE (("SendCount = %x\n", (int)tc90x_SendCount[index]));
    adapter->Resources.SendCount = tc90x_SendCount[index];
  }

  if (tc90x_ReceiveCount[index] < NIC_MINIMUM_RECEIVE_COUNT ||
      tc90x_ReceiveCount[index] > NIC_MAXIMUM_RECEIVE_COUNT)
  {
    DBGPRINT_ERROR (("ReadCommandLineChanges: Using default receive\n"));
    adapter->Resources.ReceiveCount = NIC_DEFAULT_RECEIVE_COUNT;
  }
  else
  {
    adapter->Resources.ReceiveCount = tc90x_ReceiveCount[index];
  }

  DBGPRINT_INITIALIZE (("ReceiveCount = %d\n", (int)adapter->Resources.ReceiveCount));

  if ((index < MAX_UNITS) && (flowcontrol[index] <= 0))
  {
    DBGPRINT_INITIALIZE (("User disables FlowControl\n"));
    adapter->Hardware.FlowControlSupported = FALSE;
    adapter->Hardware.FlowControlEnable = FALSE;
  }
  else
  {
    DBGPRINT_INITIALIZE (("FlowControl is enabled by default\n"));
    adapter->Hardware.FlowControlSupported = TRUE;
    adapter->Hardware.FlowControlEnable = TRUE;
  }

  if ((index < MAX_UNITS) && (downpoll[index] == 0x40))
  {
    DBGPRINT_INITIALIZE (("DownPollRate is 64\n"));
    adapter->Resources.DownPollRate = 0x40;
  }
  else
  {
    DBGPRINT_INITIALIZE (("DownPollRate is 8 by default\n"));
    adapter->Resources.DownPollRate = 0x8;
  }

  if ((index < MAX_UNITS) && (switchdelay[index] > 0))
  {
    DBGPRINT_INITIALIZE (("User enables delay for switch\n"));
    adapter->DelayStart = TRUE;
  }
  else
    adapter->DelayStart = FALSE;

  /* Possible media selections:
   * MEDIA_NONE  0
   * MEDIA_10BASE_T  1
   * MEDIA_10AUI  2
   * MEDIA_10BASE_2  3
   * MEDIA_100BASE_TX 4
   * MEDIA_100BASE_FX 5
   * MEDIA_10BASE_FL  6
   * MEDIA_AUTO_SELECT 7
   */
  if (index < MAX_UNITS)
       mediatype = media_select[index];
  else mediatype = MEDIA_NONE;

  if ((mediatype > 0) && (mediatype <= 7))
  {
    adapter->Hardware.MediaOverride = mediatype;
    DBGPRINT_INITIALIZE (("User selects Media = %i\n", mediatype));
  }
  else
    adapter->Hardware.MediaOverride = MEDIA_NONE;

  if (adapter->Hardware.MediaOverride != MEDIA_AUTO_SELECT)
  {
    if ((index < MAX_UNITS) && (full_duplex[index] > 0))
    {
      DBGPRINT_INITIALIZE (("NotAutoSelect: User enables full duplex\n"));
      if ((adapter->Hardware.MediaOverride != MEDIA_10AUI) &&
          (adapter->Hardware.MediaOverride != MEDIA_10BASE_2))
         adapter->Hardware.FullDuplexEnable = TRUE;
      adapter->Hardware.DuplexCommandOverride = TRUE;
    }
    else if ((index < MAX_UNITS) && (full_duplex[index] == 0))
    {
      DBGPRINT_INITIALIZE (("NotAutoSelect: User disables full duplex\n"));
      adapter->Hardware.FullDuplexEnable = FALSE;
      adapter->Hardware.DuplexCommandOverride = TRUE;
    }
    else
      adapter->Hardware.DuplexCommandOverride = FALSE;
  }

  DBGPRINT_INITIALIZE (("ReadCommandLineChanges: OUT\n"));
  return (NIC_STATUS_SUCCESS);
}
