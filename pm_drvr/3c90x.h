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
 */

#ifndef __3C90X_H
#define __3C90X_H

extern DWORD tc90xbc_debug;
extern int   tc90xbc_probe (struct device *dev);

#ifdef IMPLEMENT_3C90X

#define BIT_0       (1 << 0)
#define BIT_1       (1 << 1)
#define BIT_2       (1 << 2)
#define BIT_3       (1 << 3)
#define BIT_4       (1 << 4)
#define BIT_5       (1 << 5)
#define BIT_6       (1 << 6)
#define BIT_7       (1 << 7)
#define BIT_8       (1 << 8)
#define BIT_9       (1 << 9)
#define BIT_10      (1 << 10)
#define BIT_11      (1 << 11)
#define BIT_12      (1 << 12)
#define BIT_13      (1 << 13)
#define BIT_14      (1 << 14)
#define BIT_15      (1 << 15)
#define BIT_16      (1 << 16)
#define BIT_17      (1 << 17)
#define BIT_18      (1 << 18)
#define BIT_19      (1 << 19)
#define BIT_20      (1 << 20)
#define BIT_21      (1 << 21)
#define BIT_22      (1 << 22)
#define BIT_23      (1 << 23)
#define BIT_24      (1 << 24)
#define BIT_25      (1 << 25)
#define BIT_26      (1 << 26)
#define BIT_27      (1 << 27)
#define BIT_28      (1 << 28)
#define BIT_29      (1 << 29)
#define BIT_30      (1 << 30)
#define BIT_31      (1 << 31)

#define FALSE  0
#define TRUE   1

/*
 * 3Com Node Address
 */
#define EEPROM_NODE_ADDRESS_WORD_0		0x00
#define EEPROM_NODE_ADDRESS_WORD_1		0x01
#define EEPROM_NODE_ADDRESS_WORD_2		0x02

#define EEPROM_DEVICE_ID			0x03
/*
 * Possible values:
 * 
 * 0x9055 - PCI 10/100 Mbps; shared 10BASE-T/100BASE-TX connector.
 * 0x9056 - PCI 10/100 Mbps; shared 10BASE-T/100BASE-T4 connector.
 * 0x9004 - PCI 10BASE-T (TPO)
 * 0x9005 - PCI 10BASE-T/10BASE-2/AUI(COMBO)
 * 0x9006 - PCI 10BASE-T/10BASE-2/(TPC)
 */

/*
 * OEM Node address
 */
#define EEPROM_OEM_NODE_ADDRESS_WORD_0		0x0A
#define EEPROM_OEM_NODE_ADDRESS_WORD_1		0x0B
#define EEPROM_OEM_NODE_ADDRESS_WORD_2		0x0C

#define EEPROM_SOFTWARE_INFORMATION_1		0x0D

typedef struct SOFTWARE_INFORMATION_1 {
        WORD Reserved1:4;
        WORD OptimizeFor:2;
#define EEPROM_OPTIMIZE_NORMAL              0x1
#define EEPROM_OPTIMIZE_FOR_THROUGHPUT      0x2
#define EEPROM_OPTIMIZE_FOR_CPU             0x3

        WORD Reserved2:8;
        WORD LinkBeatDisable:1;
#define EEPROM_DISABLE_FULL_DUPLEX          0x00
#define EEPROM_ENABLE_FULL_DUPLEX           0x01

        WORD FullDuplexMode:1;
      } SOFTWARE_INFORMATION_1;

#define EEPROM_COMPATABILITY_WORD           0x0E
#define EEPROM_COMPATABILITY_LEVEL          0x00

typedef struct COMPATABILITY_WORD {
        WORD WarningLevel:8;
        WORD FailureLevel:8;
      } COMPATABILITY_WORD;

#define EEPROM_SOFTWARE_INFORMATION_2       0x0F
#define ENABLE_MWI_WORK                     0x0020

typedef struct SOFTWARE_INFORMATION_2 {
        WORD Reserved1:1;
        WORD BroadcastRxErrDone:1;
        WORD EncoderDecoderLoopBackErrDone:1;
        WORD WOLConnectorPresent:1;
        WORD PMEPulsed:1;
        WORD MWIErrDone:1;
        WORD AutoResetToD0:1;
        WORD D3Work:1;
      } SOFTWARE_INFORMATION_2;

#define EEPROM_CAPABILITIES_WORD            0x10

typedef struct CAPABILITIES_WORD {
        WORD SupportsPlugNPlay:1;
        WORD SupportsFullDuplex:1;
        WORD SupportsLargePackets:1;
        WORD SupportsSlaveDMA:1;
        WORD SupportsSecondDMA:1;
        WORD SupportsFullBusMaster:1;
        WORD SupportsFragBusMaster:1;
        WORD SupportsCRCPassThrough:1;
        WORD SupportsTxDone:1;
        WORD SupportsNoTxLength:1;
        WORD SupportsRxRepeat:1;
        WORD Supports100Mbps:1;
        WORD SupportsPowerManagement:1;
      } CAPABILITIES_WORD;

#define EEPROM_RESERVED_LOCATION		0x11
#define EEPROM_INTERNAL_CONFIG_WORD_0		0x12
#define EEPROM_INTERNAL_CONFIG_WORD_1		0x13
#define EEPROM_ANALOG_DIAGNOSTICS		0x14
#define EEPROM_SOFTWARE_INFORMATION_3		0x15

typedef struct SOFTWARE_INFORMATION_3 {
        WORD ForceXcvr:4;
#define EEPROM_GENERIC_MII                  0x00
#define EEPROM_100BASE_T4_MII               0x01
#define EEPROM_10BASE_T_MII                 0x02
#define EEPROM_100BASE_TX_MII               0x03
#define EEPROM_10_BASE_T_AUTONEGOTIATION    0x04
#define EEPROM_100_BASE_TX_AUTONEGOTIATION  0x04
        WORD Reserved:12;
      } SOFTWARE_INFORMATION_3;

/* Locations 0x1E - 0x1F are reserved.
 */
#define EEPROM_CHECKSUM_1			0x20

/* Locations 0x21 - 0x2F are reserved.
 */
#define EEPROM_SOS_PINS_1_TO_4			0x21
#define EEPROM_SOS_PINS_5_TO_7			0x22

