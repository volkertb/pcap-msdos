/*
 *  Wavelan ISA driver
 *
 *    Jean II - HPLB '96
 *
 * Reorganisation and extension of the driver.
 * Original copyrigth follow (see also end of this file).
 * See wavelan.p.h for details.
 */

/*
 * AT&T GIS (nee NCR) WaveLAN card:
 *  An Ethernet-like radio transceiver
 *  controlled by an Intel 82586 coprocessor.
 */

#include "wavelan.h"

/************************* MISC SUBROUTINES **************************/
/*
 * Subroutines which won't fit in one of the following category
 * (wavelan modem or i82586)
 */

/*
 * Wrapper for disabling interrupts.
 */
static inline DWORD wv_splhi (void)
{
  DWORD flags;
  save_flags(flags);
  cli();
  return (flags);
}

/*
 * Wrapper for re-enabling interrupts.
 */
static inline void wv_splx (DWORD flags)
{
  restore_flags(flags);
}

/*
 * Translate irq number to PSA irq parameter
 */
static u_char wv_irq_to_psa (int irq)
{
  if (irq < 0 || irq >= NELS(irqvals))
     return (0);
  return (irqvals[irq]);
}

/*
 * Translate PSA irq parameter to irq number 
 */
static int wv_psa_to_irq (u_char irqval)
{
  int  irq;

  for (irq = 0; irq < NELS(irqvals); irq++)
      if (irqvals[irq] == irqval)
         return (irq);
  return (-1);
}

#ifdef STRUCT_CHECK
/*
 * Sanity routine to verify the sizes of the various WaveLAN interface
 * structures.
 */
static char *wv_struct_check (void)
{
#define  SC(t,s,n)  if (sizeof(t) != s) return(n);

  SC(psa_t, PSA_SIZE, "psa_t");
  SC(mmw_t, MMW_SIZE, "mmw_t");
  SC(mmr_t, MMR_SIZE, "mmr_t");
  SC(ha_t, HA_SIZE, "ha_t");

#undef  SC

  return((char *) NULL);
}
#endif  /* STRUCT_CHECK */

/********************* HOST ADAPTER SUBROUTINES *********************/
/*
 * Usefull subroutines to manage the wavelan ISA interface
 *
 * One major difference with the Pcmcia hardware (exept the port mapping)
 * is that we have to keep the state of the Host Control Register
 * because of the interrupt enable & bus size flags.
 */

/*
 * Read from card's Host Adaptor Status Register.
 */
static inline u_short hasr_read (u_short ioaddr)
{
  return (inw(HASR(ioaddr)));
}

/*
 * Write to card's Host Adapter Command Register.
 */
static inline void hacr_write (u_short ioaddr, u_short hacr)
{
  outw (hacr, HACR(ioaddr));
}

/*------------------------------------------------------------------*/
/*
 * Write to card's Host Adapter Command Register. Include a delay for
 * those times when it is needed.
 */
static inline void
hacr_write_slow(u_short  ioaddr,
    u_short  hacr)
{
  hacr_write(ioaddr, hacr);
  /* delay might only be needed sometimes */
  udelay(1000L);
} /* hacr_write_slow */

/*------------------------------------------------------------------*/
/*
 * Set the channel attention bit.
 */
static inline void
set_chan_attn(u_short  ioaddr,
        u_short  hacr)
{
  hacr_write(ioaddr, hacr | HACR_CA);
} /* set_chan_attn */

/*------------------------------------------------------------------*/
/*
 * Reset, and then set host adaptor into default mode.
 */
static inline void
wv_hacr_reset(u_short  ioaddr)
{
  hacr_write_slow(ioaddr, HACR_RESET);
  hacr_write(ioaddr, HACR_DEFAULT);
} /* wv_hacr_reset */

/*------------------------------------------------------------------*/
/*
 * Set the i/o transfer over the ISA bus to 8 bits mode
 */
static inline void
wv_16_off(u_short  ioaddr,
    u_short  hacr)
{
  hacr &= ~HACR_16BITS;
  hacr_write(ioaddr, hacr);
} /* wv_16_off */

/*------------------------------------------------------------------*/
/*
 * Set the i/o transfer over the ISA bus to 8 bits mode
 */
static inline void
wv_16_on(u_short  ioaddr,
   u_short  hacr)
{
  hacr |= HACR_16BITS;
  hacr_write(ioaddr, hacr);
} /* wv_16_on */

/*------------------------------------------------------------------*/
/*
 * Disable interrupts on the wavelan hardware
 */
static inline void
wv_ints_off(device *  dev)
{
  net_local *  lp = (net_local *)dev->priv;
  u_short  ioaddr = dev->base_addr;
  u_long  x;

  x = wv_splhi();

  lp->hacr &= ~HACR_INTRON;
  hacr_write(ioaddr, lp->hacr);

  wv_splx(x);
} /* wv_ints_off */

/*------------------------------------------------------------------*/
/*
 * Enable interrupts on the wavelan hardware
 */
static inline void
wv_ints_on(device *  dev)
{
  net_local *  lp = (net_local *)dev->priv;
  u_short  ioaddr = dev->base_addr;
  u_long  x;

  x = wv_splhi();

  lp->hacr |= HACR_INTRON;
  hacr_write(ioaddr, lp->hacr);

  wv_splx(x);
} /* wv_ints_on */

/******************* MODEM MANAGEMENT SUBROUTINES *******************/
/*
 * Usefull subroutines to manage the modem of the wavelan
 */

/*------------------------------------------------------------------*/
/*
 * Read the Parameter Storage Area from the WaveLAN card's memory
 */
/*
 * Read bytes from the PSA.
 */
static void
psa_read(u_short  ioaddr,
   u_short  hacr,
   int    o,  /* offset in PSA */
   u_char *  b,  /* buffer to fill */
   int    n)  /* size to read */
{
  wv_16_off(ioaddr, hacr);

  while(n-- > 0)
    {
      outw(o, PIOR2(ioaddr));
      o++;
      *b++ = inb(PIOP2(ioaddr));
    }

  wv_16_on(ioaddr, hacr);
} /* psa_read */

/*------------------------------------------------------------------*/
/*
 * Write the Paramter Storage Area to the WaveLAN card's memory
 */
static void
psa_write(u_short  ioaddr,
    u_short  hacr,
    int    o,  /* Offset in psa */
    u_char *  b,  /* Buffer in memory */
    int    n)  /* Length of buffer */
{
  int  count = 0;

  wv_16_off(ioaddr, hacr);

  while(n-- > 0)
    {
      outw(o, PIOR2(ioaddr));
      o++;

      outb(*b, PIOP2(ioaddr));
      b++;

      /* Wait for the memory to finish its write cycle */
      count = 0;
      while((count++ < 100) &&
      (hasr_read(ioaddr) & HASR_PSA_BUSY))
  udelay(1000);
    }

  wv_16_on(ioaddr, hacr);
} /* psa_write */

#ifdef PSA_CRC
/*------------------------------------------------------------------*/
/*
 * Calculate the PSA CRC (not tested yet)
 * As the Wavelan drivers don't use the CRC, I won't use it either...
 * Thanks to Valster, Nico <NVALSTER@wcnd.nl.lucent.com> for the code
 * NOTE: By specifying a length including the CRC position the
 * returned value should be zero. (i.e. a correct checksum in the PSA)
 */
static u_short
psa_crc(u_short *  psa,  /* The PSA */
  int    size)  /* Number of short for CRC */
{
  int    byte_cnt;  /* Loop on the PSA */
  u_short  crc_bytes = 0;  /* Data in the PSA */
  int    bit_cnt;  /* Loop on the bits of the short */

  for(byte_cnt = 0; byte_cnt <= size; byte_cnt++ )
    {
      crc_bytes ^= psa[byte_cnt];  /* Its an xor */

      for(bit_cnt = 1; bit_cnt < 9; bit_cnt++ )
  {
    if(crc_bytes & 0x0001)
      crc_bytes = (crc_bytes >> 1) ^ 0xA001;
    else
      crc_bytes >>= 1 ;
        }
    }

  return crc_bytes;
} /* psa_crc */
#endif  /* PSA_CRC */

/*------------------------------------------------------------------*/
/*
 * Write 1 byte to the MMC.
 */
static inline void
mmc_out(u_short    ioaddr,
  u_short    o,
  u_char    d)
{
  /* Wait for MMC to go idle */
  while(inw(HASR(ioaddr)) & HASR_MMC_BUSY)
    ;

  outw((u_short) (((u_short) d << 8) | (o << 1) | 1),
       MMCR(ioaddr));
}

/*------------------------------------------------------------------*/
/*
 * Routine to write bytes to the Modem Management Controller.
 * We start by the end because it is the way it should be !
 */
static inline void
mmc_write(u_short  ioaddr,
    u_char  o,
    u_char *  b,
    int    n)
{
  o += n;
  b += n;

  while(n-- > 0 )
    mmc_out(ioaddr, --o, *(--b));
} /* mmc_write */

/*------------------------------------------------------------------*/
/*
 * Read 1 byte from the MMC.
 * Optimised version for 1 byte, avoid using memory...
 */
static inline u_char
mmc_in(u_short  ioaddr,
       u_short  o)
{
  while(inw(HASR(ioaddr)) & HASR_MMC_BUSY)
    ;
  outw(o << 1, MMCR(ioaddr));

  while(inw(HASR(ioaddr)) & HASR_MMC_BUSY)
    ;
  return (u_char) (inw(MMCR(ioaddr)) >> 8);
}

/*------------------------------------------------------------------*/
/*
 * Routine to read bytes from the Modem Management Controller.
 * The implementation is complicated by a lack of address lines,
 * which prevents decoding of the low-order bit.
 * (code has just been moved in the above function)
 * We start by the end because it is the way it should be !
 */
static inline void
mmc_read(u_short  ioaddr,
   u_char    o,
   u_char *  b,
   int    n)
{
  o += n;
  b += n;

  while(n-- > 0)
    *(--b) = mmc_in(ioaddr, --o);
} /* mmc_read */

/*------------------------------------------------------------------*/
/*
 * Wait for the frequency EEprom to complete a command...
 * I hope this one will be optimally inlined...
 */
static inline void
fee_wait(u_short  ioaddr,  /* i/o port of the card */
   int    delay,  /* Base delay to wait for */
   int    number)  /* Number of time to wait */
{
  int    count = 0;  /* Wait only a limited time */

  while((count++ < number) &&
  (mmc_in(ioaddr, mmroff(0, mmr_fee_status)) & MMR_FEE_STATUS_BUSY))
    udelay(delay);
}

/*------------------------------------------------------------------*/
/*
 * Read bytes from the Frequency EEprom (frequency select cards).
 */
static void
fee_read(u_short  ioaddr,  /* i/o port of the card */
   u_short  o,  /* destination offset */
   u_short *  b,  /* data buffer */
   int    n)  /* number of registers */
{
  b += n;    /* Position at the end of the area */

  /* Write the address */
  mmc_out(ioaddr, mmwoff(0, mmw_fee_addr), o + n - 1);

  /* Loop on all buffer */
  while(n-- > 0)
    {
      /* Write the read command */
      mmc_out(ioaddr, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_READ);

      /* Wait until EEprom is ready (should be quick !) */
      fee_wait(ioaddr, 10, 100);

      /* Read the value */
      *--b = ((mmc_in(ioaddr, mmroff(0, mmr_fee_data_h)) << 8) |
        mmc_in(ioaddr, mmroff(0, mmr_fee_data_l)));
    }
}

/*------------------------------------------------------------------*/
/*
 * Write bytes from the Frequency EEprom (frequency select cards).
 * This is a bit complicated, because the frequency eeprom has to
 * be unprotected and the write enabled.
 * Jean II
 */
static void
fee_write(u_short  ioaddr,  /* i/o port of the card */
    u_short  o,  /* destination offset */
    u_short *  b,  /* data buffer */
    int    n)  /* number of registers */
{
  b += n;    /* Position at the end of the area */

#ifdef EEPROM_IS_PROTECTED  /* disabled */
#ifdef DOESNT_SEEM_TO_WORK  /* disabled */
  /* Ask to read the protected register */
  mmc_out(ioaddr, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_PRREAD);

  fee_wait(ioaddr, 10, 100);

  /* Read the protected register */
  printk("Protected 2 : %02X-%02X\n",
   mmc_in(ioaddr, mmroff(0, mmr_fee_data_h)),
   mmc_in(ioaddr, mmroff(0, mmr_fee_data_l)));
#endif  /* DOESNT_SEEM_TO_WORK */

  /* Enable protected register */
  mmc_out(ioaddr, mmwoff(0, mmw_fee_addr), MMW_FEE_ADDR_EN);
  mmc_out(ioaddr, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_PREN);

  fee_wait(ioaddr, 10, 100);

  /* Unprotect area */
  mmc_out(ioaddr, mmwoff(0, mmw_fee_addr), o + n);
  mmc_out(ioaddr, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_PRWRITE);
#ifdef DOESNT_SEEM_TO_WORK  /* disabled */
  /* Or use : */
  mmc_out(ioaddr, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_PRCLEAR);
#endif  /* DOESNT_SEEM_TO_WORK */

  fee_wait(ioaddr, 10, 100);
#endif  /* EEPROM_IS_PROTECTED */

  /* Write enable */
  mmc_out(ioaddr, mmwoff(0, mmw_fee_addr), MMW_FEE_ADDR_EN);
  mmc_out(ioaddr, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_WREN);

  fee_wait(ioaddr, 10, 100);

  /* Write the EEprom address */
  mmc_out(ioaddr, mmwoff(0, mmw_fee_addr), o + n - 1);

  /* Loop on all buffer */
  while(n-- > 0)
    {
      /* Write the value */
      mmc_out(ioaddr, mmwoff(0, mmw_fee_data_h), (*--b) >> 8);
      mmc_out(ioaddr, mmwoff(0, mmw_fee_data_l), *b & 0xFF);

      /* Write the write command */
      mmc_out(ioaddr, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_WRITE);

      /* Wavelan doc says : wait at least 10 ms for EEBUSY = 0 */
      udelay(10000);
      fee_wait(ioaddr, 10, 100);
    }

  /* Write disable */
  mmc_out(ioaddr, mmwoff(0, mmw_fee_addr), MMW_FEE_ADDR_DS);
  mmc_out(ioaddr, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_WDS);

  fee_wait(ioaddr, 10, 100);

#ifdef EEPROM_IS_PROTECTED  /* disabled */
  /* Reprotect EEprom */
  mmc_out(ioaddr, mmwoff(0, mmw_fee_addr), 0x00);
  mmc_out(ioaddr, mmwoff(0, mmw_fee_ctrl), MMW_FEE_CTRL_PRWRITE);

  fee_wait(ioaddr, 10, 100);
#endif  /* EEPROM_IS_PROTECTED */
}

/************************ I82586 SUBROUTINES *************************/
/*
 * Usefull subroutines to manage the Ethernet controler
 */

/*------------------------------------------------------------------*/
/*
 * Read bytes from the on-board RAM.
 * Why inlining this function make it fail ???
 */
static /*inline*/ void
obram_read(u_short  ioaddr,
     u_short  o,
     u_char *  b,
     int    n)
{
  outw(o, PIOR1(ioaddr));
  insw(PIOP1(ioaddr), (WORD *) b, (n + 1) >> 1);
}

/*------------------------------------------------------------------*/
/*
 * Write bytes to the on-board RAM.
 */
static inline void
obram_write(u_short  ioaddr,
      u_short  o,
      u_char *  b,
      int    n)
{
  outw(o, PIOR1(ioaddr));
  outsw(PIOP1(ioaddr), (WORD *) b, (n + 1) >> 1);
}

/*------------------------------------------------------------------*/
/*
 * Acknowledge the reading of the status issued by the i82586
 */
static void
wv_ack(device *    dev)
{
  net_local *  lp = (net_local *)dev->priv;
  u_short  ioaddr = dev->base_addr;
  u_short  scb_cs;
  int    i;

  obram_read(ioaddr, scboff(OFFSET_SCB, scb_status),
       (BYTE *) &scb_cs, sizeof(scb_cs));
  scb_cs &= SCB_ST_INT;

  if(scb_cs == 0)
    return;

  obram_write(ioaddr, scboff(OFFSET_SCB, scb_command),
        (BYTE *) &scb_cs, sizeof(scb_cs));

  set_chan_attn(ioaddr, lp->hacr);

  for(i = 1000; i > 0; i--)
    {
      obram_read(ioaddr, scboff(OFFSET_SCB, scb_command), (BYTE *)&scb_cs, sizeof(scb_cs));
      if(scb_cs == 0)
  break;

      udelay(10);
    }
  udelay(100);

#ifdef DEBUG_CONFIG_ERROR
  if(i <= 0)
    printk(KERN_INFO "%s: wv_ack(): board not accepting command.\n",
     dev->name);
#endif
}

/*------------------------------------------------------------------*/
/*
 * Set channel attention bit and busy wait until command has
 * completed, then acknowledge the command completion.
 */
static inline int
wv_synchronous_cmd(device *  dev,
       const char *  str)
{
  net_local *  lp = (net_local *)dev->priv;
  u_short  ioaddr = dev->base_addr;
  u_short  scb_cmd;
  ach_t    cb;
  int    i;

  scb_cmd = SCB_CMD_CUC & SCB_CMD_CUC_GO;
  obram_write(ioaddr, scboff(OFFSET_SCB, scb_command),
        (BYTE *) &scb_cmd, sizeof(scb_cmd));

  set_chan_attn(ioaddr, lp->hacr);

  for (i = 1000; i > 0; i--)
    {
      obram_read(ioaddr, OFFSET_CU, (BYTE *)&cb, sizeof(cb));
      if (cb.ac_status & AC_SFLD_C)
  break;

      udelay(10);
    }
  udelay(100);

  if(i <= 0 || !(cb.ac_status & AC_SFLD_OK))
    {
#ifdef DEBUG_CONFIG_ERROR
      printk(KERN_INFO "%s: %s failed; status = 0x%x\n",
       dev->name, str, cb.ac_status);
#endif
#ifdef DEBUG_I82586_SHOW
      wv_scb_show(ioaddr);
#endif
      return -1;
    }

  /* Ack the status */
  wv_ack(dev);

  return 0;
}

/*------------------------------------------------------------------*/
/*
 * Configuration commands completion interrupt.
 * Check if done, and if ok...
 */
