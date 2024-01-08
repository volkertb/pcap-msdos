#include <sys/dxe.h>

#include "pmdrvr.h"

#undef _MODULE
#include "module.h"

#define FILL_IN  NULL

#ifdef USE_DXE3

#else

struct libc_import exports = {
                   printf, sprintf, fprintf, fwrite, fopen, fclose,
                   malloc, free,
                   memset, memcpy, memchr,
                   strchr, strcmp, strncmp, strtoul,
                   _go32_dpmi_get_protected_mode_interrupt_vector,
                   _go32_dpmi_set_protected_mode_interrupt_vector,
                   _go32_dpmi_lock_data,
                   __dpmi_unlock_linear_region,
                   __dpmi_allocate_dos_memory,
                   __dpmi_set_segment_limit,
                   __dpmi_set_descriptor_access_rights,
                   __dpmi_get_descriptor_access_rights,
                   __dpmi_allocate_ldt_descriptors,
                   __dpmi_set_segment_base_address,
                   __dpmi_get_segment_base_address,
                   __dpmi_free_ldt_descriptor,
                   uclock,
                   usleep,
                   delay,
                   sound,
                   dosmemget,
                   longjmp,
                   setjmp,
                   signal,
                   FILL_IN,   /* __djgpp_exception_state_ptr */
                   FILL_IN,   /* __djgpp_ds_alias */
                   FILL_IN,   /* EISA_bus */
                   NULL
                 };
#endif

int EISA_bus = 0;

int main (void)
{
  struct device *dev;
  dll_entry      entry = _dxe_load ("3c509.wlm");

  if (!entry)
  {
    printf ("module not loaded\n");
    return (-1);
  }

#ifdef USE_DXE3

#else
  printf ("DXE entry at %08Xh\n", (unsigned)entry);
  exports.__djgpp_ds_alias            = &__djgpp_ds_alias;
  exports.__djgpp_exception_state_ptr = &__djgpp_exception_state_ptr;
  exports.EISA_bus                    = &EISA_bus;
#endif

  dev = (*entry) (&exports, 6);
  printf ("entry() returned %08Xh\n", (unsigned)dev);

  if (dev)
  {
    printf ("Probing for %s..", dev->long_name);
    if (!(*dev->probe)(dev))
       printf ("not found\n");
  }

  return (0);
}