/* Locations 0x00 - 0xFD are flexible format locations (4kb EEPROMs)
 */
#define EEPROM_CHECKSUM_2_UPPER			0xFE
#define EEPROM_CHECKSUM_2_LOWER			0xFF

/* Locations 0x00 - 0x3FD are flexible format locations (16Kb EEPROMs)
 */
#define EEPROM_CHECKSUM_3_UPPER                 0x3FE
#define EEPROM_CHECKSUM_3_LOWER                 0x3FF
#define EEPROM_COMMAND_MASK			0xE000
#define EEPROM_COMMAND_AUTOINIT_DONE		0xE000
#define EEPROM_COMMAND_PCI_CONFIG_WRITE		0xA000
#define EEPROM_COMMAND_REGISTER_WRITE		0x6000
#define EEPROM_COMMAND_TX_FIFO_WRITE		0x2000
#define EEPROM_CURRENT_WINDOW_MASK		0x7000
#define EEPROM_ADDRESS_MASK			0x00FF
#define EEPROM_TX_BYTE_COUNT			0x03FF
#define EEPROM_FLEXIBLE_FORMAT_START		0x40
#define EEPROM_WORD_ACCESS			0x1000
#define MAX_FLEX_EEPROM_SIZE			2048

#define EEPROM_WINDOW_0				(0x0 << 0x8)
#define EEPROM_WINDOW_1				(0x1 << 0x8)
#define EEPROM_WINDOW_2				(0x2 << 0x8)
#define EEPROM_WINDOW_3				(0x3 << 0x8)
#define EEPROM_WINDOW_4				(0x4 << 0x8)
#define EEPROM_WINDOW_5				(0x5 << 0x8)
#define EEPROM_WINDOW_6				(0x6 << 0x8)
#define EEPROM_WINDOW_7				(0x7 << 0x8)

#define PCI_POWER_CONTROL   0xE0
#define PCI_PME_ENABLE      0x0100
#define PCI_PME_STATUS      0x8000

#define PCI_POWER_STATE_D0  0x00
#define PCI_POWER_STATE_D1  0x01
#define PCI_POWER_STATE_D2  0x02
#define PCI_POWER_STATE_D3  0x03

/*
 * Supported PCI device id's
 */
#define NIC_VENDOR_ID				0x10B7
#define NIC_PCI_DEVICE_ID_9055			0x9055
#define NIC_PCI_DEVICE_ID_9056			0x9056
#define NIC_PCI_DEVICE_ID_9058			0x9058
#define NIC_PCI_DEVICE_ID_9004			0x9004
#define NIC_PCI_DEVICE_ID_9005			0x9005
#define NIC_PCI_DEVICE_ID_9006			0x9006
#define NIC_PCI_DEVICE_ID_900A			0x900A
#define NIC_PCI_DEVICE_ID_905A			0x905A
#define NIC_PCI_DEVICE_ID_9200			0x9200
#define NIC_PCI_DEVICE_ID_9800			0x9800
#define NIC_PCI_DEVICE_ID_9805			0x9805
#define NIC_PCI_DEVICE_ID_4500			0x4500
#define NIC_PCI_DEVICE_ID_7646			0x7646

/* ASIC versions.
 */
#define NIC_ASIC_CYCLONE_KRAKATOA_LUCENT        0x0
#define NIC_ASIC_HURRICANE_TORNADO_LUCENT       0x1
#define NIC_ASIC_HURRICANE_NATIONAL             0x2
#define NIC_ASIC_HURRICANE_TORNADO_BROADCOM     0x3

/* Window definitions.
 */
#define REGISTER_WINDOW_0               0x0  /* setup/configuration */
#define REGISTER_WINDOW_1               0x1  /* operating set */
#define REGISTER_WINDOW_2               0x2  /* station address setup/read */
#define REGISTER_WINDOW_3               0x3  /* FIFO management */
#define REGISTER_WINDOW_4               0x4  /* diagnostics */
#define REGISTER_WINDOW_5               0x5  /* registers set by commands */
#define REGISTER_WINDOW_6               0x6  /* statistics */
#define REGISTER_WINDOW_7               0x7  /* bus master control */
#define REGISTER_WINDOW_MASK		0xE000

/*
 * Register definitions
 */
#define INTSTATUS_INTERRUPT_MASK	0x6EE

/* Window 0 registers.
 */
#define BIOS_ROM_ADDRESS_REGISTER	0x4
#define BIOS_ROM_DATA_REGISTER		0x8

#define EEPROM_COMMAND_REGISTER		0xA
#define EEPROM_BUSY_BIT			BIT_15
#define EEPROM_COMMAND_READ		0x0080
#define EEPROM_WRITE_ENABLE		0x0030
#define EEPROM_ERASE_REGISTER		0x00C0
#define EEPROM_WRITE_REGISTER		0x0040

#define EEPROM_DATA_REGISTER		0xC

#define INTSTATUS_COMMAND_REGISTER	0xE
#define INTSTATUS_INTERRUPT_LATCH	BIT_0
#define INTSTATUS_HOST_ERROR		BIT_1
#define INTSTATUS_TX_COMPLETE		BIT_2
#define INTSTATUS_RX_COMPLETE		BIT_4
#define INTSTATUS_INTERRUPT_REQUESTED	BIT_6
#define INTSTATUS_UPDATE_STATISTICS	BIT_7
#define INTSTATUS_LINK_EVENT		BIT_8
#define INTSTATUS_DOWN_COMPLETE		BIT_9
#define INTSTATUS_UP_COMPLETE		BIT_10
#define INTSTATUS_COMMAND_IN_PROGRESS	BIT_12
#define INTSTATUS_INTERRUPT_NONE	0
#define INTSTATUS_INTERRUPT_ALL		0x6EE
#define INTSTATUS_ACKNOWLEDGE_ALL	0x7FF


/* Window 2 registers.
 */
#define STATION_ADDRESS_LOW_REGISTER		0x0
#define STATION_ADDRESS_MID_REGISTER		0x2
#define STATION_ADDRESS_HIGH_REGISTER		0x4

/* Window 3 registers.
 */
