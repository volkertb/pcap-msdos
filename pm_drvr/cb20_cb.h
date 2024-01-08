/*
    The contents of this file are subject to the Mozilla Public License
    Version 1.1 (the "License"); you may not use this file except in
    compliance with the License. You may obtain a copy of the License at
    http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS IS" basis,
    WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
    for the specificlanguage governing rights and limitations under the License.

    The Original Code is a driver for Aironet CB20A card.

    The Initial Developer of the Original Code is Benjamin Reed
    <breed@almaden.ibm.com>. Portions created by Benjamin Reed
    are Copyright (C) 1999 Benjamin Reed. All Rights Reserved.

    Contributor(s): Cisco Systems, Inc.

    Portions created by Cisco Systems, Inc. are
    Copyright (C) 2003, 2004 Cisco Systems, Inc. All Rights Reserved.

 */

#ifndef __KERNEL__
#define __KERNEL__
#endif

/* #define DEBUG_MIC 1 */

#define V_CNFCTRL          0x7c    /* Reset config control port        */
#define BB_HANDSHAKE_0     0xa505  /* bootblock sanity check constants */
#define BB_HANDSHAKE_1     0xbb10

#define CB_INTENABLE       0x84    /* write */
#define CB_PENDING         0x88    /* read  */
#define CB_CLEARINT        0x80    /* write */
#define CB_FLAGBIT         0x8000  /* Magic bit for cardbus int control */
#define READLINE_DEL       0x10    /* Read cache line hack              */
#define AIRONET            0x14b9
#define MAXTXQ             32
#define TXTIME             4 
#define TXSTALL		   200     /* Tx timeout */
#define PRODNAME           "cb20"
#define KOALA_TYPE	   7     /* Card this driver is for */
#define VERSION_CODE(vers,rel,seq) ( ((vers)<<16) | ((rel)<<8) | (seq) )


/*
 * Events
 */
#define EV_CMD 0x10
#define EV_CLEARCOMMANDBUSY 0x4000
#define EV_RX         0x01
#define EV_TX         0x02
#define EV_TXEXC      0x04
#define EV_ALLOC      0x08
#define EV_XMIT       0x08
#define EV_LINK       0x80
#define EV_AWAKE      0x100
#define EV_TXCPY      0x400
#define EV_TXQCMPL    0x800
#define EV_SLEEPING   0x2000
#define EV_MIC        0x1000

#define STATUS_INTS  (EV_TXCPY|EV_CMD|EV_LINK|EV_TX|EV_RX|EV_MIC )

/*
 * I have been told that cb20 io ports are 32 bit aligned 
 * addresses 
 */

#define V_COMMAND    00
#define V_PARAM0     4
#define V_PARAM1     8
#define V_PARAM2     12
#define V_STATUS     16
#define V_RESP0      20
#define V_RESP1      24
#define V_RESP2      28
#define V_LINKSTAT   32
#define V_SELECT0    36
#define V_OFFSET0    40
#define V_RXFID      44
#define V_TXALLOCFID 48
#define V_SWS0       80
#define V_SWS1       84
#define V_SWS2       88
#define V_SWS3       92
#define V_EVSTAT     96
#define V_EVINTEN    100
#define V_EVACK      104

#define ETHERNET_ADDRESS_LENGTH          6
#define ETHERNET_HEADER_SIZE            14    // Size of the ethernet header
#define MAXIMUM_ETHERNET_PACKET_SIZE    1514
#define MIC_PACKET_LENGTH               18   // Size increase of ethernet packet to do Mic
#define MAXIMUM_RX_PACKET_SIZE          MAXIMUM_ETHERNET_PACKET_SIZE + MIC_PACKET_LENGTH
#define MAXIMUM_TX_PACKET_SIZE          MAXIMUM_ETHERNET_PACKET_SIZE + MIC_PACKET_LENGTH
#define MIC_MSGLEN_MAX		2000
#define EMMH32_MSGLEN_MAX	MIC_MSGLEN_MAX
#define CFG_OPMODE_MIC          0x8000
#define EXT_SOFT_CAPS_MIC       0x0001
#define FW_LOAD_OFFSET          0x20000 /* Where firmware image needs to go */
#define MAXTXDTRY               1000
typedef struct FIRMWARE_HEADER {
    unsigned char   text[128];
    unsigned char   file_format;
    unsigned char   device_type;
    unsigned char   bootdload_vers[2];
    unsigned char   oper_vers[2];
    unsigned short  bootdload_size;
    unsigned short  oper_size;
    unsigned short  img_chksum;
    unsigned char   _filler[114];
    unsigned short  hdr_chksum;
} FIRMWARE_IMAGE_FILE_HEADER __attribute__ ((packed)) ;



typedef struct {
	unsigned long	coeff[((EMMH32_MSGLEN_MAX)+3)>>2];
	unsigned long long	accum;	/* accumulated mic, reduced to u32 in final() */
	int	 position;		/* current position (byte offset) in message */
	union	{
		unsigned char	d8[4];
		unsigned long	d32;
	} part;		/* saves partial message word across update() calls */
} emmh32_context;

