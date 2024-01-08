/* 
 * HEADER FILE FOR Motorola 68360 QUICC Sample Driver
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



/*
 * Quicc Buffer Descriptors
 */
typedef struct tsQuiccBufDesc
{
    unsigned short bdStatus;
    unsigned short bdLength;
    void TM_FAR *  bdDataPtr;
}ttQuiccBufDesc;

typedef ttQuiccBufDesc TM_FAR * ttQuiccBufDescPtr;

/* Number of buffer discriptors */
#define	TM_QUICC_ETHER_MAX_RECV_BUFD	(unsigned short)25	
#define TM_QUICC_ETHER_MAX_XMIT_BUFD	(unsigned short)25

/*
 * IOCTL Defines
 */
#define TM_QUICC_REFILL_SCC1   (int)0x0001
#define TM_QUICC_SEND_COMPLETE (int)0x0002


/*
 * Function Prototypes
 */

/*
 * The Actual SCC1 ISR Wrapper Function
 * it must call the tfQuiccScc1HandlerIsr function
 * This routine may be in "C" if it is supported
 * or assembly if not.  It is VERY RTOS dependent
 * so it cannot be provided with the driver
 *
 * An example of this routine in assembly for uC/OS:

*
* The Interrupt Routine for a CPU32 to call the Quicc Driver
*
_tfQuiccScc1Isr
* Save the Registers
	MOVEM.L	D0-D7/A0-A6,-(A7)

* uC/OS Call to tell RTOS we are in an ISR
	JSR	_OSIntEnter

* Call the handler
	JSR	_tfQuiccScc1HandlerIsr

* uC/OS Call to tell the RTOS that we are leaving the ISR
	JSR	_OSIntExit

* Restore the registers
	MOVEM.L	(A7)+,D0-D7/A0-A6

* Return from Exception (Interrupt)
	RTE	

 */

void tfQuiccScc1Isr(void);


int tfQuiccGetPhyAddr(ttUserInterface userInterface,
        char *address);

int tfQuiccIoctl(ttUserInterface interfaceHandle, int flag);

int tfQuiccReceive(ttUserInterface  interfaceHandle,
                   char TM_FAR    **dataPtrPtr,
                   int  TM_FAR     *lengthPtr,
                   ttUserBufferPtr  userBufferHandlePtr);

int tfQuiccSend(ttUserInterface interfaceHandle,
                char TM_FAR *buffer,
                int          len,
                int          flag);
int tfQuiccEtherOpen(ttUserInterface interfaceHandle);

void tfQuiccScc1HandlerIsr(void);

/*
 * Quicc Defines
 */
#define TM_QUICC_MBAR_LOC      0xffff0000UL

/* (0b10100000 = VBA of 5) */
#define TM_QUICC_CICR_VECT     0x000000a0UL 

/*
 * The CICR register is used to set the vector base address, the IRQ Level
 * set the priorities of the SCC's.  If it is not set correctly then the 
 * SCC's will NEVER interrupt.
 *
 * We set it up to the following default:
 *
 * Priority: Lowest to Highest
 * SCCd = SCC4
 * SCCc = SCC3
 * SCCb = SCC2
 * SCCa = SCC1
 *
 * IRQ Level = 4
 *
 * Highest Priority = Original Order 0b11111
 *
 * Vector Base Address (High order of the 8 bit vector) = TM_QUICC_CICR_VECT
 */
#define TM_QUICC_CICR_DEFAULT  0x00E49F00UL|TM_QUICC_CICR_VECT


/*
 * Timer Interrupt Number
 */
#define TM_QUICC_TMR1_INT      0x00000019UL

/*
 * SCC(x) Interrupt Numbers
 */
#define TM_QUICC_SCCA_INT      0x0000001eUL
#define TM_QUICC_SCCB_INT      0x0000001dUL
#define TM_QUICC_SCCC_INT      0x0000001cUL
#define TM_QUICC_SCCD_INT      0x0000001bUL

/*
 * Timer Interrupt Bit
 */
#define TM_QUICC_TMR1_INT_MASK 0x02000000UL

/* 
 * SCC(x) Interrupt Bit
 */
#define TM_QUICC_SCC1_INT_MASK 0x40000000UL
#define TM_QUICC_SCC2_INT_MASK 0x20000000UL
#define TM_QUICC_SCC3_INT_MASK 0x10000000UL
#define TM_QUICC_SCC4_INT_MASK 0x08000000UL

/* 
 * SCC(x) Commands
 */
#define TM_QUICC_INIT_RXTX_SCC1 (unsigned short)0x0001
#define TM_QUICC_INIT_RXTX_SCC2 (unsigned short)0x0041
#define TM_QUICC_INIT_RXTX_SCC3 (unsigned short)0x0081
#define TM_QUICC_INIT_RXTX_SCC4 (unsigned short)0x00C1

#define TM_QUICC_RESTART_TX (unsigned short)0x0600

/* 
 * Ethernet Defines 
 */
/* Perform 32 bit CRC */
#define	TM_QUICC_ETHER_CRC_PRES	        0xffffffffUL 
/* Comply with 32 bit CRC */	
#define	TM_QUICC_ETHER_CRC_MASK	        0xdebb20e3UL 
/* Zero for clarity */
#define	TM_QUICC_ETHER_CRCEC	        0x00000000UL
#define	TM_QUICC_ETHER_ALEC	            0x00000000UL
#define	TM_QUICC_ETHER_DISFC	        0x00000000UL
#define	TM_QUICC_ETHER_PADS	            0x00000000UL
/* 15 Collision Trys */
#define	TM_QUICC_ETHER_RETRY_LIMIT	    (unsigned short)0x000f 
/* Maxmimum Ethernet Frame Length = 1518 (DONT CHANGE) */
#define	TM_QUICC_ETHER_MAX_FRAME_LEN    (unsigned short)0x05ee
/* Minimum Ethernet Frame Length = 64 (DONT CHANGE) */
#define	TM_QUICC_ETHER_MIN_FRAME_LEN	(unsigned short)0x0040
/* Maximum Ethernet DMA count */
#define	TM_QUICC_ETHER_MAX_DMA1_COUNT	TM_QUICC_ETHER_MAX_FRAME_LEN
#define	TM_QUICC_ETHER_MAX_DMA2_COUNT	TM_QUICC_ETHER_MAX_FRAME_LEN
/* Clear the group addresses */
#define	TM_QUICC_ETHER_GROUP_ADDR1	    0x00000000UL
#define	TM_QUICC_ETHER_GROUP_ADDR2	    0x00000000UL	
#define	TM_QUICC_ETHER_GROUP_ADDR3	    0x00000000UL	
#define	TM_QUICC_ETHER_GROUP_ADDR4	    0x00000000UL	
/* Not Used */
#define	TM_QUICC_ETHER_P_PER	        0x00000000UL
/* Individual Hash table is not used */
#define	TM_QUICC_ETHER_INDV_ADDR1	    0x00000000UL
#define	TM_QUICC_ETHER_INDV_ADDR2	    0x00000000UL
#define	TM_QUICC_ETHER_INDV_ADDR3	    0x00000000UL
#define	TM_QUICC_ETHER_INDV_ADDR4	    0x00000000UL
/* Zeroed */
#define	TM_QUICC_ETHER_T_ADDR_H	        0x00000000UL 
#define	TM_QUICC_ETHER_T_ADDR_M	        0x00000000UL 
#define	TM_QUICC_ETHER_T_ADDR_L	        0x00000000UL 


