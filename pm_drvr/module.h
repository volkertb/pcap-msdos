#ifndef __MODULE_H
#define __MODULE_H

#ifdef __DJGPP__

struct libc_import {
       int           (*printf) (const char*, ...);
       int           (*sprintf) (char *, const char*, ...);
       int           (*fprintf) (FILE*, const char*, ...);
       size_t        (*fwrite) (const void*, size_t, size_t, FILE*);
       FILE *        (*fopen) (const char*, const char*);
       int           (*fclose) (FILE*);

       void *        (*malloc) (size_t);
       void          (*free)   (void*);

       void *        (*memset) (void*, int, size_t);
       void *        (*memcpy) (void*, const void*, size_t);
       void *        (*memchr) (const void*, int, size_t);

       char *        (*strchr) (const char*, int);
       size_t        (*strlen) (const char*);
       int           (*strcmp) (const char*, const char*);
       int           (*strncmp) (const char*, const char*, size_t);
       unsigned long (*strtoul) (const char*, char**, int);

       int           (*_go32_dpmi_get_protected_mode_interrupt_vector) (int, _go32_dpmi_seginfo*);
       int           (*_go32_dpmi_set_protected_mode_interrupt_vector) (int, _go32_dpmi_seginfo*);
       int           (*_go32_dpmi_lock_data) (void*, unsigned long);
       int           (*__dpmi_unlock_linear_region) (__dpmi_meminfo*);
       int           (*__dpmi_allocate_dos_memory) (int, int*);
       int           (*__dpmi_set_segment_limit) (int, unsigned long);
       int           (*__dpmi_set_descriptor_access_rights) (int, int);
       int           (*__dpmi_get_descriptor_access_rights) (int);
       int           (*__dpmi_allocate_ldt_descriptors) (int);
       int           (*__dpmi_set_segment_base_address) (int, unsigned long);
       int           (*__dpmi_get_segment_base_address) (int, unsigned long*);
       int           (*__dpmi_free_ldt_descriptor) (int);

       uclock_t      (*uclock) (void);
       unsigned int  (*usleep) (unsigned int);
       void          (*delay)  (unsigned);
       void          (*sound)  (int);

       void          (*dosmemget) (unsigned long, size_t, void*);
       void          (*longjmp) (jmp_buf, int);
       int           (*setjmp) (jmp_buf);
       void *        (*signal) (int, void (*)(int));

       /* import data
        */
       jmp_buf        **__djgpp_exception_state_ptr;
       unsigned short  *__djgpp_ds_alias;
       int             *EISA_bus;

       void *          last_marker;
     };

typedef struct device * (*dll_entry) (struct libc_import *, int dbg_level);

extern struct libc_import imports;


#if defined(_MODULE) & !defined(USE_DXE3)
  #define printf      imports.printf
  #define sprintf     imports.sprintf
  #define fprintf     imports.fprintf
  #define fwrite      imports.fwrite
  #define fopen       imports.fopen
  #define fclose      imports.fclose
  #define malloc      imports.malloc
  #define free        imports.free
  #define memset      imports.memset
  #define memcpy      imports.memcpy
  #define memchr      imports.memchr
  #define strchr      imports.strchr
  #define strcmp      imports.strcmp
  #define strncmp     imports.strncmp
  #define strtoul     imports.strtoul
  #define strlen      imports.strlen

  #define uclock      imports.uclock
  #define usleep      imports.usleep
  #define delay       imports.delay
  #define sound       imports.sound
  #define dosmemget   imports.dosmemget
  #define longjmp     imports.longjmp
  #define setjmp      imports.setjmp
  #define signal      imports.signal

  #define _go32_dpmi_get_protected_mode_interrupt_vector imports._go32_dpmi_get_protected_mode_interrupt_vector
  #define _go32_dpmi_set_protected_mode_interrupt_vector imports._go32_dpmi_set_protected_mode_interrupt_vector
  #define _go32_dpmi_lock_data                           imports._go32_dpmi_lock_data
  #define __dpmi_unlock_linear_region                    imports.__dpmi_unlock_linear_region
  #define __dpmi_allocate_dos_memory                     imports.__dpmi_allocate_dos_memory
  #define __dpmi_set_segment_limit                       imports.__dpmi_set_segment_limit
  #define __dpmi_set_descriptor_access_rights            imports.__dpmi_set_descriptor_access_rights
  #define __dpmi_get_descriptor_access_rights            imports.__dpmi_get_descriptor_access_rights
  #define __dpmi_allocate_ldt_descriptors                imports.__dpmi_allocate_ldt_descriptors
  #define __dpmi_set_segment_base_address                imports.__dpmi_set_segment_base_address
  #define __dpmi_get_segment_base_address                imports.__dpmi_get_segment_base_address
  #define __dpmi_free_ldt_descriptor                     imports.__dpmi_free_ldt_descriptor
  #define __djgpp_ds_alias                               *imports.__djgpp_ds_alias
  #define __djgpp_exception_state_ptr                    *imports.__djgpp_exception_state_ptr
  #define EISA_bus                                       *imports.EISA_bus
#endif

#endif /* __DJGPP__ */

#endif /*__MODULE_H */