#define INTERNAL_CONFIG_REGISTER		0x0
#define INTERNAL_CONFIG_DISABLE_BAD_SSD		BIT_8
#define INTERNAL_CONFIG_ENABLE_TX_LARGE		BIT_14
#define INTERNAL_CONFIG_ENABLE_RX_LARGE		BIT_15
#define INTERNAL_CONFIG_AUTO_SELECT		BIT_24
#define INTERNAL_CONFIG_DISABLE_ROM		BIT_25
#define INTERNAL_CONFIG_TRANSCEIVER_MASK	0x00F00000L

#define MAXIMUM_PACKET_SIZE_REGISTER		0x4

#define MAC_CONTROL_REGISTER			0x6
#define MAC_CONTROL_FULL_DUPLEX_ENABLE		BIT_5
#define MAC_CONTROL_ALLOW_LARGE_PACKETS		BIT_6
#define MAC_CONTROL_FLOW_CONTROL_ENABLE 	BIT_8
#define MEDIA_OPTIONS_REGISTER			0x8
#define MEDIA_OPTIONS_100BASET4_AVAILABLE	BIT_0
#define MEDIA_OPTIONS_100BASETX_AVAILABLE	BIT_1
#define MEDIA_OPTIONS_100BASEFX_AVAILABLE	BIT_2
#define MEDIA_OPTIONS_10BASET_AVAILABLE		BIT_3
#define MEDIA_OPTIONS_10BASE2_AVAILABLE		BIT_4
#define MEDIA_OPTIONS_10AUI_AVAILABLE		BIT_5
#define MEDIA_OPTIONS_MII_AVAILABLE		BIT_6
#define MEDIA_OPTIONS_10BASEFL_AVAILABLE	BIT_8

#define RX_FREE_REGISTER			0xA
#define TX_FREE_REGISTER			0xC

/* Window 4 registers.
 */
#define PHYSICAL_MANAGEMENT_REGISTER		0x8
#define NETWORK_DIAGNOSTICS_REGISTER		0x6
#define NETWORK_DIAGNOSTICS_ASIC_REVISION	0x003E
#define NETWORK_DIAGNOSTICS_ASIC_REVISION_LOW  	0x000E
#define NETWORK_DIAGNOSTICS_UPPER_BYTES_ENABLE 	BIT_6
#define MEDIA_STATUS_REGISTER                   0xA
#define MEDIA_STATUS_SQE_STATISTICS_ENABLE	BIT_3
#define MEDIA_STATUS_CARRIER_SENSE		BIT_5
#define MEDIA_STATUS_JABBER_GUARD_ENABLE	BIT_6
#define MEDIA_STATUS_LINK_BEAT_ENABLE		BIT_7
#define MEDIA_STATUS_LINK_DETECT		BIT_11
#define MEDIA_STATUS_TX_IN_PROGRESS		BIT_12
#define MEDIA_STATUS_DC_CONVERTER_ENABLED	BIT_14
#define BAD_SSD_REGISTER			0xC
#define UPPER_BYTES_OK_REGISTER			0xD

/* Window 5 registers.
 */
#define RX_FILTER_REGISTER		0x8
#define INTERRUPT_ENABLE_REGISTER	0xA
#define INDICATION_ENABLE_REGISTER	0xC

/* Window 6 registers.
 */
#define CARRIER_LOST_REGISTER		0x0
#define SQE_ERRORS_REGISTER		0x1
#define MULTIPLE_COLLISIONS_REGISTER	0x2
#define SINGLE_COLLISIONS_REGISTER	0x3
#define LATE_COLLISIONS_REGISTER	0x4
#define RX_OVERRUNS_REGISTER		0x5
#define FRAMES_TRANSMITTED_OK_REGISTER	0x6
#define FRAMES_RECEIVED_OK_REGISTER	0x7
#define FRAMES_DEFERRED_REGISTER	0x8
#define UPPER_FRAMES_OK_REGISTER	0x9
#define BYTES_RECEIVED_OK_REGISTER	0xA
#define BYTES_TRANSMITTED_OK_REGISTER	0xC

/* Window 7 registers.
 */
#define TIMER_REGISTER			0x1A
#define TX_STATUS_REGISTER		0x1B
#define TX_STATUS_MAXIMUM_COLLISION	BIT_3
#define TX_STATUS_HWERROR		BIT_4
#define TX_STATUS_JABBER		BIT_5
#define TX_STATUS_INTERRUPT_REQUESTED	BIT_6
#define TX_STATUS_COMPLETE		BIT_7
#define INT_STATUS_AUTO_REGISTER	0x1E
#define DMA_CONTROL_REGISTER		0x20
#define DMA_CONTROL_DOWN_STALLED	BIT_2
#define DMA_CONTROL_UP_COMPLETE		BIT_3
#define DMA_CONTROL_DOWN_COMPLETE	BIT_4

#define DMA_CONTROL_ARM_COUNTDOWN       BIT_6
#define DMA_CONTROL_DOWN_IN_PROGRESS    BIT_7
#define DMA_CONTROL_COUNTER_SPEED       BIT_8
#define DMA_CONTROL_COUNTDOWN_MODE      BIT_9
#define DMA_CONTROL_DOWN_SEQ_DISABLE    BIT_17
#define DMA_CONTROL_DEFEAT_MWI          BIT_20
#define DMA_CONTROL_DEFEAT_MRL          BIT_21
#define DMA_CONTROL_UPOVERDISC_DISABLE	BIT_22
#define DMA_CONTROL_TARGET_ABORT        BIT_30
#define DMA_CONTROL_MASTER_ABORT        BIT_31

