/*
 *  k_malloc.c - Simple "kernel" malloc module.
 *
 *  G. Vanem <giva@bgnet.no> - 1998
 */

#include "pmdrvr.h"
#include "module.h"

#define MARKER 0xDEADBEEF

DWORD _virtual_base = 0;

void *k_malloc (size_t size)
{
  DWORD *p;
  void  *buf;

  size += 3;
  size &= ~3;        /* Round to dword boundary. */
  size += 4+4;       /* add space for marker and size */
  buf   = malloc (size);

  if (!buf)
     return (NULL);

  p    = (DWORD*)buf;
  *p++ = MARKER;
  *p++ = size;

  if (_go32_dpmi_lock_data (buf, size))
     printk ("kmalloc: locking data failed\n");
  return (void*)p;
}

void *k_calloc (size_t num, size_t size)
{
  void *buf = k_malloc (size*num);

  if (buf)
     memset (buf, 0, size*num);
  return (buf);
}

void k_free (void *ptr)
{
  if (ptr)
  {
    __dpmi_meminfo mem;
    DWORD  base = 0;
    DWORD *p = (DWORD*)ptr - 2;

    if (*p != MARKER)
    {
      printk ("Panic: freeing bad ptr %p\n", ptr);
      return;
    }
    __dpmi_get_segment_base_address (_my_ds(), &base);

    mem.address = base + (unsigned long)ptr;
    mem.size    = p[1];
    if (__dpmi_unlock_linear_region (&mem))
       printk ("kmalloc: unlocking data failed\n");
    free (p);
  }
}