/*
 * Mic support 
 *
 */
typedef struct _ETH_HEADER_STRUC {
    u8       Destination[ETHERNET_ADDRESS_LENGTH]__attribute__ ((packed));
    u8       Source[ETHERNET_ADDRESS_LENGTH] __attribute__ ((packed));
    u16       TypeLength __attribute__ ((packed));
} ETH_HEADER_STRUC __attribute__ ((packed)) ;


#define UNALIGN32 0

#define MIC_ACCUM(v)	\
	context->accum += (u64)val * context->coeff[coeff_position++];

#define SWAPU32(d)	( ((d)<<24) | ( ((d)&0xFF00)<<8) | (((d)>>8)&0xFF00) | ((d)>>24) )
#define ISBIGENDIAN	( (*(u8*)(&endian_ref)) == 0x12 )
#define BIGEND32(d)	( ISBIGENDIAN ? (d) : SWAPU32(d))

#ifndef UNALIGN32
#error UNALIGN32 must be defined.
#elif UNALIGN32
/* unaligned accesses are allowed -- fetch u32 and swap endian */
#define	GETBIG32(p)				BIGEND32(*(u32*)(p))
#else
/* unaligned accesses are disallowed ... slow GET32() */
#define GB(p,i,s)				( ((u32) *((u8*)(p)+i) ) << (s) )
#define GETBIG32(p)				GB(p,0,24)|GB(p,1,16)|GB(p,2,8)|GB(p,3,0)
#endif


typedef struct _MIC_BUFFER_STRUCT {
    u8  DA[ETHERNET_ADDRESS_LENGTH];
    u8  SA[ETHERNET_ADDRESS_LENGTH];
    u16 TypeLength __attribute__ ((packed));

    union {
        u8      snap[8];
        struct {
            u8          dsap;
            u8          ssap;
            u8          control;
            u8          orgCode[3];
            u8          fieldType[2];
        } llc;
    }u  __attribute__ ((packed));

  u32         MIC __attribute__ ((packed));
  u32         SEQ __attribute__ ((packed));
  u8          payload __attribute__ ((packed)); 

} MIC_BUFFER_STRUCT __attribute__ ((packed));


typedef struct _MIC_CNTX {
  emmh32_context      seed __attribute__ ((packed));       //Context - "the seed"
  u32                 rx __attribute__ ((packed)) ;         //Received sequence numbers
  u32                 window __attribute__ ((packed));    //start of window
  u32                 tx __attribute__ ((packed)) ;         //Tx sequence number
  u8                  multicast __attribute__ ((packed)); // Flag to say if it is mulitcast or not
  u8                  valid __attribute__ ((packed));     // Flag to say if context is valid or not
  u8                  key[16] __attribute__ ((packed)) ;    
}   MICCNTX __attribute__ ((packed));

typedef struct _MIC_MODULE {
    MICCNTX             mCtx;      //Multicast context
    MICCNTX             uCtx;      //Unicast context
} MICMODULE __attribute__ ((packed));

#define NUM_MODULES   2

/* expand the key to fill the MMH coefficient array */
void emmh32_setseed(emmh32_context *context, unsigned char *pkey, int keylen);
/* prepare for calculation of a new mic */
void emmh32_init(emmh32_context *context);
/* add some bytes to the mic calculation */
void emmh32_update(emmh32_context *context, unsigned char *pOctets, int len);
/* calculate the mic */
void emmh32_final(emmh32_context *context, unsigned char digest[4]);


/*
 * This vector conains all the cb20 
 * PCI id's ending with a null for the 
 * last one. Things change so be ready
 */
unsigned cb20_ids [ ] = 
{
  0x5000,    /* Rev G radio */
  0x5050,    /* Nova        */
  0xa504,    /* Rev I radio */
  0          /* Who knows   */
};


#define NOVA_SUBDEV   0x5050
#define MPI350_SUBDEV 0x5000
#define KOALA_SUBDEV  0x6000


struct proc_data {
	int release_buffer;
	int readlen;
	char *rbuffer;
	int writelen;
	int maxwritelen;
	char *wbuffer;
	void (*on_close) (struct inode *, struct file *);
};



#if (LINUX_VERSION_CODE < 0x20315)

#define   in_irq in_interrupt
#define   net_device          device
#define   dev_kfree_skb_irq(a)          dev_kfree_skb(a)
#ifdef _LINUX_NETDEVICE_H /* only if netdevice.h was included */
#define netif_start_queue(dev) clear_bit(0, (void *) &(dev)->tbusy);
#define netif_stop_queue(dev)  set_bit(0, (void *) &(dev)->tbusy);
#define check_mem_region(a,b)     0 
#define release_mem_region(a,b)   
#define request_mem_region(a,b,c) 1
#ifndef set_current_state
#define set_current_state(a)
#endif

