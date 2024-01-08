/*
 * auto_irq.c: Auto-configure IRQ lines
 *
 *  Written 1994 by Donald Becker.
 *
 *  The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
 *  Center of Excellence in Space Data and Information Sciences
 *    Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
 *
 *  This code is a general-purpose IRQ line detector for devices with
 *  jumpered IRQ lines.  If you can make the device raise an IRQ (and
 *  that IRQ line isn't already being used), these routines will tell
 *  you what IRQ line it's using -- perfect for those oh-so-cool boot-time
 *  device probes!
 *
 *  To use this, first call autoirq_setup(timeout). TIMEOUT is how many
 *  micro-seconds to detect other devices that have active IRQ lines,
 *  and can usually be zero at boot.  'autoirq_setup()' returns the bit
 *  vector of nominally-available IRQ lines (lines may be physically in-use,
 *  but not yet registered to a device).
 *  Next, set up your device to trigger an interrupt.
 *  Finally call autoirq_report(TIMEOUT) to find out which IRQ line was
 *  most recently active.  The TIMEOUT should usually be zero, but may
 *  be set to the number of usec to wait for a slow device to raise an IRQ.
 *
 *  The idea of using the setup timeout to filter out bogus IRQs came from
 *  the serial driver.
 *
 *  Simplified and adapted for djgpp by G. Vanem <gvanem@eunet.no> 1999
 */

#include "pmdrvr.h"
#include "module.h"

/*
 * IRQ handling partially based on:
 *
 * Hardware interrupt handler library
 *
 * HWINT.C - Simplified, robust, hardware interrupt handling in DJGPP
 * (C) Alaric B. Williams (alaric@abwillms.demon.co.uk) 1996
 * Use this for whatever you want, as long as all due credit is given.
 * Modified versions should be marked as such if they are to be distributed.
 * Have fun.
 */

#define PRINTK(x) do {             \
                    if (irq_debug) \
                       printk x ;  \
                  } while (0)

#define IRQ_STUB_STACK_SIZE (32*1024)

int   irq_debug = 0;
DWORD irq_stub_stacks [NUM_IRQS] LOCKED_VAR = { 0 };

void irq_umbrella_handler (int irq) LOCKED_FUNC;

struct irq_info {
       void              (*new_handler)(int);
       _go32_dpmi_seginfo  old_handler;
       _go32_dpmi_seginfo  wrapper;
       int                 used;
       int                 eoi_done;
       BYTE                old_mask;
     };

static struct  irq_info irq_cfg [NUM_IRQS] LOCKED_VAR;
struct device *irq2dev_map      [NUM_IRQS] LOCKED_VAR;

static          DWORD irq_handled; /* The irq lines we have a handler for. */
static volatile DWORD irq_bitmap;  /* The irqs we actually found. */
static volatile DWORD irq_active;  /* irq umbrella handler nesting level */
static volatile int   irq_number;  /* The latest irq number we actually found */
static unsigned int   fpu_state[200];
static int            have_fpu;

static void (*irq_stubs [NUM_IRQS]) (void) = {
              pcap_irq_stub_0,  pcap_irq_stub_1,
              pcap_irq_stub_2,  pcap_irq_stub_3,
              pcap_irq_stub_4,  pcap_irq_stub_5,
              pcap_irq_stub_6,  pcap_irq_stub_7,
              pcap_irq_stub_8,  pcap_irq_stub_9,
              pcap_irq_stub_10, pcap_irq_stub_11,
              pcap_irq_stub_12, pcap_irq_stub_13,
              pcap_irq_stub_14, pcap_irq_stub_15
            };

#if 1
  #define EOI_CMD(line) (0x60|(line))  /* specific EOI command (line 0-7) */
#else
  #define EOI_CMD(line) (0x20)         /* non-specific EOI command */
#endif

int irq_eoi_cmd (int irq)
{
  if (irq == 2)             /* don't touch IRQ cascade line */
     return (0);

  if (irq >= 8)
  {
    _outportb (0xA0, EOI_CMD(irq-8));  /* EOI to slave PIC */
    _outportb (0x20, 0x20);            /* non-specific EOI to master */
  }
  else
     _outportb (0x20, EOI_CMD(irq));   /* EOI to master PIC */

  irq_cfg[irq].eoi_done = 1;
  return (1);
}

/*
 * Interrupt base for master/slave PICs. No way these changes between
 * request_irq() and free_irq(). Otherwise we're toast..
 */
static __inline int irq_intr (int irq)
{
#if !defined(_MODULE)
  if (irq >= 8)
     return (irq - 8 + _go32_info_block.slave_interrupt_controller_base);
  return (irq + _go32_info_block.master_interrupt_controller_base);
#else
  if (irq >= 8)
     return (irq - 8 + 0x68);
  return (irq + 8);
#endif
}

