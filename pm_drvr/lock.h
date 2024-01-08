#ifndef __LOCK_H
#define __LOCK_H

#if !defined(USE_SECTION_LOCKING)
  #define LOCKED_VAR
  #define LOCKED_FUNC
#else /* rest of file */

/*
 * LOCK.H - pseudoattributes for automatic locking in DJGPP
 * (C) Alaric B. Williams (alaric@abwillms.demon.co.uk) 1996
 * Use this for whatever you want, as long as all due credit is given.
 * Modified versions should be marked as such if they are to be distributed.
 * Have fun.
 *
 * All objects marked with the appropriate LOCKED_* attributes will be locked
 * in physical memory when lock_sections() is called.
 *
 * All locked variables must be initialised.
 *
 * Assembly code wishing to have data locked should declare it in the section
 * ".ldat", and code in the section ".ltxt".
 *
 * To use this code, GCC must be capable of correctly handling the section
 * attribute, and the modified djgpp.lnk must be installed. This is a
 * GNU diff against 2.03 version:
 *
 * --- e:\djgpp\lib\djgpp.djl      Fri Jul 30 03:50:42 1999
 * +++ djgpp.lnk                   Wed Jul 12 17:13:36 2000
 * @@ -7,6 +7,9 @@
 *      *(.gnu.linkonce.t*)
 *      *(.gnu.linkonce.r*)
 *      etext  = . ; _etext = .;
 * +    sltext = . ;
 * +    *(.ltxt)
 * +    eltext = . ;
 *      . = ALIGN(0x200);
 *    }
 *    .data ALIGN(0x200) : {
 * @@ -24,6 +27,9 @@
 *      ___EH_FRAME_END__ = . ;
 *      LONG(0)
 *      edata  = . ; _edata = .;
 * +    sldata = . ;
 * +    *(.ldat)
 * +    eldata = . ;
 *       . = ALIGN(0x200);
 *    }
 *    .bss SIZEOF(.data) + ADDR(.data) :
 *
 *
 * Sample usage:
 *
 * GCC -
 *
 *    int foo    LOCKED_VAR = 0;
 *    void bar() LOCKED_FUNC;
 *
 * GAS -
 *
 *    .section .ldat
 *
 *    _foo:
 *    .long 0
 *
 *    .section .ltxt
 *
 *    _bar:
 *       ret
 *
 * Call lock_sections() as early as possible, and certainly before
 * installing any interrupt handlers!
 *
 */

#define LOCKED_VAR  __attribute__((section(".ldat"), nocommon))
#define LOCKED_FUNC __attribute__((section(".ltxt")))

#ifdef __cplusplus
extern "C" {
#endif

/* returns 0 for success, -1 for failure
 */
#ifndef __ASM
int lock_sections (void);
#endif

#ifdef __cplusplus
}
#endif

#endif  /* USE_SECTION_LOCKING */
#endif  /* __LOCK_H */