static inline int  netif_device_present(struct device *dev){
  if(dev == NULL)
    return 0;
  else
    return 1;
}
      
static inline void netif_wake_queue(struct device *dev)
{
    clear_bit(0, (void *) &(dev)->tbusy);
    mark_bh(NET_BH);
}

/* Pure 2^n version of get_order taken from 2.4 sources */
#if LINUX_VERSION_CODE < VERSION_CODE(2,2,19)
extern __inline__ int get_order(unsigned long size)
{
        int order;

        size = (size-1) >> (PAGE_SHIFT-1);
        order = -1;
        do {
                size >>= 1;
                order++;
        } while (size);
        return order;
}
static inline void netif_device_attach(struct device *dev){ }
static inline void netif_device_detach(struct device *dev){ }


#endif

static inline void netif_carrier_on(struct device *dev){ }
static inline void netif_carrier_off(struct device *dev){ }

#if LINUX_VERSION_CODE < VERSION_CODE(2,2,19) 
typedef   unsigned dma_addr_t;
#endif

#endif 
#endif 

typedef struct {
	u16 cmd;
	u16 parm0;
	u16 parm1;
	u16 parm2;
} Cmd;

typedef struct {
	u16 status;
	u16 rsp0;
	u16 rsp1;
	u16 rsp2;
} Resp;


/*
 * Offset into card memory of descriptors.
 */
#define CARD_DISCRAMOFF 0x800           

/*
 * 1 tx 1 rx and a host frame buffer section (1832 ) 
 * for each descriptor + 2k for the rid desc. ~3664
 * bytes per MAX_DESC count
 * SHAREDMEMSIZE = (MAX_DESC * (HOSTBUFSIZ*2) )  + 2048 
 */
#define MAX_DESC 1
#define HOSTBUFSIZ 2048
#define DISCDLY      20
#define VADDR(Type,HOSTP)  (Type *)(HOSTP.VirtualHostAddress)
#define CDISCP(Type,CARDP) (Type *)(p.CardRamOff)
/*
 * Flag bits for cb20_info->flags 
 */ 
#define TXBUSY        1
#define INSPAC        2
#define ASSOC         4
#define MIC_CAPABLE   8       /* Can do mic    */
#define MIC_ENABLED   16      /* Mic turned on */
#define ADHOC	      32      /* In addhoc     */
#define FLASHING      64      /* Flashing      */   
#define REAP          128     /* Unregister netdev at earliest convenience. */
#define DEVREGISTERED 256     /* Device registered */
#define NOCARD        512
#define INT_DISABLE   1024
#define FWBAD	      2048    /* Something is wrong with the firmware cant start */
#define SUCCESS 0
#define ERROR -1
#define COMMAND_BUSY 0x8000


#define SHAREDMEMSIZE     (MAX_DESC * (2 * HOSTBUFSIZ ) )  + 2048 

/* 
 * Link status bits
 */
#define LS_NOBEACON       0x8000
#define LS_MXRETRIES      0x8001
#define LS_ASSFAIL        0x8400
#define LS_ASSOC          0x0400


/* SSID rid */

#define RID_SSLIST		0xff11

typedef struct {
  unsigned short SsidLen;
  unsigned char  Ssid[32];
}PC3500_SSID;

typedef struct {
  unsigned short u16RidLen;
  PC3500_SSID aSsid[1];
}PC3500_SID_LIST;




/***********************Cb20 descriptor types *************/

/*
 * Card RID descriptor
 */
typedef struct  _CARD_RID_DESC
{
    unsigned      RID         :16;
    unsigned      length      :15;
    unsigned      valid       :1;
    unsigned long PhyHostAddress;
} CARD_RID_DESC;

/*
 * Host RID descriptor
 */
typedef struct  _HOST_RID_DESC
{
    unsigned char   *CardRamOff;            // offset into card memory of the descriptor
    CARD_RID_DESC   RIDDesc;                // card RID descriptor
    char            *VirtualHostAddress;    // virtual address of host receive buffer
} HOST_RID_DESC;

/*
 * Card receive descriptor
 */

typedef struct  _CARD_RX_DESC
{
    unsigned      RxCtrl      :15;
    unsigned      RxDone      :1;
    unsigned      length      :15;
    unsigned      valid       :1;
    unsigned long PhyHostAddress;
} CARD_RX_DESC;

/*
 * Card transmit descriptor
 */
typedef struct  _CARD_TX_DESC
{
    unsigned      Offset      :15;
    unsigned      eoc         :1;
    unsigned      length      :15;
    unsigned      valid       :1;
    unsigned long PhyHostAddress;
} CARD_TX_DESC;

/*
 * Host receive descriptor
 */
typedef struct  _HOST_TX_DESC
{
  unsigned char   *CardRamOff;          /* offset into card memory of the descriptor */
  CARD_TX_DESC    TxDesc;               /* card transmit descriptor */
  char            *VirtualHostAddress;  /*  virtual address of host receive buffer */
} HOST_TX_DESC;