#define DOWN_LIST_POINTER_REGISTER              0x24
#define DOWN_POLL_REGISTER                      0x2D
#define UP_PACKET_STATUS_REGISTER		0x30
#define UP_PACKET_STATUS_ERROR			BIT_14
#define UP_PACKET_STATUS_COMPLETE		BIT_15
#define UP_PACKET_STATUS_OVERRUN		BIT_16
#define UP_PACKET_STATUS_RUNT_FRAME		BIT_17
#define UP_PACKET_STATUS_ALIGNMENT_ERROR	BIT_18
#define UP_PACKET_STATUS_CRC_ERROR             	BIT_19
#define UP_PACKET_STATUS_OVERSIZE_FRAME        	BIT_20
#define UP_PACKET_STATUS_DRIBBLE_BITS		BIT_23
#define UP_PACKET_STATUS_OVERFLOW		BIT_24
#define UP_PACKET_STATUS_IP_CHECKSUM_ERROR	BIT_25
#define UP_PACKET_STATUS_TCP_CHECKSUM_ERROR	BIT_26
#define UP_PACKET_STATUS_UDP_CHECKSUM_ERROR	BIT_27
#define UP_PACKET_STATUS_IMPLIED_BUFFER_ENABLE	BIT_28
#define UP_PACKET_STATUS_IP_CHECKSUM_CHECKED	BIT_29
#define UP_PACKET_STATUS_TCP_CHECKSUM_CHECKED	BIT_30
#define UP_PACKET_STATUS_UDP_CHECKSUM_CHECKED	BIT_31
#define UP_PACKET_STATUS_ERROR_MASK		0x1F0000
#define FREE_TIMER_REGISTER			0x34
#define COUNTDOWN_REGISTER			0x36
#define UP_LIST_POINTER_REGISTER		0x38
#define UP_POLL_REGISTER			0x3D
#define REAL_TIME_COUNTER_REGISTER		0x40
#define CONFIG_ADDRESS_REGISTER			0x44
#define CONFIG_DATA_REGISTER			0x48
#define DEBUG_DATA_REGISTER			0x70
#define DEBUG_CONTROL_REGISTER			0x74


/*
 * Commands
 */

/* Global reset command.
 */
#define COMMAND_GLOBAL_RESET		(0x0 << 0xB)
#define GLOBAL_RESET_MASK_TP_AUI_RESET	BIT_0
#define GLOBAL_RESET_MASK_ENDEC_RESET   BIT_1
#define GLOBAL_RESET_MASK_NETWORK_RESET	BIT_2
#define GLOBAL_RESET_MASK_FIFO_RESET    BIT_3
#define GLOBAL_RESET_MASK_AISM_RESET    BIT_4
#define GLOBAL_RESET_MASK_HOST_RESET	BIT_5
#define GLOBAL_RESET_MASK_SMB_RESET     BIT_6
#define GLOBAL_RESET_MASK_VCO_RESET     BIT_7
#define GLOBAL_RESET_MASK_UP_DOWN_RESET BIT_8

#define COMMAND_SELECT_REGISTER_WINDOW	(0x1 << 0xB)
#define COMMAND_ENABLE_DC_CONVERTER	(0x2 << 0xB)
#define COMMAND_RX_DISABLE		(0x3 << 0xB)
#define COMMAND_RX_ENABLE		(0x4 << 0xB)

/* Receiver reset command.
 */
#define COMMAND_RX_RESET		(0x5 << 0xB)
#define RX_RESET_MASK_TP_AUI_RESET	BIT_0
#define RX_RESET_MASK_ENDEC_RESET	BIT_1
#define RX_RESET_MASK_NETWORK_RESET	BIT_2
#define RX_RESET_MASK_FIFO_RESET	BIT_3
#define RX_RESET_MASK_UP_RESET		BIT_8

#define COMMAND_UP_STALL		((0x6 << 0xB) | 0x0)
#define COMMAND_UP_UNSTALL		((0x6 << 0xB) | 0x1)
#define COMMAND_DOWN_STALL		((0x6 << 0xB) | 0x2)
#define COMMAND_DOWN_UNSTALL		((0x6 << 0xB) | 0x3)
#define COMMAND_TX_DONE			(0x7 << 0xB)
#define COMMAND_RX_DISCARD		(0x8 << 0xB)
#define COMMAND_TX_ENABLE		(0x9 << 0xB)
#define COMMAND_TX_DISABLE		(0xA << 0xB)

/* Transmitter reset command.
 */
#define COMMAND_TX_RESET		(0xB << 0xB)
#define TX_RESET_MASK_TP_AUI_RESET	BIT_0
#define TX_RESET_MASK_ENDEC_RESET	BIT_1
#define TX_RESET_MASK_NETWORK_RESET	BIT_2
#define TX_RESET_MASK_FIFO_RESET	BIT_3
#define TX_RESET_MASK_DOWN_RESET	BIT_8
#define COMMAND_REQUEST_INTERRUPT	(0xC << 0xB)

/* Interrupt acknowledge command.
 */
#define COMMAND_ACKNOWLEDGE_INTERRUPT	(0xD << 0xB)
#define ACKNOWLEDGE_INTERRUPT_LATCH	BIT_0
#define ACKNOWLEDGE_LINK_EVENT		BIT_1
#define ACKNOWLEDGE_INTERRUPT_REQUESTED	BIT_6
#define ACKNOWLEDGE_DOWN_COMPLETE	BIT_9
#define ACKNOWLEDGE_UP_COMPLETE		BIT_10
#define ACKNOWLEDGE_ALL_INTERRUPT	0x7FF
#define COMMAND_SET_INTERRUPT_ENABLE	(0xE << 0xB)
#define DISABLE_ALL_INTERRUPT		0x0
#define ENABLE_ALL_INTERRUPT		0x6EE
#define COMMAND_SET_INDICATION_ENABLE	(0xF << 0xB)

/* Receive filter command.
 */
#define COMMAND_SET_RX_FILTER		(0x10 << 0xB)
#define RX_FILTER_INDIVIDUAL            BIT_0
#define RX_FILTER_ALL_MULTICAST         BIT_1
#define RX_FILTER_BROADCAST             BIT_2
#define RX_FILTER_PROMISCUOUS           BIT_3
#define RX_FILTER_MULTICAST_HASH        BIT_4
#define COMMAND_TX_AGAIN		(0x13 << 0xB)
#define COMMAND_STATISTICS_ENABLE	(0x15 << 0xB)
#define COMMAND_STATISTICS_DISABLE	(0x16 << 0xB)
#define COMMAND_DISABLE_DC_CONVERTER	(0x17 << 0xB)
#define COMMAND_SET_HASH_FILTER_BIT	(0x19 << 0xB)
#define COMMAND_TX_FIFO_BISECT		(0x1B << 0xB)

/*
 * Adapter limits
 */
#define TRANSMIT_FIFO_SIZE         0x800
#define RECEIVE_FIFO_SIZE          0x800

