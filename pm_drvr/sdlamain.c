/*****************************************************************************
* sdlamain.c  WANPIPE(tm) Multiprotocol WAN Link Driver.  Main module.
*
* Author:  Gene Kozin  <genek@compuserve.com>
*
* Copyright:  (c) 1995-1997 Sangoma Technologies Inc.
*
*    This program is free software; you can redistribute it and/or
*    modify it under the terms of the GNU General Public License
*    as published by the Free Software Foundation; either version
*    2 of the License, or (at your option) any later version.
* ============================================================================
* Jan 02, 1997  Gene Kozin  Initial version.
*****************************************************************************/

#if  !defined(__KERNEL__) || !defined(MODULE)
#error  This code MUST be compiled as a kernel module!
#endif

#include <linux/config.h>	/* OS configuration options */
#include <linux/stddef.h>	/* offsetof(), etc. */
#include <linux/errno.h>	/* return codes */
#include <linux/string.h>	/* inline memset(), etc. */
#include <linux/malloc.h>	/* kmalloc(), kfree() */
#include <linux/kernel.h>	/* printk(), and other useful stuff */
#include <linux/module.h>	/* support for loadable modules */
#include <linux/ioport.h>	/* request_region(), release_region() */
#include <linux/tqueue.h>	/* for kernel task queues */
#include <linux/router.h>	/* WAN router definitions */
#include <linux/wanpipe.h>	/* WANPIPE common user API definitions */
#include <asm/segment.h>	/* kernel <-> user copy */

/****** Defines & Macros ****************************************************/

#ifdef  _DEBUG_
#define  STATIC
#else
#define  STATIC    static
#endif

#define  DRV_VERSION  3		/* version number */
#define  DRV_RELEASE  0		/* release (minor version) number */
#define  MAX_CARDS  8		/* max number of adapters */

#ifndef  CONFIG_WANPIPE_CARDS	/* configurable option */
#define  CONFIG_WANPIPE_CARDS 1
#endif

#define  CMD_OK    0		/* normal firmware return code */
#define  CMD_TIMEOUT  0xFF	/* firmware command timed out */
#define  MAX_CMD_RETRY  10	/* max number of firmware retries */

/****** Function Prototypes *************************************************/

/* Module entry points */
int init_module (void);
void cleanup_module (void);

/* WAN link driver entry points */
static int setup (wan_device_t * wandev, wandev_conf_t * conf);
static int shutdown (wan_device_t * wandev);
static int ioctl (wan_device_t * wandev, unsigned cmd, DWORD arg);

/* IOCTL hanlers */
static int ioctl_dump (sdla_t * card, sdla_dump_t * u_dump);
static int ioctl_exec (sdla_t * card, sdla_exec_t * u_exec);

/* Miscellaneous functions */
STATIC void sdla_isr (int irq, void *dev_id, struct pt_regs *regs);
STATIC void sdla_poll (void *data);

/****** Global Data **********************************************************
 * Note: All data must be explicitly initialized!!!
 */

/* private data */
static char drvname[] = "wanpipe";
static char fullname[] = "WANPIPE(tm) Multiprotocol Driver";
static char copyright[] = "(c) 1995-1996 Sangoma Technologies Inc.";
static int ncards = CONFIG_WANPIPE_CARDS;
static int active = 0;		/* number of active cards */
static sdla_t *card_array = NULL;	/* adapter data space */

/* Task queue element for creating a 'thread' */
static struct tq_struct sdla_tq = {
  NULL,				/* .next */
  0,				/* .sync */
  &sdla_poll,			/* .routine */
  NULL				/* .data */
};

/******* Kernel Loadable Module Entry Points ********************************/

/*============================================================================
 * Module 'insert' entry point.
 * o print announcement
 * o allocate adapter data space
 * o initialize static data
 * o register all cards with WAN router
 * o calibrate SDLA shared memory access delay.
 *
 * Return:  0  Ok
 *    < 0  error.
 * Context:  process
 */
