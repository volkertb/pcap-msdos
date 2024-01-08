/* 
 * SOURCE FILE FOR Motorola 68360 QUICC Sample Driver
 *
 * This device driver is derived from the Ethernet Bridge example from
 * Motorola.
 * It has been modified to interface to the Treck TCP/IP stack and to
 * provide readability.
 *
 * Since it is a derived work, it remains the property of Motorola Corp.
 *
 * We have left the definintions of the quicc registers as they
 * are in the QUICC user manual
 *
 * This is why they do not match TRECK coding standards.
 */


#include <trsocket.h>
#include <trquicc.h>


/* 
 * IMPORTANT
 * If the quicc device only receives a few frames and stops forever,
 * make sure that you are calling the tfQuiccRefillScc1RecvPool()
 * function from a low priority task
 */

/*
 * Local Function Prototypes
 */
void tfQuiccCreateXmitPool(int channel, int maxBd);
void tfQuiccCreateRecvPool(int channel, int maxBd);
void tfQuiccIssueCommand(unsigned short command);
void tfQuiccBusy(ttUserInterface interfaceHandle, int channel);
void tfQuiccRecvFrame(ttUserInterface interfaceHandle,
                      int             channel);
void tfQuiccXmitError(ttUserInterface interfaceHandle,int channel);
void tfQuiccScc1SendComplete(ttUserInterface interfaceHandle);
void tfQuiccEtherUp(int channel);
void tfQuiccEtherScc1BoardInit(void);
void tfQuiccEtherScc1Init(void);
void tfQuiccRefillScc1RecvPool(void);



static ttQuiccPtr tlQuiccPtr;

static ttQuiccBufDescPtr tlQuiccXmitBufDescPtr;
static ttQuiccBufDescPtr tlQuiccXmitIsrBufDescPtr;
static ttQuiccBufDescPtr tlQuiccXmitBufDescStartPtr;

static ttQuiccBufDescPtr tlQuiccRecvBufDescPtr;
static ttQuiccBufDescPtr tlQuiccRecvIsrBufDescPtr;
static ttQuiccBufDescPtr tlQuiccRecvBufDescStartPtr; 

static ttQuiccBufDescPtr tlQuiccRecvRefillBufDescPtr; 
unsigned long     tvQuiccRecvErrorCount;
unsigned long     tvQuiccRecvPacketCount;
unsigned long     tvQuiccSendPacketCount;
unsigned long     tvQuiccBusyPacketCount;
unsigned long     tvQuiccXmitWait;

/*
 * Four Interface handles for four SCC's 
 */
static ttUserInterface tlInterfaceArray[4];

/*
 * Table of ttUserBuffer Handles
 */
static ttUserBuffer    tlUserBufferList[TM_QUICC_ETHER_MAX_RECV_BUFD];

/*
 * Our index to the ttUserBuffer's
 */
static ttUserBufferPtr tlUserBufferPtr; 
static ttUserBufferPtr tlUserRefillBufferPtr; 



/*
 * Return the Physical Address for the device
 * For the QUICC, it should be read from EEPROM
 * because every ethernet address is Unique
 *
 * THIS ROUTINE MUST BE MODIFIED!
 *
 * To get an ethernet address address block for 
 * your company, you need to contact IEEE Standards
 * or visit http://standards.ieee.org/faqs/OUI.html
 * for information on getting an Company ID/OUI
 */
int tfQuiccGetPhyAddr(ttUserInterface userInterface,
        char *address)
{
/* 
 * We don't use the interface handle here, but we keep the compiler
 * from complaining about unused formal parameters
 */
    userInterface=userInterface;
    address[0]=0x00;
    address[1]=0xea;
    address[2]=0xc0;
    address[3]=0xf0;
    address[4]=0x00;
    address[5]=0x00;
    return 0;
}


/* 
 * Initialize SCC1 to be an ethernet channel
 */