/* Ethernet limits.
 */
#define ETHERNET_MAXIMUM_FRAME_SIZE	1514
#define ETHERNET_MINIMUM_FRAME_SIZE	60
#define ETHERNET_ADDRESS_SIZE		6
#define ETHERNET_HEADER_SIZE		14

/* Flow Control Address that gets put into the hash filter for
 * flow control enable
 */
#define NIC_FLOW_CONTROL_ADDRESS_0	0x01
#define NIC_FLOW_CONTROL_ADDRESS_1	0x80
#define NIC_FLOW_CONTROL_ADDRESS_2	0xC2
#define NIC_FLOW_CONTROL_ADDRESS_3	0x00
#define NIC_FLOW_CONTROL_ADDRESS_4	0x00
#define NIC_FLOW_CONTROL_ADDRESS_5	0x01

/* DPD Frame Start header bit definitions.
 */
#define FSH_CRC_APPEND_DISABLE		BIT_13
#define FSH_TX_INDICATE			BIT_15
#define FSH_DOWN_COMPLETE		BIT_16
#define FSH_LAST_KEEP_ALIVE_PACKET	BIT_24
#define FSH_ADD_IP_CHECKSUM		BIT_25
#define FSH_ADD_TCP_CHECKSUM		BIT_26
#define FSH_ADD_UDP_CHECKSUM		BIT_27
#define FSH_ROUND_UP_DEFEAT		BIT_28
#define FSH_DPD_EMPTY			BIT_29
#define FSH_DOWN_INDICATE		BIT_31
#define MAXIMUM_SCATTER_GATHER_LIST     0x10

/* Scatter Gather entry defintion.
 */
typedef struct SCATTER_GATHER_ENTRY {
        DWORD  Address;
        DWORD  Count;
      } SCATTER_GATHER_ENTRY;

typedef struct DPD_LIST_ENTRY {
        DWORD                       DownNextPointer;
        DWORD                       FrameStartHeader;
        struct SCATTER_GATHER_ENTRY SGList[MAXIMUM_SCATTER_GATHER_LIST];
        struct DPD_LIST_ENTRY      *Next;
        struct DPD_LIST_ENTRY      *Previous;
        DWORD                       DPDPhysicalAddress;
        BYTE                       *SocketBuffer;
        DWORD                       PacketLength;
      } DPD_LIST_ENTRY;


typedef struct UPD_LIST_ENTRY {
        DWORD                       UpNextPointer;
        DWORD                       UpPacketStatus;
        struct SCATTER_GATHER_ENTRY SGList[1];
        struct UPD_LIST_ENTRY      *Next;
        struct UPD_LIST_ENTRY      *Previous;
        DWORD                       UPDPhysicalAddress;
        BYTE                       *RxBufferVirtual;
        BYTE                       *SocketBuffer;
      } UPD_LIST_ENTRY;


/* Connector Type
 */
typedef enum CONNECTOR_TYPE {
        CONNECTOR_10BASET = 0,
        CONNECTOR_10AUI = 1,
        CONNECTOR_10BASE2 = 3,
        CONNECTOR_100BASETX = 4,
        CONNECTOR_100BASEFX = 5,
        CONNECTOR_MII = 6,
        CONNECTOR_AUTONEGOTIATION = 8,
        CONNECTOR_EXTERNAL_MII = 9,
        CONNECTOR_UNKNOWN = 0xFF
      } CONNECTOR_TYPE;

typedef enum LINK_STATE {
        LINK_UP = 0,            /* Link established */
        LINK_DOWN = 1,          /* Link lost */
        LINK_DOWN_AT_INIT = 2   /* Link lost and needs notification to NDIS */
      } LINK_STATE;

typedef struct NIC_PCI_INFORMATION {
        int   InterruptVector;
        DWORD IoBaseAddress;
      } NIC_PCI_INFORMATION;

typedef struct NIC_HARDWARE_INFORMATION {
        BYTE  CacheLineSize;
        BYTE  RevisionId;
        BYTE  Status;
#define HARDWARE_STATUS_WORKING		0x0
#define HARDWARE_STATUS_HUNG		0x1
#define HARDWARE_STATUS_FAILURE		0x2

        WORD  XcvrType;
        WORD  DeviceId;
        DWORD BitsInHashFilter;
        DWORD LinkSpeed;
        DWORD UpdateInterval;

        enum CONNECTOR_TYPE Connector;
        enum CONNECTOR_TYPE ConfigConnector;

        BOOL  HurricaneEarlyRevision;
        BYTE  FeatureSet;
#define MOTHERBOARD_FEATURE_SET         0x0
#define LOW_COST_ADAPTER_FEATURE_SET    0x1
#define STANDARD_ADAPTER_FEATURE_SET    0x2
#define SERVER_ADAPTER_FEATURE_SET      0x4

        BOOL OptimizeForThroughput;
        BOOL OptimizeForCPU;
        BOOL OptimizeNormal;

        BOOL BroadcastErrDone;
        BOOL UDPChecksumErrDone;
        BOOL FullDuplexEnable;
        BOOL DuplexCommandOverride;

        BOOL MWIErrDone;
        BOOL FlowControlEnable;
        BOOL FlowControlSupported;
        BOOL LinkBeatDisable;

        BOOL SupportsPowerManagement;
        BOOL WOLConnectorPresent;
        BOOL AutoResetToD0;
        BOOL DontSleep;
        BOOL D3Work;

        BOOL WakeOnMagicPacket;
        BOOL WakeOnLinkChange;

        BOOL SQEDisable;
        BOOL AutoSelect;
        BOOL LightTen;
        enum LINK_STATE LinkState;

        /* TryMII sets these parameters.
         */
        WORD MIIReadCommand;
        WORD MIIWriteCommand;
        WORD MIIPhyOui;
        WORD MIIPhyModel;
        WORD MIIPhyUsed;
        WORD MediaOverride;

        WORD phys;                  /* MII device addr. - for Becker's diag */
      } NIC_HARDWARE_INFORMATION;

/*
 * Command line media override values
 */
