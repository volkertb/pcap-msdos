#include "pmdrvr.h"

#undef  APIENTRY
#define APIENTRY _CC(_FAR_CALL)

APIENTRY int main (struct ModuleParams mod)
{
  printf ("Hello from PMSUB.REX\n");

  printf ("Crashing PMSUB.REX\n");
  *(char*)-1 = 0;

  printf ("mod.parent_ds = %02Xh\n", mod.parent_ds);
  printf ("mod.parent_cs = %02Xh\n", mod.parent_cs);

  printf ("Bye from PMSUB.REX\n");

  return (123);
}
