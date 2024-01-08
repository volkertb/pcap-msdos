/*
 * /usr/src/accton/accton.c
 *
 * An ACCTON EtherPocket-10T/CX/Duo driver for Linux (>= 1.1.54)
 *
 * Copyright 1994 by Volker Meyer zu Bexten (vmzb@ims.fhg.de)
 *               and Ron Smits (Ron.Smits@Netherlands.NCR.COM).
 *
 * This software may be used and distributed according to the terms of the
 * GNU Public License, incorporated herein by reference.
 *
 * This is alpha test software and the authors do not take any
 * responsibility for proper function.
 */

/*
 * Usage:
 * 
 * This driver is meant to be used as a loadable module.
 * By default, the IO address is found by probing, and the IRQ number is 7.
 * The IO adress, IRQ and debugging level can be set by specifiying them
 * on the command line like this:
 * insmod /usr/src/accton/accton.o ethpk_io=0x3bc ethpk_irq=7 ethpk_debug=4
 * This made it a lot easier to program the stuff. After having loaded
 * you must of course do an ifconfig and route of the thing.
 */

/*
 * Sources:
 * 
 * Based on skeleton.c (v1.51 9/24/94) and other driver sources.
 * Using 8390.c (v1.10 9/23/94 converted to ethpknic.c) of
 * Donald Becker (becker@cesdis.gsfc.nasa.gov) -- thanks a lot.
 * 
 * All adapter-specific information was derived from driver assembly source
 * code (last changes 06/94) of ACCTON Technology Corp. (the only author's
 * names I found in the files is Harrison Yeh and somebody named Lin).
 * Internet addresses are support@mis.accton.com.tw, YTS@accton.com.tw and 
 * SHENG@accton.com.tw.
 * The source files were sent to us by Dennis Director (dennis@math.nwu.edu)
 * who received them from ACCTON.
 * 
 * Please note that everything is based on guesses and assumptions and tests
 * with our EN2209 R01 adapters.
 * 
 * The EtherPocket models are build around an NS 8390 compatible chip,
 * just the communications go through the parallel port. 
 * The physical station address is stored in a serial EEPROM.
 * The latest model works with the enhanced printer port, but this is not
 * supported yet.
 * 
 * There is one problem however with the ENISR_RX (0x01) interrupt. 
 * It is only received once, after that, interruption only takes place
 * with other flags, e.g. when there has been a TRANSMIT and a DMA
 * acknowledge, resulting in the ISR register holding 0x43 
 * (ENISR_RX + ENISR_TX + ACCISR_DMA).
 * This is why we currently use this driver with a kind of polling mode
 * (-DPOLLING) which starts a timer to ensure that the driver falls back
 * into the interrupt handling routine. 
 * We will try to fix this problem soon!!!
 */

char *ethpk_version = "@(#)$Id: accton.c,v 3.1 1995/05/08 12:23:42 root Exp root $";

/*
 * At your own risk, -DTUNE can be used to speed this driver up.
 * That might not work for individual machines/adapters,
 * but I am testing it on my laptop (VMzB).
 * So far, I have tried these versions:
 * 0) Without -DTUNE.
 * 1) Disabling the delay statements in Delay_a_Little_Time().
 * 2) Additionally using inline for some low-level functions, increasing the 
 *    kernel size. I dropped that because the driver doesn't work then.
 * 3) Additionally using constants for the port addresses, this was dropped 
 *    later since it is inflexible and does not speed things up.
 * A "benchmarks", I used:
 * a) 1MByte copy from NFS to /tmp. Typical elapsed times were:
 * 0: 18s, 1: 12s, 2: crash, 3: no improvement.
 * b) ping -s 1554 to a fast host. Typical round-trip times were:
 * 0: 49ms, 1: 19ms, 2: crash, 3: untested.
 * c) xengine on a remote (SPARC 10) host. Typical speeds were:
 * 0: 310rpm, 1: 370rmp, 2: crash, 3: untested.
 */

#include "pmdrvr.h"
#include "ethpknic.h"
#include "accton.h"

#ifndef ETHPK_DEBUG
#define ETHPK_DEBUG 0
#endif
int ethpk_debug LOCKED_VAR = ETHPK_DEBUG;

#define Data_Port       (ethpk_io)
#define Status_Port     (ethpk_io+1)
#define Control_Port    (ethpk_io+2)

#define EEPROM_SIZE           0x10
#define ETHPK_IO_EXTENT       3
#define EEPROM_NODE_ADDR_OFFS 24
#define EEPROM_NODE_ADDR_DUP  10

/* The station (ethernet) address prefix, used for IDing the board.
 */
#define SA_ADDR0   0x00
#define SA_ADDR1   0x00
#define SA_ADDR2   0xe8
#define EN0_RSARLO 0x08    /* Remote start address reg 0 */
#define EN0_RSARHI 0x09    /* Remote start address reg 1 */

