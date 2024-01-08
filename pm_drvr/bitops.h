#ifndef _I386_BITOPS_H
#define _I386_BITOPS_H

/*
 * Copyright 1992, Linus Torvalds.
 */
#ifdef __GNUC__

/*
 * These have to be done with inline assembly: that way the bit-setting
 * is guaranteed to be atomic. All bit operations return 0 if the bit
 * was cleared before the operation and != 0 if it was not.
 *
 * bit 0 is the LSB of addr; bit 32 is the LSB of (addr+1).
 */

/*
 * Some hacks to defeat gcc over-optimizations..
 */
struct __dummy { unsigned long a[100]; };
#define ADDR (*(struct __dummy*) addr)

extern __inline__ int set_bit (int nr, void *addr)
{
  int oldbit;

  __asm__ __volatile__ (
            "btsl %2, %1\n\t"
            "sbbl %0, %0"
            : "=r" (oldbit), "=m" (ADDR)
            : "ir" (nr));
  return (oldbit);
}

extern __inline__ int clear_bit (int nr, void *addr)
{
  int oldbit;

  __asm__ __volatile__ (
            "btrl %2, %1\n\t"
            "sbbl %0, %0"
            : "=r" (oldbit), "=m" (ADDR)
            : "ir" (nr));
  return (oldbit);
}

/*
 * This routine doesn't need to be atomic.
 */
extern __inline__ int test_bit (int nr, const void *addr)
{
  return ((1UL << (nr & 31)) & (((const unsigned int*) addr)[nr >> 5])) != 0;
}

#endif  /* __GNUC__ */
#endif  /* _I386_BITOPS_H */

