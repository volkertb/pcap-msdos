/*
 * LOCK.C - pseudoattributes for automatic locking in DJGPP
 * (C) Alaric B. Williams (alaric@abwillms.demon.co.uk) 1996
 * Use this for whatever you want, as long as all due credit is given.
 * Modified versions should be marked as such if they are to be distributed.
 * Have fun.
 */

#include "pmdrvr.h"

#if defined(USE_SECTION_LOCKING)

/* markers of the locked sections, defined by ld (see lock.ld)
 */
extern char sltext __asm__("sltext");
extern char eltext __asm__("eltext");
extern char sldata __asm__("sldata");
extern char eldata __asm__("eldata");

/* macros returning sizes of locked areas in bytes
 */
#define LOCKED_TEXT_SIZE ((long)&eltext - (long)&sltext)
#define LOCKED_DATA_SIZE ((long)&eldata - (long)&sldata)

/*
 * Even if it can't lock the data, lock_sections() tries to lock
 * the code, to do the best job it can.
 */
static int locked_yet = 0;

int lock_sections (void)
{
  if (!locked_yet)
  {
    int rc;

    rc  = _go32_dpmi_lock_data (&sldata, LOCKED_DATA_SIZE);
    rc |= _go32_dpmi_lock_code (&sltext, LOCKED_TEXT_SIZE);
    locked_yet = 1;
    return (rc);
  }
  return (0);   /* okay */
}
#endif