void tfQuiccEtherScc1Init()
{
    char                  etherAddr[6];
    struct ethernet_pram *enet_pram;
    struct scc_regs      *regs;

/* Retrieve the Ethernet Address */
    tfQuiccGetPhyAddr((ttUserInterface)0,
            etherAddr);

    enet_pram = &tlQuiccPtr->pram[0].enet_scc;          
    regs = &tlQuiccPtr->scc_regs[0];

/* Disable transmit/receive */
    regs->scc_gsmra = 0;

/* 
 * Initialize paramater ram 
 */

/* points to start of receiving ring */
    enet_pram->rbase =   TM_QUICC_SCC1_RECV_BASE;
/* points to start of transmiting ring */
    enet_pram->tbase =   TM_QUICC_SCC1_XMIT_BASE;   
/* Function Codes for Transmit and Receive */
    enet_pram->rfcr =    TM_QUICC_SCC1_RECV_FUNC_CODE;
    enet_pram->tfcr =    TM_QUICC_SCC1_XMIT_FUNC_CODE;
/* We always receive a full ethernet frame so we set it to 1518 */
    enet_pram->mrblr =   TM_QUICC_SCC1_MAX_RECV_BUF_LEN;
/* A good ethernet CRC */
    enet_pram->c_mask =  TM_QUICC_ETHER_CRC_MASK;
/* The CRC we start with */
    enet_pram->c_pres =  TM_QUICC_ETHER_CRC_PRES;
/* These three registers are cleared for clarity sake */
    enet_pram->crcec =   TM_QUICC_ETHER_CRCEC;
    enet_pram->alec =    TM_QUICC_ETHER_ALEC;
    enet_pram->disfc =   TM_QUICC_ETHER_DISFC;
/* Pad Character */
    enet_pram->pads =    TM_QUICC_ETHER_PADS;
/* Max Retries when we have a collision */
    enet_pram->ret_lim = TM_QUICC_ETHER_RETRY_LIMIT;
/* Maximum Ethernet Frame Length (Always 1518) */
    enet_pram->mflr =    TM_QUICC_ETHER_MAX_FRAME_LEN;
/* Minimum Ethernet Frame Length (Always 64) */
    enet_pram->minflr =  TM_QUICC_ETHER_MIN_FRAME_LEN;
/* Maximum DMA Counts */
    enet_pram->maxd1 =   TM_QUICC_ETHER_MAX_DMA1_COUNT;
    enet_pram->maxd2 =   TM_QUICC_ETHER_MAX_DMA2_COUNT; 
/* Group Addresses */
    enet_pram->gaddr1 =  TM_QUICC_ETHER_GROUP_ADDR1;    
    enet_pram->gaddr2 =  TM_QUICC_ETHER_GROUP_ADDR2;    
    enet_pram->gaddr3 =  TM_QUICC_ETHER_GROUP_ADDR3;    
    enet_pram->gaddr4 =  TM_QUICC_ETHER_GROUP_ADDR4;    
/* 
 * Our physical (Ethernet) address (IN LITTLE ENDIAN)
 * If the physical address is:
 * 0x112233445566
 *
 * It is stored like this:
 * paddr_l=0x2211
 * paddr_m=0x4433
 * paddr_h=0x6655
 * 
 * If not done correctly, the QUICC will NOT receive any packets
 * with its destination address 
 */
    enet_pram->paddr_l = ((unsigned short)etherAddr[0]        &0x00ff)|
                         ((((unsigned short)etherAddr[1])<<8) &0xff00);
    enet_pram->paddr_m = ((unsigned short)etherAddr[2]        &0x00ff)|
                         ((((unsigned short)etherAddr[3])<<8) &0xff00);
    enet_pram->paddr_h = ((unsigned short)etherAddr[4]        &0x00ff)|
                         ((((unsigned short)etherAddr[5])<<8) &0xff00);

    enet_pram->p_per =   TM_QUICC_ETHER_P_PER;
/* Individual Hash Addresses */
    enet_pram->iaddr1 =  TM_QUICC_ETHER_INDV_ADDR1; 
    enet_pram->iaddr2 =  TM_QUICC_ETHER_INDV_ADDR2; 
    enet_pram->iaddr3 =  TM_QUICC_ETHER_INDV_ADDR3; 
    enet_pram->iaddr4 =  TM_QUICC_ETHER_INDV_ADDR4; 
/* Temp Registers */
    enet_pram->taddr_h = TM_QUICC_ETHER_T_ADDR_H;   
    enet_pram->taddr_m = TM_QUICC_ETHER_T_ADDR_M;
    enet_pram->taddr_l = TM_QUICC_ETHER_T_ADDR_L;
    tvQuiccRecvPacketCount=0L;
    tvQuiccSendPacketCount=0L;
    tvQuiccXmitWait=0L;
    tvQuiccRecvErrorCount=0L;

/* Tell the CP to initialize our Recv/Xmit parameters */
    tfQuiccIssueCommand(TM_QUICC_INIT_RXTX_SCC1);
}


/* 
 * Initialize SCC1 to be an ethernet channel    
 */
