#ifndef __ACCTON_H
#define __ACCTON_H

#define ACC_DATA            0x010    /* Board's data latch */
#define ACC_CMD             0x01C    /* Board's command register */
#define ACC_CCMD             0x00    /* Chip's command register */
#define ACC0_STARTPG         0x01    /* Starting page of ring bfr */
#define ACC0_STOPPG          0x02    /* Ending page +1 of ring bfr */
#define ACC0_BOUNDPTR        0x03    /* Boundary page of ring bfr */
#define ACC0_TSR             0x04    /* Transmit status reg */
#define ACC0_TPSR            0x04    /* Transmit starting page */
#define ACC0_TCNTLO          0x05    /* Low  byte of tx byte count */
#define ACC0_TCNTHI          0x06    /* High byte of tx byte count */
#define ACC0_ISR             0x07    /* Interrupt status reg */
#define ACC0_RADDLO          0x08    /* Low  byte of remote start address */
#define ACC0_RADDHI          0x09    /* High byte of remote start address */
#define ACC0_RCNTLO          0x0a    /* Remote byte count reg */
#define ACC0_RCNTHI          0x0b    /* Remote byte count reg */
#define ACC0_RXCR            0x0c    /* RX control reg */
#define ACC0_TXCR            0x0d    /* TX control reg */
#define ACC0_COUNTER0        0x0d    /* Rcv alignment error counter */
#define ACC0_DCFG            0x0e    /* Data configuration reg */
#define ACC0_COUNTER1        0x0e    /* Rcv CRC error counter */
#define ACC0_IMR             0x0f    /* Interrupt mask reg */
#define ACC0_COUNTER2        0x0f    /* Rcv missed frame error counter */
#define ACC1_PHYS            0x01    /* This board's physical enet addr */
#define ACC1_CURPAG          0x07    /* Current memory page */
#define ACC1_MULT            0x08    /* Desired multicast addr */
#define ACC_RESET            0x01F   /* Reset the board */
#define ACC_BYTE             0x000   /* Set card to byte mode */
#define ACC_WORD             0x001   /* Set card to word mode */
#define ACC_8SLOT            0x001   /* Indicate card is plot on  8 bit slot */
#define ACC_16SLOT           0x000   /* Indicate card is plot on 16 bit slot */
#define ACCC_STOP           0x001    /* Stop the chip */
#define ACCC_START          0x002    /* Start the chip */
#define ACCC_TRANS          0x004    /* Transmit a frame */
#define ACCC_RDMARD         0x008    /* Remote DMA read */
#define ACCC_RDMAWR         0x010    /* Remote DMA write */
#define ACCC_RDMASN         0x018    /* Remote DMA send packet */
#define ACCC_NODMA          0x020    /* No remote DMA used on this card */
#define ACCC_RDMAAB         0x020    /* Abort/Complete Remote DMA */
#define ACCC_PAGE           0x0c0    /* Page bit is 1st & 2nd MSB */
#define ACCC_PAGE0          0x000    /* Select page 0 of chip registers */
#define ACCC_PAGE1          0x040    /* Select page 1 of chip registers */
#define ACCC_PAGE2          0x080    /* Select page 2 of chip registers */
#define ACCRXCR_MON          0x020   /* Monitor mode */
#define ACCRXCR_BCST         0x00e   /* Accept broadcasts */
#define ACCTXCR_LOOP         0x002   /* Set loopback mode */
#define ACCDCFG_BM8          0x048   /* Set burst mode, 8 deep FIFO,
					8 bit mode */
#define ACCDCFG_BM16         0x049   /* Set burst mode, 8 deep FIFO,
					16 bit mode */
#define ACCDCFG_BM8AR        0x058   /* Set burst mode, 8 deep FIFO,
					8 bit mode ARM */
#define ACCISR_RXGOOD        0x001   /* Receiver, no error */
#define ACCISR_TXGOOD        0x002   /* Transmitter, no error */
#define ACCISR_RX_ERR        0x004   /* Receiver, with error */
#define ACCISR_TX_ERR        0x008   /* Transmitter, with error */
#define ACCISR_OVERRUN       0x010   /* Receiver overwrote the ring */
#define ACCISR_CNTOVRUN      0x020   /* Counters need emptying */
#define ACCISR_RESET         0x080   /* Reset completed */
#define ACCISR_ALL           0x01f   /* Interrupts we will enable
					(orginal value 3fh) */
#define ACCISR_DMA           0x040   /* Enable DMA transfer over interrupt */
#define ACCPS_RXOK           0x001   /* Received a good packet */
#define ACCTSR_TXOK          0x001   /* Transmit without error */
#define ACCTSR_COLL          0x004   /* Collided at least once */
#define ACCTSR_COLL16        0x008   /* Collided 16 times and was dropped */
#define ACCTSR_FIFOURUN      0x020   /* TX FIFO Underrun */
#define ACC_RBUF_0xSTAT              /* Received frame status */
#define ACC_RBUF_NXT_0xPG            /* Page after this frame */
#define ACC_RBUF_SIZE_0xLO           /* Length of this frame */
#define ACC_RBUF_SIZE_0xHI           /* Length of this frame */
#define ACC_RBUF_0xHDRLEN            /* Length of above header area */

#define ACC_CARD                0x0e8
#define BI_DIR			0x0e0
#define ACK_Signal		0x40
#define IRQ_ENABLE		0x10
#define SLCTIN			8
#define INIT			4
#define ATFD			2
#define STROBE			1
#define BIDIR_INIT              (BI_DIR | INIT)
#define BIDIR_INIT_ATFD         (BIDIR_INIT | ATFD)
#define INIT_ATFD               (INIT | ATFD)
#define INIT_STROBE             (INIT | STROBE)
#define EEPROM			0x10
#define FMRegister		0x0A0
#define FMData			0x0C0
#define FMTest			0x0E0
#define SMRegister		0x20
#define SMData			0x40
#define SMTest			0x60
#define Clear_Address_Latch	0x20
#define Clear_All		0
#define StartReset		0x60
#define StopReset		0x20
#define DI_High 		1
#define DI_Low			0
#define DO_High 		1
#define DO_Low			0
#define RAMADDR_8p              0x2000
#define RAMSIZE_8p              0x2000
#define TXPAGE_8p               0x20
#define PSTART_8p               0x26
#define PSTOP_8p                0x40
#define BUFFER_TEST_DATA        0x55
#define ACCTON_ID_LOW        	0x20
#define ACCTON_ID_HIGH       	0x62
#define CHANNEL_RESET        	0x80
#define ADAPTER_SETUP        	0x96
#define CARD_ID              	0x100
#define POS_BASE             	0x102

#ifdef POLLING
  extern struct timer_list ethpk_poll           LOCKED_VAR;
  extern int               ethpk_irq            LOCKED_VAR;
  extern int               ethpk_poll_interval  LOCKED_VAR;
#endif

extern void Set_Register_Value  (BYTE reg, BYTE value);
extern int  Read_Register_Value (BYTE reg);

extern void en_prn_int  (void);
extern int  ethpk_probe (struct device *dev);

#endif
