#ifndef __PMODE_IRQ_H
#define __PMODE_IRQ_H

#if 0 /* !!to-do */
struct pt_regs {
       DWORD eax;
       DWORD ebx;
       DWORD ecx;
       DWORD edx;
       DWORD esi;
       DWORD edi;
     };

typedef void *IRQ_Handler (int irq, void *id, struct pt_regs *regs);
#endif

extern WORD irq_our_ds;  /* in intwrap.s */

extern int  autoirq_setup  (int usec);
extern int  autoirq_report (int usec);

extern int  request_irq (int irq, void (*handler)(int));
extern int  free_irq    (int irq)  LOCKED_FUNC;
extern void enable_irq  (int irq)  LOCKED_FUNC;
extern void disable_irq (int irq)  LOCKED_FUNC;
extern void irq_chain   (int irq)  LOCKED_FUNC;
extern int  irq_eoi_cmd (int irq)  LOCKED_FUNC;

extern void pcap_irq_stub_0 (void) LOCKED_FUNC;
extern void pcap_irq_stub_1 (void) LOCKED_FUNC;
extern void pcap_irq_stub_2 (void) LOCKED_FUNC;
extern void pcap_irq_stub_3 (void) LOCKED_FUNC;
extern void pcap_irq_stub_4 (void) LOCKED_FUNC;
extern void pcap_irq_stub_5 (void) LOCKED_FUNC;
extern void pcap_irq_stub_6 (void) LOCKED_FUNC;
extern void pcap_irq_stub_7 (void) LOCKED_FUNC;
extern void pcap_irq_stub_8 (void) LOCKED_FUNC;
extern void pcap_irq_stub_9 (void) LOCKED_FUNC;
extern void pcap_irq_stub_10(void) LOCKED_FUNC;
extern void pcap_irq_stub_11(void) LOCKED_FUNC;
extern void pcap_irq_stub_12(void) LOCKED_FUNC;
extern void pcap_irq_stub_13(void) LOCKED_FUNC;
extern void pcap_irq_stub_14(void) LOCKED_FUNC;
extern void pcap_irq_stub_15(void) LOCKED_FUNC;

#endif