void tfQuiccEtherScc1BoardInit()
{
    unsigned char  scc1Vector;
    unsigned long  scc1Location;

/*  tlQuiccPtr->sim_mcr =   0x608f; */

/*
 * IMPLEMENTATION NOTE:
 *
 * This routine is very board specific
 * In this example we use the Atlas Communications Engines
 * HSB devlopment board for an example
 * Please refer to your boards design to modify this routine
 */

/* Atlas HSB Setup */
    tlQuiccPtr->si_sicr =   TM_QUICC_SI_SCC1_RCS_CLK2 |
                            TM_QUICC_SI_SCC1_TCS_CLK1;
                            
/* Make PortA Pins 0,1,8,9 for onchip use */
    tlQuiccPtr->pio_padir &= ~(TM_QUICC_PORTA_PA0 |
                               TM_QUICC_PORTA_PA1 |
                               TM_QUICC_PORTA_PA8 |
                               TM_QUICC_PORTA_PA9);
    
/* 
 * Use PortA pins 0,1 for SCC1 Ethernet RX/TX and 
 * pins 8,9 for Ethernet TCLK and RCLK 
 */
    tlQuiccPtr->pio_papar |= TM_QUICC_PORTA_PA0 |
                             TM_QUICC_PORTA_PA1 |
                             TM_QUICC_PORTA_PA8 |
                             TM_QUICC_PORTA_PA9;
                            
/* Clear the pending and in-service registers */
    tlQuiccPtr->intr_cipr = TM_QUICC_SCC1_INT_MASK;
    tlQuiccPtr->intr_cisr = TM_QUICC_SCC1_INT_MASK;
/* Set the CP mask register to accept interrupts from SCC1 */
    tlQuiccPtr->intr_cimr |= TM_QUICC_SCC1_INT_MASK;

    tlQuiccPtr->sdma_sdcr=   TM_QUICC_SDMA_INT_MASK |
                             TM_QUICC_SDMA_ARB_ID;

/* PortC Pins 4 & 5 are onchip */
    tlQuiccPtr->pio_pcdir &=   ~(TM_QUICC_PORTC_PC4 |
                                 TM_QUICC_PORTC_PC5);
/*  Use Pins 4 and 5 (SCC1 CTS and SD) for Ethernet CLSN and RENA */
    tlQuiccPtr->pio_pcso|=    TM_QUICC_PORTC_PC4 |
                              TM_QUICC_PORTC_PC5;
/* Clear the data register for pins 4 and 5*/
    tlQuiccPtr->pio_pcdat&=   ~(TM_QUICC_PORTC_PC4 |
                                TM_QUICC_PORTC_PC5);

/* Port B pin 12 is Ethernet TENA for HSB */
    tlQuiccPtr->pip_pbpar |= TM_QUICC_PORTB_PB12;
    tlQuiccPtr->pip_pbdir |= TM_QUICC_PORTB_PB12;
    
    scc1Vector=TM_QUICC_SCCA_INT | TM_QUICC_CICR_VECT;
    scc1Location = scc1Vector*4;

/*
 * IMPLEMENTATION NOTE:
 *
 * The Actual SCC1 ISR Wrapper Function in installed here
 * It must call the tfQuiccScc1HandlerIsr function
 * This routine may be in "C" if it is supported
 * or assembly if not.  It is VERY RTOS dependent
 * so it cannot be provided with the driver
 *
 * With some RTOS's a wrapper function may not be
 * needed.  All the driver cares about is that the
 * function tfQuiccScc1HandlerIsr function is called
 * when the interrupt occurs.
 *
 * An example of this routine in assembly for uC/OS:

*
* The Interrupt Routine for a CPU32 to call the Quicc Driver
*
_tfQuiccScc1Isr
* Save the Registers
    MOVEM.L D0-D7/A0-A6,-(A7)

* uC/OS Call to tell RTOS we are in an ISR
    JSR _OSIntEnter

* Call the handler
    JSR _tfQuiccScc1HandlerIsr

* uC/OS Call to tell the RTOS that we are leaving the ISR
    JSR _OSIntExit

* Restore the registers
    MOVEM.L (A7)+,D0-D7/A0-A6

* Return from Exception (Interrupt)
    RTE 

 */

/* Install the wrapper function */
    tfKernelInstallIsrHandler(tfQuiccScc1Isr,scc1Location);
}



/* 
 * Enable the SCC's  to recieve and transmit 
 */