#define ei_status (*(struct ei_device *)(dev->priv))

static WORD ethpk_portlist[] = { 0x278, 0x378, 0x3bc, 0 };

/* The following variables can be overruled at load time.
 */
static int ethpk_io     = 0;
static int fast_mode    = 0;
static int epp_mode     = 0;

int ethpk_irq LOCKED_VAR = 7;   /* used by ethpknic.c */

#ifdef POLLING
  struct timer_list ethpk_poll  LOCKED_VAR;
  int ethpk_poll_interval       LOCKED_VAR = 20;
  static struct device *ethpk_dev;
#endif

static int  Wait_ACK_Signal_High1 (void);
static void Wait_ACK_Signal_High (void);
static void Select_Device (BYTE cmd);

static int  ResetAdapter (struct device *dev);
static void Write_Data_To_Buffer (const char *buffer, int count);
static void Read_Data_From_Buffer (char *buffer, int count);
static void Close_Buffer_Channel (BYTE cmd);
static void Close_Channel (void);
static void Delay_a_Little_Time ();
static void Enable_Remote_Dma (int address, int count, int cmd);
static int  check_card (void);
static int  Read_EEPROM (WORD * buf, int count);
static int  ram_test (void);
static int  ethpk_open (struct device *dev);
static void ethpk_close (struct device *dev);
static void ethpk_block_output (struct device *dev, int count, const char *buf, int startpage);
static int  ethpk_block_input (struct device *dev, int count, char *buf, int ring_offset);
static int  ethpk_probe1 (struct device *dev, WORD ioaddr);

#ifdef POLLING
  static void poll_interrupt (int irq) LOCKED_FUNC;
  static void no_interrupt (int irq)   LOCKED_FUNC;
#endif


void en_prn_int (void);

void init_accton (struct device *dev);
void analyze_isr (void);
void analyze_tsr (void);

/*
 * function: Wait_ACK_Signal_High1
 * params:   none
 * returns:  0 if OK
 *           1 if function was left by timeout.
 * purpose:  Wait a long time for ACK to go high.
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
static int Wait_ACK_Signal_High1 (void)
{
  BYTE tt;
  int i = 0;

  do
  {
    tt = inb (Status_Port);
    i++;
  }
  while (!(tt & ACK_Signal) && i < 0xffff);
  if (i)
    return (0);
  return (1);
}

/*
 * function: Wait_ACK_Signal_High
 * params:   none
 * returns:  0 if OK
 *           1 if not
 * purpose:  Wait indefinitly for ACK to go high.
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
static void Wait_ACK_Signal_High (void)
{
  BYTE tt;

  do
  {
    tt = inb (Status_Port);
  }
  while (!(tt & ACK_Signal));
}

/*
 * function: Wait_ACK_Signal_Low
 * params:   none
 * returns:  0 if OK
 *           1 if not
 * purpose:  Wait indefinitly for ACK to go low.
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
static void Wait_ACK_Signal_Low (BYTE cmd)
{
  int recv;

  outb (cmd | ATFD, Control_Port);
  do
  {
    recv = inb (Status_Port);
  }
  while (recv & ACK_Signal);
  outb (cmd, Control_Port);
}

/*
 * function: Delay_a_Little_Time
 * params:   none
 * returns:  nothing
 * purpose:  Delay a certain amount of time (not optimized yet).
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
static void Delay_a_Little_Time (void)
{
  SLOW_DOWN_IO();
  SLOW_DOWN_IO();
}

/*
 * function: Output_Data_With_Clock
 * params:   byte for output
 * returns:  nothing
 * purpose:  Pass one byte to the adapter.
 * created:  11.11.94 RS (used to be the OUTPUT_PORT_VALUE and 
 *                        WAIT_ONE_CLOCK macros in the original source)
 */
static void Output_Data_With_Clock (BYTE value)
{
  outb (value, Data_Port);

  outb (INIT | ATFD, Control_Port);
  Delay_a_Little_Time();
  Delay_a_Little_Time();
  Close_Channel();

  outb (value | ACCC_START, Data_Port);

  outb (INIT | ATFD, Control_Port);
  Delay_a_Little_Time();
  Delay_a_Little_Time();
  Close_Channel();

  outb (value, Data_Port /* & (~8390_START) */ );

  outb (INIT | ATFD, Control_Port);
  Delay_a_Little_Time();
  Delay_a_Little_Time();
  Close_Channel();
}

