/*
 *  Fast replacement of gettimeofday() for DOS-libpcap.
 *
 *  by G.Vanem 1998
 *
 *  DISCARDED: Uses gettimeofday2() in Watt-32. That functions is
 *             faster and has micro-second resolution.
 */

#include <stdio.h>
#include <time.h>
#include <dos.h>
#include <tcp.h>

#include "ioport.h"
#include "pcap.h"

#ifdef __HIGHC__
#  define enable()   _inline(0xFB)
#  define disable()  _inline(0xFA)
#endif

#ifdef __DJGPP__
#  define _timezone 0
#endif

static BYTE read_cmos (BYTE port);
static void set_mode2 (void);
static long get_usec  (time_t *sec);

int pcap_gettimeofday (struct timeval *tv, struct timezone *tz)
{
  static int    init = 0;
  static time_t time0;
  static BYTE   day0,hour0,min0;
  int           day, hour, min;

  if (!tv)
     return (-1);

  min  = read_cmos (2);
  hour = read_cmos (4);
  day  = read_cmos (7);

  if (!init || day != day0)
  {
    set_mode2();
    time0 = time (NULL) / 60;
    day0  = day;
    hour0 = read_cmos (4);
    min0  = read_cmos (2);
    init  = 1;
  }

  tv->tv_sec  = 60*(min - min0) + 3600*(hour - hour0);  /* todays seconds */
  tv->tv_sec += read_cmos (0) + time0;         /* add seconds + time base */

  tv->tv_usec = get_usec (&tv->tv_sec);

  if (tz)
  {
    struct tm *tm = localtime (&tv->tv_sec);

    tz->tz_minuteswest = -60 * _timezone;
    tz->tz_dsttime     = tm->tm_isdst;
  }
  return (0);
}


static BYTE read_cmos (BYTE port)
{
  BYTE bcd;

  _outportb (0x70, port);
  bcd = _inportb (0x71);
  return (10*(bcd >> 4) + (bcd & 15));
}

static void set_mode2 (void)
{
  _outportb (0x43, 0x34);
  _outportb (0x40, 0xFF);
  _outportb (0x40, 0xFF);
}

static long get_usec (time_t *sec)
{
  BYTE lsb, msb;
  long time, usec;

  disable();
  _outportb (0x43,0);       /* latch timer 0's counter */
  lsb  = _inportb (0x40);   /* read timer LSB and MSB  */
  msb  = _inportb (0x40);
  time = lsb + (msb << 8);

  /* Now check for counter overflow.  This is tricky because the timer chip
   * doesn't let us atomically read the current counter value and the output
   * state (i.e., overflow state).  We have to read the PIC interrupt request
   * register (IRR) to see if the overflow has occured. Because we lack
   * atomicity, we use the (very accurate) heuristic that we only check for
   * overflow if the value read is close to the interrupt period.
   * E.g., if we just checked the IRR, we might read a non-overflowing value
   * close to 0, experience overflow, then read this overflow from the IRR,
   * and mistakenly add a correction to the "close to zero" value.
   *
   * We compare the counter value to heuristic constant 11890.
   * If the counter value is less than this, we assume the counter didn't
   * overflow between disabling interrupts above and latching the counter
   * value.  For example, we assume that the above 10 or so instructions
   * take less than 11932 - 11890 = 42 microseconds to execute.
   *
   * Otherwise, the counter might have overflowed.  We check for this
   * condition by reading the interrupt request register out of the PIC.
   * If it overflowed, we add in one clock period.
   */
  if (time > 11890UL)
  {
    _outportb (0x20, 0x0A);   /* write OCW3, read IRR on next read */
    if (_inportb(0x20) & 1)   /* IRQ0 line pending ? */
       time -= 11932UL;       /* yes, subtract one clock period */
    _outportb (0x20, 0x0B);   /* write OCW3, read ISR on next read */
  }
  enable();
  usec = ((11932UL - time) * 1000UL) / 1193UL;
  while (usec >= 1000000UL)
  {
    usec -= 1000000UL;
    (*sec)++;
  }
  return (usec);
}
