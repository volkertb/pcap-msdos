.386
.MODEL FLAT,C

;
; Hardware interrupt handler library
;
; INTWRAP.S - Simplified, robust, hardware interrupt handling in DJGPP
; (C) Alaric B. Williams (alaric@abwillms.demon.co.uk) 1996
; Use this for whatever you want, as long as all due credit is given.
; Modified versions should be marked as such if they are to be distributed.
; Have fun.
;

.DATA

extrn irq_stub_stacks : dword  ; array of stack-end locations

.CODE

extrn irq_umbrella_handler : near

;;
;; IRQ wrappers:
;;
;; IRQ is the 1st argument the irq_umbrella_handler function will
;; receive.
;;
MAKE_WRAPPER MACRO IRQ

PUBLIC pcap_irq_stub_&IRQ
pcap_irq_stub_&IRQ:
   pushad
   push ds
   push es
   push fs
   push gs

   add  ax, 8      ; DS = CS + 8
   mov  ds, ax
   mov  es, ax
   mov  fs, ax
   mov  gs, ax

   mov  bx, ss     ; now switch stacks
   mov  ecx, esp
   mov  ss, ax

   mov  eax, irq_stub_stacks[4*&IRQ]
   mov  esp, [eax]

   push ebx        ; save old stack on new stack
   push ecx

   cld
   push &IRQ
   call irq_umbrella_handler
   pop  edx

   pop  ecx        ; retrieve old stack
   pop  ebx

   mov  ss, bx
   mov  esp, ecx

   pop  gs
   pop  fs
   pop  es
   pop  ds
   popad
   iretd
ENDM

MAKE_WRAPPER 0
MAKE_WRAPPER 1
MAKE_WRAPPER 2
MAKE_WRAPPER 3
MAKE_WRAPPER 4
MAKE_WRAPPER 5
MAKE_WRAPPER 6
MAKE_WRAPPER 7
MAKE_WRAPPER 8
MAKE_WRAPPER 9
MAKE_WRAPPER 10
MAKE_WRAPPER 11
MAKE_WRAPPER 12
MAKE_WRAPPER 13
MAKE_WRAPPER 14
MAKE_WRAPPER 15

END