/*
 * function: Select_Device
 * params:   byte cmd: command
 * returns:  nothing
 * purpose:  Tell the EtherPocket we want to work.
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
static void Select_Device (BYTE cmd)
{
  outb (cmd, Data_Port);
  outb (INIT | SLCTIN, Control_Port);
  Delay_a_Little_Time();
  outb (INIT, Control_Port);
}

/*
 * function: Set_Register_Value
 * params:   byte reg: register address,
 *           byte value: value to be written
 * returns:  nothing
 * purpose:  Write a value to a register with a given address.
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
void Set_Register_Value (BYTE reg, BYTE value)
{
  Select_Device (reg | FMRegister);
  Delay_a_Little_Time();
  outb (value, Data_Port);
  Wait_ACK_Signal_Low (INIT);
}

/*
 * function: Read_Register_Value
 * params:   byte reg: register address,
 * returns:  int (byte) result
 * purpose:  Get a value from a register with a given address.
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
int Read_Register_Value (BYTE reg)
{
  BYTE tt;

  Select_Device (reg | FMRegister);
  Delay_a_Little_Time();
  outb (0x0ff, Data_Port);
  outb (0x0e5, Control_Port);	/* toggle ATFD */
  Delay_a_Little_Time();
  Wait_ACK_Signal_Low (BI_DIR | INIT | STROBE);	/* wait for the data */
  tt = inb (Data_Port);		/* read the byte from the printer data port */
  outb (0x04, Control_Port);	/* return to initial state */
  return (tt);
}

/*
 * function: Write_Data_To_Buffer
 * params:   const char *buffer: buffer start address
 *           int count         : buffer contents length in bytes
 * returns:  nothing
 * purpose:  Write the Data in buffer to the adapter.
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
static void Write_Data_To_Buffer (const char *buffer, int count)
{
  BYTE cmd;
  int  i;

  Select_Device (FMData);	/* check (set) fast access mode */
  Delay_a_Little_Time();
  cmd = INIT;

  for (i = 0; i != count; i++)
  {
    if (Wait_ACK_Signal_High1())
       break;                     /* ACK did not go high */
    outb (buffer[i], Data_Port);  /* output the data */
    cmd ^= ATFD;                  /* toggle ATFD */
    outb (cmd, Control_Port);
    Delay_a_Little_Time();        /* two in 0x61 in the original */
    Delay_a_Little_Time();
  }
  Close_Buffer_Channel (cmd);
}

/*
 * function: Read_Data_From_Buffer
 * params:   const char *buffer: buffer start address
 *           int count         : expected length in bytes
 * returns:  nothing
 * purpose:  Read stuff from the adapter into buffer.
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
static void Read_Data_From_Buffer (char *buffer, int count)
{
  BYTE cmd;
  int  i;

  Select_Device (FMData);
  Delay_a_Little_Time();
  cmd = 0x0e0 | INIT_STROBE;	/* ATFD high */
  outb (0xff, Data_Port);
  outb (cmd, Control_Port);
  Delay_a_Little_Time();

  for (i = 0; i != count; i++)
  {
    Wait_ACK_Signal_High();
    buffer[i] = inb (Data_Port);
    cmd ^= ATFD;
    outb (cmd, Control_Port);
  }
  Close_Buffer_Channel (cmd);
}

/*
 * function: Close_Buffer_Channel
 * params:   byte cmd : last command sent
 * returns:  nothing
 * purpose:  Close the "channel" for which cmd applies(?).
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
static void Close_Buffer_Channel (BYTE cmd)
{
  BYTE tt = cmd & 0x0de;

  outb (tt, Control_Port);
  outb (0x0a0, Data_Port);
  outb (tt |= 0x0c, Control_Port);
  Delay_a_Little_Time();
  outb (tt, Control_Port);
  outb (INIT, Control_Port);
}

/*
 * function: Close_Channel
 * params:   none
 * returns:  nothing
 * purpose:  Close the current "channel".
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
static void Close_Channel (void)
{
  outb (INIT, Control_Port);
}

/*
 * function: Enable_Remote_Dma
 * params:   int address : address in the 8390's buffer
 *           int count   : number of bytes to be transmitted
 *           int cmd     : command to be executed
 * returns:  nothing
 * purpose:  Tell the adapter we are going to transfer data.
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
static void Enable_Remote_Dma (int address, int count, int cmd)
{
  /* set the address */
  Set_Register_Value (ACC0_RADDHI, address >> 8);
  Set_Register_Value (ACC0_RADDLO, address & 0xff);

  /* set the count */
  Set_Register_Value (ACC0_RCNTHI, count >> 8);
  Set_Register_Value (ACC0_RCNTLO, count & 0xff);
  Set_Register_Value (ACC_CCMD, cmd);
}