#define MEDIA_NONE			0
#define MEDIA_10BASE_T			1
#define MEDIA_10AUI			2
#define MEDIA_10BASE_2			3
#define MEDIA_100BASE_TX		4
#define MEDIA_100BASE_FX		5
#define MEDIA_10BASE_FL			6
#define MEDIA_AUTO_SELECT		7
  
#define MII_PHY_ADDRESS				0x0C00

/*
 *--------------------- MII register definitions --------------------
 */
#define MII_PHY_CONTROL                 0    /* control reg address */
#define MII_PHY_STATUS                  1    /* status reg address */
#define MII_PHY_OUI                     2    /* most of the OUI bits */
#define MII_PHY_MODEL                   3    /* model/rev bits, and rest of OUI */
#define MII_PHY_ANAR                    4    /* Auto negotiate advertisement reg */
#define MII_PHY_ANLPAR                  5    /* auto negotiate Link Partner */
#define MII_PHY_ANER                	0x6
#define MII_PAR                     	0x19
#define MII_PCR                         0x17 /* PCS Config register */

#define MII_PHY_REGISTER_24             24   /* Register 24 of the MII */
#define MII_PHY_REGISTER_24_PVCIRC      0x01 /* Process Variation Circuit bit */

/*
 *--------------------- Bit definitions: Physical Management --------------------
 */
#define PHY_WRITE                       0x0004  /* Write to PHY (drive MDIO) */
#define PHY_DATA1                       0x0002  /* MDIO data bit */
#define PHY_CLOCK                       0x0001  /* MII clock signal */

/*
 *--------------------- Bit definitions: MII Control --------------------
 */
#define MII_CONTROL_RESET               0x8000  /* reset bit in control reg */
#define MII_CONTROL_100MB               0x2000  /* 100Mbit or 10 Mbit flag */
#define MII_CONTROL_ENABLE_AUTO         0x1000  /* autonegotiate enable */
#define MII_CONTROL_ISOLATE             0x0400  /* islolate bit */
#define MII_CONTROL_START_AUTO          0x0200  /* restart autonegotiate */
#define MII_CONTROL_FULL_DUPLEX		0x0100


/*
 *--------------------- Bit definitions: MII Status --------------------
 */
#define MII_STATUS_100MB_MASK   0xE000  // any of these indicate 100 Mbit
#define MII_STATUS_10MB_MASK    0x1800  // either of these indicate 10 Mbit
#define MII_STATUS_AUTO_DONE    0x0020  // auto negotiation complete
#define MII_STATUS_AUTO         0x0008  // auto negotiation is available
#define MII_STATUS_LINK_UP      0x0004  // link status bit
#define MII_STATUS_EXTENDED     0x0001  // extended regs exist
#define MII_STATUS_100T4        0x8000  // capable of 100BT4
#define MII_STATUS_100TXFD      0x4000  // capable of 100BTX full duplex
#define MII_STATUS_100TX        0x2000  // capable of 100BTX
#define MII_STATUS_10TFD        0x1000  // capable of 10BT full duplex
#define MII_STATUS_10T		0x0800  // capable of 10BT


/*
 *----------- Bit definitions: Auto-Negotiation Link Partner Ability ----------
 */
#define MII_ANLPAR_100T4	0x0200 // support 100BT4
#define MII_ANLPAR_100TXFD	0x0100 // support 100BTX full duplex
#define MII_ANLPAR_100TX	0x0080 // support 100BTX half duplex
#define MII_ANLPAR_10TFD	0x0040 // support 10BT full duplex
#define MII_ANLPAR_10T          0x0020 // support 10BT half duplex

/*
 *----------- Bit definitions: Auto-Negotiation Advertisement ----------
 */
#define MII_ANAR_100T4		0x0200  // support 100BT4
#define MII_ANAR_100TXFD        0x0100  // support 100BTX full duplex
#define MII_ANAR_100TX		0x0080  // support 100BTX half duplex
#define MII_ANAR_10TFD		0x0040  // support 10BT full duplex
#define MII_ANAR_10T            0x0020  // support 10BT half duplex
#define MII_ANAR_FLOWCONTROL    0x0400  // support Flow Control

#define MII_ANAR_MEDIA_MASK     0x07E0  // Mask the media selection bits
#define MII_ANAR_MEDIA_100_MASK	(MII_ANAR_100TXFD | MII_ANAR_100TX)
#define MII_ANAR_MEDIA_10_MASK	(MII_ANAR_10TFD | MII_ANAR_10T)

#define MII_100TXFD		0x01
#define MII_100T4		0x02
#define MII_100TX		0x03
#define MII_10TFD		0x04
#define MII_10T			0x05

/*
 *----------- Bit definitions: Auto-Negotiation Expansion ----------
 */
#define MII_ANER_LPANABLE	0x0001 // Link partner autonegotiatable ?
#define MII_ANER_MLF            0x0010 // Multiple Link Fault bit

/* MII Transceiver Type store in miiSelect
 */
#define MIISELECT_GENERIC	0x0000
#define MIISELECT_100BT4	0x0001
#define MIISELECT_10BT		0x0002
#define MIISELECT_100BTX	0x0003
#define MIISELECT_10BT_ANE	0x0004
#define MIISELECT_100BTX_ANE	0x0005
#define MIITXTYPE_MASK		0x000F

#define NIC_STATUS_SUCCESS              0x1
#define NIC_STATUS_FAILURE              0x2

//
// Software limits defined here.
//
#define NIC_DEFAULT_SEND_COUNT		0x40
#define NIC_DEFAULT_RECEIVE_COUNT	0x40
#define NIC_MINIMUM_SEND_COUNT		0x2
#define NIC_MAXIMUM_SEND_COUNT		0x80
#define NIC_MINIMUM_RECEIVE_COUNT	0x2
#define NIC_MAXIMUM_RECEIVE_COUNT	0x80

#define LINK_SPEED_100			100000000L
#define LINK_SPEED_10			10000000L

#define ETH_ADDR_SIZE		6
#define ETH_MULTICAST_BIT	1

typedef struct ETH_ADDR {
        BYTE Addr[ETH_ADDR_SIZE];
      } ETH_ADDR;

typedef struct NIC_RESOURCES {
        DWORD             ReceiveCount;
        DWORD             SendCount;
        DWORD             SharedMemorySize;
        BYTE             *SharedMemoryVirtual;
        struct timer_list Timer;
        DWORD             TimerInterval;
        DWORD             DownPollRate;
      } NIC_RESOURCES;