/*
 * Re-arming 8259 interrupt controller(s) should be called just after
 * entering IRQ-handler, instead of just before returning. This is because
 * the 8259 inputs are edge triggered (except possibly if having an APIC),
 * and new interrupts arriving during an IRQ-handler might be missed.
 */
#if 0
static __inline void pic_rearm (int irq)
{
  if (irq >= 8)
  {
    _outportb (0xA0, 0x0B);
    NOP();
    NOP();
    NOP();
    NOP();
    if (_inportb(0xA0))
       _outportb (0xA0, 0x20);  /* non-specific EOI */
  }
  _outportb (0x20, 0x20);
}
#endif

static void autoirq_probe (int irq)
{
  irq_number = irq;
  irq_bitmap |= 1 << irq;
  disable_irq (irq);
}

int autoirq_setup (int usec)
{
  int  irq, mask;
  long timeout;

  irq_handled = 0;
  for (irq = 0; irq < DIM(irq2dev_map); irq++)
  {
    if (request_irq(irq, autoirq_probe))
       irq_handled |= 1 << irq;
  }
  irq_number = 0;
  irq_bitmap = 0;

  timeout = jiffies + usec;

  /* Wait at least 'usec' for bogus IRQ hits
   */
  while (jiffies < timeout)
        ;

  for (irq = 0, mask = 1; irq < DIM(irq2dev_map); irq++, mask <<= 1)
  {
    if ((irq_bitmap & irq_handled) & mask)
    {
      irq_handled &= ~mask;
      free_irq (irq);
    }
  }
  return (irq_handled);
}

int autoirq_report (int usec)
{
  int  irq;
  long timeout = jiffies + usec;

  /* Hang out at least x uSec waiting for the IRQ
   */
  while (jiffies < timeout)
        if (irq_number)
           break;

  /* Retract the irq handlers that we installed
   */
  for (irq = 0; irq < DIM(irq2dev_map); irq++)
  {
    if (irq_handled & (1 << irq))
       free_irq (irq);
  }
  return (irq_number);
}

/*
 * Install High-level interrupt handler for IRQ
 */
int request_irq (int i, void (*handler)(int))
{
  struct irq_info *irq = irq_cfg + i;
  BYTE   port;
  int    vector;
  void  *stack = NULL;
  _go32_dpmi_seginfo *si;
  __dpmi_version_ret  dinfo;

  memset (&dinfo, 0, sizeof(dinfo));
  __dpmi_get_version (&dinfo);
  have_fpu = (dinfo.cpu >= 4);

  if (i < 0 || i >= DIM(irq_cfg))
  {
    PRINTK (("irq: illegal IRQ%d\n", i));
    return (0);
  }

  if (irq->used)
  {
    PRINTK (("irq: IRQ%d already in use\n", i));
    return (0);
  }

#ifdef USE_SECTION_LOCKING
  if (lock_sections())
     PRINTK (("irq: locking failed\n"));
#endif

  vector = irq_intr (i);   /* IRQ 0 -> int 8, IRQ 8 -> int 70h */
  DISABLE();

  port = i < 8 ? 0x21 : 0xA1;
  irq->old_mask = _inportb (port);

  disable_irq (i);

  if (!handler)      /* this is used to pulse the 8259 IEN bit */
  {
    enable_irq (i);
    ENABLE();
    return (1);
  }

  stack = k_malloc (IRQ_STUB_STACK_SIZE);
  if (!stack)
  {
    PRINTK (("irq: no memory\n"));
    goto failed;
  }
  irq_stub_stacks [i] = (DWORD) ((char*)stack + IRQ_STUB_STACK_SIZE - 4);

  _go32_dpmi_get_protected_mode_interrupt_vector (vector,
                                                  &irq->old_handler);
  si = &irq->wrapper;
  si->pm_selector = _my_cs();
  si->pm_offset   = (DWORD) irq_stubs [i];

  irq_our_ds = __djgpp_ds_alias;  /* setup for intwrap.s */

  if (_go32_dpmi_set_protected_mode_interrupt_vector (vector, si))
  {
    PRINTK (("irq: _go32_dpmi_set_protected_mode_interrupt_vector() failed\n"));
    goto failed;
  }

  irq->new_handler = handler;
  irq->used        = 1;

  irq_eoi_cmd (i);
  enable_irq (i);
  ENABLE();
  return (1);

failed:
  if (stack)
     k_free (stack);
  irq_stub_stacks[i] = 0UL;

  port = i < 8 ? 0x21 : 0xA1;
  _outportb (port, irq->old_mask);
  ENABLE();
  return (0);
}

/*
 * Hang out at least 5mSec waiting for the IRQ umbrella handler
 * to finish.
 */