/*
 * SCC Channel Numbers
 */
#define TM_QUICC_SCC1_CHANNEL 0
#define TM_QUICC_SCC2_CHANNEL 1
#define TM_QUICC_SCC3_CHANNEL 2
#define TM_QUICC_SCC4_CHANNEL 3

/*
 * SCC Parameter Ram 
 */
#define TM_QUICC_COMMAND_FLAG (unsigned short)0x0001
#define	TM_QUICC_SCC1_RECV_BASE	(unsigned short)0x0000  
#define	TM_QUICC_SCC1_XMIT_BASE \
         	TM_QUICC_SCC1_RECV_BASE + \
         	(TM_QUICC_ETHER_MAX_RECV_BUFD * sizeof(struct tsQuiccBufDesc) )

/* Receive normal operation */ 
#define	TM_QUICC_SCC1_RECV_FUNC_CODE	(unsigned short)0x18    
/* Transmit normal operation */ 
#define	TM_QUICC_SCC1_XMIT_FUNC_CODE	(unsigned short)0x18 
/* Max ethernet frame length (Divisable by 4)*/ 
#define	TM_QUICC_SCC1_MAX_RECV_BUF_LEN	(unsigned short)1520

#define TM_QUICC_SCC_FRAME_SYNC_ETHER   (unsigned short)0xd555


/*
 * Port A Registers
 */
#define TM_QUICC_PORTA_PA0  (unsigned short)0x0001
#define TM_QUICC_PORTA_PA1  (unsigned short)0x0002
#define TM_QUICC_PORTA_PA2  (unsigned short)0x0004
#define TM_QUICC_PORTA_PA3  (unsigned short)0x0008
#define TM_QUICC_PORTA_PA4  (unsigned short)0x0010
#define TM_QUICC_PORTA_PA5  (unsigned short)0x0020
#define TM_QUICC_PORTA_PA6  (unsigned short)0x0040
#define TM_QUICC_PORTA_PA7  (unsigned short)0x0080
#define TM_QUICC_PORTA_PA8  (unsigned short)0x0100
#define TM_QUICC_PORTA_PA9  (unsigned short)0x0200
#define TM_QUICC_PORTA_PA10 (unsigned short)0x0400
#define TM_QUICC_PORTA_PA11 (unsigned short)0x0800
#define TM_QUICC_PORTA_PA12 (unsigned short)0x1000
#define TM_QUICC_PORTA_PA13 (unsigned short)0x2000
#define TM_QUICC_PORTA_PA14 (unsigned short)0x4000
#define TM_QUICC_PORTA_PA15 (unsigned short)0x8000

/*
 * Port B Registers
 */
#define TM_QUICC_PORTB_PB0  (unsigned short)0x0001
#define TM_QUICC_PORTB_PB1  (unsigned short)0x0002
#define TM_QUICC_PORTB_PB2  (unsigned short)0x0004
#define TM_QUICC_PORTB_PB3  (unsigned short)0x0008
#define TM_QUICC_PORTB_PB4  (unsigned short)0x0010
#define TM_QUICC_PORTB_PB5  (unsigned short)0x0020
#define TM_QUICC_PORTB_PB6  (unsigned short)0x0040
#define TM_QUICC_PORTB_PB7  (unsigned short)0x0080
#define TM_QUICC_PORTB_PB8  (unsigned short)0x0100
#define TM_QUICC_PORTB_PB9  (unsigned short)0x0200
#define TM_QUICC_PORTB_PB10 (unsigned short)0x0400
#define TM_QUICC_PORTB_PB11 (unsigned short)0x0800
#define TM_QUICC_PORTB_PB12 (unsigned short)0x1000
#define TM_QUICC_PORTB_PB13 (unsigned short)0x2000
#define TM_QUICC_PORTB_PB14 (unsigned short)0x4000
#define TM_QUICC_PORTB_PB15 (unsigned short)0x8000

/*
 * Port C Registers
 */
#define TM_QUICC_PORTC_PC0  (unsigned short)0x0001
#define TM_QUICC_PORTC_PC1  (unsigned short)0x0002
#define TM_QUICC_PORTC_PC2  (unsigned short)0x0004
#define TM_QUICC_PORTC_PC3  (unsigned short)0x0008
#define TM_QUICC_PORTC_PC4  (unsigned short)0x0010
#define TM_QUICC_PORTC_PC5  (unsigned short)0x0020
#define TM_QUICC_PORTC_PC6  (unsigned short)0x0040
#define TM_QUICC_PORTC_PC7  (unsigned short)0x0080
#define TM_QUICC_PORTC_PC8  (unsigned short)0x0100
#define TM_QUICC_PORTC_PC9  (unsigned short)0x0200
#define TM_QUICC_PORTC_PC10 (unsigned short)0x0400
#define TM_QUICC_PORTC_PC11 (unsigned short)0x0800

 

/*
 * SI Clock Route Register 
 */

/*
 * SCC1 Clock Sources
 */
#define TM_QUICC_SI_SCC1_GRANT    0x00000080UL
#define TM_QUICC_SI_SCC1_CONNECT  0x00000040UL
#define TM_QUICC_SI_SCC1_RCS_BRG1 0x00000000UL
#define TM_QUICC_SI_SCC1_RCS_BRG2 0x00000008UL
#define TM_QUICC_SI_SCC1_RCS_BRG3 0x00000010UL
#define TM_QUICC_SI_SCC1_RCS_BRG4 0x00000018UL
#define TM_QUICC_SI_SCC1_RCS_CLK1 0x00000020UL
#define TM_QUICC_SI_SCC1_RCS_CLK2 0x00000028UL
#define TM_QUICC_SI_SCC1_RCS_CLK3 0x00000030UL
#define TM_QUICC_SI_SCC1_RCS_CLK4 0x00000038UL
#define TM_QUICC_SI_SCC1_TCS_BRG1 0x00000000UL
#define TM_QUICC_SI_SCC1_TCS_BRG2 0x00000001UL
#define TM_QUICC_SI_SCC1_TCS_BRG3 0x00000002UL
#define TM_QUICC_SI_SCC1_TCS_BRG4 0x00000003UL
#define TM_QUICC_SI_SCC1_TCS_CLK1 0x00000004UL
#define TM_QUICC_SI_SCC1_TCS_CLK2 0x00000005UL
#define TM_QUICC_SI_SCC1_TCS_CLK3 0x00000006UL
#define TM_QUICC_SI_SCC1_TCS_CLK4 0x00000007UL

/*
 * SCC2 Clock Sources
 */
#define TM_QUICC_SI_SCC2_GRANT    0x00008000UL
#define TM_QUICC_SI_SCC2_CONNECT  0x00004000UL

