
#include "pmdrvr.h"
#include "module.h"


/* IRQ 0: 18.2 intr/sec, or IRQ 8: 2^n intr/sec
 */
BYTE   timer_irq             LOCKED_VAR = 0;
QWORD  jiffies               LOCKED_VAR = 0;

static int   timer_debug     LOCKED_VAR = 0;
static int   timer_active    LOCKED_VAR = 0;
static int   timer_ips       LOCKED_VAR = 18;
static QWORD timer_reentries LOCKED_VAR = 0;
static QWORD num_ints        LOCKED_VAR = 0;

static struct timer_list *callout LOCKED_VAR = NULL;

static void link_timer   (struct timer_list *timer) LOCKED_FUNC;
static int  unlink_timer (struct timer_list *timer) LOCKED_FUNC;
static void timer_handler(int irq)                  LOCKED_FUNC;

static BYTE read_cmos (BYTE reg)
{
  _outportb (0x70, reg);
  usleep (10);
  return _inportb (0x71);
}

static void write_cmos (BYTE reg, BYTE val)
{
  _outportb (0x70, reg);
  usleep (10);
  _outportb (0x71, val);
}

static int set_RTC_rate (DWORD freq)
{
  BYTE  div, val;
  DWORD ofreq = freq;

  if (freq < RTC_FREQUENCY_LOW || freq > RTC_FREQUENCY_HIGH ||
      (freq & (freq-1)))
  {
    printk ("timer: illegal frequency (must be 2^n, n=1..13)\n");
    return (0);
  }
  for (div = 15; freq > 2; div--)
       freq >>= 1;

  if (timer_debug >= 1)
     printk ("timer: freq %u, divisor %u\n", (unsigned)ofreq, div);

  val = (read_cmos (RTC_STATUS_REG_A) & 0x80);
  write_cmos (RTC_STATUS_REG_A, div | 0x20);  /* set rate divisor */
  val = read_cmos (RTC_STATUS_REG_B);
  write_cmos (RTC_STATUS_REG_B, 0x40 | val);  /* enable periodic intr. */
  return (1);
}

static void set_RTC_off (void)
{
  BYTE val;

  val = (read_cmos (RTC_STATUS_REG_A) & 0x80);
  write_cmos (RTC_STATUS_REG_A, 0x20);         /* set 0 rate divisor */
  val = read_cmos (RTC_STATUS_REG_B);
  write_cmos (RTC_STATUS_REG_B, val & ~0x40);  /* disable periodic intr. */
}

static void timer_handler (int irq)
{
  struct timer_list *t;

#if defined(TEST)
  static int fan_idx LOCKED_VAR = 0;
  writew ("-\\|/"[fan_idx++] | 0x0F00, 0xB8000 + 2*78);
  fan_idx &= 3;
#endif

  jiffies += HZ / timer_ips;
  num_ints++;
  if (timer_active)
  {
    timer_reentries++;
    irq_chain (irq);
    return;
  }

  timer_active++;
  if (timer_debug >= 2)
     printk ("timer_handler(): now %u\n", (unsigned)jiffies);

  for (t = callout; t; t = t->next)
  {
    if (timer_debug >= 2)
       printk ("   1st timeout %u\n", (unsigned)t->expires);

    if (t->expires > jiffies)  /* not expired yet */
       break;

    unlink_timer (t);
    (*t->function) (t->data);
  }
  irq_chain (irq);
  timer_active--;
}

int init_timer (struct timer_list *timer)
{
  static int init LOCKED_VAR = 0;

  if (!init && !request_irq(timer_irq, timer_handler))
  {
    fprintf (stderr, "Failed to hook IRQ%d\n", timer_irq);
    return (0);
  }

  /* Enable Real-time Clock interrupt at IRQ 8 (int 70h)
   * Need to reprogram rate for each timer-init?
   */
  if (timer_irq == 8)
  {
    if (!set_RTC_rate(RTC_INTR_FREQUENCY))
       return (0);
  }

#ifndef _MODULE
  if (!init)
     atexit (stop_timer);
#endif

  timer_ips = (timer_irq == 8 ? RTC_INTR_FREQUENCY : 18);
  init = 1;
  if (timer)
     memset (timer, 0, sizeof(*timer));
  return (1);
}

/*
 * Release interrupt handler
 */
void stop_timer (void)
{
  free_irq (timer_irq);
  if (timer_irq == 8)
     set_RTC_off();
}

/*
 * add_timer(): add timer to callout list.
 * Earliest expiry is put first.
 */