/*
 * Host transmit descriptor
 */
typedef struct  _HOST_RX_DESC
{
  unsigned char   *CardRamOff;         /* offset into card memory of the descriptor */
  CARD_RX_DESC    RxDesc;              /* card receive descriptor */
  char            *VirtualHostAddress; /* virtual address of host receive buffer */
  int             pending;             
} HOST_RX_DESC;


typedef u8              MacAddr[6];

typedef struct _HDR_802_11 {
    u16         FrmCtrl;
    u16         duratation;
    MacAddr     Addr1;
    MacAddr     Addr2;
    MacAddr     Addr3;
    u16         sequence;
    MacAddr     Addr4;
}HDR_802_11;

typedef struct _HDR_802_3 {
    MacAddr     destAddr;
    MacAddr     srcAddr;
}HDR_802_3;

typedef struct _TXHDR_CTRL {
  u16     SWSupport0; /* 0   */
  u16     SWSupport1; /* 2 */
  u16     status;     /* 4 */
  u16     DataLen;    /* 6 */
  /*
    * TxCtrl
    * 
    * bit  meaning
    * ------------------------------------------
    * 0    host set (on/off) FcWep as required
    * 1    host wants interrupt on Tx complete OK
    * 2    host wants interrupt on Tx exception
    * 3    ?
    * 4    1=payload is LLC, 0=payload is Ethertype
    * 5    don't release buffer when done
    * 6    don't retry packet
    * 7    clear AID failed state
    * 8    strict order multicast
    * 9    force RTS use
    * 10   short preamble
    * 11   ?
    * 12   \
    * 13    > priority
    * 14   /
    * 15   ?
    * 
    */
  u16     TxCtrl;     /*8 */
  u16     AID;        /*10*/
  u16     TxRetries;  /*12*/
  u16     res1;       /*14*/    
}TXHDR_CTRL;

typedef struct _TXFID_HDR{
    TXHDR_CTRL  txHdr;
    u16         Plcp0;
    u16         Plcp1;
    HDR_802_11  hdr_802_11;
    u16         GapLen;
    u16         status;
  /*
   * This field is actually in this structure but is left out and handled separately
   * for historical reasons.
   *
   *  u16         payloadLen;
   */
}TXFID_HDR;



//====================================================
//      X500 commands Taken from windows cb20 driver

typedef enum {
    CMD_X500_NOP                    = 0x0000,
    CMD_X500_NOP10                  = 0x0010,
    CMD_X500_Initialize             = 0x0000,
    CMD_X500_Enable                 = 0x0001,
    CMD_X500_EnableMAC              = 0x0001,
    CMD_X500_DisableMAC             = 0x0002,
    CMD_X500_EnableRcv              = 0x0201,
    CMD_X500_EnableEvents           = 0x0401,
    CMD_X500_EnableAll              = 0x0701,
    CMD_X500_Disable                = 0x0002,
    CMD_X500_Diagnose               = 0x0003,
    CMD_X500_Allocate               = 0x000a,
    CMD_X500_AllocDescriptor        = 0x0020,   
    CMD_X500_Transmit               = 0x000b,
    CMD_X500_Dellocate              = 0x000c,
    CMD_X500_AccessRIDRead          = 0x0021,
    CMD_X500_AccessRIDWrite         = 0x0121,
    CMD_X500_EEReadConfig           = 0x0008,
    CMD_X500_EEWriteConfig          = 0x0108,
    CMD_X500_Preserve               = 0x0000,
    CMD_X500_Program                = 0x0000,
    CMD_X500_ReadMIF                = 0x0000,
    CMD_X500_WriteMIF               = 0x0000,
    CMD_X500_Configure              = 0x0000,
    CMD_X500_SMO                    = 0x0000,
    CMD_X500_GMO                    = 0x0000,
    CMD_X500_Validate               = 0x0000,
    CMD_X500_UpdateStatistics       = 0x0000,
    CMD_X500_ResetStatistics        = 0x0000,
    CMD_X500_RadioTransmitterTests  = 0x0000,
    CMD_X500_GotoSleep              = 0x0000,
    CMD_X500_SyncToBSSID            = 0x0000,
    CMD_X500_AssocedToAP            = 0x0000,
    CMD_X500_ResetCard              = 0x0004,           // (Go to download mode) 
    CMD_X500_SiteSurveyMode         = 0x0000,
    CMD_X500_SLEEP                  = 0x0005,
    CMD_X500_MagicPacketON          = 0x0086,
    CMD_X500_MagicPacketOFF         = 0x0186,
    CMD_X500_SetOperationMode       = 0x0009,       // CAM, PSP, ..
    CMD_X500_BssidListScan          = 0x0103,
}CMD_X500;

/* 
 * Descriptor types taken from 
 * windows driver.
 */