void tfQuiccEtherUp(int channel)
{
    struct scc_regs *regs;


    regs = &tlQuiccPtr->scc_regs[channel];

/* clear all events on the SCC event register and CP event reg*/
    regs->scc_scce=0xffff;

/* Clear the appropriate bit in the CISR */
    switch (channel)
    {
        case TM_QUICC_SCC1_CHANNEL:
            tlQuiccPtr->intr_cisr = TM_QUICC_SCC1_INT_MASK;
            break;
        case TM_QUICC_SCC2_CHANNEL:
            tlQuiccPtr->intr_cisr = TM_QUICC_SCC2_INT_MASK;
            break;
        case TM_QUICC_SCC3_CHANNEL:
            tlQuiccPtr->intr_cisr = TM_QUICC_SCC3_INT_MASK;
            break;
        case TM_QUICC_SCC4_CHANNEL:
            tlQuiccPtr->intr_cisr = TM_QUICC_SCC4_INT_MASK;
            break;
        default:
            break;
    }


/* 
 * mask the events we sich to interrupt on
 */
    regs->scc_sccm = 
/* An error in transmitting has occured */
                     TM_QUICC_INTR_ETHER_E_XMIT |
/* A buffer has completed transmitting */
                     TM_QUICC_INTR_ETHER_XMIT_B |
/* A ethernet frame has been received */
                     TM_QUICC_INTR_ETHER_RECV_F |
/* The busy condition has occured */
                     TM_QUICC_INTR_ETHER_BUSY;
    
/* SCC1 data sync reg  (The pattern for Ethernet)*/
    regs->scc_dsr=   TM_QUICC_SCC_FRAME_SYNC_ETHER;         

/* protocol specific mode register */
    regs->scc_psmr = 
/* To disable the reception of Broadcast packets uncomment this line */
/*                     TM_QUICC_ETHER_BROADCAST | */
/* Turn on the CRC for ethernet */
                     TM_QUICC_ETHER_ENABLE_CRC | 
/* Ignore 22 bits for ethernet */
                     TM_QUICC_ETHER_NBITS_IGNORED;


/* SCC general mode reg */
    regs->scc_gsmrb = 
/* Carrier Detect Sampling */
                      TM_QUICC_CD_SAMPLE |
/* Clear to Send Sampling */
                      TM_QUICC_CTS_SAMPLE;

/* SCC general mode reg */
    regs->scc_gsmra = 
/* Transmit Clock Inversion */
                      TM_QUICC_XMIT_CLOCK_INV | 
/* Preamble Bit Length (48 always for ethernet) */
                      TM_QUICC_PREAMBLE_48 | 
/* Preamble Bit Pattern (Alternating '1' and '0' for ethernet */
                      TM_QUICC_PREAMBLE_PAT_10 | 
/* Ethernet Mode */
                      TM_QUICC_ETHERNET_PORT;

/* enable SCC1 receive/transmit operation */
    regs->scc_gsmra |= TM_QUICC_ENABLE_RECV | TM_QUICC_ENABLE_XMIT;
        
}

/* 
 * The main initialization routine for the quicc and the scc's 
 */
int tfQuiccEtherOpen(ttUserInterface interfaceHandle)
{

    unsigned long  quiccRegisters;

    quiccRegisters=TM_QUICC_MBAR_LOC|0x00000001UL;
    tlQuiccPtr = (ttQuicc *)(quiccRegisters&0xfffffffc);

/* Interface handle is not used, but could be for multiple ethernets */
    interfaceHandle=interfaceHandle;
/* Initialize default Ethernet parameters */
    tfQuiccEtherScc1Init();
/* Create our transmit pool */
    tfQuiccCreateXmitPool(TM_QUICC_SCC1_CHANNEL,
                          TM_QUICC_ETHER_MAX_XMIT_BUFD);
/* Create our receive pool (with receive buffers) */
    tfQuiccCreateRecvPool(TM_QUICC_SCC1_CHANNEL,
                          TM_QUICC_ETHER_MAX_RECV_BUFD);
/* Initial board specific parameters */
    tfQuiccEtherScc1BoardInit();
/* Bring the ethernet scc port online */
    tfQuiccEtherUp(TM_QUICC_SCC1_CHANNEL);

/* Save our interfaceHandle for ISR use */
    tlInterfaceArray[TM_QUICC_SCC1_CHANNEL]=interfaceHandle;
    return TM_ENOERROR;
}


/* 
 * Interrupt service routine called from actual
 * ISR handlers
 */ 