/*
 * function: en_prn_int
 * params:   none
 * returns:  nothing
 * purpose:  I'm not sure what this does. Probably enables interrupts.
 *           We suspect that this routine should be called more reliably
 *           to fix our problem with interrupts!!!
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
void en_prn_int (void)
{
  if (ethpk_debug >= 3)
     printk ("en_prn_int..");

  Select_Device (0x00);		/* xor bh, bh in the original source */
  outb (0x10 | INIT, Control_Port);
  outb (0x02, Data_Port);
  outb (0x18 | INIT, Control_Port);
  Delay_a_Little_Time();
  outb (0x10 | INIT, Control_Port);

  if (ethpk_debug >= 3)
     printk ("en_prn_int done\n");
}

/*
 * function: ethpk_probe
 * params:   struct device   *dev -- device record, may be NULL(?)
 * returns:  zero on success, nonzero on failure.
 * purpose:  Check for Accton EtherPocket adapter *DEV.
 * created:  09.10.1994 VMzB
 */
int ethpk_probe (struct device *dev)
{
  WORD *port;
  WORD  base_addr = dev ? dev->base_addr : 0;
  int   rc = 0;

  if (base_addr > 0x1ff)	/* Check a single specified location. */
     rc = ethpk_probe1 (dev, base_addr);
  else if (base_addr != 0)	/* Don't probe at all. */
     rc = 0;
  else
    for (port = &ethpk_portlist[0]; *port; port++)
      if (ethpk_probe1 (dev, *port))
      {
	dev->base_addr = *port;
	rc = 1;
	break;
      }
  return (rc);
}

/*
 * function: ethpk_probe1
 * params:   struct device   *dev -- device record (alloc'ed here if NULL)
 *           int           ioaddr -- I/O base address to probe at
 * returns:  zero on success, nonzero (ENODEV) on failure.
 * purpose:  Check for Accton EtherPocket adapter *DEV at IOADDR.
 * created:  09.10.1994 VMzB
 * note:     Sorry that the following probe is not very friendly yet:
 *           We should first make sure that this possibly is an EtherPocket 
 *           without writing.
 */
static int ethpk_probe1 (struct device *dev, WORD ioaddr)
{
  struct ei_device *ei_local;
  BYTE   buf [EEPROM_SIZE * sizeof(WORD)];
  int    i, checksum;

  ethpk_io = ioaddr;   /* This is needed by Read_EEPROM and check_card!!! */

  if (ethpk_debug > 0)
     printk ("%s: IO addr %3x.\n", dev->name, ioaddr);

  checksum = Read_EEPROM ((WORD*)buf, EEPROM_SIZE);
  if (ethpk_debug > 2)
  {
    if (checksum)
       printk ("%s: Nonzero EEPROM checksum (0x%04x).\n", dev->name, checksum);

    printk ("Station Address");
    for (i = EEPROM_NODE_ADDR_OFFS; i < EEPROM_NODE_ADDR_OFFS + ETH_ALEN; ++i)
       printk (":%02x", buf[i]);
    printk ("\n");

    printk ("S. A. duplicate");
    for (i = EEPROM_NODE_ADDR_DUP; i < EEPROM_NODE_ADDR_DUP + ETH_ALEN; ++i)
       printk (":%02x", buf[i ^ 1]);

    printk ("\nEEPROM contents: '");
    for (i = 0; i < sizeof(buf) && buf[i] >= ' ' && buf[i] <= 'z'; i++)
       printk ("%c", buf[i]);
    printk ("...'");

    for (i = 0; i < sizeof(buf); ++i)
       printk ("%s%02x", (i % 8) ? " " : "\n", buf[i]);
    printk ("\n");
  }

  if (checksum)
     return (0);

  /* Read the station address PROM.
   */
  for (i = 0; i < ETH_ALEN; i++)
      dev->dev_addr[i] = buf [EEPROM_NODE_ADDR_OFFS + i];

  /* Check the first three octets of the S.A. for the manufacturer's code.
   */
  if (dev->dev_addr[0] != SA_ADDR0 ||
      dev->dev_addr[1] != SA_ADDR1 ||
      dev->dev_addr[2] != SA_ADDR2)
  {
    if (ethpk_debug)
       printk ("%s: SA %x:%x:%x does not match manufacturer code %x:%x:%x.\n",
              dev->name, dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
              SA_ADDR0, SA_ADDR1, SA_ADDR2);
    return (0);
  }
  check_card();                /* if the card is okay, this will have
                                * set fast_mode to 0 or 1, -1 means error
                                */
  if (fast_mode == -1)
  {
    printk ("%s: EEPROM looks good, but register write/read test failed.\n",
            dev->name);
    return (0);
  }

  if (!ram_test())
  {
    printk ("%s: EEPROM and register test suceeded, but RAM test failed.\n",
            dev->name);
    return (0);
  }

  /* Most initialization is properly done in ethpknic.c:ethdev_init().
   * Some entries are set before, since ethdev_init() doesn't overwrite them,
   * others have to be overwritten after ethdev_init() (that's bad style!!!)
   */
  dev->open  = ethpk_open;
  dev->close = ethpk_close;
  dev->irq   = ethpk_irq;
  ethdev_init (dev);
  dev->xmit = ei_start_xmit;

  ei_local = (struct ei_device *) dev->priv;
  ei_local->reset_8390    = init_accton;
  ei_local->block_output  = ethpk_block_output;
  ei_local->block_input   = ethpk_block_input;
  ei_local->word16        = 0;         /* only 8 bit mode */
  ei_local->rx_start_page = PSTART_8p;
  ei_local->stop_page     = PSTOP_8p;
  ei_local->current_page  = PSTART_8p + 1;

  printk ("%s: EtherPocket at IO %x, IRQ %d, %s/%s, SA %x",
          dev->name, ioaddr, dev->irq, fast_mode ? "FM" : "SM",
          epp_mode ? "EPP" : "SPP", dev->dev_addr[0]);

  for (i = 1; i < ETH_ALEN; i++)
     printk (":%x", dev->dev_addr[i]);
  printk (".\n");
  return (1);
}

