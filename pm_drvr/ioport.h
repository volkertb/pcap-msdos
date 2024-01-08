#ifndef __IOPORT_H
#define __IOPORT_H

/*
 * Macros necessary to bypass the limitation of Borland's BCC32 not
 * to allow inline port functions. Assignements should be volatile
 * to prevent the optimizer messing up. Check the .asm output to make
 * sure it doesn't.
 * NB! macro-args must be TASM/MASM compatible
 */

#if defined(__FLAT__)
#  define VOLATILE        volatile
#  define __in(p,t,z)     (_DX=(unsigned short)(p),__emit__(0xEC+z),(unsigned t)_AX)
#  define __out(p,x,z)    {_AX=(unsigned short)(x);_DX=(unsigned short)(p);__emit__(0xEE+z);}
#  define _inportb(p)     __in(p,char,0)
#  define _inportw(p)     __in(p,short,1)
#  define _outportb(p,x)  __out(p,x,0)
#  define _outportw(p,x)  __out(p,x,1)

#elif defined (__HIGHC__)
#  include <conio.h>
#  define _inportb(p)     _inb(p)
#  define _inportw(p)     _inw(p)
#  define _outportb(p,x)  _outb(p,x)
#  define _outportw(p,x)  _outw(p,x)
#  define VOLATILE

#elif defined(__DJGPP__)
#  include <pc.h>
#  define _inportb(p)     inportb(p)
#  define _inportw(p)     inportw(p)
#  define _inportl(p)     inportl(p)
#  define _outportb(p,x)  outportb(p,x)
#  define _outportw(p,x)  outportw(p,x)
#  define _outportl(p,x)  outportl(p,x)
#  define VOLATILE

#else
#  include <conio.h>
#  define _inportb(p)     inp(p)
#  define _inportw(p)     inp(p)
#  define _outportb(p,x)  outp(p,x)
#  define _outportw(p,x)  outpw(p,x)
#  define VOLATILE
#endif

/*
 *  REP INSx inliners + macros. Modified from Linux.
 */
#if defined(__GNUC__)
  extern inline void rep_insb (unsigned short port, unsigned char *buf, unsigned long bytes)
  {
     __asm__ __volatile__ ("cld ; rep ; insb"
                           : "=D" (buf), "=c" (bytes)
                           : "d" (port), "0" (buf), "1" (bytes));
  }
  extern inline void rep_insw (unsigned short port, unsigned short *buf, unsigned long words)
  {
     __asm__ __volatile__ ("cld ; rep ; insw"
                           : "=D" (buf), "=c" (words)
                           : "d" (port), "0" (buf), "1" (words));
  }
  extern inline void rep_insl (unsigned short port, unsigned long *buf, unsigned long dwords)
  {
     __asm__ __volatile__ ("cld ; rep ; insl"
                           : "=D" (buf), "=c" (dwords)
                           : "d" (port), "0" (buf), "1" (dwords));
  }
  extern inline void rep_outsb (unsigned short port, const unsigned char *buf, unsigned long bytes)
  {
    __asm__ __volatile__ ("cld ; rep ; outsb"
                          : "=S" (buf), "=c" (bytes)
                          : "d" (port), "0" (buf), "1" (bytes));
  }
  extern inline void rep_outsw (unsigned short port, const unsigned short *buf, unsigned long words)
  {
    __asm__ __volatile__ ("cld ; rep ; outsw"
                          : "=S" (buf), "=c" (words)
                          : "d" (port), "0" (buf), "1" (words));
  }
  extern inline void rep_outsl (unsigned short port, const unsigned long *buf, unsigned long dwords)
  {
    __asm__ __volatile__ ("cld ; rep ; outsl"
                          : "=S" (buf), "=c" (dwords)
                          : "d" (port), "0" (buf), "1" (dwords));
  }

#elif defined(__HIGHC__)   /* Use HighC inliners */
  #define rep_insb(port, buf, bytes)  _insb (port,(void*)buf,bytes)
  #define rep_insw(port, buf, words)  _insw (port,(void*)buf,2*(words))
  #define rep_insl(port, buf, dwords) _insd (port,(void*)buf,4*(dwords))
  #define rep_outsb(port,buf, bytes)  _outsb(port,(void*)buf,bytes)
  #define rep_outsw(port,buf, words)  _outsb(port,(void*)buf,2*(words))
  #define rep_outsl(port,buf, dwords) _outsd(port,(void*)buf,4*(dwords))

#elif defined(__WATCOMC__) /* !!to-do: make inliners */
#else
  extern void rep_insb  (unsigned port, unsigned char  *buf, int bytes);
  extern void rep_insw  (unsigned port, unsigned short  *buf, int words);
  extern void rep_insl  (unsigned port, unsigned long *buf, int dwords);
  extern void rep_outsb (unsigned port, unsigned char  *buf, int bytes);
  extern void rep_outsw (unsigned port, unsigned short  *buf, int words);
  extern void rep_outsl (unsigned port, unsigned long *buf, int dwords);
#endif

/*
 * Linux compatible I/O macros
 */
#define outb(val,port) _outportb (port, val)
#define outw(val,port) _outportw (port, val)
#define outl(val,port) _outportl (port, val)
#define inb(port)      _inportb (port)
#define inw(port)      _inportw (port)
#define inl(port)      _inportl (port)
#define insb(p,b,n)    rep_insb (p, (unsigned char*)(b), n)
#define outsb(p,b,n)   rep_outsb (p, (const unsigned char*)(b), n)
#define insw(p,b,n)    rep_insw (p, (unsigned short*)(b), n)
#define outsw(p,b,n)   rep_outsw (p, (const unsigned short*)(b), n)
#define insl(p,b,n)    rep_insl (p, (unsigned long*)(b), n)
#define outsl(p,b,n)   rep_outsl (p, (const unsigned long*)(b), n)

/*
 * Macros for small, nearly CPU independent delay
 */
#if defined(__GNUC__)
  #define __SLOW_DOWN_IO()  __asm__ __volatile__("outb %al, $0x80")
#elif defined(__HIGHC__)
  #define __SLOW_DOWN_IO()  _Inline (0xE6, 0x80)
#elif defined(__WATCOMC__)  /* !!to-do */
#else
  #define __SLOW_DOWN_IO()  _asm { out 0x80, al }
#endif

#define SLOW_DOWN_IO() do {                \
                         __SLOW_DOWN_IO(); \
                         __SLOW_DOWN_IO(); \
                       } while (0)

#endif /* __IOPORT_H */