#define TM_QUICC_SI_SCC2_RCS_BRG1 0x00000000UL
#define TM_QUICC_SI_SCC2_RCS_BRG2 0x00000800UL
#define TM_QUICC_SI_SCC2_RCS_BRG3 0x00001000UL
#define TM_QUICC_SI_SCC2_RCS_BRG4 0x00001800UL
#define TM_QUICC_SI_SCC2_RCS_CLK1 0x00002000UL
#define TM_QUICC_SI_SCC2_RCS_CLK2 0x00002800UL
#define TM_QUICC_SI_SCC2_RCS_CLK3 0x00003000UL
#define TM_QUICC_SI_SCC2_RCS_CLK4 0x00003800UL
#define TM_QUICC_SI_SCC2_TCS_BRG1 0x00000000UL
#define TM_QUICC_SI_SCC2_TCS_BRG2 0x00000100UL
#define TM_QUICC_SI_SCC2_TCS_BRG3 0x00000200UL
#define TM_QUICC_SI_SCC2_TCS_BRG4 0x00000300UL
#define TM_QUICC_SI_SCC2_TCS_CLK1 0x00000400UL
#define TM_QUICC_SI_SCC2_TCS_CLK2 0x00000500UL
#define TM_QUICC_SI_SCC2_TCS_CLK3 0x00000600UL
#define TM_QUICC_SI_SCC2_TCS_CLK4 0x00000700UL

/*
 * SCC3 Clock Sources
 */
#define TM_QUICC_SI_SCC3_GRANT    0x00800000UL
#define TM_QUICC_SI_SCC3_CONNECT  0x00400000UL

#define TM_QUICC_SI_SCC3_RCS_BRG1 0x00000000UL
#define TM_QUICC_SI_SCC3_RCS_BRG2 0x00080000UL
#define TM_QUICC_SI_SCC3_RCS_BRG3 0x00100000UL
#define TM_QUICC_SI_SCC3_RCS_BRG4 0x00180000UL
#define TM_QUICC_SI_SCC3_RCS_CLK5 0x00200000UL
#define TM_QUICC_SI_SCC3_RCS_CLK6 0x00280000UL
#define TM_QUICC_SI_SCC3_RCS_CLK7 0x00300000UL
#define TM_QUICC_SI_SCC3_RCS_CLK8 0x00380000UL
#define TM_QUICC_SI_SCC3_TCS_BRG1 0x00000000UL
#define TM_QUICC_SI_SCC3_TCS_BRG2 0x00010000UL
#define TM_QUICC_SI_SCC3_TCS_BRG3 0x00020000UL
#define TM_QUICC_SI_SCC3_TCS_BRG4 0x00030000UL
#define TM_QUICC_SI_SCC3_TCS_CLK5 0x00040000UL
#define TM_QUICC_SI_SCC3_TCS_CLK6 0x00050000UL
#define TM_QUICC_SI_SCC3_TCS_CLK7 0x00060000UL
#define TM_QUICC_SI_SCC3_TCS_CLK8 0x00070000UL

/*
 * SCC4 Clock Sources
 */
#define TM_QUICC_SI_SCC4_GRANT    0x80000000UL
#define TM_QUICC_SI_SCC4_CONNECT  0x40000000UL

#define TM_QUICC_SI_SCC4_RCS_BRG1 0x00000000UL
#define TM_QUICC_SI_SCC4_RCS_BRG2 0x08000000UL
#define TM_QUICC_SI_SCC4_RCS_BRG3 0x10000000UL
#define TM_QUICC_SI_SCC4_RCS_BRG4 0x18000000UL
#define TM_QUICC_SI_SCC4_RCS_CLK5 0x20000000UL
#define TM_QUICC_SI_SCC4_RCS_CLK6 0x28000000UL
#define TM_QUICC_SI_SCC4_RCS_CLK7 0x30000000UL
#define TM_QUICC_SI_SCC4_RCS_CLK8 0x38000000UL
#define TM_QUICC_SI_SCC4_TCS_BRG1 0x00000000UL
#define TM_QUICC_SI_SCC4_TCS_BRG2 0x01000000UL
#define TM_QUICC_SI_SCC4_TCS_BRG3 0x02000000UL
#define TM_QUICC_SI_SCC4_TCS_BRG4 0x03000000UL
#define TM_QUICC_SI_SCC4_TCS_CLK5 0x04000000UL
#define TM_QUICC_SI_SCC4_TCS_CLK6 0x05000000UL
#define TM_QUICC_SI_SCC4_TCS_CLK7 0x06000000UL
#define TM_QUICC_SI_SCC4_TCS_CLK8 0x07000000UL


/*
 * Transmit Buffer Descriptor Flags
 */
#define	TM_QUICC_XMIT_READY	(unsigned short)0x8000		/* ready bit */
#define	TM_QUICC_XMIT_PAD	(unsigned short)0x4000		/* short frame padding */
#define	TM_QUICC_XMIT_WRAP	(unsigned short)0x2000		/* wrap bit */
#define	TM_QUICC_XMIT_INTR	(unsigned short)0x1000		/* interrupt on completion */
#define	TM_QUICC_XMIT_LAST	(unsigned short)0x0800		/* last in frame */
#define	TM_QUICC_XMIT_CRC	(unsigned short)0x0400		/* transmit CRC (when last) */

/* 
 * Transmit Error Conditions
 */
#define	TM_QUICC_XMIT_ETHER_ERROR	(unsigned short)0x00c2
#define	TM_QUICC_XMIT_E_DEFER	    (unsigned short)0x0200		/* defer indication */
#define	TM_QUICC_XMIT_E_HEARTB	    (unsigned short)0x0100		/* heartbeat */
#define	TM_QUICC_XMIT_E_LATEC	    (unsigned short)0x0080		/* error: late collision */
#define	TM_QUICC_XMIT_E_LIMIT 	    (unsigned short)0x0040		/* error: retransmission limit */
#define	TM_QUICC_XMIT_E_COUNT 	    (unsigned short)0x003c		/* retry count */
#define	TM_QUICC_XMIT_E_UNDERRUN	(unsigned short)0x0002		/* error: underrun */
#define	TM_QUICC_XMIT_E_CARRIER	    (unsigned short)0x0001		/* carier sense lost */

/*
 * Receive Buffer Descriptor Flags
 */
#define	TM_QUICC_RECV_EMPTY	(unsigned short)0x8000		/* buffer empty */
#define	TM_QUICC_RECV_WRAP	(unsigned short)0x2000		/* wrap bit */
#define	TM_QUICC_RECV_INTR	(unsigned short)0x1000		/* interrupt on reception */
#define	TM_QUICC_RECV_LAST	(unsigned short)0x0800		/* last BD in frame */
#define	TM_QUICC_RECV_FIRST	(unsigned short)0x0400		/* first BD in frame */

/*
 * Receive Error Conditions 
 */
#define	TM_QUICC_RECV_ERROR	    (unsigned short)0x00ff
#define	TM_QUICC_RECV_E_TOOLONG	(unsigned short)0x0020		/* frame too long */
#define	TM_QUICC_RECV_E_NOTBYTE	(unsigned short)0x0010		/* non-octet aligned */
#define	TM_QUICC_RECV_E_SHORT	(unsigned short)0x0008		/* short frame */
#define	TM_QUICC_RECV_E_CRC	    (unsigned short)0x0004		/* receive CRC error */
#define	TM_QUICC_RECV_E_OVERRUN	(unsigned short)0x0002		/* receive overrun */
#define	TM_QUICC_RECT_E_COLL	(unsigned short)0x0001		/* collision */


