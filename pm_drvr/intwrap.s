/*
 * Hardware interrupt handler library
 *
 * INTWRAP.S - Simplified, robust, hardware interrupt handling in DJGPP
 * (C) Alaric B. Williams (alaric@abwillms.demon.co.uk) 1996
 * Use this for whatever you want, as long as all due credit is given.
 * Modified versions should be marked as such if they are to be distributed.
 * Have fun.
 *
 */

#define __ASM
#include "lock.h"

#if defined(_MODULE) || defined(USE_DXE3)
#undef USE_SECTION_LOCKING
#endif

#ifdef USE_SECTION_LOCKING
  #define DATA_SECTION  .section .ldat
  #define CODE_SECTION  .section .ltxt
#else
  #define DATA_SECTION  .section .data
  #define CODE_SECTION  .section .text
#endif

.file "intwrap.s"

DATA_SECTION

.extern _irq_stub_stacks /* array of stack-end locations */

.globl _irq_our_ds
_irq_our_ds:
   .word 0            /* initialised to __djgpp_ds_alias */


CODE_SECTION

.extern _irq_umbrella_handler

/*
 * IRQ wrappers:
 *
 * IRQ is the 1st argument the irq_umbrella_handler function will
 * receive.
 */
#define MAKE_WRAPPER(IRQ)                                   ;\
                                                            ;\
.globl _pcap_irq_stub_##IRQ                                 ;\
_pcap_irq_stub_##IRQ:                                       ;\
   pushal                                                   ;\
   pushw %ds                                                ;\
   pushw %es                                                ;\
   pushw %fs                                                ;\
   pushw %gs                                                ;\
                                                            ;\
   .byte 0x2e             /* CS: prefix */                  ;\
   movw  _irq_our_ds, %ax /* get DS */                      ;\
   movw  %ax, %ds                                           ;\
   movw  %ax, %es                                           ;\
   movw  %ax, %fs                                           ;\
   movw  %ax, %gs                                           ;\
                                                            ;\
   /* now switch stacks */                                  ;\
   movw  %ss,  %bx                                          ;\
   movl  %esp, %ecx                                         ;\
   movw  %ax,  %ss                                          ;\
                                                            ;\
   movl  $(_irq_stub_stacks+4*##IRQ), %eax                  ;\
   movl  (%eax), %esp                                       ;\
                                                            ;\
   pushl %ebx /* save old stack on new stack */             ;\
   pushl %ecx                                               ;\
                                                            ;\
   cld                                                      ;\
   pushl $##IRQ                                             ;\
   call  _irq_umbrella_handler                              ;\
   popl  %edx                                               ;\
                                                            ;\
   popl  %ecx /* retrieve old stack */                      ;\
   popl  %ebx                                               ;\
                                                            ;\
   movw  %bx, %ss                                           ;\
   movl  %ecx, %esp                                         ;\
                                                            ;\
   popw  %gs                                                ;\
   popw  %fs                                                ;\
   popw  %es                                                ;\
   popw  %ds                                                ;\
   popal                                                    ;\
   iret                                                            
   
MAKE_WRAPPER(0)
MAKE_WRAPPER(1)
MAKE_WRAPPER(2)
MAKE_WRAPPER(3)
MAKE_WRAPPER(4)
MAKE_WRAPPER(5)
MAKE_WRAPPER(6)
MAKE_WRAPPER(7)
MAKE_WRAPPER(8)
MAKE_WRAPPER(9)
MAKE_WRAPPER(10)
MAKE_WRAPPER(11)
MAKE_WRAPPER(12)
MAKE_WRAPPER(13)
MAKE_WRAPPER(14)
MAKE_WRAPPER(15)