static __inline BOOL wait_irq (void)
{
  long timeout = jiffies + 5;

  while (jiffies < timeout)
        if (irq_active == 0)
           break;

  return (irq_active == 0);
}

int free_irq (int i)
{ 
  if (i >= 0 && i < DIM(irq_cfg) && irq_cfg[i].used)
  {
    struct irq_info *irq = irq_cfg + i;
    BYTE   port;
    int    vector = irq_intr (i);
    void  *stack;

    if (!wait_irq())
       PRINTK (("irq: IRQ%d handler stuck!!\n", i));

    irq_eoi_cmd (i);
    DISABLE();
    disable_irq (i);
    irq->new_handler = NULL;
    irq->used        = 0;

    _go32_dpmi_set_protected_mode_interrupt_vector (vector,
                                                    &irq->old_handler);

    stack = (void*) ((char*)irq_stub_stacks[i] - IRQ_STUB_STACK_SIZE + 4);
    k_free (stack);
    irq_stub_stacks[i] = 0UL;
  
    port = i < 8 ? 0x21 : 0xA1;
    _outportb (port, irq->old_mask);
    ENABLE();
    return (1);
  }
  PRINTK (("irq: IRQ%d not in use\n", i));
  return (0);
}

/*
 * Enable our IRQ. The 8259 PIC uses negative logic
 */ 
void enable_irq (int i)
{
  if (i >= 0 && i < DIM(irq_cfg))
  {
    BYTE mask = (1 << (i & 7));
    BYTE port = i < 8 ? 0x21 : 0xA1;
    BYTE ctrl = _inportb (port);

    _outportb (port, ctrl & ~mask);
  }
}

/*
 * Disable our IRQ.
 */
void disable_irq (int i)
{
  if (i >= 0 && i < DIM(irq_cfg))
  {
    BYTE mask = (1 << (i & 7));
    BYTE port = i < 8 ? 0x21 : 0xA1;
    BYTE ctrl = _inportb (port);
    _outportb (port, ctrl | mask);
  }
}   

/*
 * far call (with IRETD) to a prot-mode routine
 */
static __inline void irq_simulate_intr (_go32_dpmi_seginfo *si)
{
  register DWORD selector = si->pm_selector; /* suppress warning in 'as 2.10' */

  __asm__ __volatile__ (
            "pushfl\n\t"
            "pushl %%cs\n\t"
            "pushl $next_inst\n\t"

            "pushl %0\n\t"  
            "pushl %1\n\t"
            "lret\n\t"

            "next_inst:\n\t"
            "nop\n\t"
            : /* no outputs */
            : "r" (selector), "r" (si->pm_offset)
            : "cc", "eax"
          );
}

/*
 * Call a previously installed IRQ handler
 */
void irq_chain (int i)
{
  if (i >= 0 && i < DIM(irq_cfg) && irq_cfg[i].used)
  {
    struct irq_info *irq = irq_cfg + i;
    irq_simulate_intr (&irq->old_handler);
  }
#if 0
  else
  {
    __label__ here;
    printf ("panic at %p", &&here);
  here:
  }
#endif
}

static __inline void fpu_save (void)
{
  if (have_fpu)
     __asm__ __volatile__ (
             "fnsave %0\n\t"
             "fwait\n\t"
             : "=g" (fpu_state)
           );
}

static __inline void fpu_restore (void)
{
  if (have_fpu)
     __asm__ __volatile__ (
             "frstor %0\n\t"
             "fwait\n\t"
             : "=g" (fpu_state)
           );
}

/*
 * Umbrella handler called from `pcap_irq_stub_x' in intwrap.s.
 */
void irq_umbrella_handler (int irq)
{
  volatile int safe = _printk_safe;
/*struct pt_regs *regs = (struct pt_regs*) ((DWORD*)&irq+1);  !! to-do */

  fpu_save();

  _printk_safe = 0;   /* not safe to use DOS's file I/O now */
  irq_active++;       /* increase interrupt nesting level */

  if (irq < 0 || irq >= DIM(irq_cfg))
  {
    printk ("irq: illegal IRQ%d\n", irq);
    _outportb (0x20, 0x20);
    _outportb (0xA0, 0x20);
    irq_active--;
    _printk_safe = safe;
    fpu_restore();
    return;
  }

#if 0
  /* If egde-triggered PICs, rearm the 8259 Interrupt Controller
   */
  if (edge_triggered)
     pic_rearm (irq);
#endif

  irq_cfg[irq].eoi_done = 0;

  /* Call high-level handler
   */
  if (irq_cfg[irq].new_handler)
    (*irq_cfg[irq].new_handler) (irq);

  if (!irq_cfg[irq].eoi_done)      /* handler didn't do EOI */
     irq_eoi_cmd (irq);

  irq_active--;
  _printk_safe = safe;           /* restore print-safe flag */
  fpu_restore();
}