/*
 * function: check_card
 * params:   none
 * returns:  int fast_mode - nonzero for fast mode
 * purpose:  Test wether the card is in fastmode or slowmode.
 *           At the moment only fast mode is supported. 
 *           This fast or slowmode is determined by your printer port.
 *           Check your bios for settings.
 * created:  11.11.94 RS (from a macro with same name in the original source)
 * globals:  fast_mode
 */
static int check_card (void)
{
  int i;
  BYTE tt;

  if (ethpk_debug > 3)
    printk ("check_card\n");

  outb (0x00, Data_Port);
  outb (0x00, Control_Port);
  for (i = 0; i != 6; i++)
    Delay_a_Little_Time();

  outb (INIT, Control_Port);
  for (i = 0; i != 0x1f4; i++)
    Delay_a_Little_Time();

  /* test the ethernet pocket adapter for fast or slow mode
   */
  for (i = 0; i != 0xff; i++)
  {
    /* write to the test register
     */
    Select_Device (FMTest);
    Delay_a_Little_Time();
    outb (i, Data_Port);	/* write the test data */
    outb (INIT | ATFD, Control_Port);
    Close_Channel();

    /* now read it back
     */
    Select_Device (FMTest);
    Delay_a_Little_Time();
    outb (0xff, Data_Port);
    outb (BIDIR_INIT | STROBE, Control_Port);
    Delay_a_Little_Time();
    tt = inb (Data_Port);
    Close_Channel();
    if (tt != i)
    {
      /* not fast mode, bummer */
      break;
    }
  }

  /* clear the address latch
   */
  Select_Device (Clear_Address_Latch);
  Delay_a_Little_Time();
  outb (0xff, Data_Port);
  if (!i)
       fast_mode = 0;
  else fast_mode = 1;

  if (ethpk_debug == 3)
     printk ("check_card done\n");

  return (fast_mode);
}

/*
 * function: ResetAdapter
 * params:   struct device *dev : our device
 * returns:  zero on success, nonzero on error
 * purpose:  Reset the adapter and set it up for operations.
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
static int ResetAdapter (struct device *dev)
{
  int timeout;

  if (ethpk_debug == 3)
     printk ("ResetAdapter ..");

  Select_Device (StartReset);	/* Pulse the board reset */
  Delay_a_Little_Time();
  for (timeout = jiffies + 20; jiffies <= timeout;)
    Select_Device (StopReset);	/* stop the reset */

  for (timeout = jiffies + 20; jiffies <= timeout;)
    Delay_a_Little_Time();

  init_accton (dev);
  if (!ram_test())
  {
    printk ("ram test error\n");
    return (0);
  }
  if (ethpk_debug == 3)
    printk ("ResetAdapter done\n");
  return (1);
}

/*
 * function: Read_EEPROM
 * params:   word * buf : buffer for eeprom contents
 *           int  count : number of bytes to be read
 * returns:  nothing
 * purpose:  Read the EEPROM contents.
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
static int Read_EEPROM (WORD *buf, int count)
{
  int   i, j;
  WORD  value;
  DWORD checksum = 0;

  for (i = count - 1; i >= 0; --i, ++buf)
  {
    Select_Device (Clear_All);
    Output_Data_With_Clock (0);
    Select_Device (EEPROM);

    /* enable eeprom, set read status and word address
     */
    value = i | 0x180;  /* set first three bits to 0,1,1, then 7 data bits */
    for (j = 10; j; --j)
    {
      Output_Data_With_Clock ((value & 0x200) ? DI_High : DI_Low);
      value <<= 1;
    }

    /* read eeprom word
     */
    value = 0;
    for (j = 16; j; --j)
    {
      Output_Data_With_Clock (DI_Low);
      value <<= 1;
      if (inb (Status_Port) & 0x40)
         value |= 1;
    }

    /* store to buffer
     */
    checksum += value;
    *buf = value;
    Select_Device (Clear_All);
    Output_Data_With_Clock (DI_Low);
  }
  outb (Data_Port, 0xff);

  return (checksum & 0xffff);
}