/*
 * Statistics maintained by the driver.
 */
typedef struct NIC_STATISTICS {
        /* Transmit statistics.
         */
        DWORD TxFramesOk;
        DWORD TxBytesOk;
        DWORD TxFramesDeferred;
        DWORD TxSingleCollisions;
        DWORD TxMultipleCollisions;
        DWORD TxLateCollisions;
        DWORD TxCarrierLost;

        DWORD TxMaximumCollisions;
        DWORD TxSQEErrors;
        DWORD TxHWErrors;
        DWORD TxJabberError;
        DWORD TxUnknownError;

        DWORD TxLastPackets;
        DWORD TxLastCollisions;
        DWORD TxLastDeferred;

        /* Receive statistics.
         */
        DWORD RxFramesOk;
        DWORD RxBytesOk;

        DWORD RxOverruns;
        DWORD RxBadSSD;
        DWORD RxAlignmentError;
        DWORD RxBadCRCError;
        DWORD RxOversizeError;

        DWORD RxNoBuffer;

        DWORD RxLastPackets;
        DWORD UpdateInterval;

        /* Multicasts statistics
         */
        DWORD Rx_MulticastPkts;
      } NIC_STATISTICS;

/*
 * Memory allocation
 */
#define NIC_IO_PORT_REGISTERED       0x00000001
#define NIC_INTERRUPT_REGISTERED     0x00000002
#define NIC_SHARED_MEMORY_ALLOCATED  0x00000004
#define WAIT_TIMER_REGISTERED        0x00000008
#define NIC_TIMER_REGISTERED         0x00000010

#define MAXIMUM_TEST_BUFFERS         1

typedef enum NIC_WAIT_CASES {
        CHECK_UPLOAD_STATUS,
        CHECK_DOWNLOAD_STATUS,
        CHECK_DC_CONVERTER,
        CHECK_PHY_STATUS,
        CHECK_TRANSMIT_IN_PROGRESS,
        CHECK_DOWNLOAD_SELFDIRECTED,
        AUTONEG_TEST_PACKET,
        CHECK_DMA_CONTROL,
        CHECK_CARRIER_SENSE,
        NONE
      } NIC_WAIT_CASES;


typedef struct NIC_INFORMATION {
        DWORD  IoBaseAddress;
        BYTE   DeviceName[8];
        BYTE   PermanentAddress[6];
        BYTE   StationAddress[6];
        DWORD  ResourcesReserved;

        struct NIC_PCI_INFORMATION  PCI;
        struct UPD_LIST_ENTRY      *HeadUPDVirtual;
        struct DPD_LIST_ENTRY      *HeadDPDVirtual;
        struct DPD_LIST_ENTRY      *TailDPDVirtual;

        DWORD  TestDPDVirtual[MAXIMUM_TEST_BUFFERS];
        DWORD  TestDPDPhysical[MAXIMUM_TEST_BUFFERS];
        DWORD  TestBufferVirtual[MAXIMUM_TEST_BUFFERS];
        DWORD  TestBufferPhysical[MAXIMUM_TEST_BUFFERS];

        struct NIC_RESOURCES            Resources;
        struct NIC_STATISTICS           Statistics;
        struct net_device_stats         EnetStatistics;
        struct NIC_HARDWARE_INFORMATION Hardware;
        DWORD                           BytesInDPDQueue;
        struct device                  *NextDevice;
        struct device                  *Device;
        BOOL                            InTimer;
        BOOL                            DelayStart;
        int                             Index;
        DWORD                           TxPendingQueueCount;
        BOOL                            DPDRingFull;
        BOOL                            DeviceGivenByOS;
        DWORD                           keepForGlobalReset;
        enum NIC_WAIT_CASES             WaitCases;
      } NIC_INFORMATION;


#define NIC_READ_PORT_BYTE(pAdapter, Register) \
        _inportb (pAdapter->IoBaseAddress + Register)

#define NIC_READ_PORT_WORD(pAdapter, Register) \
        _inportw (pAdapter->IoBaseAddress + Register)

#define NIC_READ_PORT_DWORD(pAdapter, Register) \
        _inportl (pAdapter->IoBaseAddress + Register)

#define NIC_WRITE_PORT_BYTE(pAdapter, Register, Value) \
        _outportb (pAdapter->IoBaseAddress + Register, Value)

#define NIC_WRITE_PORT_WORD(pAdapter, Register, Value) \
        _outportw (pAdapter->IoBaseAddress + Register, Value)

#define NIC_WRITE_PORT_DWORD(pAdapter, Register, Value) \
        _outportl (pAdapter->IoBaseAddress + Register, Value)

#define NIC_COMMAND(pAdapter, Command) \
        NIC_WRITE_PORT_WORD (pAdapter, INTSTATUS_COMMAND_REGISTER, Command)

#define NIC_MASK_ALL_INTERRUPT(pAdapter) do { \
        NIC_COMMAND (pAdapter, COMMAND_SET_INTERRUPT_ENABLE | DISABLE_ALL_INTERRUPT); \
        NIC_READ_PORT_WORD (pAdapter, INTSTATUS_COMMAND_REGISTER); \
      } while (0)

#define NIC_UNMASK_ALL_INTERRUPT(pAdapter) do { \
        NIC_COMMAND (pAdapter, COMMAND_SET_INTERRUPT_ENABLE | ENABLE_ALL_INTERRUPT); \
        NIC_READ_PORT_WORD (pAdapter, INTSTATUS_COMMAND_REGISTER); \
      } while (0)


#define NIC_ACKNOWLEDGE_ALL_INTERRUPT(pAdapter) \
        NIC_COMMAND (pAdapter, COMMAND_ACKNOWLEDGE_INTERRUPT | ACKNOWLEDGE_ALL_INTERRUPT)

#define NIC_ENABLE_ALL_INTERRUPT_INDICATION(pAdapter) \
        NIC_COMMAND (pAdapter, COMMAND_SET_INDICATION_ENABLE | ENABLE_ALL_INTERRUPT)

#define NIC_DISABLE_ALL_INTERRUPT_INDICATION(pAdapter) \
        NIC_COMMAND (pAdapter, COMMAND_SET_INDICATION_ENABLE | DISABLE_ALL_INTERRUPT)