static inline int
wv_config_complete(device *  dev,
       u_short  ioaddr,
       net_local *  lp)
{
  WORD  mcs_addr;
  WORD  status;
  int      ret;

#ifdef DEBUG_INTERRUPT_TRACE
  printk(KERN_DEBUG "%s: ->wv_config_complete()\n", dev->name);
#endif

  mcs_addr = lp->tx_first_in_use + sizeof(ac_tx_t) + sizeof(ac_nop_t)
    + sizeof(tbd_t) + sizeof(ac_cfg_t) + sizeof(ac_ias_t);

  /* Read the status of the last command (set mc list) */
  obram_read(ioaddr, acoff(mcs_addr, ac_status), (BYTE *)&status, sizeof(status));

  /* If not completed -> exit */
  if((status & AC_SFLD_C) == 0)
    ret = 0;    /* Not ready to be scrapped */
  else
    {
#ifdef DEBUG_CONFIG_ERROR
      WORD  cfg_addr;
      WORD  ias_addr;

      /* Check mc_config command */
      if(status & AC_SFLD_OK != 0)
  printk(KERN_INFO "wv_config_complete(): set_multicast_address failed; status = 0x%x\n",
         dev->name, str, status);

      /* check ia-config command */
      ias_addr = mcs_addr - sizeof(ac_ias_t);
      obram_read(ioaddr, acoff(ias_addr, ac_status), (BYTE *)&status, sizeof(status));
      if(status & AC_SFLD_OK != 0)
  printk(KERN_INFO "wv_config_complete(): set_MAC_address; status = 0x%x\n",
         dev->name, str, status);

      /* Check config command */
      cfg_addr = ias_addr - sizeof(ac_cfg_t);
      obram_read(ioaddr, acoff(cfg_addr, ac_status), (BYTE *)&status, sizeof(status));
      if(status & AC_SFLD_OK != 0)
  printk(KERN_INFO "wv_config_complete(): configure; status = 0x%x\n",
         dev->name, str, status);
#endif  /* DEBUG_CONFIG_ERROR */

      ret = 1;    /* Ready to be scrapped */
    }

#ifdef DEBUG_INTERRUPT_TRACE
  printk(KERN_DEBUG "%s: <-wv_config_complete() - %d\n", dev->name, ret);
#endif
  return ret;
}

/*------------------------------------------------------------------*/
/*
 * Command completion interrupt.
 * Reclaim as many freed tx buffers as we can.
 */
static int
wv_complete(device *  dev,
      u_short  ioaddr,
      net_local *  lp)
{
  int  nreaped = 0;

#ifdef DEBUG_INTERRUPT_TRACE
  printk(KERN_DEBUG "%s: ->wv_complete()\n", dev->name);
#endif

  /* Loop on all the transmit buffers */
  while(lp->tx_first_in_use != I82586NULL)
    {
      WORD  tx_status;

      /* Read the first transmit buffer */
      obram_read(ioaddr, acoff(lp->tx_first_in_use, ac_status), (BYTE *)&tx_status, sizeof(tx_status));

      /* Hack for reconfiguration... */
      if(tx_status == 0xFFFF)
  if(!wv_config_complete(dev, ioaddr, lp))
    break;  /* Not completed */

      /* If not completed -> exit */
      if((tx_status & AC_SFLD_C) == 0)
  break;

      /* We now remove this buffer */
      nreaped++;
      --lp->tx_n_in_use;

/*
if (lp->tx_n_in_use > 0)
  printk("%c", "0123456789abcdefghijk"[lp->tx_n_in_use]);
*/

      /* Was it the last one ? */
      if(lp->tx_n_in_use <= 0)
  lp->tx_first_in_use = I82586NULL;
      else
  {
    /* Next one in the chain */
    lp->tx_first_in_use += TXBLOCKZ;
    if(lp->tx_first_in_use >= OFFSET_CU + NTXBLOCKS * TXBLOCKZ)
      lp->tx_first_in_use -= NTXBLOCKS * TXBLOCKZ;
  }

      /* Hack for reconfiguration... */
      if(tx_status == 0xFFFF)
  continue;

      /* Now, check status of the finished command */
      if(tx_status & AC_SFLD_OK)
  {
    int  ncollisions;

    lp->stats.tx_packets++;
    ncollisions = tx_status & AC_SFLD_MAXCOL;
    lp->stats.tx_collisions += ncollisions;
#ifdef DEBUG_INTERRUPT_INFO
    if(ncollisions > 0)
      printk(KERN_DEBUG "%s: wv_complete(): tx completed after %d collisions.\n",
       dev->name, ncollisions);
#endif
  }
      else
  {
    lp->stats.tx_errors++;
#ifndef IGNORE_NORMAL_XMIT_ERRS
    if(tx_status & AC_SFLD_S10)
      {
        lp->stats.tx_carrier_errors++;
#ifdef DEBUG_INTERRUPT_ERROR
        printk(KERN_INFO "%s: wv_complete(): tx error: no CS.\n",
         dev->name);
#endif
      }
#endif  /* IGNORE_NORMAL_XMIT_ERRS */
    if(tx_status & AC_SFLD_S9)
      {
        lp->stats.tx_carrier_errors++;
#ifdef DEBUG_INTERRUPT_ERROR
        printk(KERN_INFO "%s: wv_complete(): tx error: lost CTS.\n",
         dev->name);
#endif
      }
    if(tx_status & AC_SFLD_S8)
      {
        lp->stats.tx_fifo_errors++;
#ifdef DEBUG_INTERRUPT_ERROR
        printk(KERN_INFO "%s: wv_complete(): tx error: slow DMA.\n",
         dev->name);
#endif
      }
#ifndef IGNORE_NORMAL_XMIT_ERRS
    if(tx_status & AC_SFLD_S6)
      {
        lp->stats.tx_heartbeat_errors++;
#ifdef DEBUG_INTERRUPT_ERROR
        printk(KERN_INFO "%s: wv_complete(): tx error: heart beat.\n",
         dev->name);
#endif
      }
    if(tx_status & AC_SFLD_S5)
      {
        lp->stats.tx_aborted_errors++;
#ifdef DEBUG_INTERRUPT_ERROR
        printk(KERN_INFO "%s: wv_complete(): tx error: too many collisions.\n",
         dev->name);
#endif
      }
#endif  /* IGNORE_NORMAL_XMIT_ERRS */
  }

#ifdef DEBUG_INTERRUPT_INFO
      printk(KERN_DEBUG "%s: wv_complete(): tx completed, tx_status 0x%04x\n",
       dev->name, tx_status);
#endif
    }

#ifdef DEBUG_INTERRUPT_INFO
  if(nreaped > 1)
    printk(KERN_DEBUG "%s: wv_complete(): reaped %d\n", dev->name, nreaped);
#endif

  /*
   * Inform upper layers.
   */
  if(lp->tx_n_in_use < NTXBLOCKS - 1)
    {
      dev->tbusy = 0;
      mark_bh(NET_BH);
    }

#ifdef DEBUG_INTERRUPT_TRACE
  printk(KERN_DEBUG "%s: <-wv_complete()\n", dev->name);
#endif
  return nreaped;
}

/*------------------------------------------------------------------*/
/*
 * Reconfigure the i82586, or at least ask for it...
 * Because wv_82586_config use a transmission buffer, we must do it
 * when we are sure that there is one left, so we do it now
 * or in wavelan_packet_xmit() (I can't find any better place,
 * wavelan_interrupt is not an option...), so you may experience
 * some delay sometime...
 */
static inline void
wv_82586_reconfig(device *  dev)
{
  net_local *  lp = (net_local *)dev->priv;

  /* Check if we can do it now ! */
  if(!(dev->start) || (set_bit(0, (void *)&dev->tbusy) != 0))
    {
      lp->reconfig_82586 = 1;
#ifdef DEBUG_CONFIG_INFO
      printk(KERN_DEBUG "%s: wv_82586_reconfig(): delayed (busy = %ld, start = %d)\n",
       dev->name, dev->tbusy, dev->start);
#endif
    }
  else
    wv_82586_config(dev);
}

/********************* DEBUG & INFO SUBROUTINES *********************/
/*
 * This routines are used in the code to show debug informations.
 * Most of the time, it dump the content of hardware structures...
 */

#ifdef DEBUG_PSA_SHOW
/*------------------------------------------------------------------*/
/*
 * Print the formatted contents of the Parameter Storage Area.
 */
static void
wv_psa_show(psa_t *  p)
{
  printk(KERN_DEBUG "##### wavelan psa contents: #####\n");
  printk(KERN_DEBUG "psa_io_base_addr_1: 0x%02X %02X %02X %02X\n",
   p->psa_io_base_addr_1,
   p->psa_io_base_addr_2,
   p->psa_io_base_addr_3,
   p->psa_io_base_addr_4);
  printk(KERN_DEBUG "psa_rem_boot_addr_1: 0x%02X %02X %02X\n",
   p->psa_rem_boot_addr_1,
   p->psa_rem_boot_addr_2,
   p->psa_rem_boot_addr_3);
  printk(KERN_DEBUG "psa_holi_params: 0x%02x, ", p->psa_holi_params);
  printk("psa_int_req_no: %d\n", p->psa_int_req_no);
#ifdef DEBUG_SHOW_UNUSED
  printk(KERN_DEBUG "psa_unused0[]: %02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
   p->psa_unused0[0],
   p->psa_unused0[1],
   p->psa_unused0[2],
   p->psa_unused0[3],
   p->psa_unused0[4],
   p->psa_unused0[5],
   p->psa_unused0[6]);
#endif  /* DEBUG_SHOW_UNUSED */
  printk(KERN_DEBUG "psa_univ_mac_addr[]: %02x:%02x:%02x:%02x:%02x:%02x\n",
   p->psa_univ_mac_addr[0],
   p->psa_univ_mac_addr[1],
   p->psa_univ_mac_addr[2],
   p->psa_univ_mac_addr[3],
   p->psa_univ_mac_addr[4],
   p->psa_univ_mac_addr[5]);
  printk(KERN_DEBUG "psa_local_mac_addr[]: %02x:%02x:%02x:%02x:%02x:%02x\n",
   p->psa_local_mac_addr[0],
   p->psa_local_mac_addr[1],
   p->psa_local_mac_addr[2],
   p->psa_local_mac_addr[3],
   p->psa_local_mac_addr[4],
   p->psa_local_mac_addr[5]);
  printk(KERN_DEBUG "psa_univ_local_sel: %d, ", p->psa_univ_local_sel);
  printk("psa_comp_number: %d, ", p->psa_comp_number);
  printk("psa_thr_pre_set: 0x%02x\n", p->psa_thr_pre_set);
  printk(KERN_DEBUG "psa_feature_select/decay_prm: 0x%02x, ",
   p->psa_feature_select);
  printk("psa_subband/decay_update_prm: %d\n", p->psa_subband);
  printk(KERN_DEBUG "psa_quality_thr: 0x%02x, ", p->psa_quality_thr);
  printk("psa_mod_delay: 0x%02x\n", p->psa_mod_delay);
  printk(KERN_DEBUG "psa_nwid: 0x%02x%02x, ", p->psa_nwid[0], p->psa_nwid[1]);
  printk("psa_nwid_select: %d\n", p->psa_nwid_select);
  printk(KERN_DEBUG "psa_encryption_select: %d, ", p->psa_encryption_select);
  printk("psa_encryption_key[]: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
   p->psa_encryption_key[0],
   p->psa_encryption_key[1],
   p->psa_encryption_key[2],
   p->psa_encryption_key[3],
   p->psa_encryption_key[4],
   p->psa_encryption_key[5],
   p->psa_encryption_key[6],
   p->psa_encryption_key[7]);
  printk(KERN_DEBUG "psa_databus_width: %d\n", p->psa_databus_width);
  printk(KERN_DEBUG "psa_call_code/auto_squelch: 0x%02x, ",
   p->psa_call_code[0]);
  printk("psa_call_code[]: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
   p->psa_call_code[0],
   p->psa_call_code[1],
   p->psa_call_code[2],
   p->psa_call_code[3],
   p->psa_call_code[4],
   p->psa_call_code[5],
   p->psa_call_code[6],
   p->psa_call_code[7]);
#ifdef DEBUG_SHOW_UNUSED
  printk(KERN_DEBUG "psa_reserved[]: %02X:%02X:%02X:%02X\n",
   p->psa_reserved[0],
   p->psa_reserved[1],
   p->psa_reserved[2],
   p->psa_reserved[3]);
#endif  /* DEBUG_SHOW_UNUSED */
  printk(KERN_DEBUG "psa_conf_status: %d, ", p->psa_conf_status);
  printk("psa_crc: 0x%02x%02x, ", p->psa_crc[0], p->psa_crc[1]);
  printk("psa_crc_status: 0x%02x\n", p->psa_crc_status);
} /* wv_psa_show */
#endif  /* DEBUG_PSA_SHOW */

#ifdef DEBUG_MMC_SHOW
/*------------------------------------------------------------------*/
/*
 * Print the formatted status of the Modem Management Controller.
 * This function need to be completed...
 */
static void
wv_mmc_show(device *  dev)
{
  u_short  ioaddr = dev->base_addr;
  net_local *  lp = (net_local *)dev->priv;
  mmr_t    m;

  /* Basic check */
  if(hasr_read(ioaddr) & HASR_NO_CLK)
    {
      printk(KERN_WARNING "%s: wv_mmc_show: modem not connected\n",
       dev->name);
      return;
    }

  /* Read the mmc */
  mmc_out(ioaddr, mmwoff(0, mmw_freeze), 1);
  mmc_read(ioaddr, 0, (u_char *)&m, sizeof(m));
  mmc_out(ioaddr, mmwoff(0, mmw_freeze), 0);

  /* Don't forget to update statistics */
  lp->wstats.discard.nwid += (m.mmr_wrong_nwid_h << 8) | m.mmr_wrong_nwid_l;

  printk(KERN_DEBUG "##### wavelan modem status registers: #####\n");
#ifdef DEBUG_SHOW_UNUSED
  printk(KERN_DEBUG "mmc_unused0[]: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
   m.mmr_unused0[0],
   m.mmr_unused0[1],
   m.mmr_unused0[2],
   m.mmr_unused0[3],
   m.mmr_unused0[4],
   m.mmr_unused0[5],
   m.mmr_unused0[6],
   m.mmr_unused0[7]);
#endif  /* DEBUG_SHOW_UNUSED */
  printk(KERN_DEBUG "Encryption algorythm: %02X - Status: %02X\n",
   m.mmr_des_avail, m.mmr_des_status);
#ifdef DEBUG_SHOW_UNUSED
  printk(KERN_DEBUG "mmc_unused1[]: %02X:%02X:%02X:%02X:%02X\n",
   m.mmr_unused1[0],
   m.mmr_unused1[1],
   m.mmr_unused1[2],
   m.mmr_unused1[3],
   m.mmr_unused1[4]);
#endif  /* DEBUG_SHOW_UNUSED */
  printk(KERN_DEBUG "dce_status: 0x%x [%s%s%s%s]\n",
   m.mmr_dce_status,
   (m.mmr_dce_status & MMR_DCE_STATUS_RX_BUSY) ? "energy detected,":"",
   (m.mmr_dce_status & MMR_DCE_STATUS_LOOPT_IND) ?
   "loop test indicated," : "",
   (m.mmr_dce_status & MMR_DCE_STATUS_TX_BUSY) ? "transmitter on," : "",
   (m.mmr_dce_status & MMR_DCE_STATUS_JBR_EXPIRED) ?
   "jabber timer expired," : "");
  printk(KERN_DEBUG "Dsp ID: %02X\n",
   m.mmr_dsp_id);
#ifdef DEBUG_SHOW_UNUSED
  printk(KERN_DEBUG "mmc_unused2[]: %02X:%02X\n",
   m.mmr_unused2[0],
   m.mmr_unused2[1]);
#endif  /* DEBUG_SHOW_UNUSED */
  printk(KERN_DEBUG "# correct_nwid: %d, # wrong_nwid: %d\n",
   (m.mmr_correct_nwid_h << 8) | m.mmr_correct_nwid_l,
   (m.mmr_wrong_nwid_h << 8) | m.mmr_wrong_nwid_l);
  printk(KERN_DEBUG "thr_pre_set: 0x%x [current signal %s]\n",
   m.mmr_thr_pre_set & MMR_THR_PRE_SET,
   (m.mmr_thr_pre_set & MMR_THR_PRE_SET_CUR) ? "above" : "below");
  printk(KERN_DEBUG "signal_lvl: %d [%s], ",
   m.mmr_signal_lvl & MMR_SIGNAL_LVL,
   (m.mmr_signal_lvl & MMR_SIGNAL_LVL_VALID) ? "new msg" : "no new msg");
  printk("silence_lvl: %d [%s], ", m.mmr_silence_lvl & MMR_SILENCE_LVL,
   (m.mmr_silence_lvl & MMR_SILENCE_LVL_VALID) ? "update done" : "no new update");
  printk("sgnl_qual: 0x%x [%s]\n",
   m.mmr_sgnl_qual & MMR_SGNL_QUAL,
   (m.mmr_sgnl_qual & MMR_SGNL_QUAL_ANT) ? "Antenna 1" : "Antenna 0");
#ifdef DEBUG_SHOW_UNUSED
  printk(KERN_DEBUG "netw_id_l: %x\n", m.mmr_netw_id_l);
#endif  /* DEBUG_SHOW_UNUSED */
} /* wv_mmc_show */
#endif  /* DEBUG_MMC_SHOW */

#ifdef DEBUG_I82586_SHOW
/*------------------------------------------------------------------*/
/*
 * Print the last block of the i82586 memory
 */