typedef enum _DESCRIPTOR_TYPE
{
    DESCRIPTOR_TX           =   0x01,
    DESCRIPTOR_RX           =   0x02,
    DESCRIPTOR_TXCMP        =   0x04,
    DESCRIPTOR_HOSTWRITE    =   0x08,
    DESCRIPTOR_HOSTREAD     =   0x10,
    DESCRIPTOR_HOSTRW       =   0x20,
        
} DESCRIPTOR_TYPE;


//:
// 0xFF11
typedef struct _STSSID {
	u16		num;
	u16		Len1;
	u8		ID1[32];
	u16		Len2;
	u8		ID2[32];
	u16		Len3;
	u8		ID3[32];
}STSSID;

//: Card status 
// FF50
typedef struct _STSTATUS {
  u16		u16RidLen;			// 0x0000
  u8		au8MacAddress[6];		// 0x0002
  u16		u16OperationalMode;		// 0x0008
  u16		u16ErrorCode;			// 0x000A
  u16		u16SignalStrength;		// 0x000C
  u16		SSIDlength;			// 0x000E
  u8		SSID[32];			// 0x0010
  u8		au8ApName[16];			// 0x0030
  u8		au8CurrentBssid[6];		// 0x0040
  u8		au8PreviousBssid1[6];		// 0x0046
  u8		au8PreviousBssid2[6];		// 0x004C
  u8		au8PreviousBssid3[6];		// 0x0052
  u16 	        u16BeaconPeriod;		// 0x0058
  u16		u16DtimPeriod;			// 0x005A
  u16		u16AtimDuration;		// 0x005C
  u16		u16HopPeriod;			// 0x005E
  union dschannel{
    u16		u16DsChannel;			// 0x0060
    u16		u16HopSet;			// 0x0060
  } channel;
  u16		u16HopPattern;			// 0x0062
  u16		u16HopsToBackbone;		// 0x0064
  u16		u16ApTotalLoad;			// 0x0066
  u16		u16OurGeneratedLoad;		// 0x0068
  u16		u16AccumulatedArl;		// 0x006A
  u16		u16SignalQuality;		// 0x006C
  u16		u16CurrentTxRate;		// 0x006E
  u16		u16APDeviceType;		// 0x0070
  u16		u16NormalizedSignalStrength;	// 0x0072
  u16		u16UsingShortRFHeaders;		// 0x0074
  u8		AccessPointIPAddress[4];	// 0x0076
  u16		u16MaxNoiseLevelLastSecond;	// 0x007A
  u16		u16AvgNoiseLevelLastMinute;	// 0x007C
  u16		u16MaxNoiseLevelLastMinute;	// 0x007E
  u16		u16CurrentAPPacketLoad;		// 0x0080
  u8		AdoptedCarrierSet[4];		// 0x0082
}STSTATUS;


/*
 * 0xFF10
 */
