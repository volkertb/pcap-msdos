/*
 * printk.c - Formatted printf style routines.
 *            These are safe to use in interrupt handlers
 *
 * NB! Doesn't handle modifier 'L' and floating-point formats.
 */

#include "pmdrvr.h"
#include "module.h"

typedef size_t (*out_fn) (const void*, size_t, size_t, FILE*);

PUBLIC int    _printk_safe  LOCKED_VAR = 0;
PUBLIC FILE  *_printk_file  LOCKED_VAR = NULL;
PUBLIC out_fn _printk_out   LOCKED_VAR;

STATIC char *printk_ptr     LOCKED_VAR = NULL;
STATIC char *printk_buf     LOCKED_VAR = NULL;
STATIC char *printk_end     LOCKED_VAR = NULL;

STATIC char  hexchars[]     LOCKED_VAR = "0123456789abcdef";
STATIC char  hexCHARS[]     LOCKED_VAR = "0123456789ABCDEF";
STATIC DWORD overrun_full   LOCKED_VAR = 0UL;
STATIC DWORD overrun_part   LOCKED_VAR = 0UL;

STATIC char *ip_ntoa(DWORD) LOCKED_FUNC;

#ifdef _MODULE
  #undef  isdigit
  #define isdigit(x) ((x) >= '0' && (x) <= '9')
#else
  STATIC char *get_strerror(void)  LOCKED_FUNC;
  STATIC void  printk_exit (void);
#endif

void _printk_flush (void)  /* locking not needed */
{
  if (_printk_safe > 0 && printk_ptr > printk_buf && _printk_out)
  {
    (*_printk_out) (printk_buf, printk_ptr - printk_buf, 1, _printk_file);
    printk_ptr = printk_buf;
  }
}

#ifndef _MODULE
STATIC void printk_exit (void) /* locking not needed */
{
  _printk_flush();
  if (_printk_file && _printk_file != stderr && _printk_file != stdout)
  {
    fclose (_printk_file);
    _printk_file = NULL;
  }

  if (printk_ptr)
     k_free (printk_ptr);

  printk_ptr = NULL;
  _printk_safe = 0;

  if (overrun_full + overrun_part)
     fprintf (stderr, "printk: buffer-overruns %lu/%lu\r\n",
              overrun_full, overrun_part);
}
#endif

void _printk (const char *fmt, ...)
{
  if (_printk_file && printk_ptr && fmt)
  {
    if (printk_ptr < printk_end-128) /* experimental margin */
    {
      va_list args;
      int     len, max = (int)(printk_end-printk_ptr-1);

      va_start (args, fmt);
      len = _vsnprintk (printk_ptr, max, fmt, args);
      printk_ptr += len;
      *printk_ptr = '\0';
      va_end (args);
      if (len == max)
         overrun_part++;   /* partial overruns */
    }
    else
      overrun_full++;      /* assuming nothing free */
  }
  _printk_flush();
}

int _printk_init (int size, char *file)
{
  if (!printk_buf)
  {
    printk_ptr = printk_buf = k_malloc (size);
    if (!printk_ptr)
    {
      fprintf (stderr, "printk: buffer allocation failed\n");
      return (0);
    }
  }
  _printk_file = stderr;
  if (file && (_printk_file = fopen(file,"wt")) == NULL)
  {
    fprintf (stderr, "printk: cannot open `%s'\n", file);
    return (0);
  }
  _printk_out  = fwrite;
  _printk_safe = 1;
  printk_end   = printk_ptr + size - 1;

#ifndef _MODULE
  atexit (printk_exit);
#endif
  return (1);
}

#ifndef _MODULE
/*
 *  return string describing last `errno'. Remove leading '\n' if
 *  present
 */
STATIC char *get_strerror (void)
{
  /* strerror() needs locking, but %m format shouldn't be used
   * from an IRQ handler anyway
   */
  char *err = strerror (errno);
  char *nl;
 
  /* lock needed if not inlined (-O2 ?)
   */
  if ((nl = strchr(err,'\r')) != NULL) *nl = 0; 
  if ((nl = strchr(err,'\n')) != NULL) *nl = 0;
  return (err);
}
#endif

/*
 * Make a string representation of a network IP address.
 */
STATIC char *ip_ntoa (DWORD ipaddr)
{
  static char buf[20] LOCKED_VAR;

  _snprintk (buf, sizeof(buf), "%d.%d.%d.%d",
             (BYTE)(ipaddr),
             (BYTE)(ipaddr >> 8),
             (BYTE)(ipaddr >> 16),
             (BYTE)(ipaddr >> 24));
  return (buf);
}

/*
 * _snprintk - format a message into a buffer.
 * Like sprintf except we also specify the length of the output buffer,
 * and we handle %r (recursive format), %m (error message) and %I
 * (IP address) formats. Returns the number of chars put into buf.
 *
 * NB! Doesn't handle modifier 'L' and floating-point formats.
 */
int _snprintk (char *buf, int buflen, const char *fmt, ...)
{
  int     rc;
  va_list args;
  va_start (args, fmt);
  rc = _vsnprintk (buf, buflen, fmt, args);
  va_end (args);
  return (rc);
}

/*
 * _vsnprintk - like _snprintk, takes a va_list instead of a list of args.
 */
#define OUTCHAR(c)  (buflen > 0 ? (--buflen, *buf++ = (c)) : 0)