/*
 * function: ram_test
 * params:   none
 * returns:  zero on success, nonzero on failure
 * purpose:  Test the 8390's RAM.
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
static int ram_test (void)
{
  BYTE test = 0x55;
  int  count, recv;
  BYTE cmd;

  if (ethpk_debug > 4)
     printk ("ram_test in progress\n");

  Set_Register_Value (ACC0_DCFG, ACCDCFG_BM8);
  Enable_Remote_Dma (TXPAGE_8p << 8, RAMSIZE_8p, ACCC_START + ACCC_RDMAWR);
  Select_Device (FMData);
  Delay_a_Little_Time();
  cmd = INIT;

  /* write the Data_Port to the adapter
   */
  for (count = 0; count != RAMSIZE_8p; count++)
  {
    cmd ^= ATFD;
    Wait_ACK_Signal_High();
    outb (test, Data_Port);
    outb (cmd, Control_Port);
    Delay_a_Little_Time();
  }
  Close_Buffer_Channel (cmd);

  /* now read the Data_Port back
   */
  Enable_Remote_Dma (TXPAGE_8p << 8, RAMSIZE_8p, ACCC_START + ACCC_RDMARD);
  Select_Device (FMData);
  cmd = 0x0e0 | INIT_STROBE;
  outb (0x0ff, Data_Port);
  Delay_a_Little_Time();
  outb (cmd, Control_Port);
  Delay_a_Little_Time();

  for (count = 0; count != RAMSIZE_8p; count++)
  {
    cmd ^= ATFD;
    Wait_ACK_Signal_High();
    recv = inb (Data_Port);
    outb (cmd, Control_Port);
    if (recv != test)
    {
      /* ram_test failed, bummer !
       */
      Close_Buffer_Channel (cmd);
      printk ("\nram_test error:recv %04x should be %04x, count %d.\n",
              recv, test, count);
      return (0);
    }
    if (ethpk_debug > 4)
      if (!(count % 256))
         printk (".");
  }
  Close_Buffer_Channel (cmd);
  if (ethpk_debug > 4)
     printk ("\nRam_test succeeded\n");

  return (1);
}

#ifdef POLLING
/*
 * function: poll_interrupt
 * params:   int irq : ignored
 * returns:  nothing
 * purpose:  Reaction to timer interrupt: execute our basic interrupt routine
 *           (thus faking the interrupt we might possibly have missed!!!)
 *           and restart timer.
 *           On the first call, the timer is just started.
 * created:  11.11.94 RS
 * globals:  ethpk_dev
 */
static void poll_interrupt (int irq)
{
  static int first_time LOCKED_VAR = 1;
  struct ei_device *ei_local;

  if (ethpk_debug > 3)
     printk ("Called from timer\n");
  if (first_time)
  {
    first_time = 0;
    if (ethpk_debug > 3)
       printk ("first time called, doing nothing\n");
  }
  else
  {
    ei_local = (struct ei_device*) ethpk_dev->priv;
    if (ethpk_dev->interrupt || ei_local->irqlock)
    {
      if (ethpk_debug > 3)
      {
	printk ("Will not re-enter\n");
	return;
      }
    }
    else
    {
      ei_interrupt (ethpk_irq);
      en_prn_int();
    }
  }
  ethpk_poll.expires = ethpk_poll_interval;
  add_timer (&ethpk_poll);
}

/*
 * function: no_interrupt
 * params:   int reg_ptr
 * returns:  nothing
 * purpose:  Reaction to usual interrupt: execute our basic interrupt routine.
 * created:  11.11.94 RS
 */
static void no_interrupt (int irq)
{
  struct device    *dev = irq2dev_map[irq];
  struct ei_device *ei_local;

  if (ethpk_debug > 3)
     printk ("no_interrupt, because we're polling");

  if (!dev)
  {
    /* This happens at least once during request_irq().
     */
    if (ethpk_debug > 1)
       printk ("no_interrupt(): irq %d for NULL device.\n", irq);
    return;
  }
  ei_local = (struct ei_device *) dev->priv;

  if (dev->interrupt || ei_local->irqlock)
  {
    if (ethpk_debug > 3)
    {
      printk ("Will not re-enter\n");
      sti();
    }
  }
  else
  {
    ei_interrupt (ethpk_irq);
    en_prn_int();
  }
}
#endif