static void
wv_scb_show(WORD  ioaddr)
{
  scb_t    scb;

  obram_read(ioaddr, OFFSET_SCB, (BYTE *)&scb, sizeof(scb));   

  printk(KERN_DEBUG "##### wavelan system control block: #####\n");

  printk(KERN_DEBUG "status: ");
  printk("stat 0x%x[%s%s%s%s] ",
   (scb.scb_status & (SCB_ST_CX | SCB_ST_FR | SCB_ST_CNA | SCB_ST_RNR)) >> 12,
   (scb.scb_status & SCB_ST_CX) ? "cmd completion interrupt," : "",
   (scb.scb_status & SCB_ST_FR) ? "frame received," : "",
   (scb.scb_status & SCB_ST_CNA) ? "cmd unit not active," : "",
   (scb.scb_status & SCB_ST_RNR) ? "rcv unit not ready," : "");
  printk("cus 0x%x[%s%s%s] ",
   (scb.scb_status & SCB_ST_CUS) >> 8,
   ((scb.scb_status & SCB_ST_CUS) == SCB_ST_CUS_IDLE) ? "idle" : "",
   ((scb.scb_status & SCB_ST_CUS) == SCB_ST_CUS_SUSP) ? "suspended" : "",
   ((scb.scb_status & SCB_ST_CUS) == SCB_ST_CUS_ACTV) ? "active" : "");
  printk("rus 0x%x[%s%s%s%s]\n",
   (scb.scb_status & SCB_ST_RUS) >> 4,
   ((scb.scb_status & SCB_ST_RUS) == SCB_ST_RUS_IDLE) ? "idle" : "",
   ((scb.scb_status & SCB_ST_RUS) == SCB_ST_RUS_SUSP) ? "suspended" : "",
   ((scb.scb_status & SCB_ST_RUS) == SCB_ST_RUS_NRES) ? "no resources" : "",
   ((scb.scb_status & SCB_ST_RUS) == SCB_ST_RUS_RDY) ? "ready" : "");

  printk(KERN_DEBUG "command: ");
  printk("ack 0x%x[%s%s%s%s] ",
   (scb.scb_command & (SCB_CMD_ACK_CX | SCB_CMD_ACK_FR | SCB_CMD_ACK_CNA | SCB_CMD_ACK_RNR)) >> 12,
   (scb.scb_command & SCB_CMD_ACK_CX) ? "ack cmd completion," : "",
   (scb.scb_command & SCB_CMD_ACK_FR) ? "ack frame received," : "",
   (scb.scb_command & SCB_CMD_ACK_CNA) ? "ack CU not active," : "",
   (scb.scb_command & SCB_CMD_ACK_RNR) ? "ack RU not ready," : "");
  printk("cuc 0x%x[%s%s%s%s%s] ",
   (scb.scb_command & SCB_CMD_CUC) >> 8,
   ((scb.scb_command & SCB_CMD_CUC) == SCB_CMD_CUC_NOP) ? "nop" : "",
   ((scb.scb_command & SCB_CMD_CUC) == SCB_CMD_CUC_GO) ? "start cbl_offset" : "",
   ((scb.scb_command & SCB_CMD_CUC) == SCB_CMD_CUC_RES) ? "resume execution" : "",
   ((scb.scb_command & SCB_CMD_CUC) == SCB_CMD_CUC_SUS) ? "suspend execution" : "",
   ((scb.scb_command & SCB_CMD_CUC) == SCB_CMD_CUC_ABT) ? "abort execution" : "");
  printk("ruc 0x%x[%s%s%s%s%s]\n",
   (scb.scb_command & SCB_CMD_RUC) >> 4,
   ((scb.scb_command & SCB_CMD_RUC) == SCB_CMD_RUC_NOP) ? "nop" : "",
   ((scb.scb_command & SCB_CMD_RUC) == SCB_CMD_RUC_GO) ? "start rfa_offset" : "",
   ((scb.scb_command & SCB_CMD_RUC) == SCB_CMD_RUC_RES) ? "resume reception" : "",
   ((scb.scb_command & SCB_CMD_RUC) == SCB_CMD_RUC_SUS) ? "suspend reception" : "",
   ((scb.scb_command & SCB_CMD_RUC) == SCB_CMD_RUC_ABT) ? "abort reception" : "");

  printk(KERN_DEBUG "cbl_offset 0x%x ", scb.scb_cbl_offset);
  printk("rfa_offset 0x%x\n", scb.scb_rfa_offset);

  printk(KERN_DEBUG "crcerrs %d ", scb.scb_crcerrs);
  printk("alnerrs %d ", scb.scb_alnerrs);
  printk("rscerrs %d ", scb.scb_rscerrs);
  printk("ovrnerrs %d\n", scb.scb_ovrnerrs);
}

/*------------------------------------------------------------------*/
/*
 * Print the formatted status of the i82586's receive unit.
 */
static void
wv_ru_show(device *  dev)
{
  /* net_local *lp = (net_local *) dev->priv; */

  printk(KERN_DEBUG "##### wavelan i82586 receiver unit status: #####\n");
  printk(KERN_DEBUG "ru:");
  /*
   * Not implemented yet...
   */
  printk("\n");
} /* wv_ru_show */

/*------------------------------------------------------------------*/
/*
 * Display info about one control block of the i82586 memory
 */
static void
wv_cu_show_one(device *    dev,
         net_local *  lp,
         int    i,
         u_short    p)
{
  WORD  ioaddr;
  ac_tx_t    actx;

  ioaddr = dev->base_addr;

  printk("%d: 0x%x:", i, p);

  obram_read(ioaddr, p, (BYTE *)&actx, sizeof(actx));
  printk(" status=0x%x,", actx.tx_h.ac_status);
  printk(" command=0x%x,", actx.tx_h.ac_command);

  /*
  {
    tbd_t  tbd;

    obram_read(ioaddr, actx.tx_tbd_offset, (BYTE *)&tbd, sizeof(tbd));
    printk(" tbd_status=0x%x,", tbd.tbd_status);
  }
  */

  printk("|");
}

/*------------------------------------------------------------------*/
/*
 * Print status of the command unit of the i82586
 */
static void
wv_cu_show(device *  dev)
{
  net_local *  lp = (net_local *)dev->priv;
  DWORD  i;
  u_short  p;

  printk(KERN_DEBUG "##### wavelan i82586 command unit status: #####\n");

  printk(KERN_DEBUG);
  for(i = 0, p = lp->tx_first_in_use; i < NTXBLOCKS; i++)
    {
      wv_cu_show_one(dev, lp, i, p);

      p += TXBLOCKZ;
      if(p >= OFFSET_CU + NTXBLOCKS * TXBLOCKZ)
  p -= NTXBLOCKS * TXBLOCKZ;
    }
  printk("\n");
}
#endif  /* DEBUG_I82586_SHOW */

#ifdef DEBUG_DEVICE_SHOW
/*------------------------------------------------------------------*/
/*
 * Print the formatted status of the WaveLAN PCMCIA device driver.
 */
static void
wv_dev_show(device *  dev)
{
  printk(KERN_DEBUG "dev:");
  printk(" start=%d,", dev->start);
  printk(" tbusy=%ld,", dev->tbusy);
  printk(" interrupt=%d,", dev->interrupt);
  printk(" trans_start=%ld,", dev->trans_start);
  printk(" flags=0x%x,", dev->flags);
  printk("\n");
} /* wv_dev_show */

/*------------------------------------------------------------------*/
/*
 * Print the formatted status of the WaveLAN PCMCIA device driver's
 * private information.
 */
static void
wv_local_show(device *  dev)
{
  net_local *lp;

  lp = (net_local *)dev->priv;

  printk(KERN_DEBUG "local:");
  printk(" tx_n_in_use=%d,", lp->tx_n_in_use);
  printk(" hacr=0x%x,", lp->hacr);
  printk(" rx_head=0x%x,", lp->rx_head);
  printk(" rx_last=0x%x,", lp->rx_last);
  printk(" tx_first_free=0x%x,", lp->tx_first_free);
  printk(" tx_first_in_use=0x%x,", lp->tx_first_in_use);
  printk("\n");
} /* wv_local_show */
#endif  /* DEBUG_DEVICE_SHOW */

#if defined(DEBUG_RX_INFO) || defined(DEBUG_TX_INFO)
/*------------------------------------------------------------------*/
/*
 * Dump packet header (and content if necessary) on the screen
 */
static inline void
wv_packet_info(u_char *    p,    /* Packet to dump */
         int    length,    /* Length of the packet */
         char *    msg1,    /* Name of the device */
         char *    msg2)    /* Name of the function */
{
#ifndef DEBUG_PACKET_DUMP
  printk(KERN_DEBUG "%s: %s(): dest %02X:%02X:%02X:%02X:%02X:%02X, length %d\n",
   msg1, msg2, p[0], p[1], p[2], p[3], p[4], p[5], length);
  printk(KERN_DEBUG "%s: %s(): src %02X:%02X:%02X:%02X:%02X:%02X, type 0x%02X%02X\n",
   msg1, msg2, p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13]);

#else  /* DEBUG_PACKET_DUMP */
  int    i;
  int    maxi;

  printk(KERN_DEBUG "%s: %s(): len=%d, data=\"", msg1, msg2, length);

  if((maxi = length) > DEBUG_PACKET_DUMP)
    maxi = DEBUG_PACKET_DUMP;
  for(i = 0; i < maxi; i++)
    if(p[i] >= ' ' && p[i] <= '~')
      printk(" %c", p[i]);
    else
      printk("%02X", p[i]);
  if(maxi < length)
    printk("..");
  printk("\"\n");
  printk(KERN_DEBUG "\n");
#endif  /* DEBUG_PACKET_DUMP */
}
#endif  /* defined(DEBUG_RX_INFO) || defined(DEBUG_TX_INFO) */

/*------------------------------------------------------------------*/
/*
 * This is the information which is displayed by the driver at startup
 * There  is a lot of flag to configure it at your will...
 */
static inline void
wv_init_info(device *  dev)
{
  short    ioaddr = dev->base_addr;
  net_local *  lp = (net_local *)dev->priv;
  psa_t    psa;
  int    i;

  /* Read the parameter storage area */
  psa_read(ioaddr, lp->hacr, 0, (BYTE *) &psa, sizeof(psa));

#ifdef DEBUG_PSA_SHOW
  wv_psa_show(&psa);
#endif
#ifdef DEBUG_MMC_SHOW
  wv_mmc_show(dev);
#endif
#ifdef DEBUG_I82586_SHOW
  wv_cu_show(dev);
#endif

#ifdef DEBUG_BASIC_SHOW
  /* Now, let's go for the basic stuff */
  printk(KERN_NOTICE "%s: WaveLAN at %x,", dev->name, ioaddr);
  for(i = 0; i < WAVELAN_ADDR_SIZE; i++)
    printk("%s%02X", (i == 0) ? " " : ":", dev->dev_addr[i]);
  printk(", IRQ %d", dev->irq);

  /* Print current network id */
  if(psa.psa_nwid_select)
    printk(", nwid 0x%02X-%02X", psa.psa_nwid[0], psa.psa_nwid[1]);
  else
    printk(", nwid off");

  /* If 2.00 card */
  if(!(mmc_in(ioaddr, mmroff(0, mmr_fee_status)) &
       (MMR_FEE_STATUS_DWLD | MMR_FEE_STATUS_BUSY)))
    {
      WORD  freq;

      /* Ask the EEprom to read the frequency from the first area */
      fee_read(ioaddr, 0x00 /* 1st area - frequency... */,
         &freq, 1);

      /* Print frequency */
      printk(", 2.00, %ld", (freq >> 6) + 2400L);

      /* Hack !!! */
      if(freq & 0x20)
  printk(".5");
    }
  else
    {
      printk(", PC");
      switch(psa.psa_comp_number)
  {
  case PSA_COMP_PC_AT_915:
  case PSA_COMP_PC_AT_2400:
    printk("-AT");
    break;
  case PSA_COMP_PC_MC_915:
  case PSA_COMP_PC_MC_2400:
    printk("-MC");
    break;
  case PSA_COMP_PCMCIA_915:
    printk("MCIA");
    break;
  default:
    printk("???");
  }
      printk(", ");
      switch (psa.psa_subband)
  {
  case PSA_SUBBAND_915:
    printk("915");
    break;
  case PSA_SUBBAND_2425:
    printk("2425");
    break;
  case PSA_SUBBAND_2460:
    printk("2460");
    break;
  case PSA_SUBBAND_2484:
    printk("2484");
    break;
  case PSA_SUBBAND_2430_5:
    printk("2430.5");
    break;
  default:
    printk("???");
  }
    }

  printk(" MHz\n");
#endif  /* DEBUG_BASIC_SHOW */

#ifdef DEBUG_VERSION_SHOW
  /* Print version information */
  printk(KERN_NOTICE "%s", version);
#endif
} /* wv_init_info */

/********************* IOCTL, STATS & RECONFIG *********************/
/*
 * We found here routines that are called by Linux on differents
 * occasions after the configuration and not for transmitting data
 * These may be called when the user use ifconfig, /proc/net/dev
 * or wireless extensions
 */

/*------------------------------------------------------------------*/
/*
 * Get the current ethernet statistics. This may be called with the
 * card open or closed.
 * Used when the user read /proc/net/dev
 */
static en_stats  *
wavelan_get_stats(device *  dev)
{
#ifdef DEBUG_IOCTL_TRACE
  printk(KERN_DEBUG "%s: <>wavelan_get_stats()\n", dev->name);
#endif

  return(&((net_local *) dev->priv)->stats);
}

/*------------------------------------------------------------------*/
/*
 * Set or clear the multicast filter for this adaptor.
 * num_addrs == -1  Promiscuous mode, receive all packets
 * num_addrs == 0  Normal mode, clear multicast list
 * num_addrs > 0  Multicast mode, receive normal and MC packets,
 *      and do best-effort filtering.
 */
static void
wavelan_set_multicast_list(device *  dev)
{
  net_local *  lp = (net_local *) dev->priv;

#ifdef DEBUG_IOCTL_TRACE
  printk(KERN_DEBUG "%s: ->wavelan_set_multicast_list()\n", dev->name);
#endif

#ifdef DEBUG_IOCTL_INFO
  printk(KERN_DEBUG "%s: wavelan_set_multicast_list(): setting Rx mode %02X to %d addresses.\n",
   dev->name, dev->flags, dev->mc_count);
#endif

  /* If we ask for promiscuous mode,
   * or all multicast addresses (we don't have that !)
   * or too much multicast addresses for the hardware filter */
  if((dev->flags & IFF_PROMISC) ||
     (dev->flags & IFF_ALLMULTI) ||
     (dev->mc_count > I82586_MAX_MULTICAST_ADDRESSES))
    {
      /*
       * Enable promiscuous mode: receive all packets.
       */
      if(!lp->promiscuous)
  {
    lp->promiscuous = 1;
    lp->mc_count = 0;

    wv_82586_reconfig(dev);

    /* Tell the kernel that we are doing a really bad job... */
    dev->flags |= IFF_PROMISC;
  }
    }
  else
    /* If there is some multicast addresses to send */
    if(dev->mc_list != (struct dev_mc_list *) NULL)
      {
  /*
   * Disable promiscuous mode, but receive all packets
   * in multicast list
   */
#ifdef MULTICAST_AVOID
  if(lp->promiscuous ||
     (dev->mc_count != lp->mc_count))
#endif
    {
      lp->promiscuous = 0;
      lp->mc_count = dev->mc_count;

      wv_82586_reconfig(dev);
    }
      }
    else
      {
  /*
   * Switch to normal mode: disable promiscuous mode and 
   * clear the multicast list.
   */
  if(lp->promiscuous || lp->mc_count == 0)
    {
      lp->promiscuous = 0;
      lp->mc_count = 0;

      wv_82586_reconfig(dev);
    }
      }
#ifdef DEBUG_IOCTL_TRACE
  printk(KERN_DEBUG "%s: <-wavelan_set_multicast_list()\n", dev->name);
#endif
}

/*------------------------------------------------------------------*/
/*
 * This function doesn't exist...
 */
static int
wavelan_set_mac_address(device *  dev,
      void *    addr)
{
  struct sockaddr *  mac = addr;

  /* Copy the address */
  memcpy(dev->dev_addr, mac->sa_data, WAVELAN_ADDR_SIZE);

  /* Reconfig the beast */
  wv_82586_reconfig(dev);

  return 0;
}

#ifdef WIRELESS_EXT  /* If wireless extension exist in the kernel */

/*------------------------------------------------------------------*/
/*
 * Frequency setting (for hardware able of it)
 * It's a bit complicated and you don't really want to look into it...
 * (called in wavelan_ioctl)
 */
static inline int
wv_set_frequency(u_short  ioaddr,  /* i/o port of the card */
     iw_freq *  frequency)
{
  const int  BAND_NUM = 10;  /* Number of bands */
  long    freq = 0L;  /* offset to 2.4 GHz in .5 MHz */
#ifdef DEBUG_IOCTL_INFO
  int    i;
#endif

  /* Setting by frequency */
  /* Theoritically, you may set any frequency between
   * the two limits with a 0.5 MHz precision. In practice,
   * I don't want you to have trouble with local
   * regulations... */
  if((frequency->e == 1) &&
     (frequency->m >= (int) 2.412e8) && (frequency->m <= (int) 2.487e8))
    {
      freq = ((frequency->m / 10000) - 24000L) / 5;
    }

  /* Setting by channel (same as wfreqsel) */
  /* Warning : each channel is 22MHz wide, so some of the channels
   * will interfere... */
  if((frequency->e == 0) &&
     (frequency->m >= 0) && (frequency->m < BAND_NUM))
    {
      /* frequency in 1/4 of MHz (as read in the offset register) */
      short  bands[] = { 0x30, 0x58, 0x64, 0x7A, 0x80, 0xA8, 0xD0, 0xF0, 0xF8, 0x150 };

      /* Get frequency offset */
      freq = bands[frequency->m] >> 1;
    }

  /* Verify if the frequency is allowed */
  if(freq != 0L)
    {
      u_short  table[10];  /* Authorized frequency table */

      /* Read the frequency table */
      fee_read(ioaddr, 0x71 /* frequency table */,
         table, 10);

#ifdef DEBUG_IOCTL_INFO
      printk(KERN_DEBUG "Frequency table :");
      for(i = 0; i < 10; i++)
  {
    printk(" %04X",
     table[i]);
  }
      printk("\n");
#endif

      /* Look in the table if the frequency is allowed */
      if(!(table[9 - ((freq - 24) / 16)] &
     (1 << ((freq - 24) % 16))))
  return -EINVAL;    /* not allowed */
    }
  else
    return -EINVAL;

  /* If we get a usable frequency */
  if(freq != 0L)
    {
      WORD  area[16];
      WORD  dac[2];
      WORD  area_verify[16];
      WORD  dac_verify[2];
      /* Corresponding gain (in the power adjust value table)
       * see AT&T Wavelan Data Manual, REF 407-024689/E, page 3-8
       * & WCIN062D.DOC, page 6.2.9 */
      WORD  power_limit[] = { 40, 80, 120, 160, 0 };
      int    power_band = 0;    /* Selected band */
      WORD  power_adjust;    /* Correct value */

      /* Search for the gain */
      power_band = 0;
      while((freq > power_limit[power_band]) &&
      (power_limit[++power_band] != 0))
  ;

      /* Read the first area */
      fee_read(ioaddr, 0x00,
         area, 16);

      /* Read the DAC */
      fee_read(ioaddr, 0x60,
         dac, 2);

      /* Read the new power adjust value */
      fee_read(ioaddr, 0x6B - (power_band >> 1),
         &power_adjust, 1);
      if(power_band & 0x1)
  power_adjust >>= 8;
      else
  power_adjust &= 0xFF;

#ifdef DEBUG_IOCTL_INFO
      printk(KERN_DEBUG "Wavelan EEprom Area 1 :");
      for(i = 0; i < 16; i++)
  {
    printk(" %04X",
     area[i]);
  }
      printk("\n");

      printk(KERN_DEBUG "Wavelan EEprom DAC : %04X %04X\n",
       dac[0], dac[1]);
#endif

      /* Frequency offset (for info only...) */
      area[0] = ((freq << 5) & 0xFFE0) | (area[0] & 0x1F);

      /* Receiver Principle main divider coefficient */
      area[3] = (freq >> 1) + 2400L - 352L;
      area[2] = ((freq & 0x1) << 4) | (area[2] & 0xFFEF);

      /* Transmitter Main divider coefficient */
      area[13] = (freq >> 1) + 2400L;
      area[12] = ((freq & 0x1) << 4) | (area[2] & 0xFFEF);

      /* Others part of the area are flags, bit streams or unused... */

      /* Set the value in the DAC */
      dac[1] = ((power_adjust >> 1) & 0x7F) | (dac[1] & 0xFF80);
      dac[0] = ((power_adjust & 0x1) << 4) | (dac[0] & 0xFFEF);

      /* Write the first area */
      fee_write(ioaddr, 0x00,
    area, 16);

      /* Write the DAC */
      fee_write(ioaddr, 0x60,
    dac, 2);

      /* We now should verify here that the EEprom writting was ok */

      /* ReRead the first area */
      fee_read(ioaddr, 0x00,
         area_verify, 16);

      /* ReRead the DAC */
      fee_read(ioaddr, 0x60,
         dac_verify, 2);

      /* Compare */
      if(memcmp(area, area_verify, 16 * 2) ||
   memcmp(dac, dac_verify, 2 * 2))
  {
#ifdef DEBUG_IOCTL_ERROR
    printk(KERN_INFO "Wavelan: wv_set_frequency : unable to write new frequency to EEprom (??)\n");
#endif
    return -EOPNOTSUPP;
  }

      /* We must download the frequency parameters to the
       * synthetisers (from the EEprom - area 1)
       * Note : as the EEprom is auto decremented, we set the end
       * if the area... */
      mmc_out(ioaddr, mmwoff(0, mmw_fee_addr), 0x0F);
      mmc_out(ioaddr, mmwoff(0, mmw_fee_ctrl),
        MMW_FEE_CTRL_READ | MMW_FEE_CTRL_DWLD);

      /* Wait until the download is finished */
      fee_wait(ioaddr, 100, 100);

      /* We must now download the power adjust value (gain) to
       * the synthetisers (from the EEprom - area 7 - DAC) */
      mmc_out(ioaddr, mmwoff(0, mmw_fee_addr), 0x61);
      mmc_out(ioaddr, mmwoff(0, mmw_fee_ctrl),
        MMW_FEE_CTRL_READ | MMW_FEE_CTRL_DWLD);

      /* Wait until the download is finished */
      fee_wait(ioaddr, 100, 100);

#ifdef DEBUG_IOCTL_INFO
      /* Verification of what we have done... */

      printk(KERN_DEBUG "Wavelan EEprom Area 1 :");
      for(i = 0; i < 16; i++)
  {
    printk(" %04X",
     area_verify[i]);
  }
      printk("\n");

      printk(KERN_DEBUG "Wavelan EEprom DAC : %04X %04X\n",
       dac_verify[0], dac_verify[1]);
#endif

      return 0;
    }
  else
    return -EINVAL;    /* Bah, never get there... */
}