typedef struct {
	u16 len; /* sizeof(ConfigRid) */
	u16 opmode; /* operating mode */
#define MODE_STA_IBSS 0
#define MODE_STA_ESS 1
#define MODE_AP 2
#define MODE_AP_RPTR 3
#define MODE_ETHERNET_HOST (0<<8) /* rx payloads converted */
#define MODE_LLC_HOST (1<<8) /* rx payloads left as is */
#define MODE_AIRONET_EXTEND (1<<9) /* enable Aironet extenstions */
#define MODE_AP_INTERFACE (1<<10) /* enable ap interface extensions */
#define MODE_ANTENNA_ALIGN (1<<11) /* enable antenna alignment */
#define MODE_ETHER_LLC (1<<12) /* enable ethernet LLC */
#define MODE_LEAF_NODE (1<<13) /* enable leaf node bridge */
#define MODE_CF_POLLABLE (1<<14) /* enable CF pollable */
	u16 rmode; /* receive mode */
#define RXMODE_BC_MC_ADDR 0
#define RXMODE_BC_ADDR 1 /* ignore multicasts */
#define RXMODE_ADDR 2 /* ignore multicast and broadcast */
#define RXMODE_RFMON 3 /* wireless monitor mode */
#define RXMODE_RFMON_ANYBSS 4
#define RXMODE_LANMON 5 /* lan style monitor -- data packets only */
#define RXMODE_DISABLE_802_3_HEADER (1<<8) /* disables 802.3 header on rx */
#define RXMODE_NORMALIZED_RSSI (1<<9) /* return normalized RSSI */
	u16 fragThresh;
	u16 rtsThres;
	u8 macAddr[6];
	u8 rates[8];
	u16 shortRetryLimit;
	u16 longRetryLimit;
	u16 txLifetime; /* in kusec */
	u16 rxLifetime; /* in kusec */
	u16 stationary;
	u16 ordering;
	u16 u16deviceType; /* for overriding device type */
	u16 cfpRate;
	u16 cfpDuration;
	u16 _reserved1[3];
	/*---------- Scanning/Associating ----------*/
	u16 scanMode;
#define SCANMODE_ACTIVE 0
#define SCANMODE_PASSIVE 1
#define SCANMODE_AIROSCAN 2
	u16 probeDelay; /* in kusec */
	u16 probeEnergyTimeout; /* in kusec */
        u16 probeResponseTimeout;
	u16 beaconListenTimeout;
	u16 joinNetTimeout;
	u16 authTimeout;
	u16 authType;
#define AUTH_OPEN 0x1
#define AUTH_ENCRYPT 0x101
#define AUTH_SHAREDKEY 0x102
#define AUTH_ALLOW_UNENCRYPTED 0x200
	u16 associationTimeout;
	u16 specifiedApTimeout;
	u16 offlineScanInterval;
	u16 offlineScanDuration;
	u16 linkLossDelay;
	u16 maxBeaconLostTime;
	u16 refreshInterval;
#define DISABLE_REFRESH 0xFFFF
	u16 _reserved1a[1];
	/*---------- Power save operation ----------*/
	u16 powerSaveMode;
#define POWERSAVE_CAM 0
#define POWERSAVE_PSP 1
#define POWERSAVE_PSPCAM 2
	u16 sleepForDtims;
	u16 listenInterval;
	u16 fastListenInterval;
	u16 listenDecay;
	u16 fastListenDelay;
	u16 _reserved2[2];
	/*---------- Ap/Ibss config items ----------*/
	u16 beaconPeriod;
	u16 atimDuration;
	u16 hopPeriod;
	u16 channelSet;
	u16 channel;
	u16 dtimPeriod;
	u16 bridgeDistance;
	u16 radioID;
	/*---------- Radio configuration ----------*/
	u16 radioType;
#define RADIOTYPE_DEFAULT 0
#define RADIOTYPE_802_11 1
#define RADIOTYPE_LEGACY 2
	u8 rxDiversity;
	u8 txDiversity;
	u16 txPower;
#define TXPOWER_DEFAULT 0
	u16 rssiThreshold;
#define RSSI_DEFAULT 0
        u16 modulation;
	u16 shortPreamble;
	u16 homeProduct;
	u16 radioSpecific;
	/*---------- Aironet Extensions ----------*/
	u8 nodeName[16];
	u16 arlThreshold;
	u16 arlDecay;
	u16 arlDelay;
	u16 _reserved4[1];
	/*---------- Aironet Extensions ----------*/
	u16 magicAction;
#define MAGIC_ACTION_STSCHG 1
#define MACIC_ACTION_RESUME 2
#define MAGIC_IGNORE_MCAST (1<<8)
#define MAGIC_IGNORE_BCAST (1<<9)
#define MAGIC_SWITCH_TO_PSP (0<<10)
#define MAGIC_STAY_IN_CAM (1<<10)
	u16 magicControl;
	u16 autoWake;
} ConfigRid;


typedef struct {
  u16 len;
  u8 mac[6];
  u16 mode;
  u16 errorCode;
  u16 sigQuality;
  u16 SSIDlen;
  char SSID[32];
  char apName[16];
  char bssid[4][6];
  u16 beaconPeriod;
  u16 dimPeriod;
  u16 atimDuration;
  u16 hopPeriod;
  /* Changed below to work with 
   * brain dead compilers 
   */
  union {
    u16		u16DsChannel;
    u16		u16HopSet;
  }frq;
  u16		u16HopPattern;			// 0x0062
  u16		u16HopsToBackbone;		// 0x0064
  u16           apTotalLoad;
  u16           generatedLoad;
  u16           accumulatedArl;
  u16           signalQuality;
  u16           currentXmitRate;
  u16           apDevExtensions;
  u16           normalizedSignalStrength;
} StatRid;	/* ns-collision */


//: Capabilities
typedef struct _STCAPS{
	u16		u16RidLen;			//0x0000
	u8		au8OUI[3];			//0x0002
	u8		nothing;			//0x0005
	u16		ProuctNum;			//0x0006
	u8		au8ManufacturerName[32];	//0x0008
	u8		au8ProductName[16];		//0x0028
	u8		au8ProductVersion[8];		//0x0038
	u8		au8FactoryAddress[6];		//0x0040
	u8		au8AironetAddress[6];		//0x0046
	u16		u16RadioType;			//0x004C
	u16		u16RegDomain;			//0x004E
	u8		au8Callid[6];			//0x0050
	u8		au8SupportedRates[8];		//0x0056
	u8		u8RxDiversity;			//0x005E
	u8		u8TxDiversity;			//0x005F
	u16		au16TxPowerLevels[8];		//0x0060
	u16		u16HardwareVersion;		//0x0070
	u16		u16HardwareCapabilities;	//0x0072
	u16		u16TemperatureRange;		//0x0074
	u16		u16SoftwareVersion;		//0x0076
	u16		u16SoftwareSubVersion;		//0x0078
	u16		u16InterfaceVersion	;	//0x007A
	u16		u16SoftwareCapabilities;	//0x007C
	u16		u16BootBlockVersion;		//0x007E
	u16		u16SupportedHardwareRev;	//0x0080
        u16             u16SoftwareCapabilities2;       //0x0082
        u8              au8SerialNumber[12];            //0x0084
        u16             u16AllowedFrequencies[14];      //0x0090
}STCAPS;