/*
 * function: ethpk_open
 * params:   struct device *dev : the device
 * returns:  Zero (?) on success,
 *           Nonzero on error.
 * purpose:  Open and initialize the ethpk device.
 * created:  11.11.94 RS
 */
static int ethpk_open (struct device *dev)
{
  struct ei_device *ei_local = (struct ei_device*) dev->priv;

  irq2dev_map[dev->irq] = dev;
#ifdef POLLING
  if (!request_irq (dev->irq, no_interrupt))
#else
  if (!request_irq (dev->irq, ei_interrupt))
#endif
  {
    printk ("%s: unable to get IRQ %d\n", dev->name, dev->irq);
    irq2dev_map[dev->irq] = NULL;
    return (0);
  }

  ResetAdapter (dev);
  init_accton (dev);
  en_prn_int();                /* It seems that this is always called
                                * twice, in init_accton and after init_accton
                                * at least it looks that way in pepktdrv.asm
                                */
  if (ethpk_debug > 4)
     printk ("open      %d\n"
             "word16    %d\n"
             "dmaing    %d\n"
             "irqlock   %d\n"
             "pingpong  %d\n"
             "tx_start_page  %#04x\n"
             "rx_start_page  %#04x\n"
             "stop_page      %#04x\n"
             "current_page   %#04x\n"
             "interface_num  %#04x\n"
             "txqueue        %#04x\n"
             "in_interrupt   %#04x\n",
             ei_local->open,          ei_local->word16,
             ei_local->dmaing,        ei_local->irqlock,
             ei_local->pingpong,      ei_local->tx_start_page,
             ei_local->rx_start_page, ei_local->stop_page,
             ei_local->current_page,  ei_local->interface_num,
             ei_local->txqueue,       ei_local->in_interrupt);

#ifdef POLLING
  ethpk_poll.function = poll_interrupt;
  ethpk_poll.expires  = jiffies + 1000;
  ethpk_poll.data     = 0;
  add_timer (&ethpk_poll);
  if (ethpk_debug > 3)
      printk ("Polling initialized\n");
#else
  ethpk_debug = 0;
#endif
  return (1);
}

static void ethpk_close (struct device *dev)
{
#ifdef POLLING
  /* When we are polling we must immediatly reset the timer, to avoid a
   * kernel panic, I learned the hard way.
   */
  del_timer (&ethpk_poll);
#endif
  dev->tx_busy = 1;
  dev->start   = 0;

  /* Flush the Tx and disable Rx here.
   *
   * If not IRQ or DMA jumpered, free up the line.
   */
#if 0
  outw (0x00, ioaddr+0);   /* Release the physical interrupt line. */
#endif

  free_irq (dev->irq);
  irq2dev_map[dev->irq] = NULL;

  /* Update the statistics here. */
}

void ethpk_block_output (struct device *dev, int count, const char *buf, int startpage)
{
  if (ethpk_debug > 4)
     printk ("block_output..");

  Enable_Remote_Dma (startpage << 8, count, ACCC_START + ACCC_RDMAWR);
  Write_Data_To_Buffer (buf, count);

  if (ethpk_debug > 2)   /* DMA termination address check... */
  {       
    int addr = Read_Register_Value (EN0_RSARHI) << 8 |
                                    Read_Register_Value (EN0_RSARLO);

    /* Check only the lower 8 bits so we can ignore ring wrap.
     */
    if ((startpage << 8) + count != addr)
       printk ("%s: TX address mismatch, %04x vs. %04x (actual).\n",
               dev->name, (startpage << 8) + count, addr);
  }
  if (ethpk_debug > 4)
     printk ("block_output done\n");
}

/*
 * Shifting has been removed here, 'cause the ei_receive function in
 * ethpknic.c does it.
 */
int ethpk_block_input (struct device *dev, int count, char *buf, int ring_offset)
{
  if (ethpk_debug > 4)
     printk ("ethpk_block_input..");

  Enable_Remote_Dma (ring_offset, count, ACCC_START + ACCC_RDMARD);
  Read_Data_From_Buffer (buf, count);

  if (ethpk_debug > 2)  /* DMA termination address check... */
  {    
    int addr = Read_Register_Value (EN0_RSARHI) << 8 | Read_Register_Value (EN0_RSARLO);

    /* Check only the lower 8 bits so we can ignore ring wrap.
     */
    if ((ring_offset) + count != addr)
       printk ("%s: RX address mismatch, %04x vs. %04x (actual).\n",
               dev->name, (ring_offset << 8) + count, addr);
  }
  if (ethpk_debug > 4)
     printk ("ethpk_block_input done\n");
  return (1);
}

/*
 * function: init_accton
 * params:   struct device *dev : our device
 * returns:  nothing
 * purpose:  Initialize the chip and start it up.
 * created:  11.11.94 RS (from a macro with same name in the original source)
 */