/*
 * Ethernet Interrupts
 */
#define	TM_QUICC_INTR_ETHER_STOP	(unsigned short)0x0080	/* graceful stop complete */
#define	TM_QUICC_INTR_ETHER_E_XMIT  (unsigned short)0x0010	/* transmit error */
#define	TM_QUICC_INTR_ETHER_RECV_F  (unsigned short)0x0008	/* receive frame */
#define	TM_QUICC_INTR_ETHER_BUSY	(unsigned short)0x0004	/* busy condition */
#define	TM_QUICC_INTR_ETHER_XMIT_B	(unsigned short)0x0002	/* transmit buffer */
#define	TM_QUICC_INTR_ETHER_RECV_B	(unsigned short)0x0001	/* receive buffer */

/* 
 * Ethernet mode register 
 */
#define	TM_QUICC_ETHER_IND_ADDR		 (unsigned short)0x1000	/* individual address mode */
#define TM_QUICC_ETHER_ENABLE_CRC    (unsigned short)0x0800  /* Enable CRC */
#define TM_QUICC_ETHER_LOOPBACK      (unsigned short)0x0040  /* Loop Back Mode */
#define TM_QUICC_ETHER_NBITS_IGNORED (unsigned short)0x000a  /* # of ignored bits 22 */
#define	TM_QUICC_ETHER_PROMISCUOUS	 (unsigned short)0x0200	/* promiscuous */
#define	TM_QUICC_ETHER_BROADCAST	 (unsigned short)0x0100	/* broadcast address */

/*****************************************************************
	General SCC mode register (GSMR)
*****************************************************************/
/* GSMRA */
/* SCC modes */
#define	TM_QUICC_HDLC_PORT		  0x0UL
#define	TM_QUICC_HDLC_BUS		  0x1UL
#define	TM_QUICC_APPLE_TALK		  0x2UL
#define	TM_QUICC_SS_NO7			  0x3UL
#define	TM_QUICC_UART			  0x4UL
#define	TM_QUICC_PROFI_BUS		  0x5UL
#define	TM_QUICC_ASYNC_HDLC		  0x6UL
#define	TM_QUICC_V14			  0x7UL
#define	TM_QUICC_BISYNC_PORT  	  0x8UL
#define	TM_QUICC_DDCMP_PORT		  0x9UL
#define	TM_QUICC_ETHERNET_PORT	  0xcUL

#define	TM_QUICC_ENABLE_XMIT	  0x00000010UL
#define	TM_QUICC_ENABLE_RECV	  0x00000020UL

#define	TM_QUICC_PREAMBLE_PAT_10  0x00080000UL
#define	TM_QUICC_PREAMBLE_48	  0x00800000UL
#define	TM_QUICC_XMIT_CLOCK_INV	  0x10000000UL

/* GSMRB */
#define	TM_QUICC_TRANSPARENT_CRC  0x00008000UL
#define	TM_QUICC_TRANSPARENT_RECV 0x00001000UL
#define	TM_QUICC_TRANSPARENT_XMIT 0x00000800UL
#define	TM_QUICC_CTS_SAMPLE		  0x00000080UL
#define	TM_QUICC_CD_SAMPLE		  0x00000100UL


/* 
 * SDMA Defines
 */
#define TM_QUICC_SDMA_INT_MASK (unsigned short)0x0700
#define TM_QUICC_SDMA_ARB_ID   (unsigned short)0x0040



/*
 * tbase and rbase registers
 */
#define	TM_QUICC_XMIT_BD_ADDR(quicc,pram)	((ttQuiccBufDescPtr) \
			(quicc->udata_bd_ucode +\
					pram->tbase))

#define	TM_QUICC_RECV_BD_ADDR(quicc,pram)	((ttQuiccBufDescPtr) \
			(quicc->udata_bd_ucode +\
					pram->rbase))

#define	TTBD_ADDR(quicc,pram)	((ttQuiccBufDescPtr) \
			(quicc->udata_bd_ucode +\
					pram->tbptr))

/*****************************************************************
	HDLC parameter RAM
*****************************************************************/

struct hdlc_pram {
	/*
	 * SCC parameter RAM
	 */
	unsigned short	rbase;		/* RX BD base address */
	unsigned short	tbase;		/* TX BD base address */
	unsigned char	rfcr;		/* Rx function code */
	unsigned char	tfcr;		/* Tx function code */
	unsigned short	mrblr;		/* Rx buffer length */
	unsigned long	rstate;		/* Rx internal state */
	unsigned long	rptr;		/* Rx internal data pointer */
	unsigned short	rbptr;		/* rb BD Pointer */
	unsigned short	rcount;		/* Rx internal byte count */
	unsigned long	rtemp;		/* Rx temp */
	unsigned long	tstate;		/* Tx internal state */
	unsigned long	tptr;		/* Tx internal data pointer */
	unsigned short	tbptr;		/* Tx BD pointer */
	unsigned short	tcount;		/* Tx byte count */
	unsigned long	ttemp;		/* Tx temp */
	unsigned long	rcrc;		/* temp receive CRC */
	unsigned long	tcrc;		/* temp transmit CRC */

	/*
	 * HDLC specific parameter RAM
	 */
	unsigned char	RESERVED1[4];	/* Reserved area */
	unsigned long	c_mask;		/* CRC constant */
	unsigned long	c_pres;		/* CRC preset */
	unsigned short	disfc;		/* discarded frame counter */
	unsigned short	crcec;		/* CRC error counter */
	unsigned short	abtsc;		/* abort sequence counter */
	unsigned short	nmarc;		/* nonmatching address rx cnt */
	unsigned short	retrc;		/* frame retransmission cnt */
	unsigned short	mflr;		/* maximum frame length reg */
	unsigned short	max_cnt;	/* maximum length counter */
	unsigned short	rfthr;		/* received frames threshold */
	unsigned short	rfcnt;		/* received frames count */
	unsigned short	hmask;		/* user defined frm addr mask */
	unsigned short	haddr1;	/* user defined frm address 1 */
	unsigned short	haddr2;	/* user defined frm address 2 */
	unsigned short	haddr3;	/* user defined frm address 3 */
	unsigned short	haddr4;	/* user defined frm address 4 */
	unsigned short	tmp;	/* temp */
	unsigned short	tmp_mb;	/* temp */
};



/*****************************************************************
	UART parameter RAM
*****************************************************************/

/*
 * bits in uart control characters table
 */
#define	CC_INVALID	0x8000		/* control character is valid */
#define	CC_REJ		0x4000		/* don't store char in buffer */
#define	CC_CHAR		0x00ff		/* control character */