int _vsnprintk (char *buf, int buflen, const char *fmt, va_list args)
{
  int    c, i, n, longflag;
  int    width, prec, fillch;
  int    base, len, neg, quoted, upper;
  DWORD  val = 0;
  char  *str, *f, *buf0;
  char  *format = (char*)fmt;
  BYTE  *p;
  char   num[32];

  buf0 = buf;
  buflen--;

  while (buflen > 0)
  {
    for (f = format; *f != '%' && *f; ++f)
        ;
    if (f > format)
    {
      len = f - format;
      if (len > buflen)
          len = buflen;
      memcpy (buf, format, len);   /* to-do!!: lock needed */
      buf    += len;
      buflen -= len;
      format  = f;
    }
    if (*format == 0)
       break;
    c = *++format;
    width = prec = longflag = 0;
    fillch = ' ';
    if (c == '0')
    {
      fillch = '0';
      c = *++format;
    }
    if (c == '*')
    {
      width = va_arg (args, int);
      c = *++format;
    }
    else
      while (isdigit(c))
      {
        width = width * 10 + c - '0';
        c = *++format;
      }

    if (c == '.')
    {
      c = *++format;
      if (c == '*')
      {
        prec = va_arg (args, int);
        c = *++format;
      }
      else
        while (isdigit(c))
        {
          prec = prec * 10 + c - '0';
          c = *++format;
        }
    }
    str   = 0;
    base  = 0;
    neg   = 0;
    upper = 0;
    ++format;
  nextch:
    switch (c)
    {
      case 'l':
           longflag = 1;
           c = *++format;
           goto nextch;
      case 'd':
           if (longflag)
                i = va_arg (args, long);
           else i = va_arg (args, int);
           if (i < 0)
           {
             neg = 1;
             val = -i;
           }
           else
             val = i;
           base = 10;
           break;
      case 'u':
           if (longflag)
                val  = va_arg (args, unsigned long);
           else val  = va_arg (args, unsigned int);
           base = 10;
           break;
      case 'o':
           if (longflag)
                val  = va_arg (args, unsigned long);
           else val  = va_arg (args, unsigned int);
           base = 8;
           break;
      case 'X':
           upper = 1;
           /* fall through */
      case 'x':
           if (longflag)
                val  = va_arg (args, unsigned long);
           else val  = va_arg (args, unsigned int);
           base = 16;
           break;
      case 'p':
           val   = (DWORD) va_arg (args, void*);
           base  = 16;
           neg   = 2;
           upper = 1;
           break;
      case 's':
           str = va_arg (args, char*);
           break;
      case 'c':
           num[0] = va_arg (args, int);
           num[1] = 0;
           str = num;
           break;
      case 'm':
#ifdef _MODULE
           *buf++ = '%';
           *buf++ = 'm';
#else
           str = get_strerror();
#endif
           break;
      case 'I':
           str = ip_ntoa (va_arg(args,DWORD));
           break;
      case 'r':
           f = va_arg (args, char*);
           n = _vsnprintk (buf, buflen + 1, f, va_arg(args,va_list));
           buf    += n;
           buflen -= n;
           continue;
      case 'v':                /* "visible" string */
      case 'q':                /* quoted string */
           quoted = c == 'q';
           p = va_arg (args, BYTE*);
           if (fillch == '0' && prec > 0)
              n = prec;
           else
           {
             n = strlen ((char*)p);      /* to-do!!: lock needed */
             if (prec > 0 && prec < n)
                n = prec;
           }
           while (n > 0 && buflen > 0)
           {
             c = *p++;
             --n;
             if (!quoted && c >= 0x80)
             {
               OUTCHAR ('M');
               OUTCHAR ('-');
               c -= 0x80;
             }
             if (quoted && (c == '"' || c == '\\'))
                OUTCHAR ('\\');
             if (c < 0x20 || (0x7F <= c && c < 0xA0))
             {
               if (quoted)
               {
                 OUTCHAR ('\\');
                 switch (c)
                 {
                   case '\t': OUTCHAR ('t');   break;
                   case '\n': OUTCHAR ('n');   break;
                   case '\b': OUTCHAR ('b');   break;
                   case '\f': OUTCHAR ('f');   break;
                   default  : OUTCHAR ('x');
                              OUTCHAR (hexchars[c >> 4]);
                              OUTCHAR (hexchars[c & 0xf]);
                 }
               }
               else
               {
                 if (c == '\t')
                    OUTCHAR (c);
                 else
                 {
                   OUTCHAR ('^');
                   OUTCHAR (c ^ 0x40);
                 }
               }
             }
             else
               OUTCHAR (c);
           }
           continue;
      default:
           *buf++ = '%';
           if (c != '%')
              --format;         /* so %z outputs %z etc. */
           --buflen;
           continue;
    }

    if (base)
    {
      str = num + sizeof(num);
      *--str = 0;
      while (str > num + neg)
      {
        *--str = upper ? hexCHARS [val % base] : hexchars [val % base];
        val = val / base;
        if (--prec <= 0 && val == 0)
           break;
      }
      switch (neg)
      {
        case 1: *--str = '-';
                break;
        case 2: *--str = 'x';
                *--str = '0';
                break;
      }
      len = num + sizeof(num) - 1 - str;
    }
    else
    {
      len = strlen (str);            /* to-do!!: lock needed */
      if (prec > 0 && len > prec)
         len = prec;
    }
    if (width > 0)
    {
      if (width > buflen)
          width = buflen;
      if ((n = width - len) > 0)
      {
        buflen -= n;
        for (; n > 0; --n)
            *buf++ = fillch;
      }
    }
    if (len > buflen)
        len = buflen;
    memcpy (buf, str, len);         /* to-do!!: lock needed */
    buf    += len;
    buflen -= len;
  }
  *buf = 0;
  return (buf - buf0);
}