void tfQuiccScc1HandlerIsr()
{
    register unsigned short event;
    ttUserInterface         interfaceHandle;
    struct scc_regs *regs;
    int channel;
    unsigned short *bdStatusPtr;
#ifdef TM_QUICC_USE_SEND_COMPLETE_IRQ
    void **bdDataPtrPtr;
#endif /* TM_QUICC_USE_SEND_COMPLETE_IRQ */
    
    channel=TM_QUICC_SCC1_CHANNEL;
    interfaceHandle=tlInterfaceArray[channel];
/* Points to SCC regs */
    regs = &tlQuiccPtr->scc_regs[channel]; 

/* Check to see what events have occured */
    event = regs->scc_scce;
/* Cleared event bits we hill handle */
    regs->scc_scce= event;


#ifdef TM_QUICC_USE_SEND_COMPLETE_IRQ
    if( event & TM_QUICC_INTR_ETHER_XMIT_B ) 
    {
        bdStatusPtr = &tlQuiccXmitIsrBufDescPtr->bdStatus;
        bdDataPtrPtr = &tlQuiccXmitIsrBufDescPtr->bdDataPtr;
/* An ethernet frame has completed transmitting so notify the user*/
        while (((*bdStatusPtr & TM_QUICC_XMIT_READY)==0) 
              && (*bdDataPtrPtr != (void TM_FAR *)0))
        {
/* Zero it out so we know that we have freed this one before */
            *bdDataPtrPtr=(void TM_FAR *)0;
            if (*bdStatusPtr & TM_QUICC_XMIT_WRAP)
            {
                tlQuiccXmitIsrBufDescPtr = tlQuiccXmitBufDescStartPtr;
            }
            else
            {
                tlQuiccXmitIsrBufDescPtr++;
            }
            bdStatusPtr = &tlQuiccXmitIsrBufDescPtr->bdStatus;
            bdDataPtrPtr = &tlQuiccXmitIsrBufDescPtr->bdDataPtr;
            tfNotifySentInterfaceIsr(interfaceHandle,tlQuiccXmitIsrBufDescPtr->bdLength);
        }
    }
#endif /* TM_QUICC_USE_SEND_COMPLETE_IRQ */
    if ( event & TM_QUICC_INTR_ETHER_RECV_F )
    {
/* loop on frames with the Empty bit and SHORT (notify) bit is NOT set "==0" */
/* Only notify while the frames are not empty */
        bdStatusPtr = &tlQuiccRecvIsrBufDescPtr->bdStatus;
        while ( (*bdStatusPtr & (TM_QUICC_RECV_E_SHORT | 
                                 TM_QUICC_RECV_EMPTY)) == 0 )
        {
/*    
 * Since we do not accept SHORT frames, we use the SHORT bit to 
 * mean that this frame has been notified
 * IF THIS DRIVER IS CHANGED TO ALLOW SHORT FRAMES, ANOTHER BIT
 * WILL NEED TO BE USED
 */
            *bdStatusPtr |= TM_QUICC_RECV_E_SHORT;
            if ((*bdStatusPtr & TM_QUICC_RECV_WRAP) != 0)
            {
                tlQuiccRecvIsrBufDescPtr = tlQuiccRecvBufDescStartPtr;
            }
            else
            {
                tlQuiccRecvIsrBufDescPtr++;
            }
/* Don't notify on error packets */
            if ((*bdStatusPtr & 
                    (TM_QUICC_RECV_ERROR & (~TM_QUICC_RECV_E_SHORT))) == 0)
            {
                tfNotifyReceiveInterfaceIsr(interfaceHandle);
            }
            bdStatusPtr = &tlQuiccRecvIsrBufDescPtr->bdStatus;
        }
    }
    if ( event &  TM_QUICC_INTR_ETHER_E_XMIT )
    {
/* An error on Transmit has occured */
        tfQuiccXmitError(interfaceHandle,channel);
    }
    if ( event & TM_QUICC_INTR_ETHER_BUSY )
    {
/* Busy Condition has been set */
        tfQuiccBusy(interfaceHandle,channel);
    }
/* Clear the appropriate bit in the CISR */
    tlQuiccPtr->intr_cisr |= TM_QUICC_SCC1_INT_MASK;
}

/*
 * Send a packet to the network
 */
