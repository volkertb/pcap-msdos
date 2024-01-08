#ifndef __KMALLOC_H
#define __KMALLOC_H

#ifdef __DJGPP__
  extern DWORD _virtual_base;

  extern void *k_malloc (size_t size);
  extern void *k_calloc (size_t num, size_t size);
  extern void  k_free   (void *ptr);

#else
  #define k_malloc malloc
  #define k_calloc calloc
  #define k_free   free
#endif

#endif