int init_module (void)
{
  int cnt, err = 0;

  printk (KERN_INFO "%s v%u.%u %s\n", fullname, DRV_VERSION, DRV_RELEASE, copyright);

  /* Verify number of cards and allocate adapter data space */
  ncards = min (ncards, MAX_CARDS);
  ncards = max (ncards, 1);
  card_array = kmalloc (sizeof (sdla_t) * ncards, GFP_KERNEL);
  if (card_array == NULL)
    return -ENOMEM;
  memset (card_array, 0, sizeof (sdla_t) * ncards);

  /* Register adapters with WAN router */
  for (cnt = 0; cnt < ncards; ++cnt)
  {
    sdla_t *card = &card_array[cnt];
    wan_device_t *wandev = &card->wandev;

    sprintf (card->devname, "%s%d", drvname, cnt + 1);
    wandev->magic = ROUTER_MAGIC;
    wandev->name = card->devname;
    wandev->private = card;
    wandev->setup = &setup;
    wandev->shutdown = &shutdown;
    wandev->ioctl = &ioctl;
    err = register_wandev (wandev);
    if (err)
    {
      printk (KERN_ERR "%s: %s registration failed with error %d!\n", drvname, card->devname, err);
      break;
    }
  }
  if (cnt)
    ncards = cnt;		/* adjust actual number of cards */
  else
    kfree (card_array);
  return err;
}

/*============================================================================
 * Module 'remove' entry point.
 * o unregister all adapters from the WAN router
 * o release all remaining system resources
 */
void cleanup_module (void)
{
  int i;

  for (i = 0; i < ncards; ++i)
  {
    sdla_t *card = &card_array[i];

    unregister_wandev (card->devname);
  }
  kfree (card_array);
}

/******* WAN Device Driver Entry Points *************************************/

/*============================================================================
 * Setup/confugure WAN link driver.
 * o check adapter state
 * o make sure firmware is present in configuration
 * o make sure I/O port and IRQ are specified
 * o make sure I/O region is available
 * o allocate interrupt vector
 * o setup SDLA hardware
 * o call appropriate routine to perform protocol-specific initialization
 * o mark I/O region as used
 * o if this is the first active card, then schedule background task
 *
 * This function is called when router handles ROUTER_SETUP IOCTL. The
 * configuration structure is in kernel memory (including extended data, if
 * any).
 */
static int setup (wan_device_t * wandev, wandev_conf_t * conf)
{
  sdla_t *card;
  int err = 0;
  int irq;

  /* Sanity checks */
  if ((wandev == NULL) || (wandev->private == NULL) || (conf == NULL))
    return -EFAULT;
  card = wandev->private;
  if (wandev->state != WAN_UNCONFIGURED)
    return -EBUSY		/* already configured */
      ;
  if (!conf->data_size || (conf->data == NULL))
  {
    printk (KERN_ERR "%s: firmware not found in configuration data!\n", wandev->name);
    return -EINVAL;
  }
  if (conf->ioport <= 0)
  {
    printk (KERN_ERR "%s: can't configure without I/O port address!\n", wandev->name);
    return -EINVAL;
  }
  if (conf->irq <= 0)
  {
    printk (KERN_ERR "%s: can't configure without IRQ!\n", wandev->name);
    return -EINVAL;
  }

  /* Make sure I/O port region is available */
  if (check_region (conf->ioport, SDLA_MAXIORANGE))
  {
    printk (KERN_ERR "%s: I/O region 0x%X - 0x%X is in use!\n",
	    wandev->name, conf->ioport, conf->ioport + SDLA_MAXIORANGE);
    return -EINVAL;
  }

  /* Allocate IRQ */
  irq = (conf->irq == 2) ? 9 : conf->irq;	/* IRQ2 -> IRQ9 */
  if (request_irq (irq, sdla_isr, 0, wandev->name, card))
  {
    printk (KERN_ERR "%s: can't reserve IRQ %d!\n", wandev->name, irq);
    return -EINVAL;
  }

  /* Configure hardware, load firmware, etc. */
  memset (&card->hw, 0, sizeof (sdlahw_t));
  card->hw.port = conf->ioport;
  card->hw.irq = (conf->irq == 9) ? 2 : conf->irq;
  card->hw.dpmbase = conf->maddr;
  card->hw.dpmsize = SDLA_WINDOWSIZE;
  card->hw.type = conf->hw_opt[0];
  card->hw.pclk = conf->hw_opt[1];
  err = sdla_setup (&card->hw, conf->data, conf->data_size);
  if (err)
  {
    free_irq (irq, card);
    return err;
  }

  /* Intialize WAN device data space */
  wandev->irq = irq;
  wandev->dma = 0;
  wandev->ioport = card->hw.port;
  wandev->maddr = card->hw.dpmbase;
  wandev->msize = card->hw.dpmsize;
  wandev->hw_opt[0] = card->hw.type;
  wandev->hw_opt[1] = card->hw.pclk;
  wandev->hw_opt[2] = card->hw.memory;
  wandev->hw_opt[3] = card->hw.fwid;

  /* Protocol-specific initialization */
  switch (card->hw.fwid)
  {
#ifdef  CONFIG_WANPIPE_X25
       case SFID_X25_502:
       case SFID_X25_508:
	 err = wpx_init (card, conf);
	 break;
#endif

#ifdef  CONFIG_WANPIPE_FR
       case SFID_FR502:
       case SFID_FR508:
	 err = wpf_init (card, conf);
	 break;
#endif

#ifdef  CONFIG_WANPIPE_PPP
       case SFID_PPP502:
       case SFID_PPP508:
	 err = wpp_init (card, conf);
	 break;
#endif

       default:
	 printk (KERN_ERR "%s: this firmware is not supported!\n", wandev->name);
	 err = -EINVAL;
  }
  if (err)
  {
    sdla_down (&card->hw);
    free_irq (irq, card);
    return err;
  }
  /* Reserve I/O region and schedule background task */
  request_region (card->hw.port, card->hw.io_range, wandev->name);
  if (++active == 1)
    queue_task (&sdla_tq, &tq_scheduler);
  wandev->critical = 0;
  return 0;
}

