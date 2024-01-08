#ifndef __PRINTK_H
#define __PRINTK_H

extern int      _printk_safe;     /* safe to flush buffer */
extern FILE    *_printk_file;     /* where to flush buffer */
extern size_t (*_printk_out)(const void*, size_t, size_t, FILE*);

int  _printk_init  (int size, char *file);
void _printk_flush (void);

void _printk (const char *, ...) LOCKED_FUNC
#ifdef __GNUC__
  __attribute__((format(printf,1,2)))
#endif
;

int _snprintk  (char*, int, const char*, ...) LOCKED_FUNC
#ifdef __GNUC__
  __attribute__((format(printf,3,4)))
#endif
;

int _vsnprintk (char*, int, const char*, va_list) LOCKED_FUNC;

#define printk _printk

#endif