#define COMPARE_MACS(pAddr1, pAddr2) \
        (*((DWORD*)((BYTE*)(pAddr1)+2)) == *((DWORD*)((BYTE*)(pAddr2)+2)) && \
         *((DWORD*)(pAddr1)) == *((DWORD*)(pAddr2)))


#define DEBUG_INITIALIZE	0x00000001
#define DEBUG_FUNCTION		0x00000002
#define DEBUG_IOCTL		0x00000004
#define DEBUG_GET_STATISTICS	0x00000008
#define DEBUG_SEND		0x00000010
#define DEBUG_RECEIVE		0x00000020
#define DEBUG_INTERRUPT		0x00000040
#define DEBUG_ERROR		0x80000000

#ifdef TC90XBC_DEBUG
  #define DBGPRINT_INITIALIZE(A)     if (tc90xbc_debug & DEBUG_INITIALIZE) printk A
  #define DBGPRINT_FUNCTION(A)       if (tc90xbc_debug & DEBUG_FUNCTION) printk A
  #define DBGPRINT_SEND(A)           if (tc90xbc_debug & DEBUG_SEND) printk A
  #define DBGPRINT_RECEIVE(A)        if (tc90xbc_debug & DEBUG_RECEIVE) printk A
  #define DBGPRINT_INTERRUPT(A)      if (tc90xbc_debug & DEBUG_INTERRUPT) printk A
  #define DBGPRINT_GET_STATISTICS(A) if (tc90xbc_debug & DEBUG_GET_STATISTICS) printk A
  #define DBGPRINT_IOCTL(A)          if (tc90xbc_debug & DEBUG_IOCTL) printk A
  #define DBGPRINT_INIT(A)           if (tc90xbc_debug) printk A
  #define DBGPRINT_ERROR(A)          printk A
#else
  #define DBGPRINT_INITIALIZE(A)
  #define DBGPRINT_FUNCTION(A)
  #define DBGPRINT_SEND(A)
  #define DBGPRINT_RECEIVE(A)
  #define DBGPRINT_INTERRUPT(A)
  #define DBGPRINT_GET_STATISTICS(A)
  #define DBGPRINT_QUERY(A)
  #define DBGPRINT_SET(A)
  #define DBGPRINT_IOCTL(A)
  #define DBGPRINT_INIT(A)
  #define DBGPRINT_ERROR(A)
#endif

__inline static void _assert (const char *what, const char *file, unsigned line)
{
  _printk_safe = 1;
  _printk_flush();
  fprintf (stderr, "%s (%u): Assertion \"%s\" failed\n",
           file, line, what);
  exit (-1);
}

#define ASSERT(x) do { \
                    if (!(x)) \
                       _assert (#x, __FILE__, __LINE__); \
                  } while (0)

/*
 * This routine issues a command and spins for the completion.
 */
__inline static DWORD NIC_COMMAND_WAIT (struct NIC_INFORMATION *pAdapter, WORD Command)
{
  DWORD count;
  WORD  value;

  NIC_WRITE_PORT_WORD (pAdapter, INTSTATUS_COMMAND_REGISTER, Command);

  count = jiffies + HZ;
  do
  {
    value = NIC_READ_PORT_WORD (pAdapter, INTSTATUS_COMMAND_REGISTER);
    udelay (10);
  }
  while ((value & INTSTATUS_COMMAND_IN_PROGRESS) && (count > jiffies));

  if (count < jiffies)
  {
    if (tc90xbc_debug)
       printk ("NIC_COMMAND_WAIT: timeout\r\n");
    return (NIC_STATUS_FAILURE);
  }
  return (NIC_STATUS_SUCCESS);
}

/*
 * This routine sets the countdown timer on the hardware.
 */
__inline static void SetCountDownTimer (struct NIC_INFORMATION *pAdapter)
{
  DWORD countDownValue = pAdapter->BytesInDPDQueue / 4;

  if (countDownValue < 10)
      countDownValue = 10;

  NIC_WRITE_PORT_WORD (pAdapter, COUNTDOWN_REGISTER, (WORD)countDownValue);
}

/*
 * This routine sets the checksum information.
 */
#ifdef NOT_USED
__inline static void SetRxTcpIpChecksumOffloadFlagsInSocketBuffer (
                     struct NIC_INFORMATION *Adapter,
                     SKB                    *SocketBuffer,
                     DWORD                   UpPacketStatus)
{
  /* Check if this is IP packet.
   */
  if (UpPacketStatus & UP_PACKET_STATUS_IP_CHECKSUM_CHECKED)
  {
    if (UpPacketStatus & UP_PACKET_STATUS_IP_CHECKSUM_ERROR)
    {
      DBGPRINT_ERROR (("IP checksum error\n"));
      SocketBuffer->ip_summed = CHECKSUM_NONE;
      return;
    }
    /* Check if packet is TCP packet.
     */
    if (UpPacketStatus & UP_PACKET_STATUS_TCP_CHECKSUM_CHECKED)
    {
      /* Check if TCP checksum has been offloaded to us.
       */
      if (UpPacketStatus & UP_PACKET_STATUS_TCP_CHECKSUM_ERROR)
      {
        DBGPRINT_ERROR (("TCP Checksum error\n"));
        SocketBuffer->ip_summed = CHECKSUM_NONE;
        return;
      }
    }
    /* Check if this is UDP packet.
     */
    if (UpPacketStatus & UP_PACKET_STATUS_UDP_CHECKSUM_CHECKED)
    {
      if (TRUE == Adapter->Hardware.UDPChecksumErrDone)
      {
        if (UpPacketStatus & UP_PACKET_STATUS_UDP_CHECKSUM_ERROR)
        {
          DBGPRINT_ERROR (("UDP Error"));
          SocketBuffer->ip_summed = CHECKSUM_NONE;
          return;
        }
        SocketBuffer->ip_summed = CHECKSUM_NONE;
        return;
      }
    }
  }
  SocketBuffer->ip_summed = CHECKSUM_UNNECESSARY;
}
#endif /* NOT_USED */
#endif /* IMPLEMENT_3C90X */
#endif /* __3C90X_H */