/* UART */
struct uart_pram {
	/*
	 * SCC parameter RAM
	 */
	unsigned short	rbase;		/* RX BD base address */
	unsigned short	tbase;		/* TX BD base address */
	unsigned char	rfcr;		/* Rx function code */
	unsigned char	tfcr;		/* Tx function code */
	unsigned short	mrblr;		/* Rx buffer length */
	unsigned long	rstate;		/* Rx internal state */
	unsigned long	rptr;		/* Rx internal data pointer */
	unsigned short	rbptr;		/* rb BD Pointer */
	unsigned short	rcount;		/* Rx internal byte count */
	unsigned long	rx_temp;	/* Rx temp */
	unsigned long	tstate;		/* Tx internal state */
	unsigned long	tptr;		/* Tx internal data pointer */
	unsigned short	tbptr;		/* Tx BD pointer */
	unsigned short	tcount;		/* Tx byte count */
	unsigned long	ttemp;		/* Tx temp */
	unsigned long	rcrc;		/* temp receive CRC */
	unsigned long	tcrc;		/* temp transmit CRC */

	/*
	 * UART specific parameter RAM
	 */
	unsigned char	RESERVED1[8];	/* Reserved area */
	unsigned short	max_idl;	/* maximum idle characters */
	unsigned short	idlc;		/* rx idle counter (internal) */
	unsigned short	brkcr;		/* break count register */

	unsigned short	parec;		/* Rx parity error counter */
	unsigned short	frmer;		/* Rx framing error counter */
	unsigned short	nosec;		/* Rx noise counter */
	unsigned short	brkec;		/* Rx break character counter */
	unsigned short	brkln;		/* Reaceive break length */

	unsigned short	uaddr1;		/* address character 1 */
	unsigned short	uaddr2;		/* address character 2 */
	unsigned short	rtemp;		/* temp storage */
	unsigned short	toseq;		/* Tx out of sequence char */
	unsigned short	cc[8];		/* Rx control characters */
	unsigned short	rccm;		/* Rx control char mask */
	unsigned short	rccr;		/* Rx control char register */
	unsigned short	rlbc;		/* Receive last break char */
};



/*****************************************************************
	BISYNC parameter RAM
*****************************************************************/

struct bisync_pram {
	/*
	 * SCC parameter RAM
	 */
	unsigned short	rbase;		/* RX BD base address */
	unsigned short	tbase;		/* TX BD base address */
	unsigned char	rfcr;		/* Rx function code */
	unsigned char	tfcr;		/* Tx function code */
	unsigned short	mrblr;		/* Rx buffer length */
	unsigned long	rstate;		/* Rx internal state */
	unsigned long	rptr;		/* Rx internal data pointer */
	unsigned short	rbptr;		/* rb BD Pointer */
	unsigned short	rcount;		/* Rx internal byte count */
	unsigned long	rtemp;		/* Rx temp */
	unsigned long	tstate;		/* Tx internal state */
	unsigned long	tptr;		/* Tx internal data pointer */
	unsigned short	tbptr;		/* Tx BD pointer */
	unsigned short	tcount;		/* Tx byte count */
	unsigned long	ttemp;		/* Tx temp */
	unsigned long	rcrc;		/* temp receive CRC */
	unsigned long	tcrc;		/* temp transmit CRC */

	/*
	 * BISYNC specific parameter RAM
	 */
	unsigned char	RESERVED1[4];	/* Reserved area */
	unsigned long	crcc;		/* CRC Constant Temp Value */
	unsigned short	prcrc;		/* Preset Receiver CRC-16/LRC */
	unsigned short	ptcrc;		/* Preset Transmitter CRC-16/LRC */
	unsigned short	parec;		/* Receive Parity Error Counter */
	unsigned short	bsync;		/* BISYNC SYNC Character */
	unsigned short	bdle;		/* BISYNC DLE Character */
	unsigned short	cc[8];		/* Rx control characters */
	unsigned short	rccm;		/* Receive Control Character Mask */
};

/*****************************************************************
	IOM2 parameter RAM
	(overlaid on tx bd[5] of SCC channel[2])
*****************************************************************/
struct iom2_pram {
	unsigned short	ci_data;	/* ci data */
	unsigned short	monitor_data;	/* monitor data */
	unsigned short	tstate;		/* transmitter state */
	unsigned short	rstate;		/* receiver state */
};

/*****************************************************************
	SPI/SMC parameter RAM
	(overlaid on tx bd[6,7] of SCC channel[2])
*****************************************************************/

#define	SPI_R	0x8000		/* Ready bit in BD */

struct spi_pram {
	unsigned short	rbase;		/* Rx BD Base Address */
	unsigned short	tbase;		/* Tx BD Base Address */
	unsigned char	rfcr;		/* Rx function code */
	unsigned char	tfcr;		/* Tx function code */
	unsigned short	mrblr;		/* Rx buffer length */
	unsigned long	rstate;		/* Rx internal state */
	unsigned long	rptr;		/* Rx internal data pointer */
	unsigned short	rbptr;		/* rb BD Pointer */
	unsigned short	rcount;		/* Rx internal byte count */
	unsigned long	rtemp;		/* Rx temp */
	unsigned long	tstate;		/* Tx internal state */
	unsigned long	tptr;		/* Tx internal data pointer */
	unsigned short	tbptr;		/* Tx BD pointer */
	unsigned short	tcount;		/* Tx byte count */
	unsigned long	ttemp;		/* Tx temp */
};

struct smc_uart_pram {
	unsigned short	rbase;		/* Rx BD Base Address */
	unsigned short	tbase;		/* Tx BD Base Address */
	unsigned char	rfcr;		/* Rx function code */
	unsigned char	tfcr;		/* Tx function code */
	unsigned short	mrblr;		/* Rx buffer length */
	unsigned long	rstate;		/* Rx internal state */
	unsigned long	rptr;		/* Rx internal data pointer */
	unsigned short	rbptr;		/* rb BD Pointer */
	unsigned short	rcount;		/* Rx internal byte count */
	unsigned long	rtemp;		/* Rx temp */
	unsigned long	tstate;		/* Tx internal state */
	unsigned long	tptr;		/* Tx internal data pointer */
	unsigned short	tbptr;		/* Tx BD pointer */
	unsigned short	tcount;		/* Tx byte count */
	unsigned long	ttemp;		/* Tx temp */
	unsigned short	max_idl;	/* Maximum IDLE Characters */
	unsigned short	idlc;		/* Temporary IDLE Counter */
	unsigned short	brkln;		/* Last Rx Break Length */
	unsigned short	brkec;		/* Rx Break Condition Counter */
	unsigned short	brkcr;		/* Break Count Register (Tx) */
	unsigned short	r_mask;		/* Temporary bit mask */
};

struct smc_trnsp_pram {
	unsigned short	rbase;		/* Rx BD Base Address */
	unsigned short	tbase;		/* Tx BD Base Address */
	unsigned char	rfcr;		/* Rx function code */
	unsigned char	tfcr;		/* Tx function code */
	unsigned short	mrblr;		/* Rx buffer length */
	unsigned long	rstate;		/* Rx internal state */
	unsigned long	rptr;		/* Rx internal data pointer */
	unsigned short	rbptr;		/* rb BD Pointer */
	unsigned short	rcount;		/* Rx internal byte count */
	unsigned long	rtemp;		/* Rx temp */
	unsigned long	tstate;		/* Tx internal state */
	unsigned long	tptr;		/* Tx internal data pointer */
	unsigned short	tbptr;		/* Tx BD pointer */
	unsigned short	tcount;		/* Tx byte count */
	unsigned long	ttemp;		/* Tx temp */
	unsigned short  reserved[5];	/* Reserved */
};