/*------------------------------------------------------------------*/
/*
 * Give the list of available frequencies
 */
static inline int
wv_frequency_list(u_short  ioaddr,  /* i/o port of the card */
      iw_freq *  list,  /* List of frequency to fill */
      int    max)  /* Maximum number of frequencies */
{
  u_short  table[10];  /* Authorized frequency table */
  long    freq = 0L;  /* offset to 2.4 GHz in .5 MHz + 12 MHz */
  int    i;    /* index in the table */

  /* Read the frequency table */
  fee_read(ioaddr, 0x71 /* frequency table */,
     table, 10);

  /* Look all frequencies */
  i = 0;
  for(freq = 0; freq < 150; freq++)
    /* Look in the table if the frequency is allowed */
    if(table[9 - (freq / 16)] & (1 << (freq % 16)))
      {
  /* put in the list */
  list[i].m = (((freq + 24) * 5) + 24000L) * 10000;
  list[i++].e = 1;

  /* Check number */
  if(i >= max)
    return(i);
      }

  return(i);
}

#ifdef WIRELESS_SPY
/*------------------------------------------------------------------*/
/*
 * Gather wireless spy statistics : for each packet, compare the source
 * address with out list, and if match, get the stats...
 * Sorry, but this function really need wireless extensions...
 */
static inline void
wl_spy_gather(device *  dev,
        u_char *  mac,    /* MAC address */
        u_char *  stats)    /* Statistics to gather */
{
  net_local *  lp = (net_local *) dev->priv;
  int    i;

  /* Look all addresses */
  for(i = 0; i < lp->spy_number; i++)
    /* If match */
    if(!memcmp(mac, lp->spy_address[i], WAVELAN_ADDR_SIZE))
      {
  /* Update statistics */
  lp->spy_stat[i].qual = stats[2] & MMR_SGNL_QUAL;
  lp->spy_stat[i].level = stats[0] & MMR_SIGNAL_LVL;
  lp->spy_stat[i].noise = stats[1] & MMR_SILENCE_LVL;
  lp->spy_stat[i].updated = 0x7;
      }
}
#endif  /* WIRELESS_SPY */

#ifdef HISTOGRAM
/*------------------------------------------------------------------*/
/*
 * This function calculate an histogram on the signal level.
 * As the noise is quite constant, it's like doing it on the SNR.
 * We have defined a set of interval (lp->his_range), and each time
 * the level goes in that interval, we increment the count (lp->his_sum).
 * With this histogram you may detect if one wavelan is really weak,
 * or you may also calculate the mean and standard deviation of the level...
 */
static inline void
wl_his_gather(device *  dev,
        u_char *  stats)    /* Statistics to gather */
{
  net_local *  lp = (net_local *) dev->priv;
  u_char  level = stats[0] & MMR_SIGNAL_LVL;
  int    i;

  /* Find the correct interval */
  i = 0;
  while((i < (lp->his_number - 1)) && (level >= lp->his_range[i++]))
    ;

  /* Increment interval counter */
  (lp->his_sum[i])++;
}
#endif  /* HISTOGRAM */

/*------------------------------------------------------------------*/
/*
 * Perform ioctl : config & info stuff
 * This is here that are treated the wireless extensions (iwconfig)
 */
static int
wavelan_ioctl(struct device *  dev,  /* Device on wich the ioctl apply */
        struct ifreq *  rq,  /* Data passed */
        int    cmd)  /* Ioctl number */
{
  WORD  ioaddr = dev->base_addr;
  net_local *    lp = (net_local *)dev->priv;  /* lp is not unused */
  struct iwreq *  wrq = (struct iwreq *) rq;
  psa_t      psa;
  mm_t      m;
  DWORD    x;
  int      ret = 0;

#ifdef DEBUG_IOCTL_TRACE
  printk(KERN_DEBUG "%s: ->wavelan_ioctl(cmd=0x%X)\n", dev->name, cmd);
#endif

  /* Disable interrupts & save flags */
  x = wv_splhi();

  /* Look what is the request */
  switch(cmd)
    {
      /* --------------- WIRELESS EXTENSIONS --------------- */

    case SIOCGIWNAME:
      strcpy(wrq->u.name, "Wavelan");
      break;

    case SIOCSIWNWID:
      /* Set NWID in wavelan */
      if(wrq->u.nwid.on)
  {
    /* Set NWID in psa */
    psa.psa_nwid[0] = (wrq->u.nwid.nwid & 0xFF00) >> 8;
    psa.psa_nwid[1] = wrq->u.nwid.nwid & 0xFF;
    psa.psa_nwid_select = 0x01;
    psa_write(ioaddr, lp->hacr, (char *)psa.psa_nwid - (char *)&psa,
        (BYTE *)psa.psa_nwid, 3);

    /* Set NWID in mmc */
    m.w.mmw_netw_id_l = wrq->u.nwid.nwid & 0xFF;
    m.w.mmw_netw_id_h = (wrq->u.nwid.nwid & 0xFF00) >> 8;
    mmc_write(ioaddr, (char *)&m.w.mmw_netw_id_l - (char *)&m,
        (BYTE *)&m.w.mmw_netw_id_l, 2);
    m.w.mmw_loopt_sel = 0x00;
    mmc_write(ioaddr, (char *)&m.w.mmw_loopt_sel - (char *)&m,
        (BYTE *)&m.w.mmw_loopt_sel, 1);
  }
      else
  {
    /* Disable nwid in the psa */
    psa.psa_nwid_select = 0x00;
    psa_write(ioaddr, lp->hacr,
        (char *)&psa.psa_nwid_select - (char *)&psa,
        (BYTE *)&psa.psa_nwid_select, 1);

    /* Disable nwid in the mmc (no check) */
    m.w.mmw_loopt_sel = MMW_LOOPT_SEL_DIS_NWID;
    mmc_write(ioaddr, (char *)&m.w.mmw_loopt_sel - (char *)&m,
        (BYTE *)&m.w.mmw_loopt_sel, 1);
  }
      break;

    case SIOCGIWNWID:
      /* Read the NWID */
      psa_read(ioaddr, lp->hacr, (char *)psa.psa_nwid - (char *)&psa,
         (BYTE *)psa.psa_nwid, 3);
      wrq->u.nwid.nwid = (psa.psa_nwid[0] << 8) + psa.psa_nwid[1];
      wrq->u.nwid.on = psa.psa_nwid_select;
      break;

    case SIOCSIWFREQ:
      /* Attempt to recognise 2.00 cards (2.4 GHz frequency selectable) */
      if(!(mmc_in(ioaddr, mmroff(0, mmr_fee_status)) &
     (MMR_FEE_STATUS_DWLD | MMR_FEE_STATUS_BUSY)))
  ret = wv_set_frequency(ioaddr, &(wrq->u.freq));
      else
  ret = -EOPNOTSUPP;
      break;

    case SIOCGIWFREQ:
      /* Attempt to recognise 2.00 cards (2.4 GHz frequency selectable)
       * (does it work for everybody ??? - especially old cards...) */
      if(!(mmc_in(ioaddr, mmroff(0, mmr_fee_status)) &
     (MMR_FEE_STATUS_DWLD | MMR_FEE_STATUS_BUSY)))
  {
    WORD  freq;

    /* Ask the EEprom to read the frequency from the first area */
    fee_read(ioaddr, 0x00 /* 1st area - frequency... */,
       &freq, 1);
    wrq->u.freq.m = ((freq >> 5) * 5 + 24000L) * 10000;
    wrq->u.freq.e = 1;
  }
      else
  {
    int  bands[] = { 915e6, 2.425e8, 2.46e8, 2.484e8, 2.4305e8 };

    psa_read(ioaddr, lp->hacr, (char *)&psa.psa_subband - (char *)&psa,
       (BYTE *)&psa.psa_subband, 1);

    if(psa.psa_subband <= 4)
      {
        wrq->u.freq.m = bands[psa.psa_subband];
        wrq->u.freq.e = (psa.psa_subband != 0);
      }
    else
      ret = -EOPNOTSUPP;
  }
      break;

    case SIOCGIWRANGE:
      /* Basic checking... */
      if(wrq->u.data.pointer != (caddr_t) 0)
  {
    struct iw_range  range;

    /* Verify the user buffer */
    ret = verify_area(VERIFY_WRITE, wrq->u.data.pointer,
          sizeof(struct iw_range));
    if(ret)
      break;

    /* Set the length (useless : its constant...) */
    wrq->u.data.length = sizeof(struct iw_range);

    /* Set information in the range struct */
    range.throughput = 1.6 * 1024 * 1024;  /* don't argue on this ! */
    range.min_nwid = 0x0000;
    range.max_nwid = 0xFFFF;

    /* Attempt to recognise 2.00 cards (2.4 GHz frequency selectable) */
    if(!(mmc_in(ioaddr, mmroff(0, mmr_fee_status)) &
         (MMR_FEE_STATUS_DWLD | MMR_FEE_STATUS_BUSY)))
      {
        range.num_channels = 10;
        range.num_frequency = wv_frequency_list(ioaddr, range.freq,
                  IW_MAX_FREQUENCIES);
      }
    else
      range.num_channels = range.num_frequency = 0;

    range.max_qual.qual = MMR_SGNL_QUAL;
    range.max_qual.level = MMR_SIGNAL_LVL;
    range.max_qual.noise = MMR_SILENCE_LVL;

    /* Copy structure to the user buffer */
    copy_to_user(wrq->u.data.pointer, &range,
           sizeof(struct iw_range));
  }
      break;

    case SIOCGIWPRIV:
      /* Basic checking... */
      if(wrq->u.data.pointer != (caddr_t) 0)
  {
    struct iw_priv_args  priv[] =
    {  /* cmd,    set_args,  get_args,  name */
      { SIOCSIPQTHR, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1, 0, "setqualthr" },
      { SIOCGIPQTHR, 0, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1, "getqualthr" },
      { SIOCSIPLTHR, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1, 0, "setlevelthr" },
      { SIOCGIPLTHR, 0, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1, "getlevelthr" },

      { SIOCSIPHISTO, IW_PRIV_TYPE_BYTE | 16,  0, "sethisto" },
      { SIOCGIPHISTO, 0,      IW_PRIV_TYPE_INT | 16, "gethisto" },
    };

    /* Verify the user buffer */
    ret = verify_area(VERIFY_WRITE, wrq->u.data.pointer,
          sizeof(priv));
    if(ret)
      break;

    /* Set the number of ioctl available */
    wrq->u.data.length = 6;

    /* Copy structure to the user buffer */
    copy_to_user(wrq->u.data.pointer, (u_char *) priv,
           sizeof(priv));
  }
      break;

#ifdef WIRELESS_SPY
    case SIOCSIWSPY:
      /* Set the spy list */

      /* Check the number of addresses */
      if(wrq->u.data.length > IW_MAX_SPY)
  {
    ret = -E2BIG;
    break;
  }
      lp->spy_number = wrq->u.data.length;

      /* If there is some addresses to copy */
      if(lp->spy_number > 0)
  {
    struct sockaddr  address[IW_MAX_SPY];
    int      i;

    /* Verify where the user has set his addresses */
    ret = verify_area(VERIFY_READ, wrq->u.data.pointer,
          sizeof(struct sockaddr) * lp->spy_number);
    if(ret)
      break;
    /* Copy addresses to the driver */
    copy_from_user(address, wrq->u.data.pointer,
       sizeof(struct sockaddr) * lp->spy_number);

    /* Copy addresses to the lp structure */
    for(i = 0; i < lp->spy_number; i++)
      {
        memcpy(lp->spy_address[i], address[i].sa_data,
         WAVELAN_ADDR_SIZE);
      }

    /* Reset structure... */
    memset(lp->spy_stat, 0x00, sizeof(iw_qual) * IW_MAX_SPY);

#ifdef DEBUG_IOCTL_INFO
    printk(KERN_DEBUG "SetSpy - Set of new addresses is :\n");
    for(i = 0; i < wrq->u.data.length; i++)
      printk(KERN_DEBUG "%02X:%02X:%02X:%02X:%02X:%02X \n",
       lp->spy_address[i][0],
       lp->spy_address[i][1],
       lp->spy_address[i][2],
       lp->spy_address[i][3],
       lp->spy_address[i][4],
       lp->spy_address[i][5]);
#endif  /* DEBUG_IOCTL_INFO */
  }

      break;

    case SIOCGIWSPY:
      /* Get the spy list and spy stats */

      /* Set the number of addresses */
      wrq->u.data.length = lp->spy_number;

      /* If the user want to have the addresses back... */
      if((lp->spy_number > 0) && (wrq->u.data.pointer != (caddr_t) 0))
  {
    struct sockaddr  address[IW_MAX_SPY];
    int      i;

    /* Verify the user buffer */
    ret = verify_area(VERIFY_WRITE, wrq->u.data.pointer,
          (sizeof(iw_qual) + sizeof(struct sockaddr))
          * IW_MAX_SPY);
    if(ret)
      break;

    /* Copy addresses from the lp structure */
    for(i = 0; i < lp->spy_number; i++)
      {
        memcpy(address[i].sa_data, lp->spy_address[i],
         WAVELAN_ADDR_SIZE);
        address[i].sa_family = AF_UNIX;
      }

    /* Copy addresses to the user buffer */
    copy_to_user(wrq->u.data.pointer, address,
           sizeof(struct sockaddr) * lp->spy_number);

    /* Copy stats to the user buffer (just after) */
    copy_to_user(wrq->u.data.pointer +
           (sizeof(struct sockaddr) * lp->spy_number),
           lp->spy_stat, sizeof(iw_qual) * lp->spy_number);

    /* Reset updated flags */
    for(i = 0; i < lp->spy_number; i++)
      lp->spy_stat[i].updated = 0x0;
  }  /* if(pointer != NULL) */

      break;
#endif  /* WIRELESS_SPY */

      /* ------------------ PRIVATE IOCTL ------------------ */

    case SIOCSIPQTHR:
      if(!suser())
  return -EPERM;
      psa.psa_quality_thr = *(wrq->u.name) & 0x0F;
      psa_write(ioaddr, lp->hacr, (char *)&psa.psa_quality_thr - (char *)&psa,
         (BYTE *)&psa.psa_quality_thr, 1);
      mmc_out(ioaddr, mmwoff(0, mmw_quality_thr), psa.psa_quality_thr);
      break;

    case SIOCGIPQTHR:
      psa_read(ioaddr, lp->hacr, (char *)&psa.psa_quality_thr - (char *)&psa,
         (BYTE *)&psa.psa_quality_thr, 1);
      *(wrq->u.name) = psa.psa_quality_thr & 0x0F;
      break;

    case SIOCSIPLTHR:
      if(!suser())
  return -EPERM;
      psa.psa_thr_pre_set = *(wrq->u.name) & 0x3F;
      psa_write(ioaddr, lp->hacr, (char *)&psa.psa_thr_pre_set - (char *)&psa,
         (BYTE *)&psa.psa_thr_pre_set, 1);
      mmc_out(ioaddr, mmwoff(0, mmw_thr_pre_set), psa.psa_thr_pre_set);
      break;

    case SIOCGIPLTHR:
      psa_read(ioaddr, lp->hacr, (char *)&psa.psa_thr_pre_set - (char *)&psa,
         (BYTE *)&psa.psa_thr_pre_set, 1);
      *(wrq->u.name) = psa.psa_thr_pre_set & 0x3F;
      break;

#ifdef HISTOGRAM
    case SIOCSIPHISTO:
      /* Verif if the user is root */
      if(!suser())
  return -EPERM;

      /* Check the number of intervals */
      if(wrq->u.data.length > 16)
  {
    ret = -E2BIG;
    break;
  }
      lp->his_number = wrq->u.data.length;

      /* If there is some addresses to copy */
      if(lp->his_number > 0)
  {
    /* Verify where the user has set his addresses */
    ret = verify_area(VERIFY_READ, wrq->u.data.pointer,
          sizeof(char) * lp->his_number);
    if(ret)
      break;
    /* Copy interval ranges to the driver */
    copy_from_user(lp->his_range, wrq->u.data.pointer,
       sizeof(char) * lp->his_number);

    /* Reset structure... */
    memset(lp->his_sum, 0x00, sizeof(long) * 16);
  }
      break;

    case SIOCGIPHISTO:
      /* Set the number of intervals */
      wrq->u.data.length = lp->his_number;

      /* Give back the distribution statistics */
      if((lp->his_number > 0) && (wrq->u.data.pointer != (caddr_t) 0))
  {
    /* Verify the user buffer */
    ret = verify_area(VERIFY_WRITE, wrq->u.data.pointer,
          sizeof(long) * 16);
    if(ret)
      break;

    /* Copy data to the user buffer */
    copy_to_user(wrq->u.data.pointer, lp->his_sum,
           sizeof(long) * lp->his_number);
  }  /* if(pointer != NULL) */
      break;
#endif  /* HISTOGRAM */

      /* ------------------- OTHER IOCTL ------------------- */

    default:
      ret = -EOPNOTSUPP;
    }

  /* ReEnable interrupts & restore flags */
  wv_splx(x);

#ifdef DEBUG_IOCTL_TRACE
  printk(KERN_DEBUG "%s: <-wavelan_ioctl()\n", dev->name);
#endif
  return ret;
}