/*============================================================================
 * Shut down WAN link driver. 
 * o shut down adapter hardware
 * o release system resources.
 *
 * This function is called by the router when device is being unregistered or
 * when it handles ROUTER_DOWN IOCTL.
 */
static int shutdown (wan_device_t * wandev)
{
  sdla_t *card;

  /* sanity checks */
  if ((wandev == NULL) || (wandev->private == NULL))
    return -EFAULT;
  if (wandev->state == WAN_UNCONFIGURED)
    return 0;
  if (set_bit (0, (void *) &wandev->critical))
    return -EAGAIN;
  card = wandev->private;
  wandev->state = WAN_UNCONFIGURED;
  if (--active == 0)
    schedule ();		/* stop background thread */
  sdla_down (&card->hw);
  free_irq (wandev->irq, card);
  release_region (card->hw.port, card->hw.io_range);
  wandev->critical = 0;
  return 0;
}

/*============================================================================
 * Driver I/O control. 
 * o verify arguments
 * o perform requested action
 *
 * This function is called when router handles one of the reserved user
 * IOCTLs.  Note that 'arg' stil points to user address space.
 */
static int ioctl (wan_device_t * wandev, unsigned cmd, DWORD arg)
{
  int err;

  /* sanity checks */
  if ((wandev == NULL) || (wandev->private == NULL))
    return -EFAULT;
  if (wandev->state == WAN_UNCONFIGURED)
    return -ENODEV;
  if (set_bit (0, (void *) &wandev->critical))
    return -EAGAIN;
  switch (cmd)
  {
       case WANPIPE_DUMP:
	 err = ioctl_dump (wandev->private, (void *) arg);
	 break;

       case WANPIPE_EXEC:
	 err = ioctl_exec (wandev->private, (void *) arg);
	 break;

       default:
	 err = -EINVAL;
  }
  wandev->critical = 0;
  return err;
}

/****** Driver IOCTL Hanlers ************************************************/

/*============================================================================
 * Dump adpater memory to user buffer.
 * o verify request structure
 * o copy request structure to kernel data space
 * o verify length/offset
 * o verify user buffer
 * o copy adapter memory image to user buffer
 *
 * Note: when dumping memory, this routine switches curent dual-port memory
 *   vector, so care must be taken to avoid racing conditions.
 */
static int ioctl_dump (sdla_t * card, sdla_dump_t * u_dump)
{
  sdla_dump_t dump;
  unsigned winsize;
  DWORD oldvec;			/* DPM window vector */
  int err = 0;

  if ((u_dump == NULL) || verify_area (VERIFY_READ, u_dump, sizeof (sdla_dump_t)))
    return -EFAULT;
  memcpy_fromfs ((void *) &dump, (void *) u_dump, sizeof (sdla_dump_t));
  if ((dump.magic != WANPIPE_MAGIC) || (dump.offset + dump.length > card->hw.memory))
    return -EINVAL;
  if ((dump.ptr == NULL) || verify_area (VERIFY_WRITE, dump.ptr, dump.length))
    return -EFAULT;

  winsize = card->hw.dpmsize;
  cli ();			/* >>> critical section start <<< */
  oldvec = card->hw.vector;
  while (dump.length)
  {
    unsigned pos = dump.offset % winsize;	/* current offset */
    DWORD vec = dump.offset - pos;	/* current vector */
    unsigned len = (dump.length > (winsize - pos)) ? (winsize - pos) : dump.length;

    if (sdla_mapmem (&card->hw, vec) != 0)	/* relocate window */
    {
      err = -EIO;
      break;
    }
    memcpy_tofs ((void *) (dump.ptr), (void *) (card->hw.dpmbase + pos), len);
    dump.length -= len;
    dump.offset += len;
    (char *) dump.ptr += len;
  }
  sdla_mapmem (&card->hw, oldvec);	/* restore DPM window position */
  sti ();			/* >>> critical section end <<< */
  return err;
}