struct idma_pram {
	unsigned short	ibase;		/* IDMA BD Base Address */
	unsigned short	ibptr;	/* IDMA buffer descriptor pointer */
	unsigned long	istate;	/* IDMA internal state */
	unsigned long	itemp;	/* IDMA temp */
};

struct ethernet_pram {
	/*
	 * SCC parameter RAM
	 */
	unsigned short	rbase;		/* RX BD base address */
	unsigned short	tbase;		/* TX BD base address */
	unsigned char	rfcr;		/* Rx function code */
	unsigned char	tfcr;		/* Tx function code */
	unsigned short	mrblr;		/* Rx buffer length */
	unsigned long	rstate;		/* Rx internal state */
	unsigned long	rptr;		/* Rx internal data pointer */
	unsigned short	rbptr;		/* rb BD Pointer */
	unsigned short	rcount;		/* Rx internal byte count */
	unsigned long	rtemp;		/* Rx temp */
	unsigned long	tstate;		/* Tx internal state */
	unsigned long	tptr;		/* Tx internal data pointer */
	unsigned short	tbptr;		/* Tx BD pointer */
	unsigned short	tcount;		/* Tx byte count */
	unsigned long	ttemp;		/* Tx temp */
	unsigned long	rcrc;		/* temp receive CRC */
	unsigned long	tcrc;		/* temp transmit CRC */

	/*
	 * ETHERNET specific parameter RAM
	 */
	unsigned long	c_pres;		/* preset CRC */
	unsigned long	c_mask;		/* constant mask for CRC */
	unsigned long	crcec;		/* CRC error counter */
	unsigned long	alec;		/* alighnment error counter */
	unsigned long	disfc;		/* discard frame counter */
	unsigned short	pads;		/* short frame PAD characters */
	unsigned short	ret_lim;	/* retry limit threshold */
	unsigned short	ret_cnt;	/* retry limit counter */
	unsigned short	mflr;		/* maximum frame length reg */
	unsigned short	minflr;		/* minimum frame length reg */
	unsigned short	maxd1;		/* maximum DMA1 length reg */
	unsigned short	maxd2;		/* maximum DMA2 length reg */
	unsigned short	maxd;		/* rx max DMA */
	unsigned short	dma_cnt;	/* rx dma counter */
	unsigned short	max_b;		/* max bd byte count */
	unsigned short	gaddr1;		/* group address filter 1 */
	unsigned short	gaddr2;		/* group address filter 2 */
	unsigned short	gaddr3;		/* group address filter 3 */
	unsigned short	gaddr4;		/* group address filter 4 */
	unsigned long	tbuf0_data0;	/* save area 0 - current frm */
	unsigned long	tbuf0_data1;	/* save area 1 - current frm */
	unsigned long	tbuf0_rba0;
	unsigned long	tbuf0_crc;
	unsigned short	tbuf0_bcnt;
	unsigned short	paddr_h;	/* physical address (MSB) */
	unsigned short	paddr_m;	/* physical address */
	unsigned short	paddr_l;	/* physical address (LSB) */
	unsigned short	p_per;		/* persistence */
	unsigned short	rfbd_ptr;	/* rx first bd pointer */
	unsigned short	tfbd_ptr;	/* tx first bd pointer */
	unsigned short	tlbd_ptr;	/* tx last bd pointer */
	unsigned long	tbuf1_data0;	/* save area 0 - next frame */
	unsigned long	tbuf1_data1;	/* save area 1 - next frame */
	unsigned long	tbuf1_rba0;
	unsigned long	tbuf1_crc;
	unsigned short	tbuf1_bcnt;
	unsigned short	tx_len;		/* tx frame length counter */
	unsigned short	iaddr1;		/* individual address filter 1*/
	unsigned short	iaddr2;		/* individual address filter 2*/
	unsigned short	iaddr3;		/* individual address filter 3*/
	unsigned short	iaddr4;		/* individual address filter 4*/
	unsigned short	boff_cnt;	/* back-off counter */
	unsigned short	taddr_h;	/* temp address (MSB) */
	unsigned short	taddr_m;	/* temp address */
	unsigned short	taddr_l;	/* temp address (LSB) */
};

struct transparent_pram {
	/*
	 * SCC parameter RAM
	 */
	unsigned short	rbase;		/* RX BD base address */
	unsigned short	tbase;		/* TX BD base address */
	unsigned char	rfcr;		/* Rx function code */
	unsigned char	tfcr;		/* Tx function code */
	unsigned short	mrblr;		/* Rx buffer length */
	unsigned long	rstate;		/* Rx internal state */
	unsigned long	rptr;		/* Rx internal data pointer */
	unsigned short	rbptr;		/* rb BD Pointer */
	unsigned short	rcount;		/* Rx internal byte count */
	unsigned long	rtemp;		/* Rx temp */
	unsigned long	tstate;		/* Tx internal state */
	unsigned long	tptr;		/* Tx internal data pointer */
	unsigned short	tbptr;		/* Tx BD pointer */
	unsigned short	tcount;		/* Tx byte count */
	unsigned long	ttemp;		/* Tx temp */
	unsigned long	rcrc;		/* temp receive CRC */
	unsigned long	tcrc;		/* temp transmit CRC */

	/*
	 * TRANSPARENT specific parameter RAM
	 */
	unsigned long	crc_p;		/* CRC Preset */
	unsigned long	crc_c;		/* CRC constant */
};

struct timer_pram {
	/*
	 * RISC timers parameter RAM
	 */
	unsigned short	tm_base;	/* RISC timer table base adr */
	unsigned short	tm_ptr;		/* RISC timer table pointer */
	unsigned short	r_tmr;		/* RISC timer mode register */
	unsigned short	r_tmv;		/* RISC timer valid register */
	unsigned long	tm_cmd;		/* RISC timer cmd register */
	unsigned long	tm_cnt;		/* RISC timer internal cnt */
};

/*
 * internal ram
 */