int tfQuiccSend(ttUserInterface interfaceHandle,
                char TM_FAR *bufferPtr,
                int          length,
                int          flag)
{
    ttQuiccBufDescPtr bufDescPtr;    
    unsigned short *bdStatusPtr;
    void          **bdDataPtrPtr;
/* Wait for a free buffer descriptor */    
    while (tlQuiccXmitBufDescPtr->bdStatus & TM_QUICC_XMIT_READY)
    {
        tvQuiccXmitWait++;
    }
    bufDescPtr=tlQuiccXmitBufDescPtr++;
    bdStatusPtr=&bufDescPtr->bdStatus;
    if (*bdStatusPtr & TM_QUICC_XMIT_WRAP)
    {
        tlQuiccXmitBufDescPtr = tlQuiccXmitBufDescStartPtr;
    }
    bufDescPtr->bdDataPtr=(void TM_FAR *)bufferPtr;
    bufDescPtr->bdLength=length;
/* Clear the previous status (but preserve the WRAP bit) */
    bufDescPtr->bdStatus &= TM_QUICC_XMIT_WRAP;
/* If this is the last buffer in the frame then xmit it */
    if (flag==TM_USER_BUFFER_LAST)
    {
        *bdStatusPtr |= (TM_QUICC_XMIT_READY |
                         TM_QUICC_XMIT_PAD |

#ifdef TM_QUICC_USE_SEND_COMPLETE_IRQ
                         TM_QUICC_XMIT_INTR | 
#endif /* TM_QUICC_USE_SEND_COMPLETE_IRQ */

                         TM_QUICC_XMIT_LAST |
                         TM_QUICC_XMIT_CRC);
    }
    else
    {
/* More buffers to come */
        *bdStatusPtr |= TM_QUICC_XMIT_READY;
    }
    tvQuiccSendPacketCount++;

#ifndef TM_QUICC_USE_SEND_COMPLETE_IRQ
/* Inlined send complete */
    bdStatusPtr = &tlQuiccXmitIsrBufDescPtr->bdStatus;
    bdDataPtrPtr = &tlQuiccXmitIsrBufDescPtr->bdDataPtr;
/* An ethernet frame has completed transmitting so notify the user*/
    while (((*bdStatusPtr & TM_QUICC_XMIT_READY)==0) 
          && (*bdDataPtrPtr != (void TM_FAR *)0))
    {
/* Zero it out so we know that we have freed this one before */
        *bdDataPtrPtr=(void TM_FAR *)0;
        if (*bdStatusPtr & TM_QUICC_XMIT_WRAP)
        {
            tlQuiccXmitIsrBufDescPtr = tlQuiccXmitBufDescStartPtr;
        }
        else
        {
            tlQuiccXmitIsrBufDescPtr++;
        }
        bdStatusPtr = &tlQuiccXmitIsrBufDescPtr->bdStatus;
        bdDataPtrPtr = &tlQuiccXmitIsrBufDescPtr->bdDataPtr;
        tfSendCompleteInterface(interfaceHandle);
    }
#endif /* NOT TM_QUICC_USE_SEND_COMPLETE_IRQ */
    return TM_ENOERROR;
}


/*
 * A Transmit Error has occured
 */ 
void tfQuiccXmitError(ttUserInterface interfaceHandle,int channel)
{
    interfaceHandle=interfaceHandle;
/* issue the RESTART TRANSMIT command to the CP */
    tfQuiccIssueCommand(TM_QUICC_RESTART_TX | ( channel << 6));
}


/*
 * tfQuiccReceive
 * Receive a buffer from the chip and pass back to the stack
 */
int tfQuiccReceive(ttUserInterface  interfaceHandle,
                   char TM_FAR    **dataPtrPtr,
                   int  TM_FAR     *lengthPtr,
                   ttUserBufferPtr  userBufferHandlePtr)
{
    int errorCode;
    ttQuiccBufDescPtr bufDescPtr;
    unsigned short *bdStatusPtr;
    
    errorCode=TM_ENOERROR;
    
    bdStatusPtr = &tlQuiccRecvBufDescPtr->bdStatus;

/* Skip pass error packets who are not empty */
    while (((*bdStatusPtr &  TM_QUICC_RECV_EMPTY)==0) &&
           ((*bdStatusPtr & (TM_QUICC_RECV_ERROR &
                           (~TM_QUICC_RECV_E_SHORT))) != 0))
    {
        tvQuiccRecvErrorCount++;
        tlQuiccRecvBufDescPtr++;
/* Return the userBuffer Handle for this data pointer */
        *tlUserBufferPtr++;
        if (*bdStatusPtr & TM_QUICC_RECV_WRAP)
        {
            tlQuiccRecvBufDescPtr = tlQuiccRecvBufDescStartPtr;
            tlUserBufferPtr=tlUserBufferList;
        }
        bdStatusPtr = &tlQuiccRecvBufDescPtr->bdStatus;
    }
/* Check to see if we own the Buffer Descriptor */
    if (*bdStatusPtr & TM_QUICC_RECV_EMPTY)
    {
/* Should NEVER Happen */
        errorCode=TM_EWOULDBLOCK;
    }
    else
    {
        bufDescPtr=tlQuiccRecvBufDescPtr++;
/* Return the userBuffer Handle for this data pointer */
        *userBufferHandlePtr = *tlUserBufferPtr++;
        if (*bdStatusPtr & TM_QUICC_RECV_WRAP)
        {
            tlQuiccRecvBufDescPtr = tlQuiccRecvBufDescStartPtr;
            tlUserBufferPtr=tlUserBufferList;
        }
/* We are returning the pointer to the data */
        *dataPtrPtr = (char TM_FAR *)(bufDescPtr->bdDataPtr);

/* Zero out the bufferPtr so we can refill it later */
        bufDescPtr->bdDataPtr=(void TM_FAR *)0;

/* We are returning the length */
        *lengthPtr  = bufDescPtr->bdLength;
/* Up our packet count */
        tvQuiccRecvPacketCount++;
    }
    return(errorCode);
}