void init_accton (struct device *dev)
{
  int i;

  if (ethpk_debug > 2)
     printk ("init_accton..");

  Select_Device (StartReset);
  Delay_a_Little_Time();
  Select_Device (StopReset);
  Delay_a_Little_Time();

  Set_Register_Value (ACC_CCMD, ACCC_NODMA + ACCC_PAGE0);
  Set_Register_Value (ACC0_ISR, 0xff);                    /* clear the interrupt status */
  Set_Register_Value (ACC_CCMD, ACCC_NODMA + ACCC_PAGE1); /* switch to Page 1 */

  /* set the physical address in page 1
   */
  for (i = 0; i < ETH_ALEN; i++)
    Set_Register_Value (ACC1_PHYS + i, dev->dev_addr[i]);

  /* apparently multicasting is out of the question, the registers
   * are reset to zero's
    */
  for (i = 0; i != 8; i++)
    Set_Register_Value (ACC1_MULT + i, 0xff);

  /* back to page 0
   */
  Set_Register_Value (ACC_CCMD, ACCC_NODMA + ACCC_PAGE0);
  Set_Register_Value (ACC0_DCFG, ACCDCFG_BM8);	/* set FIFO to 8 bytes */

  /* clear the count registers
   */
  Set_Register_Value (ACC0_RCNTLO, 0x00);
  Set_Register_Value (ACC0_RCNTHI, 0x00);
  Set_Register_Value (ACC0_RXCR, ACCRXCR_MON); /* receiver to monitor mode */
  Set_Register_Value (ACC0_TXCR, 0x00);        /* tranmitter mode to normal */

  /* set up shared memory, buffer ring etc.
   */
  Set_Register_Value (ACC0_STARTPG, PSTART_8p);
  Set_Register_Value (ACC0_STOPPG, PSTOP_8p);
  Set_Register_Value (ACC0_BOUNDPTR, PSTART_8p);
  Set_Register_Value (ACC_CCMD, ACCC_NODMA + ACCC_PAGE1);
  Set_Register_Value (ACC1_CURPAG, PSTART_8p + 1); /* current shared page for
                                                    * for RX to work on
                                                    */
  Set_Register_Value (ACC_CCMD, ACCC_NODMA + ACCC_PAGE0);
  Set_Register_Value (ACC0_IMR, 0x00);	/* clear all interrupts enable flags */
  Set_Register_Value (ACC0_ISR, 0xff);

  /* fire up the DS8390
   */
  Set_Register_Value (ACC_CCMD, ACCC_START + ACCC_NODMA);
  Set_Register_Value (ACC0_RXCR, ACCRXCR_BCST);	/* tell it to accept 
                                                 * broadcasts
                                                 */
  Set_Register_Value (ACC0_IMR, 0x1f);	/* tell the card it can cause
                                         * these interrupts
                                         */
  if (ethpk_debug > 2)
     printk ("init_accton done\n");
}

#ifdef ETHPK_DEBUG
struct {
    int  code;
    char meaning[40];
  } isr_codes[] = {
    { 0,              "Nothing\n"                     },
    { ENISR_RX,       "Receiver, no error\n"          },
    { ENISR_TX,       "Transmitter, no error\n"       },
    { ENISR_RX,       "Receiver, with error\n"        },
    { ENISR_TX,       "Transmitter, with error\n"     },
    { ENISR_OVER,     "Receiver overwrote the ring\n" },
    { ENISR_COUNTERS, "Counters need emptying\n"      },
    { ENISR_RDC,      "Remote DMA complete\n"         },
    { ENISR_RESET,    "Reset completed\n"             },
    { ENISR_ALL,      "Interrupts we will enable\n"   }
  };

struct {
    int  code;
    char meaning[40];
  } tsr_codes[] = {
    { 0,               "Nothing"                           },
    { ACCTSR_TXOK,     "Transmit without error"            },
    { ACCTSR_COLL,     "Collided at least once"            },
    { ACCTSR_COLL16,   "Collided 16 times and was dropped" },
    { ACCTSR_FIFOURUN, "TX FIFO underrun"                  }
  };

void analyze_isr (void)
{
  int  i;
  BYTE rc = Read_Register_Value (EN0_ISR);

  for (i = 0; i < DIM(isr_codes); i++)
      if (rc & isr_codes[i].code)
         printk ("ISR [%04x] %s", rc, isr_codes[i].meaning);
}     

void analyze_tsr (void)
{
  int  i;
  BYTE rc = Read_Register_Value (EN0_TSR);

  for (i = 0; i < DIM(tsr_codes); i++)
      if (rc & tsr_codes[i].code)
         printk ("TSR [%04x] %s\n", rc, tsr_codes[i].meaning);
}
#endif