/*------------------------------------------------------------------*/
/*
 * Get wireless statistics
 * Called by /proc/net/wireless...
 */
static iw_stats *
wavelan_get_wireless_stats(device *  dev)
{
  WORD  ioaddr = dev->base_addr;
  net_local *    lp = (net_local *) dev->priv;
  mmr_t      m;
  iw_stats *    wstats;
  DWORD    x;

#ifdef DEBUG_IOCTL_TRACE
  printk(KERN_DEBUG "%s: ->wavelan_get_wireless_stats()\n", dev->name);
#endif

  /* Disable interrupts & save flags */
  x = wv_splhi();

  if(lp == (net_local *) NULL)
    return (iw_stats *) NULL;
  wstats = &lp->wstats;

  /* Get data from the mmc */
  mmc_out(ioaddr, mmwoff(0, mmw_freeze), 1);

  mmc_read(ioaddr, mmroff(0, mmr_dce_status), &m.mmr_dce_status, 1);
  mmc_read(ioaddr, mmroff(0, mmr_wrong_nwid_l), &m.mmr_wrong_nwid_l, 2);
  mmc_read(ioaddr, mmroff(0, mmr_thr_pre_set), &m.mmr_thr_pre_set, 4);

  mmc_out(ioaddr, mmwoff(0, mmw_freeze), 0);

  /* Copy data to wireless stuff */
  wstats->status = m.mmr_dce_status;
  wstats->qual.qual = m.mmr_sgnl_qual & MMR_SGNL_QUAL;
  wstats->qual.level = m.mmr_signal_lvl & MMR_SIGNAL_LVL;
  wstats->qual.noise = m.mmr_silence_lvl & MMR_SILENCE_LVL;
  wstats->qual.updated = (((m.mmr_signal_lvl & MMR_SIGNAL_LVL_VALID) >> 7) |
        ((m.mmr_signal_lvl & MMR_SIGNAL_LVL_VALID) >> 6) |
        ((m.mmr_silence_lvl & MMR_SILENCE_LVL_VALID) >> 5));
  wstats->discard.nwid += (m.mmr_wrong_nwid_h << 8) | m.mmr_wrong_nwid_l;
  wstats->discard.code = 0L;
  wstats->discard.misc = 0L;

  /* ReEnable interrupts & restore flags */
  wv_splx(x);

#ifdef DEBUG_IOCTL_TRACE
  printk(KERN_DEBUG "%s: <-wavelan_get_wireless_stats()\n", dev->name);
#endif
  return &lp->wstats;
}
#endif  /* WIRELESS_EXT */

/************************* PACKET RECEPTION *************************/
/*
 * This part deal with receiving the packets.
 * The interrupt handler get an interrupt when a packet has been
 * successfully received and called this part...
 */

/*------------------------------------------------------------------*/
/*
 * This routine does the actual copy of data (including the ethernet
 * header structure) from the WaveLAN card to an sk_buff chain that
 * will be passed up to the network interface layer. NOTE: We
 * currently don't handle trailer protocols (neither does the rest of
 * the network interface), so if that is needed, it will (at least in
 * part) be added here.  The contents of the receive ring buffer are
 * copied to a message chain that is then passed to the kernel.
 *
 * Note: if any errors occur, the packet is "dropped on the floor"
 * (called by wv_packet_rcv())
 */
static inline void
wv_packet_read(device *    dev,
         u_short    buf_off,
         int    sksize)
{
  net_local *    lp = (net_local *) dev->priv;
  u_short    ioaddr = dev->base_addr;
  struct sk_buff *  skb;

#ifdef DEBUG_RX_TRACE
  printk(KERN_DEBUG "%s: ->wv_packet_read(0x%X, %d)\n",
   dev->name, fd_p, sksize);
#endif

  /* Allocate buffer for the data */
  if((skb = dev_alloc_skb(sksize)) == (struct sk_buff *) NULL)
    {
#ifdef DEBUG_RX_ERROR
      printk(KERN_INFO "%s: wv_packet_read(): could not alloc_skb(%d, GFP_ATOMIC).\n",
       dev->name, sksize);
#endif
      lp->stats.rx_dropped++;
      return;
    }

  skb->dev = dev;

  /* Copy the packet to the buffer */
  obram_read(ioaddr, buf_off, skb_put(skb, sksize), sksize);
  skb->protocol=eth_type_trans(skb, dev);

#ifdef DEBUG_RX_INFO
  wv_packet_info(skb->mac.raw, sksize, dev->name, "wv_packet_read");
#endif  /* DEBUG_RX_INFO */

  /* Statistics gathering & stuff associated.
   * It seem a bit messy with all the define, but it's really simple... */
#if defined(WIRELESS_SPY) || defined(HISTOGRAM)
  if(
#ifdef WIRELESS_SPY
     (lp->spy_number > 0) ||
#endif  /* WIRELESS_SPY */
#ifdef HISTOGRAM
     (lp->his_number > 0) ||
#endif  /* HISTOGRAM */
     0)
    {
      u_char  stats[3];  /* Signal level, Noise level, Signal quality */

      /* read signal level, silence level and signal quality bytes */
      /* Note : in the Pcmcia hardware, these are part of the frame. It seem
       * that for the ISA hardware, it's nowhere to be found in the frame,
       * so I'm oblige to do this (it has side effect on /proc/net/wireless)
       * Any idea ? */
      mmc_out(ioaddr, mmwoff(0, mmw_freeze), 1);
      mmc_read(ioaddr, mmroff(0, mmr_signal_lvl), stats, 3);
      mmc_out(ioaddr, mmwoff(0, mmw_freeze), 0);

#ifdef DEBUG_RX_INFO
      printk(KERN_DEBUG "%s: wv_packet_read(): Signal level %d/63, Silence level %d/63, signal quality %d/16\n",
       dev->name, stats[0] & 0x3F, stats[1] & 0x3F, stats[2] & 0x0F);
#endif

      /* Spying stuff */
#ifdef WIRELESS_SPY
      wl_spy_gather(dev, skb->mac.raw + WAVELAN_ADDR_SIZE, stats);
#endif  /* WIRELESS_SPY */
#ifdef HISTOGRAM
      wl_his_gather(dev, stats);
#endif  /* HISTOGRAM */
    }
#endif  /* defined(WIRELESS_SPY) || defined(HISTOGRAM) */

  /*
   * Hand the packet to the Network Module
   */
  netif_rx(skb);

  lp->stats.rx_packets++;

#ifdef DEBUG_RX_TRACE
  printk(KERN_DEBUG "%s: <-wv_packet_read()\n", dev->name);
#endif
}

/*------------------------------------------------------------------*/
/*
 * Transfer as many packets as we can
 * from the device RAM.
 * Called by the interrupt handler.
 */
static inline void
wv_receive(device *  dev)
{
  u_short  ioaddr = dev->base_addr;
  net_local *  lp = (net_local *)dev->priv;
  int    nreaped = 0;

#ifdef DEBUG_RX_TRACE
  printk(KERN_DEBUG "%s: ->wv_receive()\n", dev->name);
#endif

  /* Loop on each received packet */
  for(;;)
    {
      fd_t    fd;
      rbd_t    rbd;
      ushort    pkt_len;

      obram_read(ioaddr, lp->rx_head, (BYTE *) &fd, sizeof(fd));

      /* If the current frame is not complete, we have reach the end... */
      if((fd.fd_status & FD_STATUS_C) != FD_STATUS_C)
  break;    /* This is how we exit the loop */

      nreaped++;

      /* Check if frame correctly received */
      if((fd.fd_status & (FD_STATUS_B | FD_STATUS_OK)) !=
   (FD_STATUS_B | FD_STATUS_OK))
  {
    /*
     * Not sure about this one -- it does not seem
     * to be an error so we will keep quiet about it.
     */
#ifndef IGNORE_NORMAL_XMIT_ERRS
#ifdef DEBUG_RX_ERROR
    if((fd.fd_status & FD_STATUS_B) != FD_STATUS_B)
      printk(KERN_INFO "%s: wv_receive(): frame not consumed by RU.\n",
       dev->name);
#endif
#endif  /* IGNORE_NORMAL_XMIT_ERRS */

#ifdef DEBUG_RX_ERROR
    if((fd.fd_status & FD_STATUS_OK) != FD_STATUS_OK)
      printk(KERN_INFO "%s: wv_receive(): frame not received successfully.\n",
       dev->name);
#endif
  }

      /* Check is there was problems in the frame processing */
      if((fd.fd_status & (FD_STATUS_S6 | FD_STATUS_S7 | FD_STATUS_S8 |
        FD_STATUS_S9 | FD_STATUS_S10 | FD_STATUS_S11))
   != 0)
  {
    lp->stats.rx_errors++;

#ifdef DEBUG_RX_ERROR
    if((fd.fd_status & FD_STATUS_S6) != 0)
      printk(KERN_INFO "%s: wv_receive(): no EOF flag.\n", dev->name);
#endif

    if((fd.fd_status & FD_STATUS_S7) != 0)
      {
        lp->stats.rx_length_errors++;
#ifdef DEBUG_RX_ERROR
        printk(KERN_INFO "%s: wv_receive(): frame too short.\n",
         dev->name);
#endif
      }

    if((fd.fd_status & FD_STATUS_S8) != 0)
      {
        lp->stats.rx_over_errors++;
#ifdef DEBUG_RX_ERROR
        printk(KERN_INFO "%s: wv_receive(): rx DMA overrun.\n",
         dev->name);
#endif
      }

    if((fd.fd_status & FD_STATUS_S9) != 0)
      {
        lp->stats.rx_fifo_errors++;
#ifdef DEBUG_RX_ERROR
        printk(KERN_INFO "%s: wv_receive(): ran out of resources.\n",
         dev->name);
#endif
      }

    if((fd.fd_status & FD_STATUS_S10) != 0)
      {
        lp->stats.rx_frame_errors++;
#ifdef DEBUG_RX_ERROR
        printk(KERN_INFO "%s: wv_receive(): alignment error.\n",
         dev->name);
#endif
      }

    if((fd.fd_status & FD_STATUS_S11) != 0)
      {
        lp->stats.rx_crc_errors++;
#ifdef DEBUG_RX_ERROR
        printk(KERN_INFO "%s: wv_receive(): CRC error.\n", dev->name);
#endif
      }
  }

      /* Check if frame contain a pointer to the data */
      if(fd.fd_rbd_offset == I82586NULL)
#ifdef DEBUG_RX_ERROR
  printk(KERN_INFO "%s: wv_receive(): frame has no data.\n", dev->name);
#endif
      else
  {
    obram_read(ioaddr, fd.fd_rbd_offset,
         (BYTE *) &rbd, sizeof(rbd));

#ifdef DEBUG_RX_ERROR
    if((rbd.rbd_status & RBD_STATUS_EOF) != RBD_STATUS_EOF)
      printk(KERN_INFO "%s: wv_receive(): missing EOF flag.\n",
       dev->name);

    if((rbd.rbd_status & RBD_STATUS_F) != RBD_STATUS_F)
      printk(KERN_INFO "%s: wv_receive(): missing F flag.\n",
       dev->name);
#endif

    pkt_len = rbd.rbd_status & RBD_STATUS_ACNT;

    /* Read the packet and transmit to Linux */
    wv_packet_read(dev, rbd.rbd_bufl, pkt_len);
  }  /* if frame has data */

      fd.fd_status = 0;
      obram_write(ioaddr, fdoff(lp->rx_head, fd_status),
      (BYTE *) &fd.fd_status, sizeof(fd.fd_status));

      fd.fd_command = FD_COMMAND_EL;
      obram_write(ioaddr, fdoff(lp->rx_head, fd_command),
      (BYTE *) &fd.fd_command, sizeof(fd.fd_command));

      fd.fd_command = 0;
      obram_write(ioaddr, fdoff(lp->rx_last, fd_command),
      (BYTE *) &fd.fd_command, sizeof(fd.fd_command));

      lp->rx_last = lp->rx_head;
      lp->rx_head = fd.fd_link_offset;
    }  /* for(;;) -> loop on all frames */

#ifdef DEBUG_RX_INFO
  if(nreaped > 1)
    printk(KERN_DEBUG "%s: wv_receive(): reaped %d\n", dev->name, nreaped);
#endif
#ifdef DEBUG_RX_TRACE
  printk(KERN_DEBUG "%s: <-wv_receive()\n", dev->name);
#endif
}

/*********************** PACKET TRANSMISSION ***********************/
/*
 * This part deal with sending packet through the wavelan
 *
 */

/*------------------------------------------------------------------*/
/*
 * This routine fills in the appropriate registers and memory
 * locations on the WaveLAN card and starts the card off on
 * the transmit.
 *
 * The principle :
 * Each block contain a transmit command, a nop command,
 * a transmit block descriptor and a buffer.
 * The CU read the transmit block which point to the tbd,
 * read the tbd and the the content of the buffer.
 * When it has finish with it, it goes to the next command
 * which in our case is the nop. The nop point on itself,
 * so the CU stop here.
 * When we add the next block, we modify the previous nop
 * to make it point on the new tx command.
 * Simple, isn't it ?
 *
 * (called in wavelan_packet_xmit())
 */
static inline void
wv_packet_write(device *  dev,
    void *  buf,
    short  length)
{
  net_local *    lp = (net_local *) dev->priv;
  u_short    ioaddr = dev->base_addr;
  WORD  txblock;
  WORD  txpred;
  WORD  tx_addr;
  WORD  nop_addr;
  WORD  tbd_addr;
  WORD  buf_addr;
  ac_tx_t    tx;
  ac_nop_t    nop;
  tbd_t      tbd;
  int      clen = length;
  DWORD    x;

#ifdef DEBUG_TX_TRACE
  printk(KERN_DEBUG "%s: ->wv_packet_write(%d)\n", dev->name, length);
#endif

  /* Check if we need some padding */
  if(clen < ETH_ZLEN)
    clen = ETH_ZLEN;

  x = wv_splhi();

  /* Calculate addresses of next block and previous block */
  txblock = lp->tx_first_free;
  txpred = txblock - TXBLOCKZ;
  if(txpred < OFFSET_CU)
    txpred += NTXBLOCKS * TXBLOCKZ;
  lp->tx_first_free += TXBLOCKZ;
  if(lp->tx_first_free >= OFFSET_CU + NTXBLOCKS * TXBLOCKZ)
    lp->tx_first_free -= NTXBLOCKS * TXBLOCKZ;

/*
if (lp->tx_n_in_use > 0)
  printk("%c", "0123456789abcdefghijk"[lp->tx_n_in_use]);
*/

  lp->tx_n_in_use++;

  /* Calculate addresses of the differents part of the block */
  tx_addr = txblock;
  nop_addr = tx_addr + sizeof(tx);
  tbd_addr = nop_addr + sizeof(nop);
  buf_addr = tbd_addr + sizeof(tbd);

  /*
   * Transmit command.
   */
  tx.tx_h.ac_status = 0;
  obram_write(ioaddr, toff(ac_tx_t, tx_addr, tx_h.ac_status),
        (BYTE *) &tx.tx_h.ac_status,
        sizeof(tx.tx_h.ac_status));

  /*
   * NOP command.
   */
  nop.nop_h.ac_status = 0;
  obram_write(ioaddr, toff(ac_nop_t, nop_addr, nop_h.ac_status),
        (BYTE *) &nop.nop_h.ac_status,
        sizeof(nop.nop_h.ac_status));
  nop.nop_h.ac_link = nop_addr;
  obram_write(ioaddr, toff(ac_nop_t, nop_addr, nop_h.ac_link),
        (BYTE *) &nop.nop_h.ac_link,
        sizeof(nop.nop_h.ac_link));

  /*
   * Transmit buffer descriptor. 
   */
  tbd.tbd_status = TBD_STATUS_EOF | (TBD_STATUS_ACNT & clen);
  tbd.tbd_next_bd_offset = I82586NULL;
  tbd.tbd_bufl = buf_addr;
  tbd.tbd_bufh = 0;
  obram_write(ioaddr, tbd_addr, (BYTE *)&tbd, sizeof(tbd));

  /*
   * Data.
   */
  obram_write(ioaddr, buf_addr, buf, clen);

  /*
   * Overwrite the predecessor NOP link
   * so that it points to this txblock.
   */
  nop_addr = txpred + sizeof(tx);
  nop.nop_h.ac_status = 0;
  obram_write(ioaddr, toff(ac_nop_t, nop_addr, nop_h.ac_status),
        (BYTE *)&nop.nop_h.ac_status,
        sizeof(nop.nop_h.ac_status));
  nop.nop_h.ac_link = txblock;
  obram_write(ioaddr, toff(ac_nop_t, nop_addr, nop_h.ac_link),
        (BYTE *) &nop.nop_h.ac_link,
        sizeof(nop.nop_h.ac_link));

  /* If watchdog not already active, activate it... */
  if(lp->watchdog.prev == (timer_list *) NULL)
    {
      /* set timer to expire in WATCHDOG_JIFFIES */
      lp->watchdog.expires = jiffies + WATCHDOG_JIFFIES;
      add_timer(&lp->watchdog);
    }

  if(lp->tx_first_in_use == I82586NULL)
    lp->tx_first_in_use = txblock;

  if(lp->tx_n_in_use < NTXBLOCKS - 1)
    dev->tbusy = 0;

  wv_splx(x);

#ifdef DEBUG_TX_INFO
  wv_packet_info((u_char *) buf, length, dev->name, "wv_packet_write");
#endif  /* DEBUG_TX_INFO */

#ifdef DEBUG_TX_TRACE
  printk(KERN_DEBUG "%s: <-wv_packet_write()\n", dev->name);
#endif
}