/* 
 * Interrupt called on a busy condition on the line. 
 * Lost some frames here.
 */
void tfQuiccBusy(ttUserInterface interfaceHandle, int channel)
{
    interfaceHandle=interfaceHandle;
    channel=channel;
    tvQuiccBusyPacketCount++;
}


/* 
 * tfQuiccIssueCmd() is used to send commands to the communication processor
 * and waits till the command is completed
 */
void tfQuiccIssueCommand(unsigned short command)
{
    unsigned short *commandReg;

/* Save the command register for the CP */  
    commandReg = &tlQuiccPtr->cp_cr;
/* Issue the command */
    *commandReg = (command | TM_QUICC_COMMAND_FLAG);
/* wait for the CP zero the CMD_FLG */
    while( (*(volatile short *)commandReg) & TM_QUICC_COMMAND_FLAG )
    {
/* 
 * If this loop never exits:
 *
 * 1) check to make sure that the TM_QUICC_MBAR_LOC is correct 
 *
 * 2) that the tlQuicc value is correct and has been initialized
 *
 * 3) Make sure the compiler has generated good code
 *    Some compilers have been known to just AND a CPU register with
 *    TM_QUICC_COMMAND_FLAG instead of ANDING the contents of the 
 *    memory location for commandReg with TM_QUICC_COMMAND_FLAG.
 *    WORK AROUND: move the check into a seperate function
 *    and call that function from inside this loop.
 */
       ;
    } 
}


/* 
 * This routine is preparing for each SCC a pool of READY buffer 
 * descriptors that are ready to recieve. The last buffer descriptor
 * has the wrap bit set.
 */
void tfQuiccCreateRecvPool(int channel, int maxBd)
{
    int i;
    ttQuiccBufDescPtr tempBufDescPtr;
    struct ethernet_pram *enetPramAddr;
    struct scc_regs *regs;
    ttUserBuffer userBuffer;

    enetPramAddr = &tlQuiccPtr->pram[channel].enet_scc;
    regs = &tlQuiccPtr->scc_regs[channel];

    tempBufDescPtr = TM_QUICC_RECV_BD_ADDR(tlQuiccPtr,enetPramAddr);
/* Save it off for our use in the recv routine */    
    tlQuiccRecvRefillBufDescPtr=tlQuiccRecvBufDescStartPtr=
            tlQuiccRecvIsrBufDescPtr=tlQuiccRecvBufDescPtr=
            tempBufDescPtr;
    tlUserRefillBufferPtr=tlUserBufferPtr=tlUserBufferList;

    for(i=0;i< maxBd;i++,tempBufDescPtr++ ) 
    {
        tempBufDescPtr->bdDataPtr=(tfGetEthernetBuffer(&userBuffer)-2);
        if (tempBufDescPtr->bdDataPtr==(char TM_FAR *)0)
        {
/* No memory so stop the create here */
            break;
        }
        tlUserBufferList[i]=userBuffer;
        tempBufDescPtr->bdLength = 0;
        tempBufDescPtr->bdStatus = (TM_QUICC_RECV_EMPTY|TM_QUICC_RECV_INTR);
    }
    --tempBufDescPtr;
 /* signify last bit on */
    tempBufDescPtr->bdStatus |= TM_QUICC_RECV_WRAP;

}

/*
 * General function
 * Can be used to configure the device
 * or refill the receive pool
 */
int tfQuiccIoctl(ttUserInterface interfaceHandle, int flag)
{
    if (flag&TM_QUICC_REFILL_SCC1)
    {
        tfQuiccRefillScc1RecvPool();
    }
#ifndef TM_QUICC_USE_SEND_COMPLETE_IRQ
    if (flag&TM_QUICC_SEND_COMPLETE)
    {
        tfQuiccScc1SendComplete(interfaceHandle);
    }
#endif /* NOT TM_QUICC_USE_SEND_COMPLETE_IRQ */
    return TM_ENOERROR;
}
 

/*
 * Walk through receive list and allocate new buffers
 * after they have been processed
 */
