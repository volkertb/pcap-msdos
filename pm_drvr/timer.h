#ifndef __TIMER_H
#define __TIMER_H

#define RTC_INTR_FREQUENCY  64   /* must be 2^n, n=1..13 */
#define RTC_FREQUENCY_LOW   2
#define RTC_FREQUENCY_HIGH  (1<<13)

#define RTC_STATUS_REG_A    0x0A
#define RTC_STATUS_REG_B    0x0B

#if !defined(uclock_t)
#define uclock_t uint64
#endif

/*
 * The "data" field is in case you want to use the same
 * timeout function for several timeouts. You can use this
 * to distinguish between the different invocations.
 */
struct timer_list {
       struct timer_list *next;
       uclock_t     expires;
       DWORD        data;
       void       (*function)(DWORD);
     };

#if defined(__DJGPP__)  /* Only for djgpp at the moment */

extern BYTE  timer_irq;
extern QWORD jiffies;

extern int  init_timer (struct timer_list *timer) LOCKED_FUNC;
extern int  add_timer  (struct timer_list *timer) LOCKED_FUNC;
extern int  del_timer  (struct timer_list *timer) LOCKED_FUNC;
extern void stop_timer (void)                     LOCKED_FUNC;
extern int  debug_timer(int lvl);

#endif
#endif