/*------------------------------------------------------------------*/
/*
 * This routine is called when we want to send a packet (NET3 callback)
 * In this routine, we check if the the harware is ready to accept
 * the packet. We also prevent reentrance. Then, we call the function
 * to send the packet...
 */
static int
wavelan_packet_xmit(struct sk_buff *  skb,
        device *    dev)
{
  net_local *  lp = (net_local *)dev->priv;

#ifdef DEBUG_TX_TRACE
  printk(KERN_DEBUG "%s: ->wavelan_packet_xmit(0x%X)\n", dev->name,
   (unsigned) skb);
#endif

  /* This flag indicate that the hardware can't perform a transmission.
   * Theoritically, NET3 check it before sending a packet to the driver,
   * but in fact it never do that and pool continuously.
   * As the watchdog will abort too long transmissions, we are quite safe...
   */
  if(dev->tbusy)
    return 1;

  /*
   * If some higher layer thinks we've missed
   * a tx-done interrupt we are passed NULL.
   * Caution: dev_tint() handles the cli()/sti() itself.
   */
  if(skb == (struct sk_buff *)0)
    {
#ifdef DEBUG_TX_ERROR
      printk(KERN_INFO "%s: wavelan_packet_xmit(): skb == NULL\n", dev->name);
#endif
      dev_tint(dev);
      return 0;
    }

  /*
   * Block a timer-based transmit from overlapping.
   * In other words, prevent reentering this routine.
   */
  if(set_bit(0, (void *)&dev->tbusy) != 0)
#ifdef DEBUG_TX_ERROR
    printk(KERN_INFO "%s: Transmitter access conflict.\n", dev->name);
#endif
  else
    {
      /* If somebody has asked to reconfigure the controler, we can do it now */
      if(lp->reconfig_82586)
  {
    wv_82586_config(dev);
    if(dev->tbusy)
      return 1;
  }

#ifdef DEBUG_TX_ERROR
      if(skb->next)
  printk(KERN_INFO "skb has next\n");
#endif

      wv_packet_write(dev, skb->data, skb->len);
    }

  dev_kfree_skb(skb, FREE_WRITE);

#ifdef DEBUG_TX_TRACE
  printk(KERN_DEBUG "%s: <-wavelan_packet_xmit()\n", dev->name);
#endif
  return 0;
}

/********************** HARDWARE CONFIGURATION **********************/
/*
 * This part do the real job of starting and configuring the hardware.
 */

/*------------------------------------------------------------------*/
/*
 * Routine to initialize the Modem Management Controller.
 * (called by wv_hw_reset())
 */
static inline int
wv_mmc_init(device *  dev)
{
  u_short  ioaddr = dev->base_addr;
  net_local *  lp = (net_local *)dev->priv;
  psa_t    psa;
  mmw_t    m;
  int    configured;

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: ->wv_mmc_init()\n", dev->name);
#endif

  /* Read the parameter storage area */
  psa_read(ioaddr, lp->hacr, 0, (BYTE *) &psa, sizeof(psa));

#ifdef USE_PSA_CONFIG
  configured = psa.psa_conf_status & 1;
#else
  configured = 0;
#endif

  /* Is the PSA is not configured */
  if(!configured)
    {
      /* User will be able to configure NWID after (with iwconfig) */
      psa.psa_nwid[0] = 0;
      psa.psa_nwid[1] = 0;

      /* As NWID is not set : no NWID checking */
      psa.psa_nwid_select = 0;

      /* Set to standard values
       * 0x04 for AT,
       * 0x01 for MCA,
       * 0x04 for PCMCIA and 2.00 card (AT&T 407-024689/E document)
       */
      if (psa.psa_comp_number & 1)
  psa.psa_thr_pre_set = 0x01;
      else
  psa.psa_thr_pre_set = 0x04;
      psa.psa_quality_thr = 0x03;

      /* It is configured */
      psa.psa_conf_status |= 1;

#ifdef USE_PSA_CONFIG
      /* Write the psa */
      psa_write(ioaddr, lp->hacr, (char *)psa.psa_nwid - (char *)&psa,
    (BYTE *)psa.psa_nwid, 3);
      psa_write(ioaddr, lp->hacr, (char *)&psa.psa_thr_pre_set - (char *)&psa,
    (BYTE *)&psa.psa_thr_pre_set, 1);
      psa_write(ioaddr, lp->hacr, (char *)&psa.psa_quality_thr - (char *)&psa,
    (BYTE *)&psa.psa_quality_thr, 1);
      psa_write(ioaddr, lp->hacr, (char *)&psa.psa_conf_status - (char *)&psa,
    (BYTE *)&psa.psa_conf_status, 1);
#endif
    }

  /* Zero the mmc structure */
  memset(&m, 0x00, sizeof(m));

  /* Copy PSA info to the mmc */
  m.mmw_netw_id_l = psa.psa_nwid[1];
  m.mmw_netw_id_h = psa.psa_nwid[0];
  
  if(psa.psa_nwid_select & 1)
    m.mmw_loopt_sel = 0x00;
  else
    m.mmw_loopt_sel = MMW_LOOPT_SEL_DIS_NWID;

  m.mmw_thr_pre_set = psa.psa_thr_pre_set & 0x3F;
  m.mmw_quality_thr = psa.psa_quality_thr & 0x0F;

  /* Missing : encryption stuff... */

  /*
   * Set default modem control parameters.
   * See NCR document 407-0024326 Rev. A.
   */
  m.mmw_jabber_enable = 0x01;
  m.mmw_anten_sel = MMW_ANTEN_SEL_ALG_EN;
  m.mmw_ifs = 0x20;
  m.mmw_mod_delay = 0x04;
  m.mmw_jam_time = 0x38;

  m.mmw_encr_enable = 0;
  m.mmw_des_io_invert = 0;
  m.mmw_freeze = 0;
  m.mmw_decay_prm = 0;
  m.mmw_decay_updat_prm = 0;

  /* Write all info to mmc */
  mmc_write(ioaddr, 0, (u_char *)&m, sizeof(m));

  /* The following code start the modem of the 2.00 frequency
   * selectable cards at power on. It's not strictly needed for the
   * following boots...
   * The original patch was by Joe Finney for the PCMCIA driver, but
   * I've cleaned it a bit and add documentation.
   * Thanks to Loeke Brederveld from Lucent for the info.
   */

  /* Attempt to recognise 2.00 cards (2.4 GHz frequency selectable)
   * (does it work for everybody ??? - especially old cards...) */
  /* Note : WFREQSEL verify that it is able to read from EEprom
   * a sensible frequency (address 0x00) + that MMR_FEE_STATUS_ID
   * is 0xA (Xilinx version) or 0xB (Ariadne version).
   * My test is more crude but do work... */
  if(!(mmc_in(ioaddr, mmroff(0, mmr_fee_status)) &
       (MMR_FEE_STATUS_DWLD | MMR_FEE_STATUS_BUSY)))
    {
      /* We must download the frequency parameters to the
       * synthetisers (from the EEprom - area 1)
       * Note : as the EEprom is auto decremented, we set the end
       * if the area... */
      m.mmw_fee_addr = 0x0F;
      m.mmw_fee_ctrl = MMW_FEE_CTRL_READ | MMW_FEE_CTRL_DWLD;
      mmc_write(ioaddr, (char *)&m.mmw_fee_ctrl - (char *)&m,
    (BYTE *)&m.mmw_fee_ctrl, 2);

      /* Wait until the download is finished */
      fee_wait(ioaddr, 100, 100);

#ifdef DEBUG_CONFIG_INFO
      /* The frequency was in the last word downloaded... */
      mmc_read(ioaddr, (char *)&m.mmw_fee_data_l - (char *)&m,
         (BYTE *)&m.mmw_fee_data_l, 2);

      /* Print some info for the user */
      printk(KERN_DEBUG "%s: Wavelan 2.00 recognised (frequency select) : Current frequency = %ld\n",
       dev->name,
       ((m.mmw_fee_data_h << 4) |
        (m.mmw_fee_data_l >> 4)) * 5 / 2 + 24000L);
#endif

      /* We must now download the power adjust value (gain) to
       * the synthetisers (from the EEprom - area 7 - DAC) */
      m.mmw_fee_addr = 0x61;
      m.mmw_fee_ctrl = MMW_FEE_CTRL_READ | MMW_FEE_CTRL_DWLD;
      mmc_write(ioaddr, (char *)&m.mmw_fee_ctrl - (char *)&m,
    (BYTE *)&m.mmw_fee_ctrl, 2);

      /* Wait until the download is finished */
    }  /* if 2.00 card */

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: <-wv_mmc_init()\n", dev->name);
#endif
  return 0;
}

/*------------------------------------------------------------------*/
/*
 * Construct the fd and rbd structures.
 * Start the receive unit.
 * (called by wv_hw_reset())
 */
static inline int
wv_ru_start(device *  dev)
{
  net_local *  lp = (net_local *) dev->priv;
  u_short  ioaddr = dev->base_addr;
  u_short  scb_cs;
  fd_t    fd;
  rbd_t    rbd;
  u_short  rx;
  u_short  rx_next;
  int    i;

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: ->wv_ru_start()\n", dev->name);
#endif

  obram_read(ioaddr, scboff(OFFSET_SCB, scb_status), (BYTE *)&scb_cs, sizeof(scb_cs));
  if((scb_cs & SCB_ST_RUS) == SCB_ST_RUS_RDY)
    return 0;

  lp->rx_head = OFFSET_RU;

  for(i = 0, rx = lp->rx_head; i < NRXBLOCKS; i++, rx = rx_next)
    {
      rx_next = (i == NRXBLOCKS - 1) ? lp->rx_head : rx + RXBLOCKZ;

      fd.fd_status = 0;
      fd.fd_command = (i == NRXBLOCKS - 1) ? FD_COMMAND_EL : 0;
      fd.fd_link_offset = rx_next;
      fd.fd_rbd_offset = rx + sizeof(fd);
      obram_write(ioaddr, rx, (BYTE *)&fd, sizeof(fd));

      rbd.rbd_status = 0;
      rbd.rbd_next_rbd_offset = I82586NULL;
      rbd.rbd_bufl = rx + sizeof(fd) + sizeof(rbd);
      rbd.rbd_bufh = 0;
      rbd.rbd_el_size = RBD_EL | (RBD_SIZE & MAXDATAZ);
      obram_write(ioaddr, rx + sizeof(fd),
      (BYTE *) &rbd, sizeof(rbd));

      lp->rx_last = rx;
    }

  obram_write(ioaddr, scboff(OFFSET_SCB, scb_rfa_offset),
        (BYTE *) &lp->rx_head, sizeof(lp->rx_head));

  scb_cs = SCB_CMD_RUC_GO;
  obram_write(ioaddr, scboff(OFFSET_SCB, scb_command),
        (BYTE *) &scb_cs, sizeof(scb_cs));

  set_chan_attn(ioaddr, lp->hacr);

  for(i = 1000; i > 0; i--)
    {
      obram_read(ioaddr, scboff(OFFSET_SCB, scb_command),
     (BYTE *) &scb_cs, sizeof(scb_cs));
      if (scb_cs == 0)
  break;

      udelay(10);
    }

  if(i <= 0)
    {
#ifdef DEBUG_CONFIG_ERRORS
      printk(KERN_INFO "%s: wavelan_ru_start(): board not accepting command.\n",
       dev->name);
#endif
      return -1;
    }

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: <-wv_ru_start()\n", dev->name);
#endif
  return 0;
}

/*------------------------------------------------------------------*/
/*
 * Initialise the transmit blocks.
 * Start the command unit executing the NOP
 * self-loop of the first transmit block.
 *
 * Here, we create the list of send buffer used to transmit packets
 * between the PC and the command unit. For each buffer, we create a
 * buffer descriptor (pointing on the buffer), a transmit command
 * (pointing to the buffer descriptor) and a nop command.
 * The transmit command is linked to the nop, and the nop to itself.
 * When we will have finish to execute the transmit command, we will
 * then loop on the nop. By releasing the nop link to a new command,
 * we may send another buffer.
 *
 * (called by wv_hw_reset())
 */
static inline int
wv_cu_start(device *  dev)
{
  net_local *  lp = (net_local *) dev->priv;
  u_short  ioaddr = dev->base_addr;
  int    i;
  u_short  txblock;
  u_short  first_nop;
  u_short  scb_cs;

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: ->wv_cu_start()\n", dev->name);
#endif

  lp->tx_first_free = OFFSET_CU;
  lp->tx_first_in_use = I82586NULL;

  for(i = 0, txblock = OFFSET_CU;
      i < NTXBLOCKS;
      i++, txblock += TXBLOCKZ)
    {
      ac_tx_t    tx;
      ac_nop_t    nop;
      tbd_t    tbd;
      WORD  tx_addr;
      WORD  nop_addr;
      WORD  tbd_addr;
      WORD  buf_addr;

      tx_addr = txblock;
      nop_addr = tx_addr + sizeof(tx);
      tbd_addr = nop_addr + sizeof(nop);
      buf_addr = tbd_addr + sizeof(tbd);

      tx.tx_h.ac_status = 0;
      tx.tx_h.ac_command = acmd_transmit | AC_CFLD_I;
      tx.tx_h.ac_link = nop_addr;
      tx.tx_tbd_offset = tbd_addr;
      obram_write(ioaddr, tx_addr, (BYTE *) &tx, sizeof(tx));

      nop.nop_h.ac_status = 0;
      nop.nop_h.ac_command = acmd_nop;
      nop.nop_h.ac_link = nop_addr;
      obram_write(ioaddr, nop_addr, (BYTE *) &nop, sizeof(nop));

      tbd.tbd_status = TBD_STATUS_EOF;
      tbd.tbd_next_bd_offset = I82586NULL;
      tbd.tbd_bufl = buf_addr;
      tbd.tbd_bufh = 0;
      obram_write(ioaddr, tbd_addr, (BYTE *) &tbd, sizeof(tbd));
    }

  first_nop = OFFSET_CU + (NTXBLOCKS - 1) * TXBLOCKZ + sizeof(ac_tx_t);
  obram_write(ioaddr, scboff(OFFSET_SCB, scb_cbl_offset),
        (BYTE *) &first_nop, sizeof(first_nop));

  scb_cs = SCB_CMD_CUC_GO;
  obram_write(ioaddr, scboff(OFFSET_SCB, scb_command),
        (BYTE *) &scb_cs, sizeof(scb_cs));

  set_chan_attn(ioaddr, lp->hacr);

  for(i = 1000; i > 0; i--)
    {
      obram_read(ioaddr, scboff(OFFSET_SCB, scb_command),
     (BYTE *) &scb_cs, sizeof(scb_cs));
      if (scb_cs == 0)
  break;

      udelay(10);
    }

  if(i <= 0)
    {
#ifdef DEBUG_CONFIG_ERRORS
      printk(KERN_INFO "%s: wavelan_cu_start(): board not accepting command.\n",
       dev->name);
#endif
      return -1;
    }

  lp->tx_n_in_use = 0;
  dev->tbusy = 0;

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: <-wv_cu_start()\n", dev->name);
#endif
  return 0;
}

/*------------------------------------------------------------------*/
/*
 * This routine does a standard config of the WaveLAN controler (i82586).
 *
 * It initialise the scp, iscp and scb structure
 * The two first are only pointer to the next.
 * The last one is used for basic configuration and for basic
 * communication (interrupt status)
 *
 * (called by wv_hw_reset())
 */
static inline int
wv_82586_start(device *  dev)
{
  net_local *  lp = (net_local *) dev->priv;
  u_short  ioaddr = dev->base_addr;
  scp_t    scp;    /* system configuration pointer */
  iscp_t  iscp;    /* intermediate scp */
  scb_t    scb;    /* system control block */
  ach_t    cb;    /* Action command header */
  u_char  zeroes[512];
  int    i;

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: ->wv_82586_start()\n", dev->name);
#endif

  /*
   * Clear the onboard RAM.
   */
  memset(&zeroes[0], 0x00, sizeof(zeroes));
  for(i = 0; i < I82586_MEMZ; i += sizeof(zeroes))
    obram_write(ioaddr, i, &zeroes[0], sizeof(zeroes));

  /*
   * Construct the command unit structures:
   * scp, iscp, scb, cb.
   */
  memset(&scp, 0x00, sizeof(scp));
  scp.scp_sysbus = SCP_SY_16BBUS;
  scp.scp_iscpl = OFFSET_ISCP;
  obram_write(ioaddr, OFFSET_SCP, (BYTE *)&scp, sizeof(scp));

  memset(&iscp, 0x00, sizeof(iscp));
  iscp.iscp_busy = 1;
  iscp.iscp_offset = OFFSET_SCB;
  obram_write(ioaddr, OFFSET_ISCP, (BYTE *)&iscp, sizeof(iscp));

  /* Our first command is to reset the i82586 */
  memset(&scb, 0x00, sizeof(scb));
  scb.scb_command = SCB_CMD_RESET;
  scb.scb_cbl_offset = OFFSET_CU;
  scb.scb_rfa_offset = OFFSET_RU;
  obram_write(ioaddr, OFFSET_SCB, (BYTE *)&scb, sizeof(scb));

  set_chan_attn(ioaddr, lp->hacr);

  /* Wait for command to finish */
  for(i = 1000; i > 0; i--)
    {
      obram_read(ioaddr, OFFSET_ISCP, (BYTE *) &iscp, sizeof(iscp));

      if(iscp.iscp_busy == (WORD) 0)
  break;

      udelay(10);
    }

  if(i <= 0)
    {
#ifdef DEBUG_CONFIG_ERRORS
      printk(KERN_INFO "%s: wv_82586_start(): iscp_busy timeout.\n",
       dev->name);
#endif
      return -1;
    }

  /* Check command completion */
  for(i = 15; i > 0; i--)
    {
      obram_read(ioaddr, OFFSET_SCB, (BYTE *) &scb, sizeof(scb));

      if (scb.scb_status == (SCB_ST_CX | SCB_ST_CNA))
  break;

      udelay(10);
    }

  if (i <= 0)
    {
#ifdef DEBUG_CONFIG_ERRORS
      printk(KERN_INFO "%s: wv_82586_start(): status: expected 0x%02x, got 0x%02x.\n",
       dev->name, SCB_ST_CX | SCB_ST_CNA, scb.scb_status);
#endif
      return -1;
    }

  wv_ack(dev);

  /* Set the action command header */
  memset(&cb, 0x00, sizeof(cb));
  cb.ac_command = AC_CFLD_EL | (AC_CFLD_CMD & acmd_diagnose);
  cb.ac_link = OFFSET_CU;
  obram_write(ioaddr, OFFSET_CU, (BYTE *)&cb, sizeof(cb));

  if(wv_synchronous_cmd(dev, "diag()") == -1)
    return -1;

  obram_read(ioaddr, OFFSET_CU, (BYTE *)&cb, sizeof(cb));
  if(cb.ac_status & AC_SFLD_FAIL)
    {
#ifdef DEBUG_CONFIG_ERRORS
      printk(KERN_INFO "%s: wv_82586_start(): i82586 Self Test failed.\n",
       dev->name);
#endif
      return -1;
    }

#ifdef DEBUG_I82586_SHOW
  wv_scb_show(ioaddr);
#endif

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: <-wv_82586_start()\n", dev->name);
#endif
  return 0;
}