int add_timer (struct timer_list *timer)
{
  if (timer->expires < jiffies)
  {
    if (timer_debug >= 1)
       printk ("add_timer(): timeout in the past?\n");
    return (0);
  }

  if (!timer_active)
     DISABLE();
  link_timer (timer);
  if (!timer_active)
     ENABLE();
  return (1);
}

/*
 * del_timer(): remove timer from callout list.
 */
int del_timer (struct timer_list *timer)
{
  int rc;

  if (!timer_active)
     DISABLE();
  rc = unlink_timer (timer);
  if (!timer_active)
     ENABLE();
  return (rc);
}


/*
 * set timer debug level
 */
int debug_timer (int lvl)
{
  int rc = timer_debug;
  timer_debug = lvl;
  return (rc);
}

static void link_timer (struct timer_list *timer)
{
  struct timer_list *t;
  struct timer_list *prev = NULL;

  if (!callout)
  {
    callout = timer;
    timer->next = NULL;
    return;
  }
  /* Traverse callout list until earliest timer before
   * our timer is found. Or no next is found.
   */
  for (t = callout; t; t = t->next)
      if (timer->expires > t->expires || !t->next)
         prev = t;

  timer->next = prev->next;
  prev->next  = timer;
}

static int unlink_timer (struct timer_list *timer)
{
  struct timer_list *t, *prev;

  for (t = prev = callout; t; prev = t, t = t->next)
      if (t == timer)
      {
        if (t == callout)           /* first (earliest) timer */
             callout    = t->next;  /* new callout start */
        else prev->next = t->next;  /* link around timer */
        break;
      }

  if (timer_debug >= 2)
     printk ("unlink_timer(): data %u%s\n",
             (unsigned)timer->data, t ? "" : ", illegal timer");
  if (!t)
     return (0);
  return (1);
}


#ifdef TEST 

uclock_t start, stop;
BOOL     quit = FALSE;

void cleanup (void)
{
  static int done = 0;
  double elapsed;

  if (done)
     return;

  stop_timer();
  quit = 1;
  done = 1;
  stop = uclock();
  _printk_flush();

  elapsed = (double)(stop - start) / UCLOCKS_PER_SEC;
  nosound();

  signal (SIGINT, SIG_DFL);
  printf ("timer count %Lu, reentries %Lu\n", jiffies, timer_reentries);
  printf ("elapsed %.3fs, %.3f intr/sec\n", elapsed, num_ints/elapsed);
}

static void signal_int (int sig)
{
  printk ("\nGot ^C\n");
  cleanup();
}

static struct timer_list timer1, timer2, timer3 LOCKED_VAR;

static void timer_function (DWORD arg) LOCKED_FUNC;
static void timer_function (DWORD arg)
{
  static int timeouts = 0;

  printk ("timer_function(%u) called, to %3d\n",
          (unsigned)arg, ++timeouts);

  if (arg == 1)
  {
    timer1.expires = RUN_AT (1*HZ);
    add_timer (&timer1);
  }
  else if (arg == 2)
  {
    timer2.expires = RUN_AT (25*HZ/10);
    add_timer (&timer2);
  }
  else if (arg == 3)
  {
    timer3.expires = RUN_AT (3*HZ);
    add_timer (&timer3);
  }
}

int main (int argc, char **argv)
{
  if (argc < 2)
  {
    fprintf (stderr, "usage: %s [0 | 8]\n", argv[0]);
    fprintf (stderr, "\tIRQ 0 (timer-1), IRQ 8 (real-time clock)\n");
    return (1);
  }
  timer_irq = argv[1][0] - '0';

  printf ("Press ^C or any key to exit\n");

  srand (time(NULL));
  _printk_init (3000, NULL);
  signal (SIGINT, signal_int);

  timer_debug = 2;
  irq_debug = 1;

  if (!init_timer (&timer1))
     return (-1);

  start = uclock();
  timer1.expires  = RUN_AT (1*HZ);      /* 1sec */
  timer1.data     = 1;
  timer1.function = timer_function;
  add_timer (&timer1);

  init_timer (&timer2);
  timer2.expires  = RUN_AT (15*HZ/10);  /* 1.5s */
  timer2.data     = 2;
  timer2.function = timer_function;
  add_timer (&timer2);

  init_timer (&timer3);
  timer3.expires  = RUN_AT (25*HZ/10);  /* 2.5s */
  timer3.data     = 3;
  timer3.function = timer_function;
  add_timer (&timer3);

  /* handle timers asynchronously.
   * Simply flush printouts accumulated during IRQ handling.
   */
  while (callout && !quit)
  {
    __dpmi_yield();
    _printk_flush();
    if (kbhit())
       quit = 1;
  }
  cleanup();
  return (0);
}
#endif  /* TEST */