// Extended capabilities.

typedef struct _EXSTCAPS{
	u16		u16RidLen;			//0x0000
	u8		au8OUI[3];			//0x0002
	u8		nothing;			//0x0005
	u16		ProuctNum;			//0x0006
	u8		au8ManufacturerName[32];	//0x0008
	u8		au8ProductName[16];		//0x0028
	u8		au8ProductVersion[8];		//0x0038
	u8		au8FactoryAddress[6];		//0x0040
	u8		au8AironetAddress[6];		//0x0046
	u16		u16RadioType;			//0x004C
	u16		u16RegDomain;			//0x004E
	u8		au8Callid[6];			//0x0050
	u8		au8SupportedRates[8];		//0x0056
	u8		u8RxDiversity;			//0x005E
	u8		u8TxDiversity;			//0x005F
	u16		au16TxPowerLevels[8];		//0x0060
	u16		u16HardwareVersion;		//0x0070
	u16		u16HardwareCapabilities;	//0x0072
	u16		u16TemperatureRange;		//0x0074
	u16		u16SoftwareVersion;		//0x0076
	u16		u16SoftwareSubVersion;		//0x0078
	u16		u16InterfaceVersion	;	//0x007A
	u16		u16SoftwareCapabilities;	//0x007C
	u16		u16BootBlockVersion;		//0x007E
	u16		u16SupportedHardwareRev;	//0x0080
        u16             u16ExtSoftwareCapabilities;     //0x0082
}EXSTCAPS;


// Driver type element 

typedef struct _DRTYPE {
  u8  version[4];           /* Major,Minor,rel     */
  u8  flashcode;	    /* Flash type byte     */
  u16 devtype;              /* device type koala/venus/350 etc. */
}DRVRTYPE;
 
    


/*
 * MIC rid 
 */

typedef struct _STMIC {
    unsigned short ridLen __attribute__ ((packed));
    unsigned short micState __attribute__ ((packed));
    unsigned short micMulticastValid __attribute__ ((packed));
    unsigned char  micMulticast[16] __attribute__ ((packed));
    unsigned short micUnicastValid __attribute__ ((packed));
    unsigned char  micUnicast[16] __attribute__ ((packed));
} STMIC __attribute__ ((packed));

typedef struct _STMICSTATISTICS {
    u32   Size;             // size
    u8    Enabled;          // MIC enabled or not
    u32   RxSuccess;        // successful packets received
    u32   RxIncorrectMIC;   // packets dropped due to incorrect MIC comparison
    u32   RxNotMICed;       // packets dropped due to not being MIC'd
    u32   RxMICPlummed;     // packets dropped due to not having a MIC plummed
    u32   RxWrongSequence;  // packets dropped due to sequence number violation
    u32   Reserve[32];
}STMICSTATISTICS32;

/*
 * Lifted from the aironet driver by
 * breed. Names changed to avoid collision
 */

#define RID_CAPABILITIES 0xFF00
#define RID_CONFIG     0xFF10
#define RID_SSID       0xFF11
#define RID_APLIST     0xFF12
#define RID_DRVNAME    0xFF13
#define RID_ETHERENCAP 0xFF14

#define RID_WEP_TEMP   0xFF15
#define RID_WEP_PERM   0xFF16
#define RID_MODULATION 0xFF17
#define RID_ACTUALCONFIG 0xFF20 /*readonly*/
#define RID_LEAPUSERNAME 0xFF23
#define RID_LEAPPASSWORD 0xFF24
#define RID_STATUS     0xFF50
#define RID_STATS      0xFF68
#define RID_STATSDELTA 0xFF69
#define RID_STATSDELTACLEAR 0xFF6A
#define RID_MIC        0xFF57


#define VADDR(Type,HOSTP)  (Type *)(HOSTP.VirtualHostAddress)
#define FALSE       0
#define TRUE        1
#define AIROMAGIC   0xa55a
#define AIROIOCTL   SIOCDEVPRIVATE
#define AIROIDIFC   AIROIOCTL + 1



/* Ioctl constants to be used in airo_ioctl.command */

#define	AIROGCAP  		0	// Capability rid
#define AIROGCFG		1       // USED A LOT 
#define AIROGSLIST		2	// System ID list 
#define AIROGVLIST		3       // List of specified AP's
#define AIROGDRVNAM		4	//  NOTUSED
#define AIROGEHTENC		5	// NOTUSED
#define AIROGWEPKTMP		6
#define AIROGWEPKNV		7
#define AIROGSTAT		8
#define AIROGSTATSC32		9
#define AIROGSTATSD32		10
#define AIROGMICRID             11      /* Get STMIC rid      */
#define AIROGMICSTATS           12      /* Get MIC statistics */
#define AIROGFLAGS              13      /* Get driver flags   */
#define AIROGID			14	/* Get driver type element */
#define AIRORRID		15      /* Read arbitrary rid      */
#define AIRORSWVERSION          17      /* Get driver version   */