/*------------------------------------------------------------------*/
/*
 * This routine does a standard config of the WaveLAN controler (i82586).
 *
 * This routine is a violent hack. We use the first free transmit block
 * to make our configuration. In the buffer area, we create the three
 * configure command (linked). We make the previous nop point to the
 * beggining of the buffer instead of the tx command. After, we go as
 * usual to the nop command...
 * Note that only the last command (mc_set) will generate an interrupt...
 *
 * (called by wv_hw_reset(), wv_82586_reconfig())
 */
static void
wv_82586_config(device *  dev)
{
  net_local *    lp = (net_local *) dev->priv;
  u_short    ioaddr = dev->base_addr;
  WORD  txblock;
  WORD  txpred;
  WORD  tx_addr;
  WORD  nop_addr;
  WORD  tbd_addr;
  WORD  cfg_addr;
  WORD  ias_addr;
  WORD  mcs_addr;
  ac_tx_t    tx;
  ac_nop_t    nop;
  ac_cfg_t    cfg;    /* Configure action */
  ac_ias_t    ias;    /* IA-setup action */
  ac_mcs_t    mcs;    /* Multicast setup */
  struct dev_mc_list *  dmi;
  DWORD    x;

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: ->wv_82586_config()\n", dev->name);
#endif

  x = wv_splhi();

  /* Calculate addresses of next block and previous block */
  txblock = lp->tx_first_free;
  txpred = txblock - TXBLOCKZ;
  if(txpred < OFFSET_CU)
    txpred += NTXBLOCKS * TXBLOCKZ;
  lp->tx_first_free += TXBLOCKZ;
  if(lp->tx_first_free >= OFFSET_CU + NTXBLOCKS * TXBLOCKZ)
    lp->tx_first_free -= NTXBLOCKS * TXBLOCKZ;

  lp->tx_n_in_use++;

  /* Calculate addresses of the differents part of the block */
  tx_addr = txblock;
  nop_addr = tx_addr + sizeof(tx);
  tbd_addr = nop_addr + sizeof(nop);
  cfg_addr = tbd_addr + sizeof(tbd_t);  /* beggining of the buffer */
  ias_addr = cfg_addr + sizeof(cfg);
  mcs_addr = ias_addr + sizeof(ias);

  /*
   * Transmit command.
   */
  tx.tx_h.ac_status = 0xFFFF;  /* Fake completion value */
  obram_write(ioaddr, toff(ac_tx_t, tx_addr, tx_h.ac_status),
        (BYTE *) &tx.tx_h.ac_status,
        sizeof(tx.tx_h.ac_status));

  /*
   * NOP command.
   */
  nop.nop_h.ac_status = 0;
  obram_write(ioaddr, toff(ac_nop_t, nop_addr, nop_h.ac_status),
        (BYTE *) &nop.nop_h.ac_status,
        sizeof(nop.nop_h.ac_status));
  nop.nop_h.ac_link = nop_addr;
  obram_write(ioaddr, toff(ac_nop_t, nop_addr, nop_h.ac_link),
        (BYTE *) &nop.nop_h.ac_link,
        sizeof(nop.nop_h.ac_link));

  /* Create a configure action */
  memset(&cfg, 0x00, sizeof(cfg));

#if  0
  /*
   * The default board configuration.
   */
  cfg.fifolim_bytecnt   = 0x080c;
  cfg.addrlen_mode    = 0x2600;
  cfg.linprio_interframe  = 0x7820;  /* IFS=120, ACS=2 */
  cfg.slot_time        = 0xf00c;  /* slottime=12    */
  cfg.hardware         = 0x0008;  /* tx even w/o CD */
  cfg.min_frame_len     = 0x0040;
#endif  /* 0 */

  /*
   * For Linux we invert AC_CFG_ALOC(..) so as to conform
   * to the way that net packets reach us from above.
   * (See also ac_tx_t.)
   */
  cfg.cfg_byte_cnt = AC_CFG_BYTE_CNT(sizeof(ac_cfg_t) - sizeof(ach_t));
  cfg.cfg_fifolim = AC_CFG_FIFOLIM(8);
  cfg.cfg_byte8 = AC_CFG_SAV_BF(0) |
      AC_CFG_SRDY(0);
  cfg.cfg_byte9 = AC_CFG_ELPBCK(0) |
      AC_CFG_ILPBCK(0) |
      AC_CFG_PRELEN(AC_CFG_PLEN_2) |
      AC_CFG_ALOC(1) |
      AC_CFG_ADDRLEN(WAVELAN_ADDR_SIZE);
  cfg.cfg_byte10 = AC_CFG_BOFMET(0) |
       AC_CFG_ACR(0) |
       AC_CFG_LINPRIO(0);
  cfg.cfg_ifs = 32;
  cfg.cfg_slotl = 0;
  cfg.cfg_byte13 = AC_CFG_RETRYNUM(15) |
       AC_CFG_SLTTMHI(2);
  cfg.cfg_byte14 = AC_CFG_FLGPAD(0) |
       AC_CFG_BTSTF(0) |
       AC_CFG_CRC16(0) |
       AC_CFG_NCRC(0) |
       AC_CFG_TNCRS(1) |
       AC_CFG_MANCH(0) |
       AC_CFG_BCDIS(0) |
       AC_CFG_PRM(lp->promiscuous);
  cfg.cfg_byte15 = AC_CFG_ICDS(0) |
       AC_CFG_CDTF(0) |
       AC_CFG_ICSS(0) |
       AC_CFG_CSTF(0);
/*
  cfg.cfg_min_frm_len = AC_CFG_MNFRM(64);
*/
  cfg.cfg_min_frm_len = AC_CFG_MNFRM(8);

  cfg.cfg_h.ac_command = (AC_CFLD_CMD & acmd_configure);
  cfg.cfg_h.ac_link = ias_addr;
  obram_write(ioaddr, cfg_addr, (BYTE *)&cfg, sizeof(cfg));

  /* Setup the MAC address */
  memset(&ias, 0x00, sizeof(ias));
  ias.ias_h.ac_command = (AC_CFLD_CMD & acmd_ia_setup);
  ias.ias_h.ac_link = mcs_addr;
  memcpy(&ias.ias_addr[0], (BYTE *)&dev->dev_addr[0], sizeof(ias.ias_addr));
  obram_write(ioaddr, ias_addr, (BYTE *)&ias, sizeof(ias));

  /* Initialize adapter's ethernet multicast addresses */
  memset(&mcs, 0x00, sizeof(mcs));
  mcs.mcs_h.ac_command = AC_CFLD_I | (AC_CFLD_CMD & acmd_mc_setup);
  mcs.mcs_h.ac_link = nop_addr;
  mcs.mcs_cnt = WAVELAN_ADDR_SIZE * lp->mc_count;
  obram_write(ioaddr, mcs_addr, (BYTE *)&mcs, sizeof(mcs));

  /* If any address to set */
  if(lp->mc_count)
    {
      for(dmi=dev->mc_list; dmi; dmi=dmi->next)
  outsw(PIOP1(ioaddr), (u_short *) dmi->dmi_addr,
        WAVELAN_ADDR_SIZE >> 1);

#ifdef DEBUG_CONFIG_INFO
      printk(KERN_DEBUG "%s: wv_82586_config(): set %d multicast addresses:\n",
       dev->name, lp->mc_count);
      for(dmi=dev->mc_list; dmi; dmi=dmi->next)
  printk(KERN_DEBUG " %02x:%02x:%02x:%02x:%02x:%02x\n",
         dmi->dmi_addr[0], dmi->dmi_addr[1], dmi->dmi_addr[2],
         dmi->dmi_addr[3], dmi->dmi_addr[4], dmi->dmi_addr[5] );
#endif
    }

  /*
   * Overwrite the predecessor NOP link
   * so that it points to the configure action.
   */
  nop_addr = txpred + sizeof(tx);
  nop.nop_h.ac_status = 0;
  obram_write(ioaddr, toff(ac_nop_t, nop_addr, nop_h.ac_status),
        (BYTE *)&nop.nop_h.ac_status,
        sizeof(nop.nop_h.ac_status));
  nop.nop_h.ac_link = cfg_addr;
  obram_write(ioaddr, toff(ac_nop_t, nop_addr, nop_h.ac_link),
        (BYTE *) &nop.nop_h.ac_link,
        sizeof(nop.nop_h.ac_link));

  /* If watchdog not already active, activate it... */
  if(lp->watchdog.prev == (timer_list *) NULL)
    {
      /* set timer to expire in WATCHDOG_JIFFIES */
      lp->watchdog.expires = jiffies + WATCHDOG_JIFFIES;
      add_timer(&lp->watchdog);
    }

  lp->reconfig_82586 = 0;

  if(lp->tx_first_in_use == I82586NULL)
    lp->tx_first_in_use = txblock;

  if(lp->tx_n_in_use < NTXBLOCKS - 1)
    dev->tbusy = 0;

  wv_splx(x);

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: <-wv_82586_config()\n", dev->name);
#endif
}

/*------------------------------------------------------------------*/
/*
 * This routine stop gracefully the WaveLAN controler (i82586).
 * (called by wavelan_close())
 */
static inline void
wv_82586_stop(device *  dev)
{
  net_local *  lp = (net_local *) dev->priv;
  u_short  ioaddr = dev->base_addr;
  u_short  scb_cmd;

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: ->wv_82586_stop()\n", dev->name);
#endif

  /* Suspend both command unit and receive unit */
  scb_cmd = (SCB_CMD_CUC & SCB_CMD_CUC_SUS) | (SCB_CMD_RUC & SCB_CMD_RUC_SUS);
  obram_write(ioaddr, scboff(OFFSET_SCB, scb_command),
        (BYTE *)&scb_cmd, sizeof(scb_cmd));
  set_chan_attn(ioaddr, lp->hacr);

  /* No more interrupts */
  wv_ints_off(dev);

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: <-wv_82586_stop()\n", dev->name);
#endif
}

/*------------------------------------------------------------------*/
/*
 * Totally reset the wavelan and restart it.
 * Performs the following actions:
 *  1. A power reset (reset DMA)
 *  2. Initialize the radio modem (using wv_mmc_init)
 *  3. Reset & Configure LAN controller (using wv_82586_start)
 *  4. Start the LAN controller's command unit
 *  5. Start the LAN controller's receive unit
 */
static int
wv_hw_reset(device *  dev)
{
  net_local *  lp = (net_local *)dev->priv;
  u_short  ioaddr = dev->base_addr;

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: ->wv_hw_reset(dev=0x%x)\n", dev->name,
   (DWORD)dev);
#endif

  /* If watchdog was activated, kill it ! */
  if(lp->watchdog.prev != (timer_list *) NULL)
    del_timer(&lp->watchdog);

  /* Increase the number of resets done */
  lp->nresets++;

  wv_hacr_reset(ioaddr);
  lp->hacr = HACR_DEFAULT;

  if((wv_mmc_init(dev) < 0) ||
     (wv_82586_start(dev) < 0))
    return -1;

  /* Enable the card to send interrupts */
  wv_ints_on(dev);

  /* Start card functions */
  if((wv_ru_start(dev) < 0) ||
     (wv_cu_start(dev) < 0))
    return -1;

  /* Finish configuration */
  wv_82586_config(dev);

#ifdef DEBUG_CONFIG_TRACE
  printk(KERN_DEBUG "%s: <-wv_hw_reset()\n", dev->name);
#endif
  return 0;
}

/*------------------------------------------------------------------*/
/*
 * Check if there is a wavelan at the specific base address.
 * As a side effect, it read the MAC address.
 * (called in wavelan_probe() and init_module())
 */
static int
wv_check_ioaddr(u_short    ioaddr,
    u_char *  mac)
{
  int    i;    /* Loop counter */

  /* Check if the base address if available */
  if(check_region(ioaddr, sizeof(ha_t)))
    return  EADDRINUSE;    /* ioaddr already used... */

  /* Reset host interface */
  wv_hacr_reset(ioaddr);

  /* Read the MAC address from the parameter storage area */
  psa_read(ioaddr, HACR_DEFAULT, psaoff(0, psa_univ_mac_addr),
     mac, 6);

  /*
   * Check the first three octets of the addr for the manufacturer's code.
   * Note: If you can't find your wavelan card, you've got a
   * non-NCR/AT&T/Lucent ISA cards, see wavelan.p.h for detail on
   * how to configure your card...
   */
  for(i = 0; i < (sizeof(MAC_ADDRESSES) / sizeof(char) / 3); i++)
    if((mac[0] == MAC_ADDRESSES[i][0]) &&
       (mac[1] == MAC_ADDRESSES[i][1]) &&
       (mac[2] == MAC_ADDRESSES[i][2]))
      return 0;

#ifdef DEBUG_CONFIG_INFO
  printk(KERN_WARNING "Wavelan (0x%3X) : Your MAC address might be : %02X:%02X:%02X...\n",
   ioaddr, mac[0], mac[1], mac[2]);
#endif
    return ENODEV;
}

/************************ INTERRUPT HANDLING ************************/

/*
 * This function is the interrupt handler for the WaveLAN card. This
 * routine will be called whenever: 
 */
static void
wavelan_interrupt(int      irq,
      void *    dev_id,
      struct pt_regs *  regs)
{
  device *  dev;
  u_short  ioaddr;
  net_local *  lp;
  u_short  hasr;
  u_short  status;
  u_short  ack_cmd;

  if((dev = (device *) (irq2dev_map[irq])) == (device *) NULL)
    {
#ifdef DEBUG_INTERRUPT_ERROR
      printk(KERN_WARNING "wavelan_interrupt(): irq %d for unknown device.\n",
       irq);
#endif
      return;
    }

#ifdef DEBUG_INTERRUPT_TRACE
  printk(KERN_DEBUG "%s: ->wavelan_interrupt()\n", dev->name);
#endif

  lp = (net_local *) dev->priv;
  ioaddr = dev->base_addr;

  /* Prevent reentrance. What should we do here ? */
#ifdef DEBUG_INTERRUPT_ERROR
  if(dev->interrupt)
    printk(KERN_INFO "%s: wavelan_interrupt(): Re-entering the interrupt handler.\n",
     dev->name);
#endif
  dev->interrupt = 1;

  if((hasr = hasr_read(ioaddr)) & HASR_MMC_INTR)
    {
      u_char  dce_status;

      /*
       * Interrupt from the modem management controller.
       * This will clear it -- ignored for now.
       */
      mmc_read(ioaddr, mmroff(0, mmr_dce_status), &dce_status, sizeof(dce_status));
#ifdef DEBUG_INTERRUPT_ERROR
      printk(KERN_INFO "%s: wavelan_interrupt(): unexpected mmc interrupt: status 0x%04x.\n",
       dev->name, dce_status);
#endif
    }

  if((hasr & HASR_82586_INTR) == 0)
    {
      dev->interrupt = 0;
#ifdef DEBUG_INTERRUPT_ERROR
      printk(KERN_INFO "%s: wavelan_interrupt(): interrupt not coming from i82586\n",
       dev->name);
#endif
      return;
    }

  /* Read interrupt data */
  obram_read(ioaddr, scboff(OFFSET_SCB, scb_status),
       (BYTE *) &status, sizeof(status));

  /*
   * Acknowledge the interrupt(s).
   */
  ack_cmd = status & SCB_ST_INT;
  obram_write(ioaddr, scboff(OFFSET_SCB, scb_command),
        (BYTE *) &ack_cmd, sizeof(ack_cmd));
  set_chan_attn(ioaddr, lp->hacr);

#ifdef DEBUG_INTERRUPT_INFO
  printk(KERN_DEBUG "%s: wavelan_interrupt(): status 0x%04x.\n",
   dev->name, status);
#endif

  /* Command completed. */
  if((status & SCB_ST_CX) == SCB_ST_CX)
    {
#ifdef DEBUG_INTERRUPT_INFO
      printk(KERN_DEBUG "%s: wavelan_interrupt(): command completed.\n",
       dev->name);
#endif
      wv_complete(dev, ioaddr, lp);

      /* If watchdog was activated, kill it ! */
      if(lp->watchdog.prev != (timer_list *) NULL)
  del_timer(&lp->watchdog);
      if(lp->tx_n_in_use > 0)
  {
    /* set timer to expire in WATCHDOG_JIFFIES */
    lp->watchdog.expires = jiffies + WATCHDOG_JIFFIES;
    add_timer(&lp->watchdog);
  }
    }

  /* Frame received. */
  if((status & SCB_ST_FR) == SCB_ST_FR)
    {
#ifdef DEBUG_INTERRUPT_INFO
      printk(KERN_DEBUG "%s: wavelan_interrupt(): received packet.\n",
       dev->name);
#endif
      wv_receive(dev);
    }

  /* Check the state of the command unit */
  if(((status & SCB_ST_CNA) == SCB_ST_CNA) ||
     (((status & SCB_ST_CUS) != SCB_ST_CUS_ACTV) && dev->start))
    {
#ifdef DEBUG_INTERRUPT_ERROR
      printk(KERN_INFO "%s: wavelan_interrupt(): CU inactive -- restarting\n",
       dev->name);
#endif
      wv_hw_reset(dev);
    }