void tfQuiccRefillScc1RecvPool()
{
    ttUserBuffer userBuffer;
    unsigned short *bdStatusPtr;
    void          **bdDataPtrPtr;

    bdDataPtrPtr = &tlQuiccRecvRefillBufDescPtr->bdDataPtr;
    bdStatusPtr  = &tlQuiccRecvRefillBufDescPtr->bdStatus;
/* Loop while it is empty or and error packet */
    while ((*bdDataPtrPtr==(void TM_FAR *)0) ||
          ((*bdStatusPtr & TM_QUICC_RECV_EMPTY)==0) &&
          ((*bdStatusPtr & (TM_QUICC_RECV_ERROR &
                           (~TM_QUICC_RECV_E_SHORT))) != 0))
    {
/* Check to make sure it is not an error packet */
        if ((*bdStatusPtr &  (TM_QUICC_RECV_ERROR &  
                            (~TM_QUICC_RECV_E_SHORT))) == 0)
        {
/* If it is not get a new buffer, otherwise just reuse it */
            tlQuiccRecvRefillBufDescPtr->bdDataPtr=
                    (tfGetEthernetBuffer(&userBuffer)-2);
            if (*bdDataPtrPtr == (char TM_FAR *)0)
            {
/* No memory so stop the refill */
                break;
            }
            *tlUserRefillBufferPtr=userBuffer;
        }
        tlQuiccRecvRefillBufDescPtr->bdLength = 0;
/* Clear the status Except for the wrap bit */
        *bdStatusPtr &= TM_QUICC_RECV_WRAP;
/* Mark this BD as ready to use */
        *bdStatusPtr |= (TM_QUICC_RECV_EMPTY|TM_QUICC_RECV_INTR);
        if (*bdStatusPtr & TM_QUICC_RECV_WRAP)
        {
            tlQuiccRecvRefillBufDescPtr = tlQuiccRecvBufDescStartPtr;
            tlUserRefillBufferPtr=tlUserBufferList;
        }
        else
        {
            tlQuiccRecvRefillBufDescPtr++;
            tlUserRefillBufferPtr++;
        }
        bdDataPtrPtr = &tlQuiccRecvRefillBufDescPtr->bdDataPtr;
        bdStatusPtr  = &tlQuiccRecvRefillBufDescPtr->bdStatus;
    }
}

#ifndef TM_QUICC_USE_SEND_COMPLETE_IRQ
/*
 * Walk through send list and post send completes
 */
void tfQuiccScc1SendComplete(ttUserInterface interfaceHandle)
{
    unsigned short *bdStatusPtr;
    void          **bdDataPtrPtr;

    bdStatusPtr = &tlQuiccXmitIsrBufDescPtr->bdStatus;
    bdDataPtrPtr = &tlQuiccXmitIsrBufDescPtr->bdDataPtr;
/* An ethernet frame has completed transmitting so notify the user*/
    while (((*bdStatusPtr & TM_QUICC_XMIT_READY)==0) 
          && (*bdDataPtrPtr != (void TM_FAR *)0))
    {
/* Zero it out so we know that we have freed this one before */
        *bdDataPtrPtr=(void TM_FAR *)0;
        if (*bdStatusPtr & TM_QUICC_XMIT_WRAP)
        {
            tlQuiccXmitIsrBufDescPtr = tlQuiccXmitBufDescStartPtr;
        }
        else
        {
            tlQuiccXmitIsrBufDescPtr++;
        }
        bdStatusPtr = &tlQuiccXmitIsrBufDescPtr->bdStatus;
        bdDataPtrPtr = &tlQuiccXmitIsrBufDescPtr->bdDataPtr;
        tfSendCompleteInterface(interfaceHandle);
    }

}
#endif /* NOT TM_QUICC_USE_SEND_COMPLETE_IRQ */


/* 
 * Prepare a transmit pool, set the status bits in the buffer descriptors
 * to be not READY to transmit and set the last buffer descriptor to
 * be wrapped
 */
void tfQuiccCreateXmitPool(int channel, int maxBd)
{
    int i;
    ttQuiccBufDescPtr tempBufDescPtr;
    struct ethernet_pram *enetPramAddr;
    struct scc_regs *regs;

/* Save the ethernet parameter address */
    enetPramAddr = &tlQuiccPtr->pram[channel].enet_scc;
    regs = &tlQuiccPtr->scc_regs[channel];

    tempBufDescPtr = TM_QUICC_XMIT_BD_ADDR(tlQuiccPtr,enetPramAddr);

/* Save it off for our use in the xmit routine */    
    tlQuiccXmitIsrBufDescPtr=tlQuiccXmitBufDescStartPtr=
            tlQuiccXmitBufDescPtr=tempBufDescPtr;
    for(i=0;i< maxBd;i++,tempBufDescPtr++ ) {
        tempBufDescPtr-> bdStatus = 0;
        tempBufDescPtr-> bdLength = 0;
        tempBufDescPtr-> bdDataPtr = (void TM_FAR *)0;
    }
    --tempBufDescPtr;
/* signify last bit on */
    tempBufDescPtr->bdStatus |= TM_QUICC_XMIT_WRAP; 
}