/*============================================================================
 * Execute adapter firmware command.
 * o verify request structure
 * o copy request structure to kernel data space
 * o call protocol-specific 'exec' function
 */
static int ioctl_exec (sdla_t * card, sdla_exec_t * u_exec)
{
  sdla_exec_t exec;

  if (card->exec == NULL)
    return -ENODEV;
  if ((u_exec == NULL) || verify_area (VERIFY_READ, u_exec, sizeof (sdla_exec_t)))
    return -EFAULT;
  memcpy_fromfs ((void *) &exec, (void *) u_exec, sizeof (sdla_exec_t));
  if ((exec.magic != WANPIPE_MAGIC) || (exec.cmd == NULL))
    return -EINVAL;
  return card->exec (card, exec.cmd, exec.data);
}

/******* Miscellaneous ******************************************************/

/*============================================================================
 * SDLA Interrupt Service Routine.
 * o acknowledge SDLA hardware interrupt.
 * o call protocol-specific interrupt service routine, if any.
 */
STATIC void sdla_isr (int irq, void *dev_id, struct pt_regs *regs)
{
#define  card  ((sdla_t*)dev_id)

  if (!card || (card->wandev.state == WAN_UNCONFIGURED))
    return;
  if (card->in_isr)
  {
    printk (KERN_WARNING "%s: interrupt re-entrancy on IRQ %d!\n", card->devname, card->wandev.irq);
    return;
  }
  card->in_isr = 1;
  sdla_intack (&card->hw);
  if (card->isr)
    card->isr (card);
  card->in_isr = 0;

#undef  card
}

/*============================================================================
 * SDLA polling routine.
 * This routine simulates kernel thread to perform various housekeeping job.
 *
 * o for each configured device call its poll() routine
 * o if there is at least one active card, then reschedule itself once again
 */
STATIC void sdla_poll (void *data)
{
  int i;

  for (i = 0; i < ncards; ++i)
  {
    sdla_t *card = &card_array[i];

    if ((card->wandev.state != WAN_UNCONFIGURED) && card->poll && !set_bit (0, (void *) &card->wandev.critical))
    {
      card->poll (card);
      card->wandev.critical = 0;
    }
  }
  if (active)
    queue_task (&sdla_tq, &tq_scheduler);
}

/*============================================================================
 * This routine is called by the protocol-specific modules when network
 * interface is being open.  The only reason we need this, is because we
 * have to call MOD_INC_USE_COUNT, but cannot include 'module.h' where it's
 * defined more than once into the same kernel module.
 */
void wanpipe_open (sdla_t * card)
{
  ++card->open_cnt;
  MOD_INC_USE_COUNT;
}

/*============================================================================
 * This routine is called by the protocol-specific modules when network
 * interface is being closed.  The only reason we need this, is because we
 * have to call MOD_DEC_USE_COUNT, but cannot include 'module.h' where it's
 * defined more than once into the same kernel module.
 */
void wanpipe_close (sdla_t * card)
{
  --card->open_cnt;
  MOD_DEC_USE_COUNT;
}

/*============================================================================
 * Set WAN device state.
 */
void wanpipe_set_state (sdla_t * card, int state)
{
  DWORD flags;

  save_flags (flags);
  cli ();
  if (card->wandev.state != state)
  {
    switch (state)
    {
	 case WAN_CONNECTED:
	   printk (KERN_INFO "%s: link connected!\n", card->devname);
	   break;

	 case WAN_CONNECTING:
	   printk (KERN_INFO "%s: link connecting...\n", card->devname);
	   break;

	 case WAN_DISCONNECTED:
	   printk (KERN_INFO "%s: link disconnected!\n", card->devname);
	   break;
    }
    card->wandev.state = state;
  }
  card->state_tick = jiffies;
  restore_flags (flags);
}

/****** End *****************************************************************/
