
#include "pmdrvr.h"
#include "module.h"

#include <libc/file.h>

int  errno = 0;
FILE __dj_stdout = { 0, 0, 0, 0, _IOWRT | _IOFBF, 1 };
FILE __dj_stderr = { 0, 0, 0, 0, _IOWRT | _IONBF, 2 };

struct libc_import   imports;
static struct device dummy = {
                     "dummy",
                     "Dummy test module"
                    };

struct device *dll_main (struct libc_import *li)
{
  unsigned *src = (unsigned*) li;
  unsigned *dst = (unsigned*) &imports;

  while (*src)
    *dst++ = *src++;

  _printk_init (1024, NULL);
  printk ("dll_main()\n");
  return (&dummy);
}