  /* Check the state of the command unit */
  if(((status & SCB_ST_RNR) == SCB_ST_RNR) ||
     (((status & SCB_ST_RUS) != SCB_ST_RUS_RDY) && dev->start))
    {
#ifdef DEBUG_INTERRUPT_ERROR
      printk(KERN_INFO "%s: wavelan_interrupt(): RU not ready -- restarting\n",
       dev->name);
#endif
      wv_hw_reset(dev);
    }

  dev->interrupt = 0;

#ifdef DEBUG_INTERRUPT_TRACE
  printk(KERN_DEBUG "%s: <-wavelan_interrupt()\n", dev->name);
#endif
}

/*------------------------------------------------------------------*/
/*
 * Watchdog : when we start a transmission, we set a timer in the
 * kernel.  If the transmission complete, this timer is disabled. If
 * it expire, it try to unlock the hardware.
 *
 * Note : this watchdog doesn't work on the same principle as the
 * watchdog in the previous version of the ISA driver. I make it this
 * way because the overhead of add_timer() and del_timer() is nothing
 * and that it avoid calling the watchdog, saving some CPU...
 */
static void
wavelan_watchdog(u_long    a)
{
  device *    dev;
  net_local *    lp;
  WORD  ioaddr;
  DWORD    x;
  DWORD    nreaped;

  dev = (device *) a;
  ioaddr = dev->base_addr;
  lp = (net_local *) dev->priv;

#ifdef DEBUG_INTERRUPT_TRACE
  printk(KERN_DEBUG "%s: ->wavelan_watchdog()\n", dev->name);
#endif

#ifdef DEBUG_INTERRUPT_ERROR
  printk(KERN_INFO "%s: wavelan_watchdog: watchdog timer expired\n",
   dev->name);
#endif

  x = wv_splhi();

  dev = (device *) a;
  ioaddr = dev->base_addr;
  lp = (net_local *) dev->priv;

  if(lp->tx_n_in_use <= 0)
    {
      wv_splx(x);
      return;
    }

  nreaped = wv_complete(dev, ioaddr, lp);

#ifdef DEBUG_INTERRUPT_INFO
  printk(KERN_DEBUG "%s: wavelan_watchdog(): %d reaped, %d remain.\n",
   dev->name, nreaped, lp->tx_n_in_use);
#endif

#ifdef DEBUG_PSA_SHOW
  {
    psa_t    psa;
    psa_read(dev, 0, (BYTE *) &psa, sizeof(psa));
    wv_psa_show(&psa);
  }
#endif
#ifdef DEBUG_MMC_SHOW
  wv_mmc_show(dev);
#endif
#ifdef DEBUG_I82586_SHOW
  wv_cu_show(dev);
#endif

  /* If no buffer has been freed */
  if(nreaped == 0)
    {
#ifdef DEBUG_INTERRUPT_ERROR
      printk(KERN_INFO "%s: wavelan_watchdog(): cleanup failed, trying reset\n",
       dev->name);
#endif
      wv_hw_reset(dev);
    }
  else
    /* Re-set watchodog for next transmission */
    if(lp->tx_n_in_use > 0)
      {
  /* set timer to expire in WATCHDOG_JIFFIES */
  lp->watchdog.expires = jiffies + WATCHDOG_JIFFIES;
  add_timer(&lp->watchdog);
      }

  wv_splx(x);

#ifdef DEBUG_INTERRUPT_TRACE
  printk(KERN_DEBUG "%s: <-wavelan_watchdog()\n", dev->name);
#endif
}

/********************* CONFIGURATION CALLBACKS *********************/
/*
 * Here are the functions called by the linux networking (NET3) for
 * initialization, configuration and deinstallations of the Wavelan
 * ISA Hardware.
 */

/*------------------------------------------------------------------*/
/*
 * Configure and start up the WaveLAN PCMCIA adaptor.
 * Called by NET3 when it "open" the device.
 */
static int
wavelan_open(device *  dev)
{
  u_long  x;

#ifdef DEBUG_CALLBACK_TRACE
  printk(KERN_DEBUG "%s: ->wavelan_open(dev=0x%x)\n", dev->name,
   (DWORD) dev);
#endif

  /* Check irq */
  if(dev->irq == 0)
    {
#ifdef DEBUG_CONFIG_ERRORS
      printk(KERN_WARNING "%s: wavelan_open(): no irq\n", dev->name);
#endif
      return -ENXIO;
    }

  if((irq2dev_map[dev->irq] != (device *) NULL) ||
     /* This is always true, but avoid the false IRQ. */
     ((irq2dev_map[dev->irq] = dev) == (device *) NULL) ||
     (request_irq(dev->irq, &wavelan_interrupt, 0, "WaveLAN", NULL) != 0))
    {
      irq2dev_map[dev->irq] = (device *) NULL;
#ifdef DEBUG_CONFIG_ERRORS
      printk(KERN_WARNING "%s: wavelan_open(): invalid irq\n", dev->name);
#endif
      return -EAGAIN;
    }

  x = wv_splhi();
  if(wv_hw_reset(dev) != -1)
    {
      dev->interrupt = 0;
      dev->start = 1;
    }
  else
    {
      free_irq(dev->irq, NULL);
      irq2dev_map[dev->irq] = (device *) NULL;
#ifdef DEBUG_CONFIG_ERRORS
      printk(KERN_INFO "%s: wavelan_open(): impossible to start the card\n",
       dev->name);
#endif
      return -EAGAIN;
    }
  wv_splx(x);

  MOD_INC_USE_COUNT;

#ifdef DEBUG_CALLBACK_TRACE
  printk(KERN_DEBUG "%s: <-wavelan_open()\n", dev->name);
#endif
  return 0;
}

/*------------------------------------------------------------------*/
/*
 * Shutdown the WaveLAN ISA card.
 * Called by NET3 when it "close" the device.
 */
static int
wavelan_close(device *  dev)
{
  net_local *  lp = (net_local *)dev->priv;

#ifdef DEBUG_CALLBACK_TRACE
  printk(KERN_DEBUG "%s: ->wavelan_close(dev=0x%x)\n", dev->name,
   (DWORD) dev);
#endif

  /* Not do the job twice... */
  if(dev->start == 0)
    return 0;

  dev->tbusy = 1;
  dev->start = 0;

  /* If watchdog was activated, kill it ! */
  if(lp->watchdog.prev != (timer_list *) NULL)
    del_timer(&lp->watchdog);

  /*
   * Flush the Tx and disable Rx.
   */
  wv_82586_stop(dev);

  free_irq(dev->irq, NULL);
  irq2dev_map[dev->irq] = (device *) NULL;

  MOD_DEC_USE_COUNT;

#ifdef DEBUG_CALLBACK_TRACE
  printk(KERN_DEBUG "%s: <-wavelan_close()\n", dev->name);
#endif
  return 0;
}

/*------------------------------------------------------------------*/
/*
 * Probe an i/o address, and if the wavelan is there configure the
 * device structure
 * (called by wavelan_probe() & via init_module())
 */
static int
wavelan_config(device *  dev)
{
  u_short  ioaddr = dev->base_addr;
  u_char  irq_mask;
  int    irq;
  net_local *  lp;

#ifdef DEBUG_CALLBACK_TRACE
  printk(KERN_DEBUG "%s: ->wavelan_config(dev=0x%x, ioaddr=0x%x)\n", dev->name,
   (DWORD)dev, ioaddr);
#endif

  /* Check irq arg on command line */
  if(dev->irq != 0)
    {
      irq_mask = wv_irq_to_psa(dev->irq);

      if(irq_mask == 0)
  {
#ifdef DEBUG_CONFIG_ERROR
    printk(KERN_WARNING "%s: wavelan_config(): invalid irq %d -- ignored.\n",
     dev->name, dev->irq);
#endif
    dev->irq = 0;
  }
      else
  {
#ifdef DEBUG_CONFIG_INFO
    printk(KERN_DEBUG "%s: wavelan_config(): changing irq to %d\n",
     dev->name, dev->irq);
#endif
    psa_write(ioaddr, HACR_DEFAULT,
        psaoff(0, psa_int_req_no), &irq_mask, 1);
    wv_hacr_reset(ioaddr);
  }
    }

  psa_read(ioaddr, HACR_DEFAULT, psaoff(0, psa_int_req_no), &irq_mask, 1);
  if((irq = wv_psa_to_irq(irq_mask)) == -1)
    {
#ifdef DEBUG_CONFIG_ERROR
      printk(KERN_INFO "%s: wavelan_config(): could not wavelan_map_irq(%d).\n",
       dev->name, irq_mask);
#endif
      return EAGAIN;
    }

  dev->irq = irq;

  request_region(ioaddr, sizeof(ha_t), "wavelan");

  dev->mem_start = 0x0000;
  dev->mem_end = 0x0000;
  dev->if_port = 0;

  /* Initialize device structures */
  dev->priv = kmalloc(sizeof(net_local), GFP_KERNEL);
  if(dev->priv == NULL)
    return -ENOMEM;
  memset(dev->priv, 0x00, sizeof(net_local));
  lp = (net_local *)dev->priv;

  /* Back link to the device structure */
  lp->dev = dev;
  /* Add the device at the beggining of the linked list */
  lp->next = wavelan_list;
  wavelan_list = lp;

  lp->hacr = HACR_DEFAULT;

  lp->watchdog.function = wavelan_watchdog;
  lp->watchdog.data = (DWORD) dev;
  lp->promiscuous = 0;
  lp->mc_count = 0;

  /*
   * Fill in the fields of the device structure
   * with ethernet-generic values.
   */
  ether_setup(dev);

  dev->open = wavelan_open;
  dev->stop = wavelan_close;
  dev->hard_start_xmit = wavelan_packet_xmit;
  dev->get_stats = wavelan_get_stats;
  dev->set_multicast_list = &wavelan_set_multicast_list;
  dev->set_mac_address = &wavelan_set_mac_address;

#ifdef WIRELESS_EXT  /* If wireless extension exist in the kernel */
  dev->do_ioctl = wavelan_ioctl;
  dev->get_wireless_stats = wavelan_get_wireless_stats;
#endif

  dev->mtu = WAVELAN_MTU;

  /* Display nice info */
  wv_init_info(dev);

#ifdef DEBUG_CALLBACK_TRACE
  printk(KERN_DEBUG "%s: <-wavelan_config()\n", dev->name);
#endif
  return 0;
}

/*------------------------------------------------------------------*/
/*
 * Check for a network adaptor of this type.
 * Return '0' iff one exists.
 * (There seem to be different interpretations of
 * the initial value of dev->base_addr.
 * We follow the example in drivers/net/ne.c.)
 * (called in "Space.c")
 */
 
int wavelan_probe(device *  dev)
{
  short    base_addr;
  mac_addr  mac;    /* Mac address (check wavelan existence) */
  int    i;
  int    r;

#ifdef DEBUG_CALLBACK_TRACE
  printk(KERN_DEBUG "%s: ->wavelan_probe(dev=0x%x (base_addr=0x%x))\n",
   dev->name, (DWORD)dev, (DWORD)dev->base_addr);
#endif

#ifdef  STRUCT_CHECK
  if (wv_struct_check() != (char *) NULL)
    {
      printk(KERN_WARNING "%s: wavelan_probe(): structure/compiler botch: \"%s\"\n",
       dev->name, wv_struct_check());
      return ENODEV;
    }
#endif  /* STRUCT_CHECK */

  /* Check the value of the command line parameter for base address */
  base_addr = dev->base_addr;

  /* Don't probe at all. */
  if(base_addr < 0)
    {
#ifdef DEBUG_CONFIG_ERRORS
      printk(KERN_WARNING "%s: wavelan_probe(): invalid base address\n",
       dev->name);
#endif
      return ENXIO;
    }

  /* Check a single specified location. */
  if(base_addr > 0x100)
    {
      /* Check if the is something at this base address */
      if((r = wv_check_ioaddr(base_addr, mac)) == 0)
  {
    memcpy(dev->dev_addr, mac, 6);  /* Copy mac address */
    r = wavelan_config(dev);
  }

#ifdef DEBUG_CONFIG_INFO
      if(r != 0)
  printk(KERN_DEBUG "%s: wavelan_probe(): no device at specified base address (0x%X) or address already in use\n",
         dev->name, base_addr);
#endif

#ifdef DEBUG_CALLBACK_TRACE
      printk(KERN_DEBUG "%s: <-wavelan_probe()\n", dev->name);
#endif
      return r;
    }

  /* Scan all possible address of the wavelan hardware */
  for(i = 0; i < NELS(iobase); i++)
    {
      /* Check if the is something at this base address */
      if(wv_check_ioaddr(iobase[i], mac) == 0)
  {
    dev->base_addr = iobase[i];    /* Copy base address */
    memcpy(dev->dev_addr, mac, 6);  /* Copy mac address */
    if(wavelan_config(dev) == 0)
      {
#ifdef DEBUG_CALLBACK_TRACE
        printk(KERN_DEBUG "%s: <-wavelan_probe()\n", dev->name);
#endif
        return 0;
      }
  }
    }

  /* We may have touch base_addr : another driver may not like it... */
  dev->base_addr = base_addr;

#ifdef DEBUG_CONFIG_INFO
  printk(KERN_DEBUG "%s: wavelan_probe(): no device found\n",
   dev->name);
#endif

  return ENODEV;
}

/****************************** MODULE ******************************/
/*
 * Module entry point : insertion & removal
 */

#ifdef  MODULE
/*------------------------------------------------------------------*/
/*
 * Insertion of the module...
 * I'm now quite proud of the multi-device support...
 */
int
init_module(void)
{
  mac_addr  mac;    /* Mac address (check wavelan existence) */
  int    ret = 0;
  int    i;

#ifdef DEBUG_MODULE_TRACE
  printk(KERN_DEBUG "-> init_module()\n");
#endif

  /* If probing is asked */
  if(io[0] == 0)
    {
#ifdef DEBUG_CONFIG_ERRORS
      printk(KERN_WARNING "wavelan init_module(): doing device probing (bad !)\n");
      printk(KERN_WARNING "Specify base addresses while loading module to correct the problem\n");
#endif

      /* Copy the basic set of address to be probed */
      for(i = 0; i < NELS(iobase); i++)
  io[i] = iobase[i];
    }


  /* Loop on all possible base addresses */
  i = -1;
  while((io[++i] != 0) && (i < NELS(io)))
    {
      /* Check if the is something at this base address */
      if(wv_check_ioaddr(io[i], mac) == 0)
  {
    device *  dev;

    /* Create device and set basics args */
    dev = kmalloc(sizeof(struct device), GFP_KERNEL);
    memset(dev, 0x00, sizeof(struct device));
    dev->name = name[i];
    dev->base_addr = io[i];
    dev->irq = irq[i];
    dev->init = &wavelan_config;
    memcpy(dev->dev_addr, mac, 6);  /* Copy mac address */

    /* Try to create the device */
    if(register_netdev(dev) != 0)
      {
        /* DeAllocate everything */
        /* Note : if dev->priv is mallocated, there is no way to fail */
        kfree_s(dev, sizeof(struct device));
        ret = -EIO;
      }
  }  /* If there is something at the address */
    }    /* Loop on all addresses */

#ifdef DEBUG_CONFIG_ERRORS
  if(wavelan_list == (net_local *) NULL)
    printk(KERN_WARNING "wavelan init_module(): No device found\n");
#endif

#ifdef DEBUG_MODULE_TRACE
  printk(KERN_DEBUG "<- init_module()\n");
#endif
  return ret;
}

/*------------------------------------------------------------------*/
/*
 * Removal of the module
 */
void
cleanup_module(void)
{
#ifdef DEBUG_MODULE_TRACE
  printk(KERN_DEBUG "-> cleanup_module()\n");
#endif

  /* Loop on all devices and release them */
  while(wavelan_list != (net_local *) NULL)
    {
      device *  dev = wavelan_list->dev;

#ifdef DEBUG_CONFIG_INFO
      printk(KERN_DEBUG "%s: cleanup_module(): removing device at 0x%x\n",
       dev->name, (DWORD) dev);
#endif

      /* Release the ioport-region. */
      release_region(dev->base_addr, sizeof(ha_t));

      /* Remove definitely the device */
      unregister_netdev(dev);

      /* Unlink the device */
      wavelan_list = wavelan_list->next;

      /* Free pieces */
      kfree_s(dev->priv, sizeof(struct net_local));
      kfree_s(dev, sizeof(struct device));
    }

#ifdef DEBUG_MODULE_TRACE
  printk(KERN_DEBUG "<- cleanup_module()\n");
#endif
}
#endif  /* MODULE */

/*
 * This software may only be used and distributed
 * according to the terms of the GNU Public License.
 *
 * This software was developed as a component of the
 * Linux operating system.
 * It is based on other device drivers and information
 * either written or supplied by:
 *  Ajay Bakre (bakre@paul.rutgers.edu),
 *  Donald Becker (becker@cesdis.gsfc.nasa.gov),
 *  Loeke Brederveld (Loeke.Brederveld@Utrecht.NCR.com),
 *  Anders Klemets (klemets@it.kth.se),
 *  Vladimir V. Kolpakov (w@stier.koenig.ru),
 *  Marc Meertens (Marc.Meertens@Utrecht.NCR.com),
 *  Pauline Middelink (middelin@polyware.iaf.nl),
 *  Robert Morris (rtm@das.harvard.edu),
 *  Jean Tourrilhes (jt@hplb.hpl.hp.com),
 *  Girish Welling (welling@paul.rutgers.edu),
 *
 * Thanks go also to:
 *  James Ashton (jaa101@syseng.anu.edu.au),
 *  Alan Cox (iialan@iiit.swan.ac.uk),
 *  Allan Creighton (allanc@cs.usyd.edu.au),
 *  Matthew Geier (matthew@cs.usyd.edu.au),
 *  Remo di Giovanni (remo@cs.usyd.edu.au),
 *  Eckhard Grah (grah@wrcs1.urz.uni-wuppertal.de),
 *  Vipul Gupta (vgupta@cs.binghamton.edu),
 *  Mark Hagan (mhagan@wtcpost.daytonoh.NCR.COM),
 *  Tim Nicholson (tim@cs.usyd.edu.au),
 *  Ian Parkin (ian@cs.usyd.edu.au),
 *  John Rosenberg (johnr@cs.usyd.edu.au),
 *  George Rossi (george@phm.gov.au),
 *  Arthur Scott (arthur@cs.usyd.edu.au),
 *  Peter Storey,
 * for their assistance and advice.
 *
 * Please send bug reports, updates, comments to:
 *
 * Bruce Janson                                    Email:  bruce@cs.usyd.edu.au
 * Basser Department of Computer Science           Phone:  +61-2-9351-3423
 * University of Sydney, N.S.W., 2006, AUSTRALIA   Fax:    +61-2-9351-3838
 */