typedef struct tsQuicc {
/* BASE + 0x000: user data memory */
	volatile unsigned char	udata_bd_ucode[0x400];	/*user data bd's Ucode*/
	volatile unsigned char	udata_bd[0x200];	/*user data Ucode*/
	volatile unsigned char	ucode_ext[0x100];	/*Ucode Extention ram*/
	volatile unsigned char	RESERVED1[0x500];	/* Reserved area */

/* BASE + 0xc00: PARAMETER RAM */
	union {
		struct scc_pram {
			union {
				struct hdlc_pram	h;
				struct uart_pram	u;
				struct bisync_pram	b;
				struct transparent_pram	t;
				unsigned char	RESERVED66[0x70];
			} pscc;		/* scc parameter area (protocol dependent) */
			union {
				struct {
					unsigned char	RESERVED70[0x10];
					struct spi_pram	spi;
					unsigned char	RESERVED72[0x8];
					struct timer_pram	timer;
				} timer_spi;
				struct {
					struct idma_pram idma;
					union {
						struct smc_uart_pram u;
						struct smc_trnsp_pram t;
					} psmc;
				} idma_smc;
			} pothers;
		} scc;
		struct ethernet_pram	enet_scc;
		unsigned char	pr[0x100];
	} pram[4];
/* reserved */

/* BASE + 0x1000: INTERNAL REGISTERS */
/* SIM */
	volatile unsigned long	sim_mcr;	/* module configuration reg */
	volatile unsigned short	sim_simtr;	/* module test register */
	volatile unsigned char	RESERVED2[0x2];	/* Reserved area */
	volatile unsigned char	sim_avr;	/* auto vector reg */
	volatile unsigned char	sim_rsr;	/* reset status reg */
	volatile unsigned char	RESERVED3[0x2];	/* Reserved area */
	volatile unsigned char	sim_clkocr;	/* CLCO control register */
	volatile unsigned char	RESERVED62[0x3];	/* Reserved area */
	volatile unsigned short	sim_pllcr;	/* PLL control register */
	volatile unsigned char	RESERVED63[0x2];	/* Reserved area */
	volatile unsigned short	sim_cdvcr;	/* Clock devider control register */
	volatile unsigned short	sim_pepar;	/* Port E pin assignment register */
	volatile unsigned char	RESERVED64[0xa];	/* Reserved area */
	volatile unsigned char	sim_sypcr;	/* system protection control */
	volatile unsigned char	sim_swiv;	/* software interrupt vector */
	volatile unsigned char	RESERVED6[0x2];	/* Reserved area */
	volatile unsigned short	sim_picr;	/* periodic interrupt control reg */
	volatile unsigned char	RESERVED7[0x2];	/* Reserved area */
	volatile unsigned short	sim_pitr;	/* periodic interrupt timing reg */
	volatile unsigned char	RESERVED8[0x3];	/* Reserved area */
	volatile unsigned char	sim_swsr;	/* software service */
	volatile unsigned long	sim_bkar;	/* breakpoint address register*/
	volatile unsigned long	sim_bkcr;	/* breakpoint control register*/
	volatile unsigned char	RESERVED10[0x8];	/* Reserved area */
/* MEMC */
	volatile unsigned long	memc_gmr;	/* Global memory register */
	volatile unsigned short	memc_mstat;	/* MEMC status register */
	volatile unsigned char	RESERVED11[0xa];	/* Reserved area */
	volatile unsigned long	memc_br0;	/* base register 0 */
	volatile unsigned long	memc_or0;	/* option register 0 */
	volatile unsigned char	RESERVED12[0x8];	/* Reserved area */
	volatile unsigned long	memc_br1;	/* base register 1 */
	volatile unsigned long	memc_or1;	/* option register 1 */
	volatile unsigned char	RESERVED13[0x8];	/* Reserved area */
	volatile unsigned long	memc_br2;	/* base register 2 */
	volatile unsigned long	memc_or2;	/* option register 2 */
	volatile unsigned char	RESERVED14[0x8];	/* Reserved area */
	volatile unsigned long	memc_br3;	/* base register 3 */
	volatile unsigned long	memc_or3;	/* option register 3 */
	volatile unsigned char	RESERVED15[0x8];	/* Reserved area */
	volatile unsigned long	memc_br4;	/* base register 3 */
	volatile unsigned long	memc_or4;	/* option register 3 */
	volatile unsigned char	RESERVED16[0x8];	/* Reserved area */
	volatile unsigned long	memc_br5;	/* base register 3 */
	volatile unsigned long	memc_or5;	/* option register 3 */
	volatile unsigned char	RESERVED17[0x8];	/* Reserved area */
	volatile unsigned long	memc_br6;	/* base register 3 */
	volatile unsigned long	memc_or6;	/* option register 3 */
	volatile unsigned char	RESERVED18[0x8];	/* Reserved area */
	volatile unsigned long	memc_br7;	/* base register 3 */
	volatile unsigned long	memc_or7;	/* option register 3 */
	volatile unsigned char	RESERVED9[0x28];	/* Reserved area */
/* TEST */
	volatile unsigned short	test_tstmra;	/* master shift a */
	volatile unsigned short	test_tstmrb;	/* master shift b */
	volatile unsigned short	test_tstsc;	/* shift count */
	volatile unsigned short	test_tstrc;	/* repetition counter */
	volatile unsigned short	test_creg;	/* control */
	volatile unsigned short	test_dreg;	/* destributed register */
	volatile unsigned char	RESERVED58[0x404];	/* Reserved area */
/* IDMA1 */
	volatile unsigned short	idma_iccr;	/* channel configuration reg */
	volatile unsigned char	RESERVED19[0x2];	/* Reserved area */
	volatile unsigned short	idma1_cmr;	/* dma mode reg */
	volatile unsigned char	RESERVED68[0x2];	/* Reserved area */
	volatile unsigned long	idma1_sapr;	/* dma source addr ptr */
	volatile unsigned long	idma1_dapr;	/* dma destination addr ptr */
	volatile unsigned long	idma1_bcr;	/* dma byte count reg */
	volatile unsigned char	idma1_fcr;	/* function code reg */
	volatile unsigned char	RESERVED20;	/* Reserved area */
	volatile unsigned char	idma1_cmar;	/* channel mask reg */
	volatile unsigned char	RESERVED21;	/* Reserved area */
	volatile unsigned char	idma1_csr;	/* channel status reg */
	volatile unsigned char	RESERVED22[0x3];	/* Reserved area */
/* SDMA */
	volatile unsigned char	sdma_sdsr;	/* status reg */
	volatile unsigned char	RESERVED23;	/* Reserved area */
	volatile unsigned short	sdma_sdcr;	/* configuration reg */
	volatile unsigned long	sdma_sdar;	/* address reg */
/* IDMA2 */
	volatile unsigned char	RESERVED69[0x2];	/* Reserved area */
	volatile unsigned short	idma2_cmr;	/* dma mode reg */
	volatile unsigned long	idma2_sapr;	/* dma source addr ptr */
	volatile unsigned long	idma2_dapr;	/* dma destination addr ptr */
	volatile unsigned long	idma2_bcr;	/* dma byte count reg */
	volatile unsigned char	idma2_fcr;	/* function code reg */
	volatile unsigned char	RESERVED24;	/* Reserved area */
	volatile unsigned char	idma2_cmar;	/* channel mask reg */
	volatile unsigned char	RESERVED25;	/* Reserved area */
	volatile unsigned char	idma2_csr;	/* channel status reg */
	volatile unsigned char	RESERVED26[0x7];	/* Reserved area */
/* Interrupt Controller */
	volatile unsigned long	intr_cicr;	/* CP interrupt configuration reg*/
	volatile unsigned long	intr_cipr;	/* CP interrupt pending reg */
	volatile unsigned long	intr_cimr;	/* CP interrupt mask reg */
	volatile unsigned long	intr_cisr;	/* CP interrupt in service reg*/
/* Parallel I/O */
	volatile unsigned short	pio_padir;	/* port A data direction reg */
	volatile unsigned short	pio_papar;	/* port A pin assignment reg */
	volatile unsigned short	pio_paodr;	/* port A open drain reg */
	volatile unsigned short	pio_padat;	/* port A data register */
	volatile unsigned char	RESERVED28[0x8];	/* Reserved area */
	volatile unsigned short	pio_pcdir;	/* port C data direction reg */
	volatile unsigned short	pio_pcpar;	/* port C pin assignment reg */
	volatile unsigned short	pio_pcso;	/* port C special options */
	volatile unsigned short	pio_pcdat;	/* port C data register */
	volatile unsigned short	pio_pcint;	/* port C interrupt cntrl reg */
	volatile unsigned char	RESERVED29[0x16];	/* Reserved area */
/* Timer */
	volatile unsigned short	timer_tgcr;	/* timer global configuration  reg */
	volatile unsigned char	RESERVED30[0xe];	/* Reserved area */
	volatile unsigned short	timer_tmr1;	/* timer 1 mode reg */
	volatile unsigned short	timer_tmr2;	/* timer 2 mode reg */
	volatile unsigned short	timer_trr1;	/* timer 1 referance reg */
	volatile unsigned short	timer_trr2;	/* timer 2 referance reg */
	volatile unsigned short	timer_tcr1;	/* timer 1 capture reg */
	volatile unsigned short	timer_tcr2;	/* timer 2 capture reg */
	volatile unsigned short	timer_tcn1;	/* timer 1 counter reg */
	volatile unsigned short	timer_tcn2;	/* timer 2 counter reg */
	volatile unsigned short	timer_tmr3;	/* timer 3 mode reg */
	volatile unsigned short	timer_tmr4;	/* timer 4 mode reg */
	volatile unsigned short	timer_trr3;	/* timer 3 referance reg */
	volatile unsigned short	timer_trr4;	/* timer 4 referance reg */
	volatile unsigned short	timer_tcr3;	/* timer 3 capture reg */
	volatile unsigned short	timer_tcr4;	/* timer 4 capture reg */
	volatile unsigned short	timer_tcn3;	/* timer 3 counter reg */
	volatile unsigned short	timer_tcn4;	/* timer 4 counter reg */
	volatile unsigned short	timer_ter1;	/* timer 1 event reg */
	volatile unsigned short	timer_ter2;	/* timer 2 event reg */
	volatile unsigned short	timer_ter3;	/* timer 3 event reg */
	volatile unsigned short	timer_ter4;	/* timer 4 event reg */
	volatile unsigned char	RESERVED34[0x8];	/* Reserved area */
/* CP */
	volatile unsigned short	cp_cr;		/* command register */
	volatile unsigned char	RESERVED35[0x2];	/* Reserved area */
	volatile unsigned short	cp_rccr;	/* main configuration reg */
	volatile unsigned char	RESERVED37;	/* Reserved area */
	volatile unsigned char	cp_rmds;	/* development support status reg */
	volatile unsigned long	cp_rmdr;	/* development support control reg */
	volatile unsigned short	cp_cpcr1;	/* CP Control Register 1 */
	volatile unsigned short	cp_cpcr2;	/* CP Control Register 2 */
	volatile unsigned short	cp_cpcr3;	/* CP Control Register 3 */
	volatile unsigned short	cp_cpcr4;	/* CP Control Register 4 */
	volatile unsigned char	RESERVED59[0x2];	/* Reserved area */
	volatile unsigned short	cp_rter;	/* RISC timers event reg */
	volatile unsigned char	RESERVED38[0x2];	/* Reserved area */
	volatile unsigned short	cp_rtmr;	/* RISC timers mask reg */
	volatile unsigned char	RESERVED39[0x14];	/* Reserved area */
/* BRG */
	volatile unsigned long	brgc1;		/* BRG1 configuration reg */
	volatile unsigned long	brgc2;		/* BRG2 configuration reg */
	volatile unsigned long	brgc3;		/* BRG3 configuration reg */
	volatile unsigned long	brgc4;		/* BRG4 configuration reg */
/* SCC registers */
	struct scc_regs {
		volatile unsigned long	scc_gsmra; /* SCC general mode reg */
		volatile unsigned long	scc_gsmrb; /* SCC general mode reg */
		volatile unsigned short	scc_psmr;	/* protocol specific mode register */
		volatile unsigned char	RESERVED42[0x2]; /* Reserved area */
		volatile unsigned short	scc_todr; /* SCC transmit on demand */
		volatile unsigned short	scc_dsr;	/* SCC data sync reg */
		volatile unsigned short	scc_scce;	/* SCC event reg */
		volatile unsigned char	RESERVED43[0x2];/* Reserved area */
		volatile unsigned short	scc_sccm;	/* SCC mask reg */
		volatile unsigned char	RESERVED44[0x1];/* Reserved area */
		volatile unsigned char	scc_sccs;	/* SCC status reg */
		volatile unsigned char	RESERVED45[0x8]; /* Reserved area */
	} scc_regs[4];
/* SMC */
	struct smc_regs {
		volatile unsigned char	RESERVED46[0x2]; /* Reserved area */
		volatile unsigned short	smc_smcmr;	/* SMC mode reg */
		volatile unsigned char	RESERVED60[0x2];/* Reserved area */
		volatile unsigned char	smc_smce;	/* SMC event reg */
		volatile unsigned char	RESERVED47[0x3]; /* Reserved area */
		volatile unsigned char	smc_smcm;	/* SMC mask reg */
		volatile unsigned char	RESERVED48[0x5]; /* Reserved area */
	} smc_regs[2];
/* SPI */
	volatile unsigned short	spi_spmode;	/* SPI mode reg */
	volatile unsigned char	RESERVED51[0x4];	/* Reserved area */
	volatile unsigned char	spi_spie;	/* SPI event reg */
	volatile unsigned char	RESERVED52[0x3];	/* Reserved area */
	volatile unsigned char	spi_spim;	/* SPI mask reg */
	volatile unsigned char	RESERVED53[0x2];	/* Reserved area */
	volatile unsigned char	spi_spcom;	/* SPI command reg */
	volatile unsigned char	RESERVED54[0x4];	/* Reserved area */
/* PIP */
	volatile unsigned short	pip_pipc;	/* pip configuration reg */
	volatile unsigned char	RESERVED65[0x2];	/* Reserved area */
	volatile unsigned short	pip_ptpr;	/* pip timing parameters reg */
	volatile unsigned long	pip_pbdir;	/* port b data direction reg */
	volatile unsigned long	pip_pbpar;	/* port b pin assignment reg */
	volatile unsigned long	pip_pbodr;	/* port b open drain reg */
	volatile unsigned long	pip_pbdat;	/* port b data reg */
	volatile unsigned char	RESERVED71[0x18];	/* Reserved area */
/* Serial Interface */
	volatile unsigned long	si_simode;	/* SI mode register */
	volatile unsigned char	si_sigmr;	/* SI global mode register */
	volatile unsigned char	RESERVED55; /* Reserved area */
	volatile unsigned char	si_sistr;	/* SI status register */
	volatile unsigned char	si_sicmr;	/* SI command register */
	volatile unsigned char	RESERVED56[0x4]; /* Reserved area */
	volatile unsigned long	si_sicr;	/* SI clock routing */
	volatile unsigned long	si_sirp;	/* SI ram pointers */
	volatile unsigned char	RESERVED57[0xc]; /* Reserved area */
	volatile unsigned char	si_siram[0x100]; /* SI routing ram */
} ttQuicc;

typedef ttQuicc * ttQuiccPtr;