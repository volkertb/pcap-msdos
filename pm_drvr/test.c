#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <pharlap.h>

#define __EXC_INTERNAL
#include <mw/exc.h>

#include "pmdrvr.h"

//#define FAR_SEG
#define PUBLIC

APIENTRY void foo (char *buf);

void bar (void)
{
  char buf[20];
  foo (buf);
  printf ("buf = `%s'\n",buf);
}

int main (int argc, char **argv)
{
  struct ModuleParams mod;

  excCoreFile = 0;
  InstallExcHandler (NULL);

  if (_dx_run_module("pmsub.rex",&mod) == 0)
  {
    _dos_freemem (mod.child_cs);
    _dos_freemem (mod.child_ds);
    printf ("\nchild returned %d\n", mod.child_code);
  }

//bar(n);
  return (0);
}

/*---------------------------------------------------------*/

#ifdef FAR_SEG
#pragma Cclass         ("FAR")
#pragma Dclass         ("FAR")
#pragma Code           ("FAR_CODE")
#pragma Data           ("FAR_DATA")
#pragma BSS            ("FAR_BSS")
#pragma Static_segment ("FAR_CONST")
#endif

PUBLIC char far_hello1[] = "hello world";
static char far_hello2[] = "hello world";

PUBLIC buf1[100];

APIENTRY void foo (char *buf)
{
  char *var = far_hello1;
  strcpy (buf,var);
}

#ifdef FAR_SEG
#pragma Data()
#endif