/* Leave gap of 40 commands after AIROGSTATSD32 for future */

#define AIROPCAP               	AIROGSTATSD32 + 40
#define AIROPVLIST              AIROPCAP      + 1
#define AIROPSLIST		AIROPVLIST    + 1
#define AIROPCFG		AIROPSLIST    + 1
#define AIROPSIDS		AIROPCFG      + 1
#define AIROPAPLIST		AIROPSIDS     + 1
#define AIROPMACON		AIROPAPLIST   + 1	/* Enable mac  */
#define AIROPMACOFF		AIROPMACON    + 1 	/* Disable mac */
#define AIROPSTCLR		AIROPMACOFF   + 1
#define AIROPWEPKEY		AIROPSTCLR    + 1
#define AIROPWEPKEYNV		AIROPWEPKEY   + 1
#define AIROPLEAPPWD            AIROPWEPKEYNV + 1
#define AIROPLEAPUSR            AIROPLEAPPWD  + 1

/* Flash codes */

#define AIROFLSHRST	       AIROPWEPKEYNV  + 40 /* 100 */
#define AIROFLSHGCHR           AIROFLSHRST    + 1  /* 101 */
#define AIROFLSHSTFL           AIROFLSHGCHR   + 1  /* 102 */
#define AIROFLSHPCHR           AIROFLSHSTFL   + 1  /* 103 */
#define AIROFLPUTBUF           AIROFLSHPCHR   + 1  /* 104 */
#define AIRORESTART            AIROFLPUTBUF   + 1  /* 105 */

#define FLASHSIZE	32768
#define AUXMEMSIZE         (256 * 1024)





typedef enum {
    NONE,
    NOMIC,
    NOMICPLUMMED,
    SEQUENCE,
    INCORRECTMIC,
} MIC_ERROR;
  
/* 
 * List of active devices
 */
typedef struct cb20_devlist {
  struct net_device *dev;
  struct cb20_devlist *next;
}CB20_CARDS;  




struct cb20_info {
  struct net_device_stats	stats;
  int open;
#if (LINUX_VERSION_CODE < 0x020363)
  char name[8];
#endif
  struct net_device             *next;
  struct net_device             *dev;
  struct pci_dev           *pcip;
  MICMODULE       mod[2];                  /* MIC stuff         */
  u8              snap[8];
  u32             updateMultiSeq;
  u32             updateUniSeq;
  STMICSTATISTICS32 micstats;
#if (LINUX_VERSION_CODE < 0x20311)
  struct proc_dir_entry proc_entry;
  struct proc_dir_entry proc_statsdelta_entry;
  struct proc_dir_entry proc_stats_entry;
  struct proc_dir_entry proc_status_entry;
  struct proc_dir_entry proc_config_entry;
  struct proc_dir_entry proc_ssid_entry;
  struct proc_dir_entry proc_aplist_entry;
  struct proc_dir_entry proc_wepkey_entry;
#endif
  //  struct proc_dir_entry *proc_entry;
  struct proc_dir_entry *device;
  
  HOST_RX_DESC    rxfids[MAX_DESC];        /* *Virtaddr[]                      */
  HOST_TX_DESC    txfids[MAX_DESC];        /* *Virtaddr[]                      */
  HOST_RID_DESC   ConfigDesc;              /* Rid descriptor                   */
  unsigned   long ridbus;                  /* Phys addr of ConfigDesc! 3/31/02 */
  unsigned   long pci_controlbase;         /* PCI addr Contol                  */
  unsigned   char *controlregmembase;      /* Virtaddr                         */
  unsigned   int  controlmembasesize;      /* length                           */
  unsigned   long pci_auxbase;             /* PCI addr AUX                     */
  unsigned   char *auxregmembase;          /* Virtaddr                         */
  unsigned   int  auxmembasesize;          /* length                           */
  unsigned   char *SharedRegion;           /* Virtaddr                         */
  unsigned   int  SharedSize;              /* length                           */
  dma_addr_t      SharedBusaddr;           /* Bus address of shared descriptor region */
  unsigned   char *flashram;
  u32             flashsize;
  struct timer_list timer;
  int             irqline;
  int             iosize;                  /* IO Port region size */
  int             iobase;		   /* IO Base address     */
  int             registered;
  int             macstatus;
  ConfigRid       config;
  spinlock_t mpi_lock;			  /* Interrupt handler lock  */
  spinlock_t txlist_lock;
  spinlock_t txd_lock;
  spinlock_t aux_lock;
  spinlock_t cmd_lock;
  struct sk_buff_head txq;
  int txdfc;
  unsigned flags; 
  char rfMonitor;
  unsigned char saveConfig[2048];
  unsigned char saveSSID[2048];
  unsigned char saveAPList[2048];
#ifdef WIRELESS_EXT
  struct iw_statistics  wstats;         // wireless stats
#endif
};



